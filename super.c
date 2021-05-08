#include "tools.h"
#include "sunfs.h"
#include "file.h"
#include "sunfs_buddysystem.h"
#include "inode.h"

void get_sunfs_superblock(struct super_block *sb)
{
    struct sunfs_super_block *p_sunfs_sb = __va(PADDR_START);

    sb->s_maxbytes = MAX_LFS_FILESIZE;
    sb->s_blocksize = le32_to_cpu(p_sunfs_sb->s_blocksize);
    sb->s_blocksize_bits = PAGE_SHIFT;
}

static struct super_operations sunfs_ops = {
    .alloc_inode = sunfs_alloc_inode,
    .statfs = simple_statfs,
    .destroy_inode = sunfs_drop_inode,
};

/*     
 *       Function 'sunfs_init' is used for initializing sunfs_super_block & root sunfs_inode in persistent memory.     
 *       We do not receive any parameters in here because we do not have super_block_info
 *       So we can re-implement super_block_info later and modify this function.
 */
bool sunfs_init(void)
{
    memset((void *)__va(PADDR_START), 0, 64);
    memset((void *)__va(INODEZONE_START), 0, sizeof(struct sunfs_inode) * ((unsigned int)1 << 17));
    memset((void *)__va(LOGZONE_START), 0, (unsigned int)1 << 21);
    // We do not memset data zone because before we clear pages before we alloc them.

    struct sunfs_super_block *super;
    struct sunfs_inode *root;

    super = sunfs_get_super(); //return the first super_block which starts in PADDR_START
    root = sunfs_get_inode(SUNFS_ROOT_INO);

    // init sunfs_super_block
    super->s_blocksize = cpu_to_le32(1 << 12);
    super->free_inode = cpu_to_le32(1 << 18);
    super->s_magic = cpu_to_le32(SUNFS_SUPER_MAGIC); // actually I don't know what is MAGIC now.

    super->StartADDR = cpu_to_le64(__va(PADDR_START));
    super->head_log = cpu_to_le64(__va(LOGZONE_START));
    super->tail_log = cpu_to_le64(__va(LOGZONE_START));

    //init root sunfs_inode

    root->i_size = cpu_to_le64(0);
    root->i_uid = cpu_to_le32(from_kuid(&init_user_ns, current_fsuid()));
    root->i_gid = cpu_to_le32(from_kgid(&init_user_ns, current_fsgid()));
    root->i_mode = cpu_to_le16(S_IFDIR | 0755);
    root->i_atime = root->i_ctime = cpu_to_le32(get_seconds());

    root->pre_logitem = cpu_to_le64(0);
    struct sunfs_page *pg = sunfs_getpage(0);
    if (pg == NULL)
    {
        printk("We can not alloc page !\n");
        return 0;
    }
    root->ptr_PMD = cpu_to_le64(pg->vaddr);
    root->active = 1;

    return 1;
}

int sunfs_fill_super(struct super_block *sb, void *data, int silent)
{
    struct inode *root;
    int err;

    sunfs_init();
#ifdef DIRECTSET
    sb->s_maxbytes = MAX_LFS_FILESIZE;
    sb->s_blocksize = PAGE_SIZE;
    sb->s_blocksize_bits = PAGE_SHIFT;
#else
    struct sunfs_super_block *super = sunfs_get_super();
    sb->s_maxbytes = MAX_LFS_FILESIZE;
    sb->s_blocksize = le32_to_cpu(super->s_blocksize);
    sb->s_blocksize_bits = PAGE_SHIFT;
#endif
    sb->s_op = &sunfs_ops;
    
    sunfs_inode_table_init();
    printk("Make the root of sunfs.\n");

    root = sunfs_iget(sb, SUNFS_ROOT_INO);
    if (!root)
    {
        printk("We can not alloc ROOT inode for sunfs!\n");
        return -ENOMEM;
    }

    sb->s_root = d_make_root(root);
    if (!sb->s_root)
        return -ENOMEM;
    return 0;
}

static struct dentry *sunfs_mount(struct file_system_type *fs_type, int flags, const char *dev_nme, void *data)
{
    printk("Ready to mount sunfs to system.\n");
    return mount_nodev(fs_type, flags, data, sunfs_fill_super);
}

struct kmem_cache *sunfs_inode_cachep;

void InodeCacheInit(void)
{
    //note: we set ctor as NULL, may be incorect.
    sunfs_inode_cachep =
        kmem_cache_create("sunfs_inode_cache", sizeof(struct sunfs_inode_info), 0, 0, NULL);
    if (sunfs_inode_cachep == NULL)
        printk("Inode cache allocs error!\n");
    return;
}

void InodeCacheDestroy(void)
{
    kmem_cache_destroy(sunfs_inode_cachep);
    return;
}

static struct file_system_type sunfs_type =
    {
        .owner = THIS_MODULE,
        .name = "sun_fs",
        .mount = sunfs_mount,
        .kill_sb = kill_litter_super,
        //.fs_flags   =   FS_USERNS_MOUNT,
};

static int __init init_sunfs(void)
{
    //Init sunfs
    InitPageTableForSunfs(PADDR_START, PADDR_END);
    Buddysystem_init();
    InodeCacheInit();
    int err;
    printk("ready to register sunfs.\n");
    err = register_filesystem(&sunfs_type);
    if (!err)
        printk("register success.\n");
    testFunction();
    return err;
}

static void __exit exit_sunfs(void)
{
    printk("ready to unregister sunfs.\n");
    InodeCacheDestroy();
    unregister_filesystem(&sunfs_type);
}

MODULE_AUTHOR("Ruinan Sun");
MODULE_DESCRIPTION("My first attempt on file system");
MODULE_LICENSE("GPL");
module_init(init_sunfs);
module_exit(exit_sunfs);
