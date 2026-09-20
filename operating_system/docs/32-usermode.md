# Chapter 32 — Ring 3

[← The scheduler](31-scheduler.md) · [Contents](README.md) · [Next: System calls →](33-syscalls.md)

---

## Goal

Build the wall. Get a program running at privilege level 3, where it cannot execute privileged
instructions, cannot touch kernel pages, and cannot raise its own privilege — and understand exactly
which mechanism enforces each of those.

This is the chapter where Nimbus becomes an operating system rather than a program with subroutines.

---

## 1. Four rings, two used

The x86 has four privilege levels, 0 through 3. The idea was a hierarchy: kernel at 0, device drivers
at 1, system services at 2, applications at 3.

Nobody uses rings 1 and 2. Two reasons:

**Paging only has one bit.** `PTE_USER` (Chapter 23, §3.3) distinguishes "ring 0–2" from "ring 3".
There is no way to make a page visible to ring 1 but not ring 2, so the finer distinctions cannot be
backed by memory protection.

**Portability.** Most other architectures have two levels. An OS that used four could not be ported.

So: ring 0 is the kernel, ring 3 is everything else. The two bits in `CS` are the boundary, and
Chapter 15 built the descriptors that carry them.

---

## 2. What ring 3 cannot do

Three separate mechanisms, and it is worth keeping them apart because they fail differently.

### 2.1 Privileged instructions

`cli`, `sti`, `hlt`, `lgdt`, `lidt`, `ltr`, `invlpg`, `mov` to or from a control register, and a few
more. Executing any of them at CPL 3 raises `#GP` — general protection, vector 13.

```c
    if (regs->int_no == INT_GENERAL_PROTECTION || ...)
        describe_selector_error(regs->err_code);
```

with an error code of 0:

```c
    if (err == 0) {
        kprintf("  error code 0: no segment involved\n"
                "  (usually a privileged instruction executed in ring 3,\n"
                "   or a write to a read-only control register)\n");
```

That is the single most common `#GP` once userland exists, which is why the message names it.

### 2.2 I/O ports

`in` and `out` are allowed only if CPL ≤ `EFLAGS.IOPL`, which is 0, *or* if the port's bit is clear
in the TSS I/O permission bitmap.

```c
    tss.iomap_base = sizeof(tss_entry_t);
```

Chapter 15, §4.2: setting the base past the end of the structure means "no bitmap", so every port
access from ring 3 faults.

That matters: a program that can write to port `0x64` can reboot the machine, and one that can write
to `0x1F0`–`0x1F7` can issue disk commands directly.

### 2.3 Memory

**This is the real one.**

`PTE_USER` clear on every kernel page. An access from ring 3 to such a page raises `#PF` with
`user = 1, present = 1` — a protection violation, not a missing page.

```c
        uint32_t flags = PTE_PRESENT | PTE_WRITABLE;
        /*  No PTE_USER anywhere in the kernel half. This one missing bit is
         *  the entire boundary between a user process and the kernel */
```

Note what is **not** doing this work: segmentation. Chapter 15, §3.2:

> Our ring 3 descriptors have base 0 and limit 4 GiB, which includes the kernel's higher half at
> `0xC0000000`. A user program can *name* kernel addresses. What stops it reading them is paging.

A user program can compute `0xC0100000`, store it in a pointer, and pass it to a syscall. It just
cannot dereference it.

Which is exactly why Chapter 33 has to validate pointers.

---

## 3. There is no instruction that lowers privilege

This is the fact the whole chapter turns on.

There is no `enter_ring3`. There is no way to set `CPL` directly — it is the bottom two bits of `CS`,
and `CS` cannot be assigned (Chapter 2, §2.4).

The only instructions that load `CS` are far jumps, far calls, far returns and `iret`. Of those, only
`iret` can *lower* privilege.

So the way down from ring 0 to ring 3 is to **return from an interrupt that never happened**.

```nasm
enter_usermode:
    mov ebx, [esp + 4]             ; entry point in userland
    mov ecx, [esp + 8]             ; top of the user stack

    mov ax, 0x23                   ; SEL_UDATA (descriptor 4, RPL 3)
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push 0x23                      ; SS   -- user stack segment
    push ecx                       ; ESP  -- user stack pointer

    pushf
    pop  eax
    or   eax, 0x200                ; set IF, bit 9
    push eax                       ; EFLAGS

    push 0x1B                      ; CS -- SEL_UCODE, RPL 3
    push ebx                       ; EIP

    iret
```

### 3.1 Data segments can be loaded now

```nasm
    mov ax, 0x23
    mov ds, ax
```

Loading a ring 3 data selector while at CPL 0 is legal:

> the CPU only checks that our *current* privilege (0) is numerically ≤ the descriptor's DPL (3), and
> 0 ≤ 3. Going the other way is what is forbidden.

### 3.2 The frame, pushed in reverse

`iret` pops `EIP`, `CS`, `EFLAGS`, and — because the popped `CS` has a lower privilege — `ESP` and
`SS`.

So we push them in the order `iret` will pop them, which means pushing the last-popped item first:
`SS`, `ESP`, `EFLAGS`, `CS`, `EIP`.

Getting the order wrong gives a `#GP` with an error code pointing at whatever landed in the `CS`
slot, which is at least a hint.

### 3.3 The one line that matters most

```nasm
    pushf
    pop  eax
    or   eax, 0x200                ; set IF, bit 9
    push eax                       ; EFLAGS
```

The comment in the source is not overstated:

> This one line is the difference between a working system and a machine that locks up the moment the
> first user program starts. Interrupts are off right now. If we hand ring 3 an EFLAGS with IF clear,
> the timer never fires again, the scheduler never runs again, and userland owns the CPU forever —
> and ring 3 cannot execute `sti` to fix it, because `sti` is a privileged instruction. The only
> place IF can be set for a user process is here, in the frame we hand to `iret`.

Trace the failure: `iret` with `IF` clear, the program runs, the timer does not tick, `sched_tick`
never runs, `need_resched` is never set, and nothing ever preempts. The machine appears to hang with
a program running at 100%.

And there is nothing userland can do about it. `sti` from ring 3 is `#GP`.

### 3.4 RPL 3 in the selectors

```nasm
    push 0x1B                      ; CS -- SEL_UCODE, RPL 3
```

`0x1B` is `0x18 | 3` — descriptor 3, with a requested privilege level of 3 (Chapter 15, §1).

`iret` checks that the RPL of the target `CS` is numerically ≥ the current CPL. Pushing `0x18`
instead gives RPL 0, which means "return to ring 0" — and the CPU refuses, because the descriptor's
DPL is 3 and you cannot enter a DPL-3 segment at CPL 0.

`#GP` with error code `0x18`. At least it names the selector.

---

## 4. Coming back: the TSS in anger

When the CPU takes an interrupt while at CPL 3, it must switch to a ring 0 stack before pushing
anything. It reads `ss0:esp0` from the TSS.

```c
void tss_set_kernel_stack(uint32_t esp0)
{
    tss.ss0  = SEL_KDATA;
    tss.esp0 = esp0;
}
```

Called on every context switch:

```c
    tss_set_kernel_stack(next->kernel_stack);
```

Chapter 15, §5 gave the bug you get without it, and it is worth repeating because it is the one that
costs a day:

> There is **one TSS for the whole machine**. When process A makes a system call, the CPU reads
> `esp0` and pushes A's trap frame there. When the scheduler switches to B and B makes a system call,
> the CPU reads `esp0` again — and if nobody updated it, it is still pointing at A's kernel stack.
>
> So B's trap frame lands on top of A's. A is blocked in the middle of a system call with its saved
> registers on that stack. When A is next scheduled and returns from its call, it restores registers
> that B overwrote.

The symptom is two processes corrupting each other with no apparent connection between them.

### 4.1 The two frame shapes

```
    interrupt from ring 0              interrupt from ring 3
    ---------------------              ---------------------
                                       SS
                                       ESP        (the user stack pointer)
    EFLAGS                             EFLAGS
    CS                                 CS
    EIP        <- ESP                  EIP        <- ESP, on the KERNEL stack
```

Three values or five, depending on where the interrupt came from.

`registers_t` describes the five-value case, so for a ring 0 fault the last two fields hold whatever
was on the stack already. That is why the register dump checks:

```c
    if ((r->cs & 3) == 3)
        kprintf("  user esp=%08x ss=%04x  (fault came from ring 3)\n",
                r->useresp, r->ss & 0xFFFF);
    else
        kprintf("  (fault came from ring 0; no user stack was pushed)\n");
```

> Printing them unconditionally invents evidence, and inventing evidence during a crash diagnosis is
> worse than printing nothing.

---

## 5. Starting the first user process

`enter_usermode` exists and is correct, and `task_spawn_user` does not use it. Instead it builds a
trap frame by hand:

```c
    registers_t *frame = (registers_t *)(t->kernel_stack - sizeof(registers_t));
    memset(frame, 0, sizeof(registers_t));

    frame->eip     = entry;
    frame->cs      = SEL_UCODE;
    frame->eflags  = 0x202;          /* IF set, plus the always-1 bit 1 */
    frame->useresp = user_esp;
    frame->ss      = SEL_UDATA;
    frame->ds      = SEL_UDATA;

    uint32_t *sp = (uint32_t *)frame;
    *--sp = (uint32_t)isr_return;
    *--sp = 0; *--sp = 0; *--sp = 0; *--sp = 0;
    t->context = (context_t *)sp;
```

### 5.1 Why not just call `enter_usermode`?

Because the task must be *schedulable* before it runs. `enter_usermode` would have to be called by
the task itself, from its own kernel stack, after the scheduler picked it — which means a wrapper
function, which means a special case.

Instead:

- The trap frame sits at the top of the new kernel stack, exactly where `esp0` will point.
- Below it, a context whose `eip` is `isr_return` — the label in the middle of `isr_common_stub`
  (Chapter 16, §6.4).
- The scheduler picks the task, `switch_context` pops four zeros and `ret`s to `isr_return`, which
  pops `DS`, `popa`s, skips `int_no`/`err_code`, and `iret`s into userland.

**The task unwinds a trap frame it never pushed**, exactly as if it had just finished a system call.

Same mechanism as `fork` (Chapter 34), same mechanism as a preempted task resuming
(Chapter 31, §4.3). One path, three uses.

### 5.2 `0x202`

`EFLAGS` with bit 9 (`IF`) and bit 1 set.

Bit 1 is reserved and **always 1** on every x86. Writing 0 to it is ignored by the hardware, but
including it makes the value match what you will see in a register dump, which saves a moment of
confusion.

### 5.3 Writing the user stack through the direct map

```c
    paddr_t  stack_phys = paging_virt_to_phys(t->directory, stack_bottom);
    uint8_t *stack_kv   = (uint8_t *)P2V(stack_phys & PTE_FRAME_MASK);

    uint32_t *top = (uint32_t *)(stack_kv + PAGE_SIZE);
    *--top = 0;                                   /* argv = NULL              */
    *--top = 0;                                   /* argc = 0                 */
```

We are not running in the new address space, so `USER_STACK_TOP` is not a usable pointer here.

Translate to physical, then reach it through the kernel's direct map (Chapter 24, §2.1). Cheaper than
switching `CR3`, and it leaves the currently running task undisturbed.

`task_exec_regs` does the opposite — it switches first, because it needs to write argv strings whose
addresses must be user-space addresses. Both are correct; the difference is whether you need the
addresses or just the bytes.

---

## 6. The file descriptors that make a shell possible

```c
    vfs_node_t *con = console_device_node();
    for (int i = 0; i < 3; i++)
        t->fds[i] = file_open_node(con, i == 0 ? O_RDONLY : O_WRONLY);
```

stdin, stdout and stderr, all pointing at `/dev/console`.

> They are not magic — they are simply the first three descriptors, and they are inherited by every
> child, which is the entire mechanism behind `>` and `|`.

Descriptors 0, 1 and 2 have no special meaning to the kernel. The convention is entirely in userland:
`printf` writes to 1 because `libc.h` says `STDOUT_FILENO 1`, and the shell redirects by replacing
descriptor 1 in the child before `exec`.

That is why redirection needs no kernel support beyond `dup2`.

---

## 7. Running it

```c
    if (!task_spawn_user("/bin/sh"))
        kprintf("could not start /bin/sh\n");
```

```
[    0.412] inf  elf: segment 0 -> [08048000,0804a3c0) r-x
[    0.412] inf  elf: segment 1 -> [0804b000,0804b8e0) rw-
[    0.413] inf  task: pid 1 (/bin/sh) entry 08048080, user esp bffffff8

Nimbus shell. Type `help`.
nimbus>
```

A prompt, printed by a program running at privilege level 3, in its own address space, that reached
the screen through a system call.

### 7.1 Confirming the privilege level

```c
static int32_t sys_getpid(registers_t *regs UNUSED)
{
    kprintf("syscall from cs=%04x (cpl %d)\n", regs->cs, regs->cs & 3);
    return current_task->pid;
}
```

```
syscall from cs=001b (cpl 3)
```

`0x1B`, ring 3. That is the wall, observed.

### 7.2 Testing each mechanism

Three user programs, one per mechanism from §2:

```c
int main(void) { __asm__ volatile ("cli"); return 0; }
```
```
EXCEPTION 13: general protection fault
  error code 0: no segment involved
  (usually a privileged instruction executed in ring 3, ...)
  killing the faulting process
```

```c
int main(void) { __asm__ volatile ("outb %0, %1" :: "a"(0), "Nd"(0x64)); return 0; }
```
```
EXCEPTION 13: general protection fault
  error code 0: no segment involved
```

```c
int main(void) { return *(volatile int *)0xC0100000; }
```
```
PAGE FAULT at c0100000  (eip=08048091)
  protection violation, read, ring 3
  killing pid 4
```

Three attacks, three different exceptions, three clean kills, and a shell that keeps running.

**That is the whole point of Part IV.** A program can be wrong, or hostile, and the machine carries
on.

---

## 8. What could go wrong

| Symptom | Cause |
|---|---|
| Machine hangs the moment userland starts | `IF` not set in the `iret` frame (§3.3) |
| `#GP` on `iret` with error code `0x18` | RPL missing from the `CS` selector |
| Double fault on the first syscall | `esp0` zero, or no TSS loaded |
| Two processes corrupt each other | `tss_set_kernel_stack` not called on switch |
| User program can read kernel memory | `PTE_USER` set somewhere in the kernel half |
| `#GP` immediately after `iret` | `SS` selector wrong, or its RPL missing |
| Fault in `task_spawn_user` | Wrote to `USER_STACK_TOP` without switching or translating |
| Program starts and immediately faults at a low address | Stack pointer wrong, or crt0 expecting a different layout |

---

## 9. Exercises

🟢 **32.1** Write a user program for each of the three mechanisms in §7.2 and confirm the exception
in each case.

🟢 **32.2** Change `frame->eflags` to `0x2` (IF clear) and boot. Describe precisely what you observe
and why nothing recovers.

🟢 **32.3** Change `SEL_UCODE` to `0x18` (RPL 0) and read the `#GP` error code.

🟡 **32.4** Remove `tss_set_kernel_stack` from `schedule()` and run two processes that both make
frequent system calls. How long before something breaks, and what does the crash look like?

🟡 **32.5** Set `PTE_USER` on the kernel's direct map and write a user program that dumps kernel
memory. Then put it back and confirm the dump faults.

🟡 **32.6** Give a user process access to port `0x3F8` via the TSS I/O permission bitmap, and have it
write to the serial port directly with no syscall. This needs the bitmap from Exercise 15.6.

🔴 **32.7** Implement `sysenter`/`sysexit` as a faster path than `int 0x80`. They need three MSRs set
up and have strict requirements on the GDT layout. Measure the difference with `rdtsc`.

🔴 **32.8** Use ring 1 for something: put a "driver" in ring 1 with `IOPL` 1 so it can do port I/O but
not touch control registers. Then discover the limitation from §1 — paging cannot distinguish it from
ring 0 — and write down what that means for the design.

---

## What we covered

- Four rings, two used, and the two reasons nobody uses the middle two.
- Three separate enforcement mechanisms — privileged instructions, `IOPL`/the I/O bitmap, and
  `PTE_USER` — and which one is doing the real work.
- That a user program can *name* kernel addresses and only paging stops it, which is why Chapter 33
  exists.
- No instruction lowers privilege: the only way down is `iret` on a frame we built.
- Why data segments can be loaded at CPL 0 but `CS` cannot.
- The `IF` bit in the `iret` frame — one line, and the machine hangs forever without it, with no way
  for userland to recover.
- RPL 3 in the selectors, and the `#GP` you get without it.
- `esp0` read on every ring 3 → ring 0 transition, one TSS for the machine, and the cross-process
  corruption that follows from forgetting to update it.
- Two trap frame shapes, and not inventing evidence in a register dump.
- Starting pid 1 by hand-building a trap frame and pointing its context at `isr_return` — the same
  trick as `fork` and as a preempted task resuming.
- Writing a user stack through the direct map instead of switching address spaces.
- Three file descriptors with no kernel meaning, and why that is what makes redirection free.

[Chapter 33](33-syscalls.md) opens the one door in the wall, and spends most of its length on why
that is harder than it sounds.

---

[← The scheduler](31-scheduler.md) · [Contents](README.md) · [Next: System calls →](33-syscalls.md)
