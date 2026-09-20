/* ===========================================================================
 *  nimbus/include/nimbus/vfs.h  --  the virtual filesystem
 * ===========================================================================
 *
 *  The VFS is the idea that made Unix portable and it is one of the great
 *  pieces of software design: every filesystem, every device and every pipe
 *  presents the same seven operations, so `cat` does not know or care whether
 *  its argument is a FAT16 file, a tar entry in the initrd, the keyboard, or
 *  the write end of a pipe.
 *
 *  Underneath, a vfs_node is a struct with function pointers. That is all
 *  polymorphism is, and writing it out in C -- rather than inheriting it from
 *  a language feature -- makes the cost and the mechanism visible.
 *
 *  Explained in: docs/39-vfs.md
 * =========================================================================== */
#ifndef NIMBUS_VFS_H
#define NIMBUS_VFS_H

#include <nimbus/types.h>

#define VFS_NAME_MAX    64
#define VFS_PATH_MAX    256

#define VFS_FILE        0x01
#define VFS_DIRECTORY   0x02
#define VFS_CHARDEVICE  0x04
#define VFS_BLOCKDEVICE 0x08
#define VFS_PIPE        0x10
#define VFS_SYMLINK     0x20
#define VFS_MOUNTPOINT  0x40

struct vfs_node;

typedef ssize_t (*vfs_read_t)  (struct vfs_node *node, off_t offset, size_t size, uint8_t *buf);
typedef ssize_t (*vfs_write_t) (struct vfs_node *node, off_t offset, size_t size, const uint8_t *buf);
typedef int     (*vfs_open_t)  (struct vfs_node *node, uint32_t flags);
typedef int     (*vfs_close_t) (struct vfs_node *node);

/*  readdir returns entry number `index` of a directory, or NULL past the end.
 *  An index rather than an opaque cursor because it is simple and because our
 *  filesystems can all seek to the nth entry cheaply. Real kernels use a
 *  cookie, because on a B-tree filesystem "the nth entry" is not cheap and is
 *  not even stable across a concurrent create.                                */
typedef struct dirent *(*vfs_readdir_t)(struct vfs_node *node, uint32_t index);
typedef struct vfs_node *(*vfs_finddir_t)(struct vfs_node *node, const char *name);
typedef int     (*vfs_create_t)(struct vfs_node *dir, const char *name, uint32_t flags);
typedef int     (*vfs_unlink_t)(struct vfs_node *dir, const char *name);

typedef struct dirent {
    char     name[VFS_NAME_MAX];
    uint32_t inode;
    uint32_t type;
} dirent_t;

typedef struct stat {
    uint32_t size;
    uint32_t flags;
    uint32_t inode;
} stat_t;

typedef struct vfs_node {
    char          name[VFS_NAME_MAX];
    uint32_t      flags;          /* VFS_FILE, VFS_DIRECTORY, ...             */
    uint32_t      inode;          /* meaningful only to the driver that owns it */
    uint32_t      length;         /* size in bytes                            */
    uint32_t      permissions;
    uint32_t      refcount;

    /*  `impl` and `device` are the driver's two scratch fields. FAT16 puts the
     *  starting cluster in one and a pointer to its mount state in the other.
     *  Giving the driver typed storage instead would mean the VFS knowing
     *  about every filesystem, which is the coupling the VFS exists to avoid. */
    uint32_t      impl;
    void         *device;

    vfs_read_t    read;
    vfs_write_t   write;
    vfs_open_t    open;
    vfs_close_t   close;
    vfs_readdir_t readdir;
    vfs_finddir_t finddir;
    vfs_create_t  create;
    vfs_unlink_t  unlink;

    struct vfs_node *mounted;     /* if a filesystem is mounted here          */
} vfs_node_t;

/*  An *open* file is not a node. Two processes can have the same file open at
 *  different offsets, and after fork() a parent and child share one offset --
 *  so the offset lives here, in a refcounted object between the fd table and
 *  the node. Getting this split wrong is the classic beginner mistake and
 *  Chapter 43 shows exactly which programs break.                             */
typedef struct file {
    vfs_node_t *node;
    off_t       offset;
    uint32_t    flags;
    uint32_t    refcount;
} file_t;

extern vfs_node_t *vfs_root;

void        vfs_init(void);

ssize_t     vfs_read(vfs_node_t *node, off_t offset, size_t size, uint8_t *buf);
ssize_t     vfs_write(vfs_node_t *node, off_t offset, size_t size, const uint8_t *buf);
int         vfs_open(vfs_node_t *node, uint32_t flags);
int         vfs_close(vfs_node_t *node);
dirent_t   *vfs_readdir(vfs_node_t *node, uint32_t index);
vfs_node_t *vfs_finddir(vfs_node_t *node, const char *name);

/*  Path resolution: turn "/home/roy/notes.txt" into a node, walking mount
 *  points as it goes. The single most security-sensitive function in a
 *  filesystem, and Chapter 39 walks through why ".." is harder than it looks. */
vfs_node_t *vfs_lookup(const char *path);
vfs_node_t *vfs_lookup_parent(const char *path, char *last_component);

int         vfs_mount(const char *path, vfs_node_t *fs_root);
int         vfs_stat(const char *path, stat_t *out);

/*  An in-memory directory node, used for the root and for synthetic
 *  directories like /dev.                                                     */
vfs_node_t *vfs_make_directory(const char *name);
int         vfs_dir_add(vfs_node_t *dir, vfs_node_t *child);

#endif /* NIMBUS_VFS_H */
