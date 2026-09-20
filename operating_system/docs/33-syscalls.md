# Chapter 33 — System calls

[← Ring 3](32-usermode.md) · [Contents](README.md) · [Next: fork, exec, exit, wait →](34-fork-exec.md)

> 📖 **Line by line:** [syscall.c](line-by-line/nimbus-syscall.md)

---

## Goal

Open the one door in the wall Chapter 32 built, and then spend most of the chapter on why that is
harder than it sounds.

The mental model to hold throughout: **every value that arrives here was chosen by code we do not
trust.** Not "code that might have a bug" — code that may be actively trying to make the kernel write
somewhere it should not.

---

## 1. The mechanism

```c
static inline int32_t syscall3(uint32_t n, uint32_t a, uint32_t b, uint32_t c)
{
    int32_t ret;
    __asm__ volatile ("int $0x80"
                      : "=a"(ret)
                      : "a"(n), "b"(a), "c"(b), "d"(c)
                      : "memory");
    return ret;
}
```

```
    eax = call number
    ebx, ecx, edx, esi, edi = arguments 1..5
    return value in eax; negative values are -errno
```

Linux's 32-bit convention, chosen for the same reasons Linux chose it:

> those are the registers `int 0x80` does not already have a use for, and passing in registers avoids
> the kernel having to read the user stack — which it would have to validate, page by page, before
> touching.

That last clause is the real reason. Arguments on the stack would mean five separate user pointers to
validate before you could even find out which call was being made.

### 1.1 Five arguments, and what to do about six

`EBP` is the sixth candidate and is the frame pointer, so Linux's original convention stopped at
five and used a pointer to a struct for calls needing more. `mmap` is the famous example: it takes
six arguments and the 32-bit ABI passes a pointer to an array.

Nimbus needs at most three.

### 1.2 Errors as small negative numbers

```c
#define EPERM        1
#define ENOENT       2
...
#define ENOSYS      38
```

One register comes back. Errors travel in it, as `-errno`.

That works because no syscall of ours legitimately returns a value in [−4095, −1]. `read` returns a
count or a negative; `sbrk` returns an address, and addresses in that range are not valid user
addresses.

The userland side has to know:

```c
void *sbrk(int increment)
{
    int32_t r = syscall1(SYS_SBRK, (uint32_t)increment);
    if (r < 0 && r > -4096) return (void *)-1;      /* an -errno, not an address */
    return (void *)r;
}
```

### 1.3 The `"memory"` clobber

```c
    __asm__ volatile ("int $0x80" : ... : "memory");
```

Not optional, and Chapter 14, §1 of the libc source spells out why:

```c
    buf[0] = 'x';  read(fd, buf, 10);  if (buf[0] == 'x') ...
```

Without `"memory"`, GCC believes `int $0x80` cannot touch `buf`, keeps `'x'` in a register, and
compiles the comparison to `true`.

---

## 2. The gate

```c
    idt_set_gate(INT_SYSCALL, isr_syscall_stub[0], SEL_KCODE, IDT_INTERRUPT_GATE_U);
```

`IDT_INTERRUPT_GATE_U` is `0xEE` — **DPL 3**.

That one bit is the door. Every other vector is DPL 0, so ring 3 cannot invoke it with `int`
(Chapter 16, §3.1).

Still an *interrupt* gate, not a trap gate, so `IF` is cleared on entry:

> A syscall handler that starts with interrupts enabled must be re-entrant from the first
> instruction; ours is not, and does not need to be. `syscall_dispatch()` re-enables them once it is
> on a safe footing.

### 2.1 Why `int 0x80` and not `sysenter`

`sysenter`/`sysexit` (Intel) and `syscall`/`sysret` (AMD) are the fast paths. They skip the IDT
lookup and the stack switch, costing maybe 80 cycles against 250 for `int 0x80`.

They need three MSRs configured, they impose a fixed GDT layout, and `sysenter` does not save the
return address — userland has to arrange that itself, which is why Linux puts a helper in the vDSO.

`int 0x80` needs nothing beyond the IDT that already exists. Exercise 32.7.

---

## 3. The table

```c
typedef int32_t (*syscall_fn)(registers_t *);

static syscall_fn syscall_table[SYS_MAX] = {
    [SYS_EXIT]    = sys_exit,
    [SYS_WRITE]   = sys_write,
    [SYS_READ]    = sys_read,
    ...
};
```

Designated initialisers, so the array index and the constant cannot drift apart. Adding a call means
adding a `#define` and one line here.

```c
void syscall_dispatch(registers_t *regs)
{
    uint32_t number = regs->eax;

    if (number >= SYS_MAX || !syscall_table[number]) {
        LOG_WARN("unknown syscall %u from pid %d", number, current_task->pid);
        regs->eax = (uint32_t)(-ENOSYS);
        return;
    }
    ...
```

> A table rather than a switch, for one reason that matters: the bounds check is explicit and
> impossible to forget. A switch on an untrusted integer is fine, but a table indexed by one is a
> memory read at an attacker-chosen offset unless somebody wrote the comparison — so we write it
> once, here, where it is the first statement in the function.

`number` is `regs->eax`, which is whatever the user program put in `EAX`. It could be 4 billion.

### 3.1 Numbers are append-only

```c
 *  Once a number is published it can never be reused for anything else: an old
 *  binary will keep calling it. Linux still has a `stat` at number 106 that
 *  nothing has called since 1999. We are not shipping to anyone, but the habit
 *  is worth forming, so numbers here are append-only and gaps stay gaps.
```

### 3.2 Returning through the frame

```c
    int32_t result = syscall_table[number](regs);

    cli();

    regs->eax = (uint32_t)result;
```

Not `return result`. The value goes into the **saved** `EAX` in the trap frame.

> The stub restores the whole frame with `popa` on the way out, so writing the register here would be
> overwritten a moment later. This indirection is exactly why `interrupt_dispatch` takes a pointer.

Chapter 16, §6.3 flagged this as one of three features that depend on `registers_t` being passed by
pointer. This is the first of them.

---

## 4. Interrupts on, and what that means

```c
    sti();

    int32_t result = syscall_table[number](regs);

    cli();
```

> Interrupts back on. We arrived through an interrupt gate, which cleared IF; leaving them off for
> the duration of a system call would mean a `read()` from the disk blocks the timer, the keyboard
> and everything else for the whole transfer.
>
> This is also the moment the kernel becomes re-entrant: from here on another task can be scheduled
> in the middle of this call, and every data structure we touch needs to survive that.

That second paragraph is the important one and it is easy to skim.

Before this line, the kernel is effectively single-threaded: `kmain` runs to completion, interrupt
handlers run with `IF` clear and do not block.

After it, **a system call can be preempted**. Task A can be halfway through `vfs_write` when the
timer fires and task B starts its own `vfs_write`. Every global the VFS touches is now shared
between two concurrent flows.

That is why Chapter 36 exists, and why the races it describes did not exist before this line.

---

## 5. Validating a user pointer

The longest section, because it is the one that matters.

```c
bool user_range_ok(const void *user_ptr, size_t n, bool need_write)
{
    uintptr_t start = (uintptr_t)user_ptr;

    if (n == 0) return true;
    if (start >= KERNEL_VIRTUAL_BASE) return false;
    if (start + n < start) return false;                  /* wrapped */
    if (start + n > KERNEL_VIRTUAL_BASE) return false;

    for (uintptr_t v = ALIGN_DOWN(start, PAGE_SIZE); v < start + n; v += PAGE_SIZE) {
        uint32_t *pte = paging_get_entry(current_task->directory, v, false);
        if (!pte || !(*pte & PTE_PRESENT)) return false;
        if (!(*pte & PTE_USER)) return false;
        if (need_write && !(*pte & PTE_WRITABLE)) return false;
    }
    return true;
}
```

### 5.1 Check one: is it in the user half?

```c
    if (start >= KERNEL_VIRTUAL_BASE) return false;
```

> A pointer of `0xC0100000` is a perfectly valid address — it is the kernel's own code. Without this
> check, `read(fd, (void*)0xC0100000, 4096)` asks the kernel to overwrite itself with the contents of
> a file, from ring 3, using an entirely legitimate system call.

That is the attack. It needs no bug in the kernel, no clever timing, no memory corruption. Just a
syscall with a pointer the caller chose.

Chapter 32, §2.3 said a user program can *name* kernel addresses and only paging stops it. Here is
the exception: inside a syscall, **the kernel is doing the dereferencing**, at CPL 0, where paging
does not stop it.

### 5.2 Check two: the overflow

```c
    if (start + n < start) return false;                  /* wrapped */
```

> With ptr = `0xBFFFF000` and n = `0x80000000`, the sum wraps to `0x3FFFF000`, which is a perfectly
> respectable user address, and a naive range check passes.

Both `start` and `n` come from registers the user chose. `start + n` is 32-bit arithmetic and wraps
silently.

This check is two comparisons and it is the one most often missing. It has a CVE-shaped history in
several real kernels.

### 5.3 Check three: is every page mapped, with the right permissions?

```c
    for (uintptr_t v = ALIGN_DOWN(start, PAGE_SIZE); v < start + n; v += PAGE_SIZE) {
        uint32_t *pte = paging_get_entry(current_task->directory, v, false);
        if (!pte || !(*pte & PTE_PRESENT)) return false;
        if (!(*pte & PTE_USER)) return false;
        if (need_write && !(*pte & PTE_WRITABLE)) return false;
    }
```

Three conditions per page:

**Present**, or the copy would fault — and a fault inside the kernel while copying on a user's behalf
is a *kernel* page fault, which is a panic (Chapter 26, §5). A user program passing a bad pointer
would take down the machine.

**`PTE_USER`**, or the program is naming a kernel page that happens to be below 3 GiB. (There are
none in Nimbus, but the check costs nothing and the invariant should not be relied on.)

**`PTE_WRITABLE`** if we are going to write. Otherwise `read(fd, buf, n)` with `buf` pointing at the
program's own `.text` would have the kernel overwrite read-only memory — and with `CR0.WP` set
(Chapter 24, §5.5) that would fault; without it, it would silently succeed.

`ALIGN_DOWN` on the start because a range beginning mid-page still needs that page checked.

### 5.4 The race we are not fixing

There is a window between the check and the copy. On a uniprocessor with a single-threaded process,
nothing can unmap the pages in between — there is no other flow of control in that address space.

Add threads (Exercise 29.8) and that stops being true: another thread could `munmap` the buffer
between validation and `memcpy`. This is a genuine **time-of-check-to-time-of-use** bug and it is why
real kernels do not validate up front.

### 5.5 What real kernels do instead

A **fixup table**. The copy routines are marked, and the page fault handler looks up the faulting
`eip` in a table of "if a fault happens here, jump there instead":

```
    .section .fixup
    3:  movl $-EFAULT, %eax
        jmp  2b
    .section __ex_table
        .long 1b, 3b        /* if a fault happens at 1b, resume at 3b */
```

The copy is attempted directly; if it faults, the handler redirects to an error path that returns
`-EFAULT`.

Advantages: no up-front walk (faster for large buffers), correct under concurrent unmapping, and it
handles demand-paged pages that are legitimately not present yet.

Cost: a section, a handler case, and assembly for every copy routine. About fifty lines, and
Exercise 33.7.

### 5.6 Strings are harder

```c
bool user_string_copy(char *dst, const char *user_src, size_t max)
{
    uintptr_t addr = (uintptr_t)user_src;

    for (size_t i = 0; i < max; i++, addr++) {
        if (i == 0 || (addr & PAGE_MASK) == 0) {
            if (!user_range_ok((const void *)addr, 1, false)) return false;
        }

        dst[i] = *(const char *)addr;
        if (dst[i] == '\0') return true;
    }

    dst[max - 1] = '\0';
    return false;
}
```

> Copying a string is harder than copying a buffer, because we do not know how long it is until we
> have read it — and we cannot read it until we know it is safe.

So: validate and copy one byte at a time, re-checking only when crossing a page boundary. One walk
per 4096 bytes instead of one per byte.

**And it always terminates:**

```c
    dst[max - 1] = '\0';
    return false;
```

> Ran out of room. Terminate anyway — an unterminated buffer handed to the rest of the kernel is
> worse than a truncated path.

It returns `false`, so the caller rejects it. But if a future caller ignores the return value, at
least it gets a valid C string rather than a walk off the end of a stack buffer.

---

## 6. The calls, and what each one has to check

```c
static int32_t sys_write(registers_t *regs)
{
    int         fd  = (int)regs->ebx;
    const void *buf = (const void *)regs->ecx;
    size_t      len = (size_t)regs->edx;

    if (len > 1 * MiB) return -EINVAL;
    if (!user_range_ok(buf, len, false)) return -EFAULT;

    file_t *f = fd_get(fd);
    if (!f) return -EBADF;
    if (!f->node->write) return -EINVAL;

    ssize_t n = vfs_write(f->node, f->offset, len, (const uint8_t *)buf);
    if (n > 0 && !(f->node->flags & VFS_CHARDEVICE)) f->offset += (off_t)n;
    return (int32_t)n;
}
```

Five checks before any work:

**Length bound.** `len` is user-chosen. A 4 GiB write would walk 4 GiB of page tables in
`user_range_ok` — a denial of service with one syscall. 1 MiB is arbitrary and generous.

**Range validation**, §5.

**`fd_get`** bounds the descriptor:

```c
file_t *fd_get(int fd)
{
    if (fd < 0 || fd >= MAX_FDS || !current_task) return NULL;
    return current_task->fds[fd];
}
```

> Both bounds. `fd < 0` is not hypothetical: every failed syscall returns a negative number, and a
> program that forgets to check will hand that straight back to `read()`. Without the lower check,
> fd = −2 indexes eight bytes before the array — which in `struct task` is `cwd`, a pointer the kernel
> would then treat as a `file_t *`.

That is a very concrete demonstration of why both bounds matter. A missing lower bound is not "reads
garbage"; it is "reads an adjacent struct field and treats it as an object pointer".

**The operation exists.** A node with no `write` gets `-EINVAL` rather than a call through NULL.

### 6.1 `sys_exec`: three levels of untrusted

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
    argv[argc] = NULL;
```

> argv arrives as a user pointer to an array of user pointers to user strings. Three levels, every
> one of them untrusted, and the array has no length — it ends with a NULL that the caller may have
> forgotten.

So: each array element is `copy_from_user`'d individually, each string is `user_string_copy`'d, and
there is a hard cap of 31.

Without the cap, an argv with no NULL walks until it hits an unmapped page — which the validation
catches, but only after allocating 250 KiB of copies. The cap makes the failure bounded.

Note the cleanup on the error path:

```c
    int rc = task_exec_regs(regs, path, argv);

    for (int i = 0; i < argc; i++) kfree(argv[i]);
    return rc;
```

Every allocated copy is freed, whether `exec` succeeded or not. A leak here is a leak per failed
command, which a shell will find quickly.

---

## 7. Running it

```
nimbus> echo hello
hello
```

Every character of that took: `int 0x80` with `EAX = 1`, a privilege change, a stack switch via the
TSS, a trap frame, a dispatch, a range check, an fd lookup, a VFS call, a console write, and an
`iret` back.

### 7.1 Tracing

```c
    LOG_DEBUG("syscall %u(%08x, %08x, %08x) from pid %d",
              number, regs->ebx, regs->ecx, regs->edx, current_task->pid);
```

```
[    2.451] dbg  syscall 1(00000001, 0804b0a0, 00000006) from pid 4
[    2.452] dbg  syscall 0(00000000, 00000000, 00000000) from pid 4
[    2.452] dbg  syscall 7(bffffe94, 00000000, 00000000) from pid 1
```

`strace` in four lines. Call 1 is `write(1, 0x804b0a0, 6)`, call 0 is `exit(0)`, call 7 is the
shell's `wait`.

This is worth having behind a flag permanently.

### 7.2 Testing the validation

```c
int main(void)
{
    write(1, (void *)0xC0100000, 100);      /* kernel address       */
    write(1, (void *)0xBFFFF000, 0x80000000); /* overflow           */
    write(1, (void *)0x00000000, 10);       /* unmapped            */
    write(-1, "x", 1);                      /* negative fd          */
    write(999, "x", 1);                     /* out of range fd      */
    return 0;
}
```

All five return negative. Nothing panics, nothing is printed, and the shell survives.

```
[   12.003] dbg  syscall 1(00000001, c0100000, 00000064) from pid 4   -> -14 EFAULT
[   12.003] dbg  syscall 1(00000001, bffff000, 80000000) from pid 4   -> -22 EINVAL
[   12.003] dbg  syscall 1(00000001, 00000000, 0000000a) from pid 4   -> -14 EFAULT
[   12.004] dbg  syscall 1(ffffffff, 0804b0a4, 00000001) from pid 4   ->  -9 EBADF
[   12.004] dbg  syscall 1(000003e7, 0804b0a4, 00000001) from pid 4   ->  -9 EBADF
```

Five attacks, five clean rejections. **That is what this chapter is for.**

---

## 8. Exercises

🟢 **33.1** Add the syscall trace from §7.1 behind a compile-time flag and use it to watch the shell
run `ls`.

🟢 **33.2** Remove the `fd < 0` check in `fd_get` and call `write(-2, ...)`. Work out what
`current_task->fds[-2]` actually reads, then run it.

🟢 **33.3** Remove the overflow check in `user_range_ok` and construct the attack from §5.2.

🟡 **33.4** Add a syscall counter per number, printed by a shell command. Run `ls | cat` and see
which calls dominate.

🟡 **33.5** Remove the `len > 1 * MiB` bound and call `write(1, buf, 0xFFFFF000)`. Time how long the
validation walk takes.

🟡 **33.6** Add `SYS_GETTIMEOFDAY` returning a struct by pointer. Note that you now have a
kernel-to-user copy whose destination must be validated for *writing*.

🔴 **33.7** Implement the fixup table from §5.5: a `__ex_table` section, assembly copy routines with
labelled fault points, and a page fault handler case that looks up `regs->eip` and redirects. Then
remove the up-front validation and confirm the same five attacks still fail cleanly.

🔴 **33.8** Implement `sysenter`. Set `IA32_SYSENTER_CS`, `_ESP` and `_EIP`, arrange for userland to
know its own return address, and measure the difference against `int 0x80` with `rdtsc`.

---

## What we covered

- The register convention, and why arguments in registers rather than on the stack is a security
  decision as much as a performance one.
- Errors as small negative numbers in one register, and the range that makes it unambiguous.
- The `"memory"` clobber, and the compiled-away comparison you get without it.
- One gate at DPL 3, still an interrupt gate, and why `int 0x80` rather than `sysenter`.
- A table with the bounds check as its first statement, and why that is better than a switch.
- Returning through `regs->eax` rather than `return`.
- `sti()` as the moment the kernel becomes re-entrant, and everything Chapter 36 will be about.
- Three checks on a user range: the kernel half, the arithmetic overflow, and every page's presence
  and permissions.
- The time-of-check-to-time-of-use race we do not have yet, and the fixup table that real kernels use
  instead.
- Strings validated a page at a time, and always terminated even on failure.
- Five checks in `sys_write`, including a length bound against a one-syscall denial of service.
- Why `fd < 0` matters concretely: it reads an adjacent struct field and treats it as a pointer.
- Three levels of untrusted indirection in `exec`'s argv, and the cap that bounds the failure.

[Chapter 34](34-fork-exec.md) makes one process into two, and then replaces one with a program.

---

[← Ring 3](32-usermode.md) · [Contents](README.md) · [Next: fork, exec, exit, wait →](34-fork-exec.md)
