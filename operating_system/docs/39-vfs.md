# Chapter 39 — A virtual filesystem

[← Partitions](38-block-layer.md) · [Contents](README.md) · [Next: The initrd →](40-initrd.md)

> 📖 **Line by line:** [vfs.c](line-by-line/nimbus-vfs.md)

---

## Goal

Build the abstraction that makes a FAT16 file, the keyboard, and the write end of a pipe
interchangeable. Seven function pointers in a struct, and path resolution that does not let anyone
escape.

---

## 1. The idea

The VFS is the idea that made Unix portable, and it is one of the great pieces of software design.

> every filesystem, every device and every pipe presents the same seven operations, so `cat` does not
> know or care whether its argument is a FAT16 file, a tar entry in the initrd, the keyboard, or the
> write end of a pipe.

Look at `cat`:

```c
static int cat_fd(int fd)
{
    char buf[512];
    ssize_t n;

    while ((n = read(fd, buf, sizeof(buf))) > 0)
        write(STDOUT_FILENO, buf, (size_t)n);

    return n < 0 ? 1 : 0;
}
```

Nine lines, no conditionals, and it works on all four. `cat /mnt/file`, `cat /dev/console`,
`ls | cat`, `cat < file` — the same nine lines.

Underneath, a `vfs_node` is a struct with function pointers.

> That is all polymorphism is, and writing it out in C — rather than inheriting it from a language
> feature — makes the cost and the mechanism visible.

---

## 2. The node

```c
typedef struct vfs_node {
    char          name[VFS_NAME_MAX];
    uint32_t      flags;
    uint32_t      inode;
    uint32_t      length;
    uint32_t      permissions;
    uint32_t      refcount;

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

    struct vfs_node *mounted;
} vfs_node_t;
```

### 2.1 The two scratch fields

```c
    /*  `impl` and `device` are the driver's two scratch fields. FAT16 puts the
     *  starting cluster in one and a pointer to its mount state in the other.
     *  Giving the driver typed storage instead would mean the VFS knowing
     *  about every filesystem, which is the coupling the VFS exists to avoid. */
```

What each driver puts there:

| Driver | `impl` | `device` |
|---|---|---|
| FAT16 file | starting cluster | `fat_fs_t *` |
| FAT16 directory | cluster, or `ROOT_DIR_SENTINEL` | `fat_fs_t *` |
| initrd file | (unused) | `initrd_file_t *` |
| in-memory directory | (unused) | head of a child list |
| pipe | 0 = read end, 1 = write end | `pipe_t *` |
| console | (unused) | (unused) |

Untyped storage is the price of not coupling the VFS to its drivers. The alternative — a union, or a
`void *private` with a per-driver struct — is what Linux does, and it is cleaner at the cost of a
second allocation per node.

### 2.2 The flags

```c
#define VFS_FILE        0x01
#define VFS_DIRECTORY   0x02
#define VFS_CHARDEVICE  0x04
#define VFS_BLOCKDEVICE 0x08
#define VFS_PIPE        0x10
#define VFS_SYMLINK     0x20
#define VFS_MOUNTPOINT  0x40
```

Flags rather than an enum, because a node can be more than one thing — a mount point is a directory
*and* a mount point.

`VFS_CHARDEVICE` earns its place in the syscall layer:

```c
    ssize_t n = vfs_read(f->node, f->offset, len, (uint8_t *)buf);
    if (n > 0 && !(f->node->flags & VFS_CHARDEVICE)) f->offset += (off_t)n;
```

Chapter 20, §5.1: a console is not seekable, so its offset must not advance.

---

## 3. The dispatch layer

```c
ssize_t vfs_read(vfs_node_t *node, off_t offset, size_t size, uint8_t *buf)
{
    if (!node || !node->read) return -EINVAL;
    return node->read(node, offset, size, buf);
}
```

Six functions that all look the same. The checks are the point:

> A filesystem that does not implement write leaves the pointer NULL, and without these guards the
> first write to a read-only filesystem would jump to address zero — which, since we deliberately
> leave the first page unmapped, at least faults cleanly rather than executing whatever happens to be
> at 0. Still: a NULL check is cheaper than a page fault, and `-EINVAL` is a better diagnostic than a
> stack trace.

That last clause is the argument. Both behaviours are safe; one produces an error code and one
produces a panic.

### 3.1 Following mounts

```c
dirent_t *vfs_readdir(vfs_node_t *node, uint32_t index)
{
    if (!node || !(node->flags & VFS_DIRECTORY) || !node->readdir) return NULL;

    if (node->mounted) node = node->mounted;

    return node->readdir(node, index);
}
```

> If something is mounted here, the directory the caller wants is the mounted filesystem's root, not
> the empty directory it was mounted over. Doing this in readdir and finddir — rather than once at
> lookup time — is what makes a mount point transparent.

Two lines, in two functions, and `/mnt` behaves like a directory that happens to contain the disk.

The alternative — resolving mounts during path walking — means every caller that holds a node has to
wonder whether it is the mount point or the mounted root. Doing it at the operation makes the
question disappear.

---

## 4. In-memory directories

The root and `/dev` have no backing store. A node, a list of children, three operations.

```c
typedef struct ramdir_entry {
    vfs_node_t          *node;
    struct ramdir_entry *next;
} ramdir_entry_t;
```

```c
    /*  Children hang off a node's `device` field as a linked list. Reusing a
     *  generic scratch field rather than adding `struct vfs_node *children` to
     *  the node keeps the VFS structure free of any one implementation's
     *  private state — which is exactly the discipline that lets FAT16 put a
     *  cluster number in the same slot. */
```

An in-memory directory is *a driver*, with no special status. That discipline is worth holding to: as
soon as the VFS struct grows a field that only one implementation uses, it grows three more.

### 4.1 The static dirent, deliberately

```c
static dirent_t ramdir_dirent;

static dirent_t *ramdir_readdir(vfs_node_t *node, uint32_t index)
{
    ...
        strlcpy(ramdir_dirent.name, e->node->name, VFS_NAME_MAX);
        ...
        return &ramdir_dirent;
}
```

> A single static dirent, overwritten on every call. That makes readdir() non-reentrant and means the
> caller must copy the result before calling again — which is exactly the contract the real POSIX
> readdir() has, and for exactly the same reason. It is a genuinely bad interface that we are
> matching on purpose, because it is the one every program expects.

POSIX's `readdir` returns a pointer to storage the implementation owns, valid until the next call.
That is why `readdir_r` exists, and why it was then deprecated for being worse.

Our syscall layer copies immediately:

```c
    dirent_t *entry = vfs_readdir(f->node, regs->edx);
    if (!entry) return 0;

    if (!copy_to_user((void *)regs->ecx, entry, sizeof(dirent_t)))
        return -EFAULT;
```

so userland never sees the aliasing.

---

## 5. Path resolution

> The most security-sensitive function in any filesystem, and the one most often written casually.

```c
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
    ...
```

One walker, two entry points:

```c
vfs_node_t *vfs_lookup(const char *path)          { return walk(path, false, NULL); }
vfs_node_t *vfs_lookup_parent(const char *p, char *l) { return walk(p, true, l); }
```

`vfs_lookup_parent` is what `create` and `unlink` need: the directory, plus the final component as a
string, because the thing being named does not exist yet.

### 5.1 Component extraction, and the refusal

```c
        size_t len = 0;
        while (path[len] && path[len] != '/') {
            if (len >= VFS_NAME_MAX - 1) return NULL;   /* too long: refuse */
            len++;
        }
```

**A component longer than `VFS_NAME_MAX` is an error, not a truncation.**

> Truncating means "/etc/passwd_backup" can be made to open "/etc/passwd", which is a real historical
> vulnerability.

Silent truncation turns a name the user chose into a *different* name that the system trusts. Every
filesystem that truncated has had this bug.

### 5.2 Collapsing separators

```c
        path += len;
        while (*path == '/') path++;            /* collapse "//" and skip "/" */
```

`/a//b` and `/a/b` are the same path. `/a/b/` is `/a/b`.

Not collapsing means `//etc/passwd` produces an empty component, which some code treats as "." and
some as an error, and the disagreement is where bugs live.

### 5.3 `.` and `..`

```c
        if (len == 0 || strcmp(component, ".") == 0) {
            /*  "." and an empty component (from a trailing slash) are no-ops. */
        } else if (strcmp(component, "..") == 0) {
            if (node == vfs_root) {
                /* ".." at the root is the root, as on every Unix. */
            } else {
                return NULL;
            }
        } else {
            node = vfs_finddir(node, component);
            if (!node) return NULL;
        }
```

We have no parent pointers, so `..` is not resolvable and returns NULL.

> Chapter 39's exercises add a parent pointer and explain what it costs when a directory is renamed
> underneath you.

That cost is real and it is why `..` is hard. If `/a/b` is open and `/a/b` is then moved to `/c/b`, a
cached parent pointer is stale. Real filesystems resolve `..` through the *dentry cache*, which is
invalidated on rename — a whole subsystem.

`..` at the root being the root is universal: `cd /..` lands in `/`. It is what stops a path escaping
upwards, and it is also why chroot works.

### 5.4 What is deliberately absent

```
 *    * Only absolute paths, and paths relative to the task's cwd. There is no
 *      symlink following, because there are no symlinks.
```

**No symlinks**, so no loop detection, no `ELOOP`, no symlink-race attacks.

Symlinks are the source of most filesystem security bugs, because they let a path mean something
different at resolution time than it did at check time. `open("/tmp/x")` where `/tmp/x` is a symlink
to `/etc/passwd`, created between a check and the open, is the classic TOCTOU attack.

Adding them means: a `VFS_SYMLINK` flag, following in `walk` with a depth limit, and an `O_NOFOLLOW`
for callers who must not. Exercise 39.7.

---

## 6. Mounting

```c
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
```

One pointer.

> The directory's original contents are not deleted, merely hidden. Unmount and they reappear. That
> behaviour surprises people the first time they mount over a non-empty directory, and it is the
> correct one: the mount is a view, not an edit.

This is why `mount /dev/sdb1 /home` on a machine whose `/home` already has files makes them
inaccessible but not lost, and why unmounting brings them back.

### 6.1 No unmount

`vfs_umount` does not exist, and the reason is instructive: unmounting safely requires knowing that
nothing is using the filesystem.

That means: no open files on it, no process with a cwd inside it, no cached nodes referencing it. A
refcount per filesystem, decremented in every close, and `-EBUSY` when it is non-zero.

Ours has `refcount` on nodes but nothing aggregates it per mount. Exercise 39.5.

---

## 7. The cost of the abstraction

Worth being explicit, because "abstraction is free" is not true.

**An indirect call per operation.** `node->read(...)` is a load and an indirect branch, which the
branch predictor handles less well than a direct call. Perhaps 5–20 cycles against a disk read of
300,000. Irrelevant.

**Eight pointers per node**, 32 bytes of a ~120-byte struct. A shared operations table — one `struct
vfs_ops *` instead of eight pointers — is what Linux does and saves 28 bytes per node at the cost of
one more indirection. At a few hundred nodes, ours is fine.

**No caching.** `vfs_finddir` calls straight through to the filesystem every time, which for FAT16 is
a directory scan, which is sector reads (Chapter 38, §4.1).

Linux's dentry cache sits exactly here: a hash table from (parent, name) to node, so repeated lookups
of the same path cost nothing. It is the single biggest performance feature of the Linux VFS and it
is about 400 lines. Exercise 39.6.

---

## 8. Running it

```
[    0.420] inf  vfs: root created
[    0.421] inf  console: line discipline ready
[    0.430] inf  vfs: mounted fat16 at /mnt
```

```
nimbus> ls /
bin/  dev/  mnt/
nimbus> ls /dev
console
nimbus> ls /bin
sh  ls  cat  echo  hexdump
sleep  true  false  forktest
nimbus> ls /mnt
greeting.txt
```

Four directories, four completely different implementations, one `ls`.

### 8.1 The demonstration

```
nimbus> cat /mnt/greeting.txt
hello
nimbus> cat /dev/console
typed input echoes back
typed input echoes back
nimbus> ls /bin | cat
sh  ls  cat  echo  hexdump
sleep  true  false  forktest
```

Three sources: a FAT16 file on a disk, a character device, and a pipe. `cat` is the same nine lines
in all three cases, and at no point does it, the syscall layer, or the fd layer contain a conditional
distinguishing them.

**That uniformity is the entire argument for a VFS**, and this is where it becomes visible.

### 8.2 Watching the dispatch

```c
ssize_t vfs_read(vfs_node_t *node, off_t offset, size_t size, uint8_t *buf)
{
    if (!node || !node->read) return -EINVAL;
    LOG_DEBUG("vfs_read(%s) -> %p", node->name, (void *)node->read);
    return node->read(node, offset, size, buf);
}
```

```
[   14.201] dbg  vfs_read(greeting.txt) -> 0xc0109a40
[   14.203] dbg  vfs_read(console) -> 0xc0106120
[   14.205] dbg  vfs_read(pipe:r) -> 0xc010b8e0
```

Three different function addresses from the same call site. That is the polymorphism, observed.

---

## 9. What could go wrong

| Symptom | Cause |
|---|---|
| Panic on write to a read-only fs | Missing NULL check in the dispatch |
| Mount point shows the empty directory | `node->mounted` not followed in `finddir`/`readdir` |
| `readdir` returns the same entry twice | Caller did not copy before calling again |
| `/a//b` fails | Separators not collapsed |
| A long filename opens the wrong file | Truncation instead of refusal |
| `..` does nothing useful | Expected — no parent pointers |
| Paths resolve relative to the wrong directory | `current_task->cwd` not set on process creation |
| Stale node after a file is deleted | No invalidation; ours has no cache, so this does not arise |

---

## 10. Exercises

🟢 **39.1** Add the dispatch trace from §8.2 and watch a pipeline.

🟢 **39.2** Make `walk` truncate long components instead of refusing, and construct the
`passwd_backup` attack.

🟢 **39.3** Mount something over `/bin` and confirm the original contents reappear when you clear
`node->mounted`.

🟡 **39.4** Add a `parent` pointer to `vfs_node_t`, set it in `vfs_dir_add` and in the FAT16 driver,
and implement `..`. Then work out what happens after a rename.

🟡 **39.5** Add per-filesystem refcounting and implement `vfs_umount` with `-EBUSY`.

🟡 **39.6** Implement a dentry cache: a hash table from (parent node, name) to node, checked in
`vfs_finddir` and invalidated in `unlink` and `create`. Measure `ls /mnt` before and after.

🔴 **39.7** Add symlinks: a `VFS_SYMLINK` flag, a `readlink` operation, following in `walk` with a
depth limit of 8, and `O_NOFOLLOW`. Then write the TOCTOU attack from §5.4 and confirm `O_NOFOLLOW`
prevents it.

🔴 **39.8** Replace the eight function pointers with a shared `vfs_ops *` table. Measure the memory
saved with a hundred open nodes, and the extra indirection's cost.

---

## What we covered

- Seven operations that make four unrelated things interchangeable, demonstrated by a nine-line
  `cat`.
- Function pointers in a struct as polymorphism written out, with the cost visible.
- Two untyped scratch fields, what each driver puts in them, and why typed storage would couple the
  VFS to its drivers.
- Flags rather than an enum, and the one that stops a console's offset advancing.
- A dispatch layer whose NULL checks turn a page fault into an error code.
- Following mounts at the operation rather than at lookup, so a mount point is transparent.
- In-memory directories as an ordinary driver with no special status.
- A static `dirent` that is a bad interface matched deliberately, because it is the one programs
  expect.
- Path resolution: refusing long components rather than truncating, collapsing separators, and `..`
  that does nothing because parent pointers are harder than they look.
- No symlinks, and the TOCTOU class that buys us.
- Mount as one pointer, a view rather than an edit, and why unmount needs refcounting.
- What the abstraction costs: an indirect call, 32 bytes a node, and a missing dentry cache.

[Chapter 40](40-initrd.md) mounts the first filesystem — a tar archive that the bootloader put in
memory.

---

[← Partitions](38-block-layer.md) · [Contents](README.md) · [Next: The initrd →](40-initrd.md)
