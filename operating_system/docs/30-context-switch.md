# Chapter 30 — The context switch

[← What a process is](29-what-is-a-process.md) · [Contents](README.md) · [Next: The scheduler →](31-scheduler.md)

> 📖 **Line by line:** [cpu.asm](line-by-line/nimbus-switch.md)

---

## Goal

Fourteen instructions, one at a time. This is the whole of multitasking, and it is worth reading as
carefully as anything in this book.

---

## 1. The function

```nasm
switch_context:
    mov eax, [esp + 4]             ; eax = old, a context_t**
    mov edx, [esp + 8]             ; edx = new, a context_t*

    push ebp
    push ebx
    push esi
    push edi

    mov [eax], esp                 ; *old = esp

    mov esp, edx                   ; switch stacks

    pop edi
    pop esi
    pop ebx
    pop ebp

    ret
```

Called as:

```c
    switch_context(&prev->context, next->context);
```

That is it. Everything else in Part IV — preemption, blocking, `fork`, `exec` — is machinery that
decides *when* to call this and what state to have ready.

---

## 2. What we do not have to save

Sixteen registers exist. We save five. The reason is the calling convention.

System V cdecl (Chapter 3, §5.1) divides registers into:

| | Registers | Rule |
|---|---|---|
| **Caller-saved** | `EAX`, `ECX`, `EDX` | a function may destroy them |
| **Callee-saved** | `EBX`, `ESI`, `EDI`, `EBP` | a function must preserve them |

`switch_context` is called from C. Whoever called it has already spilled anything it cared about in
`EAX`, `ECX` and `EDX` — that is what "caller-saved" means. Those three registers are *already*
dead as far as the caller is concerned.

So we save the callee-saved four. Plus:

- **`ESP`**, which is the switch.
- **`EIP`**, which is already on the stack, because we were called.

Five values, and two of them we get for free.

### 2.1 What about the segment registers?

`CS`, `DS`, `ES`, `FS`, `GS`, `SS` are the same in every kernel context — `SEL_KCODE` and
`SEL_KDATA`. A task in the kernel always has kernel segments loaded, because `isr_common_stub` set
them on the way in (Chapter 16, §6.2).

A task's *user* segments are restored by `iret` from the trap frame, not by us.

### 2.2 What about the FPU?

Nimbus does not use floating point in the kernel, and userland has no FPU state because `CR0.EM` is
effectively leaving it unavailable — a floating-point instruction in a user program raises
`#NM`, exception 7.

A kernel that supports FPU in userland must save and restore 512 bytes of `FXSAVE` state per switch,
which is expensive — so real kernels do it **lazily**: set `CR0.TS` on switch, and let exception 7
tell you the first time the new task actually uses the FPU. If it never does, the state is never
touched.

That is about sixty lines and Exercise 30.8.

---

## 3. Instruction by instruction

Suppose task A calls `switch_context(&A->context, B->context)`.

### Before

```
    A's kernel stack                B's kernel stack
    ----------------                ----------------
    ...                             ...
    caller's frame                  [ ebp ]
    ret addr -> back in schedule()  [ ebx ]
    &A->context      <- [esp+4]     [ esi ]
    B->context       <- [esp+8]     [ edi ]  <- B->context points here
                                    [ eip ] -> where B last called switch_context
    ESP -----^
```

B's stack already looks like that because B was switched *away from* earlier — its stack was left in
exactly the state the next four lines will produce.

### `mov eax, [esp + 4]` / `mov edx, [esp + 8]`

Fetch the two arguments. `[esp]` is the return address; `[esp+4]` and `[esp+8]` are arguments 1
and 2.

`EAX` and `EDX` because they are caller-saved and therefore free to clobber.

### `push ebp` / `push ebx` / `push esi` / `push edi`

Save the callee-saved four onto **A's** stack.

The order is not arbitrary:

> Push in this order so that the resulting memory layout, read from the lowest address upwards, is
> edi, esi, ebx, ebp, eip — which is exactly the field order of `struct context`. If you reorder
> these four pushes, reorder the struct with them or the scheduler will restore EBP into EDI.

```c
typedef struct context {
    uint32_t edi;      /* lowest address  */
    uint32_t esi;
    uint32_t ebx;
    uint32_t ebp;
    uint32_t eip;      /* highest address */
} context_t;
```

A stack grows down, so the last thing pushed is at the lowest address. Push `ebp` first and it ends
up highest; push `edi` last and it ends up lowest. Reading the memory upwards gives the struct.

### `mov [eax], esp`

```c
    *old = esp;
```

**This one line is the save.**

The saved `ESP` *is* the saved context: the five values live on A's own kernel stack, and one pointer
finds them all. There is no separate save area, no `memcpy`, no per-task register block.

That is why `context_t *context` in the task struct is a pointer rather than a struct — it points
into the task's own stack.

### `mov esp, edx`

**The switch.**

From this instruction on we are executing on B's kernel stack — still in A's code, for one more
nanosecond.

Everything that was a local variable, everything reachable through `EBP`, every return address: all
of it now belongs to a different task. The only things that survive are the registers, and we are
about to overwrite four of them.

### `pop edi` / `pop esi` / `pop ebx` / `pop ebp`

Restore B's callee-saved four from B's stack. Exactly the reverse of the pushes, which is why the
order had to match.

### `ret`

Pops B's saved `EIP` and jumps there.

**And that return address belongs to a different task.**

B last executed `switch_context` at some point in the past — inside `schedule()`, inside its own
call chain, possibly seconds ago, possibly in a different address space. `ret` resumes it exactly
there.

> Nothing about the instruction knows it just changed universes.

---

## 4. The two-sided nature of it

The strangest property, and the one worth sitting with:

```c
    switch_context(&prev->context, next->context);

    /* execution resumes here when *prev* is scheduled again */
    irq_restore(flags);
```

`switch_context` is called by one task and **returns in another**. Then, later, it returns *again* in
the original one.

From A's point of view: it called a function, and eventually that function returned. In between,
possibly seconds passed, other processes ran, the disk was read and the screen was redrawn. A cannot
tell.

The comment in [`sched.c`](../nimbus/kernel/sched.c) says it:

> Execution resumes here when *prev* is scheduled again — possibly seconds later, possibly in a
> different address space, and with `current_task` once more pointing at prev.

That is the whole illusion. A function call that takes an unpredictable amount of wall-clock time and
returns as if nothing happened.

---

## 5. A new task's first switch

Chapter 29, §8.1 built a stack for a task that has never run:

```
    high   kernel_thread_exit   <- where `entry` returns to
           entry                <- popped by switch_context's `ret`
           0                       ebp
           0                       ebx
           0                       esi
    low    0                       edi   <- context points here
```

Trace `switch_context` against it:

- `mov esp, edx` — `ESP` now points at the `edi` slot.
- Four pops — `EDI`, `ESI`, `EBX`, `EBP` all become zero. `ESP` now points at the `entry` slot.
- `ret` — pops `entry` into `EIP`. **The task starts.**

And `ESP` now points at `kernel_thread_exit`, so when `entry` executes its own `ret`, that is where
it goes.

> There is no special case anywhere in the scheduler for a task that has not started yet, because
> there does not need to be.

The same trick, in a different shape, is how `fork` gives a child a trap frame it never pushed
(Chapter 34) and how `task_spawn_user` starts pid 1 (Chapter 32).

---

## 6. What must happen around it

`switch_context` only swaps registers and stacks. Two other things must change, and
[`sched.c`](../nimbus/kernel/sched.c) does them immediately before:

```c
    if (next->directory && next->directory != current_directory)
        paging_switch_directory(next->directory);

    tss_set_kernel_stack(next->kernel_stack);

    switch_context(&prev->context, next->context);
```

### 6.1 `CR3` first

> CR3 first: after this instruction we are running in the new task's address space. That is safe only
> because the kernel half of every directory is identical, so the code we are executing, the stack we
> are on and the task structs we are touching are all still mapped.

Chapter 28, §6. The instruction `write_cr3` changes the meaning of every address, including the
address of the next instruction. It works only because the kernel half is shared.

Note that `prev`'s stack — which `mov [eax], esp` is about to write to — is also in the kernel half,
so it is still reachable after the `CR3` change.

### 6.2 Then `esp0`

> the next time this task traps in from ring 3, the CPU will read the TSS to find a ring 0 stack.
> Update it now, while we still know which task is about to run.

Chapter 15, §5 gave the bug you get without it: two processes making system calls scribble over each
other's kernel stacks, and the crash happens in whichever returns second, in a function that did
nothing wrong.

### 6.3 And interrupts, around the whole thing

```c
    uint32_t flags = irq_save();
    ...
    switch_context(&prev->context, next->context);
    irq_restore(flags);
```

The window between "choose the next task" and "switch to it" must be atomic. An interrupt in the
middle could block the task we just chose, and we would switch to something not runnable.

The subtle part:

> the flags are saved and restored around the switch rather than blindly re-enabled, because
> schedule() is called both from ordinary code (where interrupts were on) and from the tail of an
> interrupt handler (where they were off). Note that the restore happens *after* the switch returns —
> and the switch returns in a different task, whose saved flags are its own. That is not a bug; it is
> how each task keeps its own interrupt state across a switch.

`flags` is a local. Every task has its own copy on its own stack. When A resumes, it restores A's
flags; when B resumed, it restored B's.

---

## 7. Why it cannot be C

Three things GCC will not do:

**Change `ESP` mid-function and keep going.** GCC owns the stack pointer within a function; it uses
it for locals and it assumes it did not move.

**Return to a different address than it was called from.** Which is exactly what `ret` does here.

**Guarantee the push order.** Even if you wrote inline assembly for the pushes, GCC might insert its
own prologue between them.

It is also one of the few places where the assembly is genuinely *clearer* than any C would be. The
mechanism is "swap two stack pointers", and fourteen instructions say that plainly.

---

## 8. Watching it happen

### 8.1 In GDB

```bash
make debug
```

```
(gdb) target remote :1234
(gdb) break switch_context
(gdb) continue

Breakpoint 1, switch_context ()
(gdb) info registers esp
esp   0xc010cf24

(gdb) x/8x $esp
0xc010cf24:  0xc0105a31  0xc0113f10  0xc0114228  0x00000000

(gdb) stepi 7
(gdb) info registers esp
esp   0xc0114210          <- a different stack entirely

(gdb) stepi 5
(gdb) bt
#0  schedule () at kernel/sched.c:118
#1  sleep_ms (ms=500) at kernel/sched.c:241
#2  counter_thread () at kernel/main.c:52
```

`ESP` jumps from one task's stack to another's between two instructions, and the backtrace afterwards
is a completely different call chain.

That is the clearest demonstration available and it takes two minutes.

### 8.2 Counting switches

```c
static uint32_t switches = 0;
...
    current_task = next;
    switches++;
```

Add it to `ps`:

```
nimbus> ps
 PID PPID  STATE     TICKS  NAME
   0    0  running    1523  idle
   1    0  sleeping     12  counter
switches: 3046
```

At 100 Hz with a 5-tick slice, a busy system switches about 20 times a second per runnable task. If
the count is far higher, something is yielding in a loop; far lower, something is not being
preempted.

---

## 9. How expensive is it?

Fourteen instructions is perhaps 20 cycles. But the *real* cost is elsewhere:

| Cost | Cycles (roughly) |
|---|---|
| The fourteen instructions | ~20 |
| `CR3` reload (if the address space changes) | ~100, plus a full TLB flush |
| TLB misses afterwards | hundreds to thousands |
| Cache misses on the new task's working set | thousands |
| `tss_set_kernel_stack` | ~5 |

So a switch between two threads *sharing* an address space is cheap — no `CR3`, no TLB flush — and a
switch between processes is one to two orders of magnitude more expensive.

That asymmetry is the entire reason threads exist as a concept distinct from processes, and it is
why the `!=` check in §6.1 matters:

```c
    if (next->directory && next->directory != current_directory)
```

It is also why `PTE_GLOBAL` (Chapter 23, §3.7) is worth having in a real kernel: it stops the `CR3`
reload flushing the kernel's own translations, which are identical either side of the switch.

---

## 10. Exercises

🟢 **30.1** Reorder `push ebx` and `push esi` in `cpu.asm` without changing `context_t`. Predict the
symptom, then run it.

🟢 **30.2** Put a breakpoint on `switch_context` in GDB and step through it twice, recording `ESP`
before and after each time. Confirm the two stacks alternate.

🟢 **30.3** Add the switch counter from §8.2 and watch it under `forktest`.

🟡 **30.4** Save and restore `ESI` only, leaving `EDI`, `EBX` and `EBP` alone. Find a workload that
breaks, and explain why it takes a while.

🟡 **30.5** Remove the `!=` check on the directory and measure the effect with `rdtsc` (Exercise
18.8) on two kernel threads that share the kernel address space.

🟡 **30.6** Write `switch_context` in C with inline assembly and see how far you get before the
compiler defeats you. Document exactly where it breaks.

🔴 **30.7** Implement `PTE_GLOBAL` for the kernel's direct map and measure the switch cost before and
after with `rdtsc`.

🔴 **30.8** Add lazy FPU switching: set `CR0.TS` on every switch, handle exception 7 by `FXRSTOR`ing
the new task's state and clearing `TS`, and `FXSAVE` on the way out. You will need a 512-byte
16-byte-aligned area per task.

---

## What we covered

- Fourteen instructions that are the whole of multitasking.
- Why five registers and not sixteen: the calling convention already saved the rest.
- Why segments and FPU state are not here, and what lazy FPU switching would add.
- Each instruction: the two argument fetches, four pushes in an order that must match the struct, the
  one line that *is* the save, the one line that *is* the switch, four pops, and a `ret` into a
  different task.
- The saved `ESP` being the entire saved context — five values on the task's own stack, found by one
  pointer.
- A function that is called in one task and returns in another, and then returns again in the first.
- How a never-run task's hand-built stack falls out of the same fourteen instructions with no special
  case.
- What must happen around it: `CR3` first (safe only because the kernel half is shared), then `esp0`,
  with interrupts off — and why the flags restore lands in a different task.
- Why GCC cannot produce this, and why it is clearer in assembly anyway.
- What it actually costs, and why that asymmetry is the reason threads exist.

[Chapter 31](31-scheduler.md) decides *who* to switch to, and when.

---

[← What a process is](29-what-is-a-process.md) · [Contents](README.md) · [Next: The scheduler →](31-scheduler.md)
