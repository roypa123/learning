/* ===========================================================================
 *  nimbus/fs/vfs.c  --  the virtual filesystem
 * ===========================================================================
 *
 *  The VFS is an interface, not a filesystem. It defines what a "file" is --
 *  a thing with read, write, open, close, readdir and finddir -- and then
 *  every real filesystem, every device and every pipe implements that
 *  interface. Above the VFS, nothing knows the difference.
 *
 *  This is the design that made Unix portable, and it is worth appreciating
 *  how small it is. Seven function pointers in a struct. That is the whole
 *  abstraction. `cat /etc/motd` and `cat /dev/console` take the same code
 *  path, differing only in which function pointer gets called, and neither
 *  `cat` nor the syscall layer nor the file descriptor table contains a single
 *  conditional distinguishing them.
 *
 *  Explained in: docs/39-vfs.md
 *  Line by line: docs/line-by-line/nimbus-vfs.md
 * =========================================================================== */

#include <nimbus/vfs.h>
#include <nimbus/fs.h>
#include <nimbus/heap.h>
#include <nimbus/kernel.h>
#include <nimbus/string.h>
#include <nimbus/task.h>
#include <nimbus/syscall.h>

vfs_node_t *vfs_root = NULL;

/* ---------------------------------------------------------------------------
 *  The dispatch layer
 *
 *  Six functions that all look the same: check the pointer exists, call it.
 *  The checks are the point. A filesystem that does not implement write leaves
 *  the pointer NULL, and without these guards the first write to a read-only
 *  filesystem would jump to address zero -- which, since we deliberately leave
 *  the first page unmapped, at least faults cleanly rather than executing
 *  whatever happens to be at 0. Still: a NULL check is cheaper than a page
 *  fault, and -EINVAL is a better diagnostic than a stack trace.
 * ------------------------------------------------------------------------- */

ssize_t vfs_read(vfs_node_t *node, off_t offset, size_t size, uint8_t *buf)
{
    if (!node || !node->read) return -EINVAL;
    return node->read(node, offset, size, buf);
}

ssize_t vfs_write(vfs_node_t *node, off_t offset, size_t size, const uint8_t *buf)
{
    if (!node || !node->write) return -EINVAL;
    return node->write(node, offset, size, buf);
}

int vfs_open(vfs_node_t *node, uint32_t flags)
{
    if (!node) return -ENOENT;
    node->refcount++;
    if (node->open) return node->open(node, flags);
    return 0;
}

int vfs_close(vfs_node_t *node)
{
    if (!node) return -EINVAL;

    if (node->refcount > 0) node->refcount--;
    if (node->close) return node->close(node);
    return 0;
}

dirent_t *vfs_readdir(vfs_node_t *node, uint32_t index)
{
    if (!node || !(node->flags & VFS_DIRECTORY) || !node->readdir) return NULL;

    /*  Follow a mount point. If something is mounted here, the directory the
     *  caller wants is the mounted filesystem's root, not the empty directory
     *  it was mounted over. Doing this in readdir and finddir -- rather than
     *  once at lookup time -- is what makes a mount point transparent.        */
    if (node->mounted) node = node->mounted;

    return node->readdir(node, index);
}

vfs_node_t *vfs_finddir(vfs_node_t *node, const char *name)
{
    if (!node || !(node->flags & VFS_DIRECTORY)) return NULL;
    if (node->mounted) node = node->mounted;
    if (!node->finddir) return NULL;

    return node->finddir(node, name);
}

/* ===========================================================================
 *  In-memory directories
 *
 *  The root, /dev, and any other synthetic directory. These have no backing
 *  store at all: a node, a list of children, and the three operations that a
 *  directory needs.
 * =========================================================================== */

/*  Children hang off a node's `device` field as a linked list. Reusing a
 *  generic scratch field rather than adding `struct vfs_node *children` to the
 *  node keeps the VFS structure free of any one implementation's private
 *  state -- which is exactly the discipline that lets FAT16 put a cluster
 *  number in the same slot.                                                   */
typedef struct ramdir_entry {
    vfs_node_t          *node;
    struct ramdir_entry *next;
} ramdir_entry_t;

static dirent_t ramdir_dirent;   /* returned by readdir; see the note below */

static dirent_t *ramdir_readdir(vfs_node_t *node, uint32_t index)
{
    ramdir_entry_t *e = (ramdir_entry_t *)node->device;

    for (uint32_t i = 0; e; e = e->next, i++) {
        if (i != index) continue;

        /*  A single static dirent, overwritten on every call. That makes
         *  readdir() non-reentrant and means the caller must copy the result
         *  before calling again -- which is exactly the contract the real
         *  POSIX readdir() has, and for exactly the same reason. It is a
         *  genuinely bad interface that we are matching on purpose, because
         *  it is the one every program expects.                               */
        strlcpy(ramdir_dirent.name, e->node->name, VFS_NAME_MAX);
        ramdir_dirent.inode = e->node->inode;
        ramdir_dirent.type  = e->node->flags;
        return &ramdir_dirent;
    }
    return NULL;
}

static vfs_node_t *ramdir_finddir(vfs_node_t *node, const char *name)
{
    for (ramdir_entry_t *e = (ramdir_entry_t *)node->device; e; e = e->next)
        if (strcmp(e->node->name, name) == 0)
            return e->node;
    return NULL;
}

vfs_node_t *vfs_make_directory(const char *name)
{
    vfs_node_t *dir = (vfs_node_t *)kcalloc(1, sizeof(vfs_node_t));
    if (!dir) return NULL;

    strlcpy(dir->name, name, VFS_NAME_MAX);
    dir->flags    = VFS_DIRECTORY;
    dir->readdir  = ramdir_readdir;
    dir->finddir  = ramdir_finddir;
    dir->refcount = 1;
    dir->device   = NULL;
    return dir;
}

int vfs_dir_add(vfs_node_t *dir, vfs_node_t *child)
{
    if (!dir || !(dir->flags & VFS_DIRECTORY) || !child) return -EINVAL;

    ramdir_entry_t *e = (ramdir_entry_t *)kmalloc(sizeof(ramdir_entry_t));
    if (!e) return -ENOMEM;

    e->node = child;
    e->next = (ramdir_entry_t *)dir->device;
    dir->device = e;
    return 0;
}

/* ===========================================================================
 *  Path resolution
 *
 *  The most security-sensitive function in any filesystem, and the one most
 *  often written casually.
 *
 *  Nimbus keeps it deliberately strict and deliberately simple:
 *
 *    * Only absolute paths, and paths relative to the task's cwd. There is no
 *      symlink following, because there are no symlinks.
 *    * "." is skipped. ".." is *not* implemented as a parent pointer -- we do
 *      not store one -- so it resolves by re-walking from the root, which is
 *      slow and completely immune to the class of bug where ".." escapes a
 *      mount point or a chroot.
 *    * A component longer than VFS_NAME_MAX is an error, not a truncation.
 *      Truncating means "/etc/passwd_backup" can be made to open
 *      "/etc/passwd", which is a real historical vulnerability.
 * ========================================================================= */

/*  Split a path into components, resolving as we go. `stop_before_last` makes
 *  the same walker serve both vfs_lookup and vfs_lookup_parent.               */
static vfs_node_t *walk(const char *path, bool stop_before_last, char *last_out)
{
    if (!path || !*path) return NULL;

    vfs_node_t *node;

    if (path[0] == '/') {
        node = vfs_root;
        path++;
    } else {
        node = (current_task && current_task->cwd) ? current_task->cwd : vfs_root;
    }

    if (!node) return NULL;

    /*  A path of just "/" resolves to the root and has no components.         */
    if (!*path) {
        if (stop_before_last) return NULL;
        return node;
    }

    char component[VFS_NAME_MAX];

    for (;;) {
        /*  Collect one component. */
        size_t len = 0;
        while (path[len] && path[len] != '/') {
            if (len >= VFS_NAME_MAX - 1) return NULL;   /* too long: refuse */
            len++;
        }

        memcpy(component, path, len);
        component[len] = '\0';

        path += len;
        while (*path == '/') path++;            /* collapse "//" and skip "/" */

        bool is_last = (*path == '\0');

        if (is_last && stop_before_last) {
            if (last_out) strlcpy(last_out, component, VFS_NAME_MAX);
            return node;
        }

        if (len == 0 || strcmp(component, ".") == 0) {
            /*  "." and an empty component (from a trailing slash) are no-ops. */
        } else if (strcmp(component, "..") == 0) {
            /*  We have no parent pointers, so ".." from anywhere other than a
             *  path we walked ourselves is not resolvable. Returning the root
             *  is wrong; returning NULL is honest. Chapter 39's exercises add
             *  a parent pointer and explain what it costs when a directory is
             *  renamed underneath you.                                        */
            if (node == vfs_root) {
                /* ".." at the root is the root, as on every Unix. */
            } else {
                return NULL;
            }
        } else {
            node = vfs_finddir(node, component);
            if (!node) return NULL;
        }

        if (is_last) return node;
    }
}

vfs_node_t *vfs_lookup(const char *path)
{
    return walk(path, false, NULL);
}

vfs_node_t *vfs_lookup_parent(const char *path, char *last_component)
{
    return walk(path, true, last_component);
}

int vfs_stat(const char *path, stat_t *out)
{
    vfs_node_t *node = vfs_lookup(path);
    if (!node) return -ENOENT;

    out->size  = node->length;
    out->flags = node->flags;
    out->inode = node->inode;
    return 0;
}

/* ---------------------------------------------------------------------------
 *  Mounting
 *
 *  Attach a filesystem's root node to an existing directory. From then on,
 *  every lookup that passes through that directory is redirected -- which is
 *  what `node->mounted` does, checked in vfs_finddir and vfs_readdir above.
 *
 *  The directory's original contents are not deleted, merely hidden. Unmount
 *  and they reappear. That behaviour surprises people the first time they
 *  mount over a non-empty directory, and it is the correct one: the mount is a
 *  view, not an edit.
 * ------------------------------------------------------------------------- */
int vfs_mount(const char *path, vfs_node_t *fs_root)
{
    vfs_node_t *target = vfs_lookup(path);
    if (!target) return -ENOENT;
    if (!(target->flags & VFS_DIRECTORY)) return -ENOTDIR;
    if (target->mounted) return -EBUSY;

    target->mounted = fs_root;
    target->flags  |= VFS_MOUNTPOINT;

    LOG_INFO("vfs: mounted %s at %s", fs_root->name, path);
    return 0;
}

void vfs_init(void)
{
    vfs_root = vfs_make_directory("/");
    if (!vfs_root) panic("vfs: could not create the root directory");

    LOG_INFO("vfs: root created");
}
