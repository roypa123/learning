# Line by line: `nimbus/fs/vfs.c` and `fs/fd.c`

[Index](README.md) · [Chapter 39](../39-vfs.md) · [Chapter 43](../43-fds-and-pipes.md)

---

# `vfs.c`

## The dispatch layer

```c
ssize_t vfs_read(vfs_node_t *node, off_t offset, size_t size, uint8_t *buf)
{
    if (!node || !node->read) return -EINVAL;
    return node->read(node, offset, size, buf);
}
```
Six functions that all look the same. **The checks are the point.**

> A filesystem that does not implement write leaves the pointer NULL, and without these guards the
> first write to a read-only filesystem would jump to address zero — which, since we deliberately
> leave the first page unmapped, at least faults cleanly rather than executing whatever happens to be
> at 0. Still: a NULL check is cheaper than a page fault, and `-EINVAL` is a better diagnostic than a
> stack trace.

```c
int vfs_open(vfs_node_t *node, uint32_t flags)
{
    node->refcount++;
    if (node->open) return node->open(node, flags);
    return 0;
}
```
The refcount is incremented by the VFS regardless of whether the driver has an `open`.

```c
dirent_t *vfs_readdir(vfs_node_t *node, uint32_t index)
{
    if (!node || !(node->flags & VFS_DIRECTORY) || !node->readdir) return NULL;

    if (node->mounted) node = node->mounted;

    return node->readdir(node, index);
}
```
⚠️ **Following the mount here, not at lookup time.**

> the directory the caller wants is the mounted filesystem's root, not the empty directory it was
> mounted over. Doing this in readdir and finddir — rather than once at lookup time — is what makes a
> mount point transparent.

Resolving mounts during path walking would mean every caller holding a node has to wonder whether it
is the mount point or the mounted root.

Note the `!node->readdir` check happens *before* the redirect, which is a small inconsistency: a bare
directory node used as a mount point must still have a `readdir`. `vfs_make_directory` gives it one.

---

## In-memory directories

```c
typedef struct ramdir_entry {
    vfs_node_t          *node;
    struct ramdir_entry *next;
} ramdir_entry_t;
```
⚠️ Children hang off the node's `device` field:

> Reusing a generic scratch field rather than adding `struct vfs_node *children` to the node keeps
> the VFS structure free of any one implementation's private state — which is exactly the discipline
> that lets FAT16 put a cluster number in the same slot.

An in-memory directory is *a driver*, with no special status. As soon as the VFS struct grows a field
only one implementation uses, it grows three more.

```c
static dirent_t ramdir_dirent;
```
⚠️ **A single static, overwritten on every call.**

> That makes readdir() non-reentrant and means the caller must copy the result before calling again —
> which is exactly the contract the real POSIX readdir() has, and for exactly the same reason. It is
> a genuinely bad interface that we are matching on purpose, because it is the one every program
> expects.

The syscall layer copies immediately, so userland never sees the aliasing.

```c
int vfs_dir_add(vfs_node_t *dir, vfs_node_t *child)
{
    ramdir_entry_t *e = (ramdir_entry_t *)kmalloc(sizeof(ramdir_entry_t));
    if (!e) return -ENOMEM;

    e->node = child;
    e->next = (ramdir_entry_t *)dir->device;
    dir->device = e;
```
Prepend, so `ls /` lists in reverse insertion order. Nobody sorts.

---

## `walk`

```c
    if (path[0] == '/') {
        node = vfs_root;
        path++;
    } else {
        node = (current_task && current_task->cwd) ? current_task->cwd : vfs_root;
    }
```
The `current_task &&` guard matters: `vfs_lookup` is called during boot, before `task_init`.

```c
    if (!*path) {
        if (stop_before_last) return NULL;
        return node;
    }
```
A path of just `/` resolves to the root and has no components. `vfs_lookup_parent("/")` is
meaningless, hence the NULL.

```c
        size_t len = 0;
        while (path[len] && path[len] != '/') {
            if (len >= VFS_NAME_MAX - 1) return NULL;   /* too long: refuse */
            len++;
        }
```
⚠️ **Refuse, not truncate.**

> Truncating means "/etc/passwd_backup" can be made to open "/etc/passwd", which is a real historical
> vulnerability.

Silent truncation turns a name the user chose into a *different* name the system trusts.

```c
        path += len;
        while (*path == '/') path++;
```
⚠️ Collapses `//` and skips the separator. `/a//b` and `/a/b` are the same path.

Not collapsing leaves an empty component, which some code treats as "." and some as an error — and
the disagreement is where bugs live.

```c
        bool is_last = (*path == '\0');

        if (is_last && stop_before_last) {
            if (last_out) strlcpy(last_out, component, VFS_NAME_MAX);
            return node;
        }
```
One walker, two entry points. `vfs_lookup_parent` is what `create` and `unlink` need: the directory
plus the final name as a string, because the thing being named does not exist yet.

```c
        } else if (strcmp(component, "..") == 0) {
            if (node == vfs_root) {
                /* ".." at the root is the root, as on every Unix. */
            } else {
                return NULL;
            }
        }
```
⚠️ No parent pointers, so `..` is not resolvable.

Adding one is easy; keeping it correct is not — if `/a/b` is open and then moved to `/c/b`, a cached
parent pointer is stale. Real filesystems resolve `..` through a dentry cache invalidated on rename.

`..` at the root being the root is universal, and it is what stops a path escaping upwards.

**No symlinks**, which removes: loop detection, `ELOOP`, `O_NOFOLLOW`, and the entire TOCTOU class
where a path means something different at resolution time than at check time.

---

## `vfs_mount`

```c
    if (target->mounted) return -EBUSY;

    target->mounted = fs_root;
    target->flags  |= VFS_MOUNTPOINT;
```
One pointer.

> The directory's original contents are not deleted, merely hidden. Unmount and they reappear. That
> behaviour surprises people the first time they mount over a non-empty directory, and it is the
> correct one: the mount is a view, not an edit.

⚠️ No `vfs_umount`. Unmounting safely requires knowing nothing is using the filesystem — no open
files, no process with a cwd inside it, no cached nodes. That needs a per-filesystem refcount.

---

# `fd.c`

## `file_open_node`

```c
    f->node     = node;
    f->offset   = 0;
    f->flags    = flags;
    f->refcount = 1;
```
Three levels: `fd` → `file_t` → `vfs_node_t`.

> Two processes open the same file; they get different `struct file`s, so they have independent
> offsets [...] Then one of them forks; the parent and child share the *same* `struct file`, so they
> share an offset, and `(echo a; echo b) > f` produces "a\nb\n" instead of "b\n". Both behaviours are
> required, and neither is expressible without exactly this three-level split.

```c
    if (flags & O_APPEND) f->offset = node->length;
```
⚠️ Positions at the end **on open**, which is not the same as a real `O_APPEND`:

> a real O_APPEND re-seeks before *every* write, so that two processes appending to one log file
> cannot overwrite each other. Ours is the simpler version, and the difference is exactly the bug you
> get in a multi-process logger.

```c
    if (flags & O_TRUNC) {
        node->length = 0;
    }
```
⚠️ Sets the length to 0 and **leaks the cluster chain**. `echo x > bigfile` repeatedly leaks the old
clusters every time. Known rather than discovered.

---

## `file_unref`

```c
    if (f->refcount > 0) f->refcount--;
    if (f->refcount > 0) return;

    vfs_close(f->node);
    kfree(f);
```
⚠️ **The last close is a signal**, not just cleanup:

> which for a pipe is what signals EOF to the other end, and is why a shell must close *both* ends of
> a pipe in the parent or the reader never sees end-of-file and the pipeline hangs forever.

⚠️ Read, decrement, test. Safe on a uniprocessor with no preemption inside this function; with
threads sharing a descriptor table, two concurrent unrefs could both see 1 and both free.
`atomic_dec_and_test` exists for this and is not used here — a small honest gap.

---

## `fd_alloc`

```c
    for (int fd = 0; fd < MAX_FDS; fd++) {
        if (!current_task->fds[fd]) {
            current_task->fds[fd] = f;
```
⚠️ **The lowest free descriptor, and that is a POSIX guarantee:**

```c
    close(1);
    open("out.txt", ...);   /* guaranteed to return 1 */
```

> which is how redirection worked before dup2 existed.

A linear scan from 0 is therefore the *correct* implementation, not merely a simple one.

```c
    uint32_t flags = irq_save();
```
Protects against preemption between finding the slot and claiming it. `current_task->fds` is
per-process and a process is single-threaded here — but protecting it now costs nothing and is
correct in advance.

---

[Index](README.md) · [Chapter 39](../39-vfs.md) · [Chapter 43](../43-fds-and-pipes.md)
