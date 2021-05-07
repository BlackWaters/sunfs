#include "inode.h"

struct address_space_operations sunfs_aops =
    {
        .readpage = simple_readpage,
        .write_begin = simple_write_begin,
        .write_end = simple_write_end,
};

const struct file_operations sunfs_file_ops =
    {
        .read = sunfs_file_read,
        .write = sunfs_file_write,
        .read_iter = generic_file_read_iter,
        .write_iter = generic_file_write_iter,
        .mmap = generic_file_mmap,
        .llseek = generic_file_llseek,
};

const struct inode_operations sunfs_inode_file_ops =
    {
        .setattr = simple_setattr,
        .getattr = simple_getattr,
};

const struct inode_operations sunfs_dir_ops =
    {
        .create = sunfs_create,
        .lookup = simple_lookup,
        .link = simple_link,
        .unlink = simple_unlink,
        .mkdir = sunfs_mkdir,
        .mknod = sunfs_mknod,
        .rmdir = simple_rmdir,
        .rename = simple_rename,
};

struct inode *sunfs_alloc_inode(struct super_block *sb)
{
    struct sunfs_inode_info *info =
        kmem_cache_alloc(sunfs_inode_cachep, GFP_KERNEL);
    if (info == NULL)
    {
        printk("Error in sunfs_alloc_inode.\n");
        return NULL;
    }
    info->fpmd = InitFirstPage();
    info->num_pages = 0;
    //we do not set ino in this function
    inode_init_once(&info->vfs_inode);
    return &info->vfs_inode;
}

void sunfs_drop_inode(struct inode *inode)
{
    struct sunfs_inode_info *info = SUNFS_INIFO(inode);
    delete_pages(info);
    kmem_cache_free(sunfs_inode_cachep, info);
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
    /*
     *  get sunfs_inode from inode table
     */
    si = sunfs_get_inode(ino);
    if (!si)
        return ERR_PTR(-ENOMEM);
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
    info->fpmd = pg->vaddr;
    si->ptr_PMD = cpu_to_le64(info->fpmd);
    sunfs_update_inode(inode, si);
    kfree(pg);
    return inode;
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

}

void sunfs_inode_table_init()
{
    unsigned int step=sizeof(struct sunfs_inode);
    unsigned int addr=0;
    for (addr=__va(INODEZONE_START);addr<__va(DATAZONE_START);addr+=step)
    {
        struct sunfs_inode *si=(struct sunfs_inode *)i;
                
    }
}
