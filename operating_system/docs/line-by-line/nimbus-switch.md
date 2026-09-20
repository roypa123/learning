# Line by line: `nimbus/boot/cpu.asm` (switch and usermode)

[Index](README.md) · [Chapter 30](../30-context-switch.md) · [Chapter 32](../32-usermode.md)

The two routines that cannot be C.

---

# `switch_context`

```c
    switch_context(&prev->context, next->context);
```

```nasm
switch_context:
    mov eax, [esp + 4]             ; eax = old, a context_t**
    mov edx, [esp + 8]             ; edx = new, a context_t*
```
`[esp]` is the return address. `EAX` and `EDX` because they are caller-saved and free to clobber.

```nasm
    push ebp
    push ebx
    push esi
    push edi
```
⚠️ **The order is not arbitrary.**

A stack grows down, so the last thing pushed is at the lowest address. Push `ebp` first and it ends
up highest; push `edi` last and it ends up lowest. Reading the memory upwards gives:

```c
typedef struct context {
    uint32_t edi;      /* lowest  */
    uint32_t esi;
    uint32_t ebx;
    uint32_t ebp;
    uint32_t eip;      /* highest */
} context_t;
```

Reorder these four without reordering the struct and the scheduler restores `EBP` into `EDI`.

**Why only four?** The System V convention says a function may destroy `EAX`, `ECX` and `EDX`.
Whoever called us is a C function, so it has already spilled anything it cared about in those. We
only need the callee-saved set — plus `ESP`, which is the switch, and `EIP`, which is already on the
stack because we were called.

```nasm
    mov [eax], esp                 ; *old = esp
```
⚠️ **This one line is the save.**

The saved `ESP` *is* the saved context: the five values live on the task's own kernel stack, and one
pointer finds them all. No separate save area, no `memcpy`, no per-task register block.

Which is why `context_t *context` in the task struct is a pointer — it points into the task's own
stack.

```nasm
    mov esp, edx                   ; switch stacks
```
⚠️ **The switch.**

From this instruction we are executing on the new task's kernel stack — still in the old task's code,
for one more nanosecond.

Everything that was a local, everything through `EBP`, every return address: all of it now belongs to
a different task.

```nasm
    pop edi
    pop esi
    pop ebx
    pop ebp
```
Exact reverse of the pushes.

```nasm
    ret
```
⚠️ Pops the **new** task's saved `EIP`.

That return address belongs to a different task. It last executed `switch_context` at some point in
the past, inside `schedule()`, possibly seconds ago, possibly in a different address space.

> Nothing about the instruction knows it just changed universes.

## A new task

`task_spawn_kernel` builds:

```
    high   kernel_thread_exit   <- where `entry` returns to
           entry                <- popped by this `ret`
           0                       ebp
           0                       ebx
           0                       esi
    low    0                       edi   <- context points here
```

Trace it: `mov esp, edx` points at `edi`; four pops give zeros; `ret` pops `entry` and the task
starts. `ESP` then points at `kernel_thread_exit`, so when `entry` returns, that is where it goes.

> There is no special case anywhere in the scheduler for a task that has not started yet, because
> there does not need to be.

## What is not saved

**Segment registers** — the same in every kernel context, set by `isr_common_stub` on the way in, and
the user ones restored by `iret` from the trap frame.

**FPU state** — Nimbus does not use floating point. A kernel that supports it in userland saves 512
bytes of `FXSAVE` per switch, so real kernels do it **lazily**: set `CR0.TS` on switch and let
exception 7 tell you the first time the new task actually uses the FPU.

---

# `enter_usermode`

There is no instruction that lowers privilege. `CS` cannot be assigned, and the only instructions
that load it are far jumps, far calls, far returns and `iret` — of which only `iret` can lower.

So the way down is to **return from an interrupt that never happened**.

```nasm
enter_usermode:
    mov ebx, [esp + 4]             ; entry point
    mov ecx, [esp + 8]             ; user stack top

    mov ax, 0x23
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
```
Loading a ring 3 data selector at CPL 0 is legal: the CPU checks that the current privilege (0) is
numerically ≤ the descriptor's DPL (3). Going the other way is what is forbidden.

```nasm
    push 0x23                      ; SS
    push ecx                       ; ESP
```
Pushed in the order `iret` will **pop**, so the last-popped item goes first.

```nasm
    pushf
    pop  eax
    or   eax, 0x200
    push eax                       ; EFLAGS
```
⚠️ **The most consequential line in the file.**

> Interrupts are off right now. If we hand ring 3 an EFLAGS with IF clear, the timer never fires
> again, the scheduler never runs again, and userland owns the CPU forever — and ring 3 cannot
> execute `sti` to fix it, because `sti` is a privileged instruction. The only place IF can be set
> for a user process is here, in the frame we hand to `iret`.

The machine appears to hang with a program running at 100%, and nothing recovers.

```nasm
    push 0x1B                      ; CS -- SEL_UCODE, RPL 3
    push ebx                       ; EIP

    iret
```
⚠️ `0x1B` is `0x18 | 3` — descriptor 3 with RPL 3.

`iret` checks that the target `CS`'s RPL is ≥ the current CPL. Pushing `0x18` means "return to ring
0", and the CPU refuses because the descriptor's DPL is 3. `#GP` with error code `0x18`.

The CPU pops `EIP` and `CS`, sees privilege 3 against the current 0, and therefore also pops `ESP`
and `SS` and switches stacks.

## Why `task_spawn_user` does not use it

It exists and is correct, and `task_spawn_user` builds a trap frame by hand instead — because the
task must be *schedulable* before it runs, and calling `enter_usermode` would need a wrapper
function, which means a special case.

Instead: the trap frame at the top of the new kernel stack (where `esp0` will point), and a context
whose `eip` is `isr_return`. The scheduler picks the task, `switch_context` `ret`s into
`isr_return`, and the task unwinds a trap frame it never pushed.

Same mechanism as `fork`, same mechanism as a preempted task resuming. One path, three uses.

---

# The two smaller routines

## `gdt_flush`

Covered in [nimbus-gdt.md](nimbus-gdt.md). The far jump to the next instruction is the only way to
reload `CS`.

## `idt_load`

```nasm
idt_load:
    mov eax, [esp + 4]
    lidt [eax]
    ret
```
No reload dance: there is no "current interrupt descriptor" cached in a register. The next interrupt
simply uses the new table.

## `tss_flush`

```nasm
tss_flush:
    mov ax, 0x2B
    ltr ax
    ret
```
`0x2B` is descriptor 5 with RPL 3 — see [nimbus-gdt.md](nimbus-gdt.md).

---

## What must happen around a switch

From `sched.c`:

```c
    if (next->directory && next->directory != current_directory)
        paging_switch_directory(next->directory);

    tss_set_kernel_stack(next->kernel_stack);

    switch_context(&prev->context, next->context);
```

⚠️ **`CR3` first.** After that instruction we are in the new address space — safe only because the
kernel half of every directory is identical, so the code, the stack and the task structs are all
still mapped.

⚠️ **Then `esp0`**, or the next ring 3 trap lands on the previous task's kernel stack.

⚠️ **And interrupts off around the whole thing**, with the flags restored *after* the switch returns —
which is in a different task, whose saved flags are its own. Each task keeps its own interrupt state
across a switch.

---

## Cost

| | Cycles (roughly) |
|---|---|
| The fourteen instructions | ~20 |
| `CR3` reload (if the address space changes) | ~100, plus a full TLB flush |
| TLB misses afterwards | hundreds to thousands |
| Cache misses on the new working set | thousands |

⚠️ A switch between two threads *sharing* an address space is cheap; between processes it is one to
two orders of magnitude more.

That asymmetry is the entire reason threads exist as a concept distinct from processes, and it is why
the `!=` check on the directory matters.

---

[Index](README.md) · [Chapter 30](../30-context-switch.md) · [Chapter 32](../32-usermode.md)
