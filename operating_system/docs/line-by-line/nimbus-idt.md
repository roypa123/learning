# Line by line: `boot/isr.asm`, `kernel/idt.c`, `kernel/isr.c`

[Index](README.md) · [Chapter 16](../16-idt-exceptions.md) · [Chapter 17](../17-pic-irqs.md)

Forty-nine doors into the kernel, and the one C function behind all of them.

---

# `boot/isr.asm`

## The two macros

```nasm
%macro ISR_NOERR 1
global isr%1
isr%1:
    push dword 0
    push dword %1
    jmp  isr_common_stub
%endmacro

%macro ISR_ERR 1
global isr%1
isr%1:
    push dword %1
    jmp  isr_common_stub
%endmacro
```
⚠️ **The whole reason there are two.** For eight of the 32 exceptions the CPU pushes an error code;
for the other 24 it does not.

Without the fake zero, half our handlers would see a stack one dword deeper than the other half, and
every access to `regs->eip` would be right half the time.

`isr%1` concatenates, producing `isr0`, `isr1`, …

The eight with error codes are 8, 10, 11, 12, 13, 14, 17 and 30. No pattern — Volume 3, Table 6-1.

---

## `isr_common_stub`

```nasm
isr_common_stub:
    pusha
```
Pushes `EAX ECX EDX EBX ESP EBP ESI EDI` in that order, so they appear on the stack **in reverse**
with `EDI` lowest.

⚠️ That is why `registers_t` lists them `edi, esi, ebp, esp_dummy, ebx, edx, ecx, eax`. The struct is
a description of the stack, not a convenience.

```nasm
    mov  eax, ds
    push eax
```
The interrupted code might have been ring 3 with user segments loaded.

⚠️ Only `DS` is saved, not `ES`/`FS`/`GS`. Safe *because* we reload all four with the same kernel
selector and `iret` restores the ring 3 ones from the frame.

Would **not** be safe in a kernel that used `FS` or `GS` for per-CPU data — which is exactly what an
SMP kernel does.

```nasm
    mov  ax, 0x10
    mov  ds, ax
    mov  es, ax
    mov  fs, ax
    mov  gs, ax
```
`SEL_KDATA`. Every memory access through `DS` must go to the kernel's descriptor.

```nasm
    push esp
    call interrupt_dispatch
    add  esp, 4
```
⚠️ `ESP` now points at the bottom of a complete `registers_t`. Passed **by pointer**, so a handler can
*modify* the saved state.

Three features depend on that:

- a system call returns a value by writing `regs->eax`;
- `exec` replaces a program by rewriting `regs->eip` and `regs->useresp`;
- a debugger single-steps by setting `EFLAGS.TF` in `regs->eflags`.

```nasm
global isr_return
isr_return:
    pop  eax
    mov  ds, ax
    mov  es, ax
    mov  fs, ax
    mov  gs, ax

    popa
    add  esp, 8
    iret
```
⚠️ **The `isr_return` label is not decoration.**

`task.c` builds a kernel stack for a forked child whose top contents are a copy of its parent's
`registers_t`, and points the child's context here. The child then unwinds a trap frame it never
pushed and `iret`s into userland as if it had just made a system call.

Three uses of one path: a forked child, the first user process, and a preempted task resuming.

`add esp, 8` discards `int_no` and `err_code` — the CPU knows nothing about them and `iret` would
take them for `EIP` and `CS`.

---

## The stub table

```nasm
section .data
global isr_stub_table
isr_stub_table:
%assign vector 0
%rep 48
    dd isr %+ vector
%assign vector vector + 1
%endrep
```
48 addresses, generated from the same numbers as the stubs — so they cannot drift apart.

A hand-written table with `isr13` at index 12 gives a page fault handler that thinks it is a general
protection fault, which is a genuinely horrible afternoon.

`%+` is explicit token concatenation, needed because `isr` and `vector` are separate tokens.

---

# `kernel/idt.c`

```c
extern uint32_t isr_stub_table[48];
extern uint32_t isr_syscall_stub[1];
```
Declared as `uint32_t`, not function pointers — we only want the numeric address, and a function
pointer array would invite someone to call one from C, which would push a return address the stub's
`iret` is not expecting.

```c
void idt_set_gate(uint8_t vector, uint32_t handler, uint16_t selector, uint8_t flags)
{
    idt[vector].offset_low  = (uint16_t)(handler & 0xFFFF);
    idt[vector].offset_high = (uint16_t)((handler >> 16) & 0xFFFF);
```
The handler address split across bytes 0–1 and 6–7 — 80286 compatibility again.

```c
    memset(idt, 0, sizeof(idt));
```
An entry with the present bit clear means "not handled", and the CPU responds with a `#GP` — which we
*do* handle and which prints something useful. Leaving `.bss` uninitialised would mean a stray
interrupt jumping to a random address.

In practice `.bss` is already zero; this removes the dependency on that.

```c
    for (int v = 0; v < 48; v++)
        idt_set_gate((uint8_t)v, isr_stub_table[v], SEL_KCODE, IDT_INTERRUPT_GATE_K);
```
`SEL_KCODE` is what causes the privilege switch: the gate names a ring 0 segment.

`0x8E` = present, **DPL 0**, 32-bit interrupt gate.

⚠️ DPL 0 means ring 3 cannot reach these with `int`. If the page fault vector were DPL 3, a user
program could execute `int 14` and hand the kernel a fabricated fault — with an error code of its
choosing, at a moment of its choosing, with `CR2` left over from an earlier real fault.

```c
    idt_set_gate(INT_SYSCALL, isr_syscall_stub[0], SEL_KCODE, IDT_INTERRUPT_GATE_U);
```
`0xEE` = **DPL 3**. The one door in the wall.

Still an *interrupt* gate, not a trap gate, so `IF` is cleared on entry. `syscall_dispatch`
re-enables once it is on a safe footing.

```c
    idt_pointer.limit = (uint16_t)(sizeof(idt) - 1);
```
2048 − 1 = 2047. Minus one, as always.

---

# `kernel/isr.c`

## `interrupt_dispatch`

```c
    if (vector == INT_PAGE_FAULT) { page_fault_handler(regs); return; }
    if (vector == INT_SYSCALL)    { syscall_dispatch(regs);   return; }
```
Two special cases before the table.

The page fault gets its own path because it is the only exception we routinely *expect* — demand
paging, stack growth and copy-on-write all arrive here as faults that are not errors.

```c
    } else if (vector >= IRQ_BASE && vector < IRQ_BASE + 16) {
        static uint32_t complaints = 0;
        if (complaints < 8) {
            LOG_WARN("unhandled IRQ %u", vector - IRQ_BASE);
            complaints++;
        }
    }
```
⚠️ The counter matters. A device stuck asserting its line produces an interrupt storm, and a log line
per interrupt at 10,000 per second fills a disk and hides everything else.

Eight lines, then silence.

```c
    if (vector >= IRQ_BASE && vector < IRQ_BASE + 16) {
        pic_send_eoi((uint8_t)(vector - IRQ_BASE));
```
⚠️ Only for hardware IRQs. An EOI for a CPU exception tells the PIC to un-stack an interrupt that was
never stacked, corrupting its priority state.

```c
        if (timer_need_resched && sched_enabled) {
            timer_need_resched = false;
            schedule();
        }
    }
```
⚠️ **After the EOI.** Switching first would leave the PIC waiting for this interrupt's
acknowledgement while an entirely different task ran — and no device could interrupt until this task
was scheduled again.

And here rather than in `timer_callback`, so there is exactly one place in the kernel where
preemption happens. We are on this task's kernel stack holding a complete trap frame; `schedule()`
saves that stack pointer, and when the task is picked again it returns right here and walks back out
through `isr_common_stub`.

---

## `unhandled_exception`

```c
    if (regs->int_no == INT_GENERAL_PROTECTION || ...)
        describe_selector_error(regs->err_code);
```
`#GP`, `#TS`, `#NP` and `#SS` push an error code referring to a **descriptor**:

```
    bit 0      external
    bit 1      the index is in the IDT
    bit 2      with bit 1 clear: the LDT
    bits 3-15  the selector index
```

⚠️ **Not the same layout as a page fault's**, which is the usual source of confusion when a `#GP`
prints something that looks like a page fault code.

Error code 0 means no descriptor was involved — usually a privileged instruction in ring 3, which
once userland exists is the most common `#GP` you will see.

```c
    if ((regs->cs & 3) == 3) {
        kprintf("killing the faulting process\n");
        task_exit(-(int)regs->int_no);
    }

    panic("unhandled exception %u (%s) in kernel mode at eip=%08x", ...);
```
**Ring 3 kills the process; ring 0 kills the machine.** The test is the bottom two bits of the saved
`CS`.

A user program that divides by zero is a bug in that program. A kernel that does has no idea what
state its data structures are in, and continuing risks writing corruption to a disk.

---

## `isr_dump_registers` (in printk.c)

```c
    kprintf("  esi=%08x edi=%08x ebp=%08x esp=%08x\n",
            r->esi, r->edi, r->ebp, (uint32_t)r + sizeof(registers_t));
```
⚠️ `ESP` is **computed**, not read. `regs->esp_dummy` is the pre-`pusha` value and is not what anyone
wants; the interrupted stack pointer is the address just past the whole frame.

```c
    if ((r->cs & 3) == 3)
        kprintf("  user esp=%08x ss=%04x  (fault came from ring 3)\n", ...);
    else
        kprintf("  (fault came from ring 0; no user stack was pushed)\n");
```
⚠️ The CPU pushes `SS:ESP` only on a privilege change. For a kernel-mode fault those two slots hold
whatever was on the stack already.

Printing them unconditionally invents evidence, and inventing evidence during a crash diagnosis is
worse than printing nothing.

---

[Index](README.md) · [Chapter 16](../16-idt-exceptions.md) · [Chapter 17](../17-pic-irqs.md)
