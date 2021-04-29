
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

struct inode *sunfs_get_inode(struct super_block *sb, const struct inode *dir, umode_t mode, dev_t dev)
{
    struct inode *inode = new_inode(sb);

    if (inode)
    {
        inode->i_ino = get_next_ino();
        INIT_LIST_HEAD(&inode->i_lru);
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
    }
    return inode;
}

int sunfs_mknod(struct inode *dir, struct dentry *dentry, umode_t mode, dev_t dev)
{
    struct inode *inode = sunfs_get_inode(dir->i_sb, dir, mode, dev);
    int error = -ENOSPC;
    if (inode)
    {
        d_instantiate(dentry, inode);
        dget(dentry);
        error = 0;
        dir->i_mtime = dir->i_ctime = current_time(dir);
    }
    return error;
}

int sunfs_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode)
{
    printk("Try to mkdir__by sunfs.\n");
    int error = sunfs_mknod(dir, dentry, mode | S_IFDIR, 0);
    if (!error)
        inc_nlink(dir);
    return error;
}

int sunfs_create(struct inode *dir, struct dentry *dentry, umode_t mode, bool excl)
{
    printk("Try to create a file__by sunfs.\n");
    return sunfs_mknod(dir, dentry, mode | S_IFREG, 0);
}
