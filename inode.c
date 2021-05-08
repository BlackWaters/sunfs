#include "inode.h"

struct rb_root sunfs_inode_table_root;
struct mutex inode_table_lock;
struct kmem_cache *sunfs_inotree_cachep;

struct address_space_operations sunfs_aops = {
    .readpage = simple_readpage,
    .write_begin = simple_write_begin,
    .write_end = simple_write_end,
};

const struct file_operations sunfs_file_ops = {
    .read = sunfs_file_read,
    .write = sunfs_file_write,
    .read_iter = generic_file_read_iter,
    .write_iter = generic_file_write_iter,
    .mmap = generic_file_mmap,
    .llseek = generic_file_llseek,
};

const struct inode_operations sunfs_inode_file_ops = {
    .setattr = simple_setattr,
    .getattr = simple_getattr,
};

const struct inode_operations sunfs_dir_ops = {
    .create = sunfs_create,
    .lookup = simple_lookup,
    .link = simple_link,
    .unlink = simple_unlink,
    .mkdir = sunfs_mkdir,
    .mknod = sunfs_mknod,
    .rmdir = simple_rmdir,
    .rename = simple_rename,
};

/*
 *  Note: Only alloc vfs_inode for sunfs.
 */
struct inode *sunfs_alloc_inode(struct super_block *sb)
{
    struct sunfs_inode_info *info =
        kmem_cache_alloc(sunfs_inode_cachep, GFP_KERNEL);
    if (info == NULL)
    {
        printk("Error in sunfs_alloc_inode.\n");
        return NULL;
    }
    info->fpmd = InitFirstPage(); //NULL
    info->num_pages = 0;
    //we do not set ino in this function
    inode_init_once(&info->vfs_inode);
    return &info->vfs_inode;
}

/*
 *  Free sunfs_inode_info, delete pages, free ino, and free sunfs_inode
 */
void sunfs_drop_inode(struct inode *inode)
{
    struct sunfs_inode_info *info = SUNFS_INIFO(inode);
    unsigned int ino = info->vfs_inode.i_ino;
    delete_pages(info);
    kmem_cache_free(sunfs_inode_cachep, info);
    //free sunfs_inode
    sunfs_free_ino(ino, ino);
    sunfs_free_inode(sunfs_get_inode(ino));
    return;
}

// get inode from sunfs with ino
struct inode *sunfs_iget(struct super_block *sb, unsigned int ino)
{
    struct inode *inode;
    struct sunfs_inode *si;
    int err;

    inode = iget_locked(sb, ino);
    if (!inode)
        return ERR_PTR(-ENOMEM);
    if (!(inode->i_state & I_NEW)) //old inode, just return.
        return inode;
    //  New inode, we should init this inode
    //  make sure sunfs_inode was initialized before

    si = sunfs_get_inode(ino);
    if (!si)
    {
        err = -EACCES;
        goto fail;
    }
    err = sunfs_read_inode(inode, si);
    if (err)
        goto fail;
    inode->i_ino = ino;
    unlock_new_inode(inode);
    return inode;
fail:
    iget_failed(inode);
    return ERR_PTR(err);
}

/*
 *  Read sunfs_inode, and use it to update vfs_inode
 */
unsigned int sunfs_read_inode(struct inode *inode, struct sunfs_inode *si)
{
    printk("enter sunfs_read_inode");
    inode->i_size = le64_to_cpu(si->i_size);
    inode->i_mode = le16_to_cpu(si->i_mode);
    i_uid_write(inode, le32_to_cpu(si->i_uid));
    i_gid_write(inode, le32_to_cpu(si->i_gid));
    inode->i_atime.tv_sec = le32_to_cpu(si->i_atime);
    inode->i_ctime.tv_sec = le32_to_cpu(si->i_ctime);
    inode->i_atime.tv_nsec = inode->i_ctime.tv_nsec = 0;

    // init inode operations
    inode->i_mapping->a_ops = &sunfs_aops;
    switch (inode->i_mode & S_IFMT)
    {
    default:
        init_special_inode(inode, inode->i_mode, inode->i_rdev);
        break;
    case S_IFREG:
        inode->i_op = &sunfs_inode_file_ops;
        inode->i_fop = &sunfs_file_ops;
        break;
    case S_IFDIR:
        inode->i_op = &sunfs_dir_ops;
        inode->i_fop = &simple_dir_operations;
        set_nlink(inode, 2); // directly set 2 here
        break;
    case S_IFLNK:
        break;
    }
    return 0;
}

struct inode *sunfs_new_inode(struct super_block *sb, const struct inode *dir, umode_t mode, dev_t dev)
{
    struct inode *inode = new_inode(sb);
    if (!inode)
        return ERR_PTR(-ENOMEM);

    //init inode
    inode_init_owner(inode, dir, mode);
    inode->i_mapping->a_ops = &sunfs_aops;
    //mapping_set_gfp_mask(inode->i_mapping, GFP_HIGHUSER);
    //mapping_set_unevictable(inode->i_mapping);
    inode->i_atime = inode->i_mtime = inode->i_ctime = current_time(inode);
    switch (mode & S_IFMT)
    {
    default:
        init_special_inode(inode, mode, dev);
        break;
    case S_IFREG:
        inode->i_op = &sunfs_inode_file_ops;
        inode->i_fop = &sunfs_file_ops;
        break;
    case S_IFDIR:
        inode->i_op = &sunfs_dir_ops;
        inode->i_fop = &simple_dir_operations;
        inc_nlink(inode);
        break;
    case S_IFLNK:
        break;
    }

    // get sunfs_inode and initialize
    unsigned int ino;
    struct sunfs_inode *si;
    struct sunfs_inode_info *info;
    //  get sunfs_inode from inode table
    ino = sunfs_get_newino();
    if (ino == -1)
    {
        printk("Can not get ino!\n");
        return ERR_PTR(-ENOMEM);
    }
    // update inode->i_ino
    inode->i_ino = ino;
    si = sunfs_get_inode(ino);
    if (!si)
        return ERR_PTR(-ENOMEM);

    // For data node, we can alloc file page until we need it.
    if ((mode & S_IFMT) == S_IFREG)
        goto Update;

    struct sunfs_page *pg;
    pg = sunfs_getpage(0);
    if (!pg)
    {
        printk("We can not alloc page!\n");
        return ERR_PTR(-ENOMEM);
    }
    info = SUNFS_INIFO(inode);
    if (!info)
    {
        printk("We can not get sunfs_indo_info!\n");
        return ERR_PTR(-ENOMEM);
    }
    // new alloc page, update related var
    info->fpmd = pg->vaddr;
    si->ptr_PMD = cpu_to_le64(info->fpmd);
    kfree(pg);
Update:
    // set inode active
    si->active = 1;
    sunfs_update_inode(inode, si);
    return inode;
}

void sunfs_free_inode(struct sunfs_inode *si)
{
    /*
     * free log here ...
     * free si->si->pre_logitem and so on ...
     */
    memset(si, 0, sizeof(struct sunfs_inode));
}

void sunfs_update_inode(struct inode *inode, struct sunfs_inode *si)
{
    si->i_mode = cpu_to_le16(inode->i_mode);

    si->i_uid = cpu_to_le32(i_uid_read(inode));
    si->i_gid = cpu_to_le32(i_gid_read(inode));
    si->i_atime = cpu_to_le32(inode->i_atime.tv_sec);
    si->i_ctime = cpu_to_le32(inode->i_ctime.tv_sec);

    si->i_size = cpu_to_le64(inode->i_size);
    si->pre_logitem = cpu_to_le64(0);

    //We do not set pmd in here
}

int sunfs_mknod(struct inode *dir, struct dentry *dentry, umode_t mode, dev_t dev)
{
    struct inode *inode = sunfs_new_inode(dir->i_sb, dir, mode, dev); // get new inode and sunfs_inode, initialized.
    int error = -ENOSPC;
    if (inode)
    {
        d_instantiate(dentry, inode);
        dget(dentry);
        error = 0;
        dir->i_mtime = dir->i_ctime = current_time(dir);
        // We need write back to sunfs_inode of dir, need log here.
        // Now we just write back to sunfs_inode directly.
        sunfs_update_time(dir);
    }
    return error;
}

int sunfs_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode)
{
    printk("Try to mkdir__by sunfs.\n");
    int error = sunfs_mknod(dir, dentry, mode | S_IFDIR, 0);
    if (!error)
        inc_nlink(dir); //we do not have nlink in sunfs_inode now.
    return error;
}

int sunfs_create(struct inode *dir, struct dentry *dentry, umode_t mode, bool excl)
{
    printk("Try to create a file__by sunfs.\n");
    return sunfs_mknod(dir, dentry, mode | S_IFREG, 0);
}

void sunfs_update_time(struct inode *inode)
{
    unsigned int ino = inode->i_ino;
    struct sunfs_inode *si = sunfs_get_inode(ino);
    if (!si)
    {
        printk("We can not get sunfs_inode in 'sunfs_update_time'!\n");
        return;
    }
    si->i_atime = cpu_to_le32(inode->i_atime.tv_sec);
    si->i_ctime = cpu_to_le32(inode->i_ctime.tv_sec);
    return;
}

int sunfs_get_newino()
{
    struct rb_node *node;
    node = rb_first(&sunfs_inode_table_root);
    if (node == NULL)
    {
        printk("Inotree is empty!\n");
        return -1;
    }
    struct sunfs_inotree *data = container_of(node, struct sunfs_inotree, node);
    unsigned int ret = data->l;
    data->l++;
    if (data->l > data->r)
    {
        printk("This inotree is empty, erase.\n");
        rb_erase(node, &sunfs_inode_table_root);
        kmem_cache_free(sunfs_inotree_cachep, data);
    }
    printk("Get new ino: %d\n", ret);
    return ret;
}

bool sunfs_free_ino(unsigned int l, unsigned int r)
{
    struct sunfs_inotree *data = kmem_cache_alloc(sunfs_inotree_cachep, GFP_KERNEL);
    if (!data)
    {
        printk("We can not alloc sunfs_inotree from slab!\n");
        return 0;
    }
    data->l = l;
    data->r = r;
    bool ret = sunfs_inode_table_insert(&sunfs_inode_table_root, data);
    if (!ret)
        printk("Insert inotree error!\n");
    return ret;
}

bool sunfs_inode_table_init(void)
{
    unsigned int step = sizeof(struct sunfs_inode);
    unsigned long addr = 0;
    unsigned int l = 1, r = 0, now_ino = 0;
    struct sunfs_inotree *new;

    mutex_init(&inode_table_lock);
    sunfs_inode_table_root.rb_node = NULL;

    sunfs_inotree_cachep = kmem_cache_create("sunfs_inotree_cache", sizeof(struct sunfs_inotree), 0, 0, NULL);
    if (sunfs_inotree_cachep == NULL)
        printk("Inotree Cache allocs error!\n");

    for (addr = __va(INODEZONE_START + step); addr < __va(DATAZONE_START); addr += step)
    {
        now_ino++;
        struct sunfs_inode *si = (struct sunfs_inode *)addr;
        bool flag = si->active;
        if (!flag)
            //This inode is not active, expand r
            r++;
        else
        {
            //This inode is active
            if (l <= r)
            {
                new = kmem_cache_alloc(sunfs_inotree_cachep, GFP_KERNEL);
                if (new == NULL)
                {
                    printk("Can not alloc inotree from slab");
                    return false;
                }
                new->l = l;
                new->r = r;
                printk("Insert segment [%d,%d]\n", l, r);
                sunfs_inode_table_insert(&sunfs_inode_table_root, new);
            }
            //update [l,r]
            l = now_ino + 1;
            r = now_ino;
        }
    }
    if (l <= r)
    {
        new = kmem_cache_alloc(sunfs_inotree_cachep, GFP_KERNEL);
        if (new == NULL)
        {
            printk("Can not alloc inotree from slab");
            return false;
        }
        new->l = l;
        new->r = r;
        printk("Insert segment [%d,%d]\n", l, r);
        sunfs_inode_table_insert(&sunfs_inode_table_root, new);
    }
    return true;
}

bool inotree_seg_cross(const struct sunfs_inotree *a, const struct sunfs_inotree *b)
{
    if (a->r >= b->l && a->l <= b->r)
        return true;
    return false;
}

bool sunfs_inode_table_insert(struct rb_root *root, struct sunfs_inotree *data)
{
    struct rb_node **new = &(root->rb_node), *parent = NULL, *prev, *next;
    struct sunfs_inotree *pa, *tmp;

    while (*new)
    {
        struct sunfs_inotree *cur = container_of(*new, struct sunfs_inotree, node);
        parent = *new;
        /*
        if (inotree_seg_cross(cur,data)) 
        {
            printk("Insert inotree, segment cross!\n");
            return false; // seg cross, error!
        }*/
        if (data->r < cur->l)
            new = &((*new)->rb_left);
        else if (data->l > cur->r)
            new = &((*new)->rb_right);
        else
        {
            printk("Insert inotree, segment cross!\n");
            return false; // seg cross, error!
        }
    }
    if (parent == NULL)
        goto Insert;
    //before insert, check whether we can merge this node with prev/next
    pa = container_of(parent, struct sunfs_inotree, node);
    if (data->r < pa->l)
    {
        if (data->r + 1 == pa->l)
        {
            pa->l = data->l;
            prev = rb_prev(parent);
            if (prev)
            {
                tmp = container_of(prev, struct sunfs_inotree, node);
                if (tmp->r + 1 == pa->l) //we can merge prev inotree
                    tmp->r = pa->r;
                rb_erase(parent, &sunfs_inode_table_root);
                kmem_cache_free(sunfs_inotree_cachep, pa);
            }
            return true;
        }
    }
    else
    {
        if (pa->r + 1 == data->l)
        {
            pa->r = data->r;
            next = rb_next(parent);
            if (next)
            {
                tmp = container_of(next, struct sunfs_inotree, node);
                if (tmp->l == pa->r + 1) //we can merge next inotree
                    tmp->l = pa->l;
                rb_erase(parent, &sunfs_inode_table_root);
                kmem_cache_free(sunfs_inotree_cachep, pa);
            }
            return true;
        }
    }
Insert:
    //Can't merge, just insert new node
    rb_link_node(&data->node, parent, new);
    rb_insert_color(&data->node, root);
    return true;
}
