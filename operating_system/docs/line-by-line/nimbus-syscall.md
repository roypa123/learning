# Line by line: `nimbus/kernel/syscall.c`

[Index](README.md) · [Chapter 33](../33-syscalls.md)

The most security-relevant file in the kernel. Every value here was chosen by code we do not trust.

---

## `user_range_ok`

```c
bool user_range_ok(const void *user_ptr, size_t n, bool need_write)
{
    uintptr_t start = (uintptr_t)user_ptr;

    if (n == 0) return true;
```
A zero-length range is vacuously fine, and checking it first avoids the page loop doing nothing
awkward.

```c
    if (start >= KERNEL_VIRTUAL_BASE) return false;
```
⚠️ **Check one.**

> A pointer of `0xC0100000` is a perfectly valid address — it is the kernel's own code. Without this
> check, `read(fd, (void*)0xC0100000, 4096)` asks the kernel to overwrite itself with the contents of
> a file, from ring 3, using an entirely legitimate system call.

No bug required, no clever timing. Just a syscall with a pointer the caller chose.

Chapter 32 said paging stops a user program reading kernel memory. This is the exception: inside a
syscall the *kernel* does the dereferencing, at CPL 0, where paging does not stop it.

```c
    if (start + n < start) return false;                  /* wrapped */
    if (start + n > KERNEL_VIRTUAL_BASE) return false;
```
⚠️ **Check two.**

> With ptr = `0xBFFFF000` and n = `0x80000000`, the sum wraps to `0x3FFFF000`, which is a perfectly
> respectable user address, and a naive range check passes.

Both operands come from registers the user chose, and 32-bit arithmetic wraps silently. Two
comparisons, and the one most often missing — it has a CVE-shaped history in several real kernels.

```c
    for (uintptr_t v = ALIGN_DOWN(start, PAGE_SIZE); v < start + n; v += PAGE_SIZE) {
        uint32_t *pte = paging_get_entry(current_task->directory, v, false);
        if (!pte || !(*pte & PTE_PRESENT)) return false;
        if (!(*pte & PTE_USER)) return false;
        if (need_write && !(*pte & PTE_WRITABLE)) return false;
    }
```
⚠️ **Check three**, per page:

**Present**, or the copy faults — and a kernel page fault while copying on a user's behalf is a
*panic*. A user program passing a bad pointer would take down the machine.

**`PTE_USER`**, or the program is naming a kernel page that happens to be below 3 GiB.

**`PTE_WRITABLE`** when writing, or `read(fd, buf, n)` with `buf` in the program's `.text` would have
the kernel overwrite read-only memory.

`ALIGN_DOWN` on the start, because a range beginning mid-page still needs that page checked.

`create = false`, or a failed check would allocate a page table on every bad pointer.

### The race we do not have yet

On a uniprocessor with a single-threaded process nothing can unmap the pages between the check and
the copy.

⚠️ Add threads and that stops being true — a genuine time-of-check-to-time-of-use bug, and why real
kernels use a **fixup table** instead: attempt the copy, and have the page fault handler redirect a
fault at a marked instruction to an error path returning `-EFAULT`.

---

## `user_string_copy`

```c
    for (size_t i = 0; i < max; i++, addr++) {
        if (i == 0 || (addr & PAGE_MASK) == 0) {
            if (!user_range_ok((const void *)addr, 1, false)) return false;
        }

        dst[i] = *(const char *)addr;
        if (dst[i] == '\0') return true;
    }
```
⚠️ A string's length is unknown until it has been read, and it cannot be read until it is known safe.
So: validate and copy one byte at a time, re-checking only on a page boundary.

One walk per 4096 bytes rather than one per byte.

```c
    dst[max - 1] = '\0';
    return false;
```
⚠️ **Always terminates**, even on failure.

> an unterminated buffer handed to the rest of the kernel is worse than a truncated path.

It returns `false` so the caller rejects it — but if a future caller ignores the return value, at
least it gets a valid C string.

---

## `syscall_dispatch`

```c
    uint32_t number = regs->eax;

    if (number >= SYS_MAX || !syscall_table[number]) {
        LOG_WARN("unknown syscall %u from pid %d", number, current_task->pid);
        regs->eax = (uint32_t)(-ENOSYS);
        return;
    }
```
⚠️ **The bounds check is the first statement.**

> A switch on an untrusted integer is fine, but a table indexed by one is a memory read at an
> attacker-chosen offset unless somebody wrote the comparison — so we write it once, here.

`number` is whatever the user put in `EAX`. It could be four billion.

```c
    sti();
```
⚠️ **The moment the kernel becomes re-entrant.**

We arrived through an interrupt gate, which cleared `IF`. Leaving it off for the duration would mean
a disk read blocking the timer and the keyboard for the whole transfer.

But from here, another task can be scheduled in the middle of this call, and every data structure we
touch must survive that. Everything Chapter 36 is about starts at this line.

```c
    int32_t result = syscall_table[number](regs);

    cli();

    regs->eax = (uint32_t)result;
```
⚠️ The value goes into the **saved** `EAX` in the trap frame, not the real register — the stub
restores the whole frame with `popa` on the way out.

This indirection is exactly why `interrupt_dispatch` takes a pointer.

---

## `sys_write`

```c
    if (len > 1 * MiB) return -EINVAL;
```
⚠️ `len` is user-chosen. A 4 GiB write would walk 4 GiB of page tables in `user_range_ok` — a denial
of service with one syscall.

```c
    if (!user_range_ok(buf, len, false)) return -EFAULT;

    file_t *f = fd_get(fd);
    if (!f) return -EBADF;
    if (!f->node->write) return -EINVAL;
```
Four checks before any work. The last means a node with no `write` gets `-EINVAL` rather than a call
through NULL.

```c
    ssize_t n = vfs_write(f->node, f->offset, len, (const uint8_t *)buf);
    if (n > 0 && !(f->node->flags & VFS_CHARDEVICE)) f->offset += (off_t)n;
```
⚠️ Character devices do not advance the offset — a console is not seekable, and without this the
offset would grow forever.

---

## `fd_get`, in `fs/fd.c`

```c
    if (fd < 0 || fd >= MAX_FDS || !current_task) return NULL;
```
⚠️ **Both bounds**, and the lower one is not hypothetical:

> every failed syscall returns a negative number, and a program that forgets to check will hand that
> straight back to `read()`. Without the lower check, fd = −2 indexes eight bytes before the array —
> which in `struct task` is `cwd`, a pointer the kernel would then treat as a `file_t *`.

Not "reads garbage": reads an adjacent struct field and treats it as an object pointer.

---

## `sys_exec`

```c
    char **user_argv = (char **)regs->ecx;

    if (user_argv) {
        for (argc = 0; argc < 31; argc++) {
            char *user_str;
            if (!copy_from_user(&user_str, &user_argv[argc], sizeof(char *)))
                return -EFAULT;
            if (!user_str) break;

            char *copy = (char *)kmalloc(256);
            if (!copy) { argc--; break; }

            if (!user_string_copy(copy, user_str, 256)) {
                kfree(copy);
                return -EFAULT;
            }
            argv[argc] = copy;
        }
    }
```
⚠️ **Three levels of untrusted indirection**: a user pointer to an array of user pointers to user
strings — and the array has no length, only a NULL the caller may have forgotten.

Each element `copy_from_user`'d individually, each string `user_string_copy`'d, hard cap of 31.

Without the cap, an argv with no NULL walks until it hits an unmapped page — caught, but only after
allocating 250 KiB of copies.

```c
    int rc = task_exec_regs(regs, path, argv);

    for (int i = 0; i < argc; i++) kfree(argv[i]);
    return rc;
```
Every copy freed whether `exec` succeeded or not. A leak here is a leak per failed command, which a
shell finds quickly.

---

## `sys_sbrk`

```c
    if (increment > 0) {
        vaddr_t new_brk = old_brk + (uint32_t)increment;

        if (new_brk >= USER_STACK_TOP - USER_STACK_SIZE * 8) return -ENOMEM;

        paging_map_range(current_task->directory, old_brk,
                         (size_t)increment, PTE_WRITABLE | PTE_USER);
```
Refuses to grow into the stack region.

⚠️ Returns the **old** break, which is what `malloc` wants — the address of the new memory.

```c
    } else {
        vaddr_t new_brk = old_brk - (uint32_t)(-increment);
        if (new_brk < USER_HEAP_BASE) return -EINVAL;
```
`-(uint32_t)increment` for a negative `increment`. Done in unsigned so `INT32_MIN` does not overflow.

---

## `sys_pipe`

```c
    int fds[2];
    fds[0] = fd_alloc(fr);
    fds[1] = fd_alloc(fw);

    if (fds[0] < 0 || fds[1] < 0) {
        if (fds[0] >= 0) fd_close(fds[0]);
        if (fds[1] >= 0) fd_close(fds[1]);
        return -EMFILE;
    }
```
Both or neither. A half-created pipe leaves one descriptor pointing at an object whose other end will
never be closed.

---

## `sys_dup2`

```c
    if (oldfd == newfd) return newfd;

    if (current_task->fds[newfd]) file_unref(current_task->fds[newfd]);

    file_ref(f);
    current_task->fds[newfd] = f;
```
⚠️ **The self-dup check must come first.** Without it, `dup2(1, 1)` unrefs the file — possibly freeing
it — and then stores a dangling pointer.

---

## `sys_reboot`

```c
    uint8_t good = 0x02;
    while (good & 0x02) good = inb(0x64);
    outb(0x64, 0xFE);
```
Wait for the 8042's input buffer to drain, then pulse the reset line.

> the 8042 has an output pin wired to the CPU's reset, for the same 1984 reason it has one wired to
> A20.

---

## The table

```c
static syscall_fn syscall_table[SYS_MAX] = {
    [SYS_EXIT]    = sys_exit,
    [SYS_WRITE]   = sys_write,
    ...
};
```
Designated initialisers, so the index and the constant cannot drift apart.

⚠️ Numbers are **append-only**. Once published, a number can never be reused — an old binary will
keep calling it.

---

[Index](README.md) · [Chapter 33](../33-syscalls.md)
