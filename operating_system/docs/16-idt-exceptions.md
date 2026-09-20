# Chapter 16 — Interrupts I: the IDT and CPU exceptions

[← The GDT](15-gdt.md) · [Contents](README.md) · [Next: The PIC and IRQs →](17-pic-irqs.md)

> 📖 **Line by line:** [idt.c / isr.asm](line-by-line/nimbus-idt.md)

---

## Goal

Install an interrupt descriptor table and handle the thirty-two exceptions the CPU can raise. By the
end of this chapter a divide by zero produces a diagnostic instead of a reboot, and the kernel has
the entry path that everything else in Part IV will use.

This is the chapter where the kernel becomes event-driven.

---

## 1. Three things arrive the same way

Until now the kernel has run in a straight line: `kmain` calls functions, they return. An operating
system does not work like that. It mostly does not run at all — a user program does — and it wakes
up when something happens.

Three different somethings, all delivered by the same mechanism:

**Exceptions** — the running code did something impossible. Divided by zero, executed an invalid
opcode, touched an unmapped page. Vectors 0–31, defined by Intel.

**Hardware interrupts** — a device wants attention. A key was pressed, a disk read finished, the
timer ticked. Vectors 32–47 after we remap the PIC in Chapter 17.

**Software interrupts** — the running code asked for something. `int 0x80`, our system call gate,
Chapter 33.

In every case the CPU saves a little state, looks up an entry in a table we installed, and jumps. The
table is the IDT.

The mental model worth building now: **a kernel is a pile of interrupt handlers with some data
structures between them.**

---

## 2. What the CPU does

When vector N fires:

1. Look up entry N in the IDT (256 entries, 8 bytes each).
2. Check the gate is present and, for a software `int`, that the current privilege level is numerically ≤ the gate's DPL.
3. **If the gate's code segment is more privileged than the current CS**, switch stacks: read `ss0:esp0` from the TSS, load them, and push the *old* `SS` and `ESP`.
4. Push `EFLAGS`, `CS`, `EIP`.
5. For some exceptions, push an error code.
6. If it is an interrupt gate, clear `IF`.
7. Load `CS:EIP` from the gate and continue.

Step 3 is the one that makes the frame shape vary, and Chapter 3, §4 covered it. Step 6 is the
difference between an interrupt gate and a trap gate.

The resulting stack, arriving from ring 0:

```
    high    EFLAGS
            CS
            EIP          <- ESP
```

and from ring 3:

```
    high    SS
            ESP          (the user stack pointer)
            EFLAGS
            CS
            EIP          <- ESP, on the KERNEL stack from the TSS
```

---

## 3. A gate descriptor

```c
typedef struct idt_entry {
    uint16_t offset_low;     /* handler address, bits 0..15   */
    uint16_t selector;       /* which GDT code segment        */
    uint8_t  zero;           /* must be 0                     */
    uint8_t  type_attr;      /* P | DPL(2) | 0 | type(4)      */
    uint16_t offset_high;    /* handler address, bits 16..31  */
} PACKED idt_entry_t;
```

Eight bytes, same as a GDT entry, different meaning: instead of describing a region of memory it
describes *a place to jump to*.

The 32-bit handler address is split across bytes 0–1 and 6–7. Same reason as the GDT: the 286's gates
were six bytes with a 16-bit offset, and the 386 bolted the high half onto the end.

`selector` is a GDT selector — which code segment the handler runs in. Ours is always `SEL_KCODE`
(`0x08`), and that is what makes the privilege switch happen: the gate names a ring 0 segment, so
entering it from ring 3 raises the privilege level.

### 3.1 `type_attr`

```
    bit  7    P, present
    bits 6-5  DPL
    bit  4    0 (this is a system descriptor)
    bits 3-0  type
```

Four values matter:

```c
#define IDT_INTERRUPT_GATE_K  0x8E   /* 1 00 0 1110 : present, DPL 0, 32-bit interrupt */
#define IDT_INTERRUPT_GATE_U  0xEE   /* 1 11 0 1110 : present, DPL 3, 32-bit interrupt */
#define IDT_TRAP_GATE_K       0x8F   /* 1 00 0 1111 : present, DPL 0, 32-bit trap      */
#define IDT_TRAP_GATE_U       0xEF
```

**Interrupt gate versus trap gate is one bit.** An interrupt gate clears `IF` on entry; a trap gate
leaves it alone.

Hardware interrupt handlers want `IF` cleared, so they are not re-entered by the same device
mid-handler. Debug traps want it left alone so that the debugger can breathe. Everything in Nimbus is
an interrupt gate.

**DPL is the privilege required to invoke the vector with an `int` instruction.** Hardware can always
deliver an interrupt regardless of DPL — the check only applies to the software path.

This is why every vector except the syscall gate is DPL 0:

```c
    for (int v = 0; v < 48; v++)
        idt_set_gate((uint8_t)v, isr_stub_table[v], SEL_KCODE, IDT_INTERRUPT_GATE_K);

    idt_set_gate(INT_SYSCALL, isr_syscall_stub[0], SEL_KCODE, IDT_INTERRUPT_GATE_U);
```

If the page fault vector were DPL 3, a user program could execute `int 14` and hand the kernel a
fabricated page fault — with an error code of its choosing, at a moment of its choosing, with `CR2`
left over from some earlier real fault. The handler would then "fix" a fault that never happened.

One bit, and it is the difference between a syscall interface and an arbitrary-kernel-entry
interface.

---

## 4. Why the handlers must be assembly

Two reasons, and Chapter 3, §1 named both.

**The return.** A C function ends with `ret`, which pops one dword. An interrupt handler must end
with `iret`, which pops three or five. GCC will not emit one.

**The entry.** The CPU does not save the general-purpose registers. If the first thing that runs is
compiled C, that C clobbers `EAX` before anything has saved it, and the interrupted program resumes
with a corrupted register and no clue why.

> GCC does have `__attribute__((interrupt))`, which emits `iret` and saves registers. It produces
> subtly different stack layouts across versions, it does not let you unify the error-code and
> no-error-code cases, and it hides exactly the mechanism this book exists to show.

---

## 5. Forty-nine stubs from two macros

An IDT entry can only point at an address. It cannot pass an argument, so the handler for vector 13
has no way of knowing it is vector 13 — unless every vector gets its own tiny stub that pushes the
number.

```nasm
%macro ISR_NOERR 1
global isr%1
isr%1:
    push dword 0                   ; fake error code, so all frames match
    push dword %1                  ; the vector number
    jmp  isr_common_stub
%endmacro

%macro ISR_ERR 1
global isr%1
isr%1:
    ; no push of zero: the CPU already pushed a real error code
    push dword %1
    jmp  isr_common_stub
%endmacro
```

### 5.1 The error code inconsistency

For eight of the thirty-two exceptions the CPU pushes a 32-bit error code. For the other
twenty-four it does not.

If we did nothing, half our handlers would see a stack one dword deeper than the other half, and
every access to `regs->eip` would be right half the time.

So the no-error-code stubs push a zero themselves. Every frame in the kernel then has identical
shape, and one `registers_t` describes all of them.

The eight with error codes are 8, 10, 11, 12, 13, 14, 17 and 30. There is no pattern; it is a list
you check against Volume 3, Table 6-1 and then never think about again.

### 5.2 The stub list

```nasm
ISR_NOERR 0      ; #DE  divide error
ISR_NOERR 1      ; #DB  debug
ISR_NOERR 2      ;      non-maskable interrupt
ISR_NOERR 3      ; #BP  breakpoint (int3)
...
ISR_ERR   8      ; #DF  double fault          <- always error code 0
ISR_NOERR 9      ;      coprocessor segment overrun (386 only)
ISR_ERR   10     ; #TS  invalid TSS
ISR_ERR   11     ; #NP  segment not present
ISR_ERR   12     ; #SS  stack-segment fault
ISR_ERR   13     ; #GP  general protection
ISR_ERR   14     ; #PF  page fault            <- CR2 holds the address
...
```

Plus sixteen for the IRQs and one for `int 0x80`.

### 5.3 The table that cannot drift

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

Without this, `idt.c` needs 49 `extern void isrN(void);` declarations and 49 calls to
`idt_set_gate`. With it, a four-line loop.

The real benefit is that the table and the stubs are generated from the same numbers, so they cannot
disagree. A hand-written table with `isr13` at index 12 produces a page fault handler that thinks it
is a general protection fault, which is a genuinely horrible afternoon.

---

## 6. The common stub

```nasm
isr_common_stub:
    pusha

    mov  eax, ds
    push eax

    mov  ax, 0x10                  ; SEL_KDATA
    mov  ds, ax
    mov  es, ax
    mov  fs, ax
    mov  gs, ax

    push esp
    call interrupt_dispatch
    add  esp, 4

    pop  eax
    mov  ds, ax
    mov  es, ax
    mov  fs, ax
    mov  gs, ax

    popa
    add  esp, 8
    iret
```

Fifteen instructions, and each block is doing something specific.

### 6.1 `pusha`

Pushes `EAX ECX EDX EBX ESP EBP ESI EDI`, in that order, so they appear on the stack in *reverse*
order with `EDI` lowest. That is exactly why `registers_t` lists them that way:

```c
typedef struct registers {
    uint32_t ds;
    uint32_t edi, esi, ebp, esp_dummy, ebx, edx, ecx, eax;
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags, useresp, ss;
} registers_t;
```

**This struct is not a convenience.** Its field order is exactly the stack layout, and reordering a
field without changing `isr.asm` makes the kernel read `EIP` out of the slot holding `EDI`.

`esp_dummy` is where `pusha` stored `ESP` — its value *before* the pusha, which is not useful, and
which `popa` deliberately discards. Naming it "dummy" stops anyone trusting it.

### 6.2 Saving only `DS`

```nasm
    mov  eax, ds
    push eax
```

The interrupted code might have been ring 3 with user segments loaded. The handler must run with
kernel segments or every memory access through `DS` goes to the wrong descriptor.

Only `DS` is saved, not `ES`/`FS`/`GS`. That is a deliberate shortcut and it is safe *because* we
always reload all four with the same kernel selector, and `iret` restores the ring 3 ones from the
frame along with `CS` and `SS`.

It would not be safe in a kernel that used `FS` or `GS` for per-CPU data — which is exactly what an
SMP kernel does, and exactly the thing Chapter 48 warns about.

### 6.3 `push esp` — the pointer that makes everything work

```nasm
    push esp
    call interrupt_dispatch
    add  esp, 4
```

After `pusha` and the `DS` push, `ESP` points at the bottom of a complete `registers_t`. We pass that
address as the single argument.

**By pointer, not by value**, so that a handler can *modify* the saved state. That is not a
convenience; it is how three separate things work:

- A system call returns a value by writing `regs->eax` (Chapter 33).
- `exec` replaces a running program by rewriting `regs->eip` and `regs->useresp` (Chapter 34).
- A debugger single-steps by setting `EFLAGS.TF` in `regs->eflags` (Chapter 47).

All three work because the stub restores the whole frame from the same memory the handler just
edited.

### 6.4 The unwind, and one label

```nasm
global isr_return
isr_return:
    pop  eax
    mov  ds, ax
    ...
    popa
    add  esp, 8
    iret
```

Exactly the reverse of the entry. `add esp, 8` discards `int_no` and `err_code`, which the CPU knows
nothing about and which `iret` would otherwise take for `EIP` and `CS`.

The `isr_return` label is not decoration. Chapter 34 builds a brand-new kernel stack for a forked
child whose top contents are a copy of its parent's `registers_t`, and points the child's first
scheduled instruction *here*. The child then unwinds a trap frame it never pushed and `iret`s into
userland as if it had just made a system call — which, from its point of view, it had.

---

## 7. The dispatcher

```c
void interrupt_dispatch(registers_t *regs)
{
    uint32_t vector = regs->int_no;

    if (vector == INT_PAGE_FAULT) { page_fault_handler(regs); return; }
    if (vector == INT_SYSCALL)    { syscall_dispatch(regs);   return; }

    if (handlers[vector]) {
        handlers[vector](regs);
    } else if (vector < 32) {
        unhandled_exception(regs);
    } else if (vector >= IRQ_BASE && vector < IRQ_BASE + 16) {
        static uint32_t complaints = 0;
        if (complaints < 8) {
            LOG_WARN("unhandled IRQ %u", vector - IRQ_BASE);
            complaints++;
        }
    }

    if (vector >= IRQ_BASE && vector < IRQ_BASE + 16) {
        pic_send_eoi((uint8_t)(vector - IRQ_BASE));
        ...
    }
}
```

One C function, reachable from all forty-nine stubs.

The page fault gets its own path because it is the only exception we routinely *expect*: demand
paging, stack growth and copy-on-write all arrive here as faults that are not errors (Chapter 26).

The `complaints < 8` counter is a small thing worth copying. An unhandled IRQ from a device we do not
drive is not fatal — but a device stuck asserting its line produces an interrupt storm, and a log
line per interrupt at 10,000 per second fills a disk and hides everything else. Eight lines, then
silence.

---

## 8. The thirty-two exceptions

The ones you will actually meet, in order of how often:

| # | Name | What it means | Usually caused by |
|---|---|---|---|
| 14 | `#PF` Page fault | An address was not mapped, or the access was not permitted | Everything, in Part III |
| 13 | `#GP` General protection | A catch-all protection violation | Bad segment, privileged instruction in ring 3, bad IDT entry |
| 6 | `#UD` Invalid opcode | The CPU does not recognise these bytes | Jumped into data |
| 8 | `#DF` Double fault | A fault occurred while handling a fault | No stack, or no handler for the first fault |
| 0 | `#DE` Divide error | `div` by zero, or a quotient that does not fit | Chapter 6, §3.1 |
| 3 | `#BP` Breakpoint | `int3` executed | A debugger |
| 12 | `#SS` Stack fault | Something wrong with `SS` or a stack access | A stack that grew past its segment |
| 7 | `#NM` Device not available | An FPU instruction with `CR0.TS` set | Lazy FPU switching, which we do not do |

### 8.1 Double faults, and the fault that cannot be reported

Vector 8 is special. It fires when the CPU cannot deliver an exception — because the handler's stack
is invalid, or the IDT entry is not present, or the handler's code segment is bad.

Its error code is always zero and carries no information. What it tells you is "something went wrong
while something else was going wrong", and the *first* fault is the one you want.

If the double fault handler *also* cannot run, the CPU gives up and performs a **triple fault**,
which on a PC is wired to the reset line. The machine reboots instantly, with no output.

That is the failure mode this book warned about in Chapter 0, and Chapter 47 is about finding the
first fault in the sequence. The short version:

```bash
qemu-system-i386 ... -no-reboot -d int -D log.txt
```

and look for the first `v=` line in the cascade.

### 8.2 The error code for selector faults

```c
static void describe_selector_error(uint32_t err)
{
    if (err == 0) {
        kprintf("  error code 0: no segment involved\n"
                "  (usually a privileged instruction executed in ring 3,\n"
                "   or a write to a read-only control register)\n");
        return;
    }
    kprintf("  error code %08x: %s, table=%s, index=%u\n",
            err,
            (err & 1) ? "external" : "internal",
            (err & 2) ? "IDT" : ((err & 4) ? "LDT" : "GDT"),
            (err >> 3) & 0x1FFF);
}
```

`#GP`, `#TS`, `#NP` and `#SS` push an error code that refers to a *descriptor*:

```
    bit 0      external: the fault came from a hardware interrupt
    bit 1      the index refers to the IDT, not the GDT
    bit 2      with bit 1 clear: the index refers to the LDT
    bits 3-15  the selector index
```

This layout is **not** the same as a page fault's, which is the usual source of confusion when a
`#GP` prints something that looks like a page fault code.

An error code of 0 means no descriptor was involved, which for a `#GP` usually means a privileged
instruction in ring 3 — which, once userland exists, is the single most common `#GP` you will see.

---

## 9. Dying informatively

```c
static void unhandled_exception(registers_t *regs)
{
    kprintf("\nEXCEPTION %u: %s\n", regs->int_no, isr_exception_name(regs->int_no));

    if (regs->int_no == INT_GENERAL_PROTECTION || ...)
        describe_selector_error(regs->err_code);

    isr_dump_registers(regs);

    if ((regs->cs & 3) == 3) {
        kprintf("killing the faulting process\n");
        task_exit(-(int)regs->int_no);
    }

    panic("unhandled exception %u (%s) in kernel mode at eip=%08x",
          regs->int_no, isr_exception_name(regs->int_no), regs->eip);
}
```

**A fault in ring 3 kills the process. A fault in ring 0 kills the machine.**

The test is the bottom two bits of the saved `CS` — the privilege level the code was running at when
the exception happened.

That asymmetry is the right one. A user program that divides by zero is a bug in that program; the
kernel is intact and should carry on. A kernel that divides by zero has no idea what state its data
structures are in, and continuing risks writing corruption to a disk.

### 9.1 The register dump, and the two fields that are sometimes lies

```c
void isr_dump_registers(registers_t *r)
{
    kprintf("  eax=%08x ebx=%08x ecx=%08x edx=%08x\n", ...);
    kprintf("  esi=%08x edi=%08x ebp=%08x esp=%08x\n",
            r->esi, r->edi, r->ebp, (uint32_t)r + sizeof(registers_t));
    kprintf("  eip=%08x cs=%04x eflags=%08x\n", ...);
    kprintf("  int=%u err=%08x ds=%04x\n", ...);

    if ((r->cs & 3) == 3)
        kprintf("  user esp=%08x ss=%04x  (fault came from ring 3)\n",
                r->useresp, r->ss & 0xFFFF);
    else
        kprintf("  (fault came from ring 0; no user stack was pushed)\n");
}
```

Two details worth copying.

**`ESP` is computed, not read.** `regs->esp_dummy` is the pre-`pusha` value and is not what anyone
wants. The interrupted stack pointer is the address just past the whole frame, so
`(uint32_t)r + sizeof(registers_t)` is correct.

**`useresp` and `ss` are only printed when they are real.** The CPU pushes them only on a privilege
change, so for a kernel-mode fault those two slots hold whatever was on the stack already. Printing
them unconditionally invents evidence, and inventing evidence during a crash diagnosis is worse than
printing nothing.

---

## 10. Running it

```c
    idt_init();
    __asm__ volatile ("int $3");        /* breakpoint */
```

```
EXCEPTION 3: breakpoint
  eax=00000000 ebx=00010000 ecx=c0107000 edx=000003f8
  esi=00000000 edi=00000000 ebp=c0106fd8 esp=c0106fc0
  eip=c0101a47 cs=0008 eflags=00000046
  int=3 err=00000000 ds=0010
  (fault came from ring 0; no user stack was pushed)

*** KERNEL PANIC ***
unhandled exception 3 (breakpoint) in kernel mode at eip=c0101a47
```

Check `eip` against the disassembly:

```bash
$ i686-elf-objdump -d bin/nimbus.elf | grep -A1 c0101a45
c0101a45:  cc                     int3
c0101a47:  83 ec 0c               sub    esp,0xc
```

`EIP` is the instruction *after* `int3`, which is correct for a trap — traps report the next
instruction, faults report the faulting one. That distinction matters for the page fault handler,
which must be able to re-execute the instruction that faulted.

And a divide by zero:

```c
    volatile int zero = 0;
    volatile int boom = 1 / zero;
```

```
EXCEPTION 0: divide error
  ...
  eip=c0101a63 cs=0008 eflags=00000046
```

Both of those produce a diagnostic and a clean stop instead of an instant reboot. That is what this
chapter bought.

---

## 11. Exercises

🟢 **16.1** Trigger each of `int3`, a divide by zero, and an invalid opcode
(`__asm__ volatile(".byte 0x0f, 0x0b")`). Confirm the vector numbers and names.

🟢 **16.2** Change the syscall gate to `IDT_INTERRUPT_GATE_K` (DPL 0) and, once Chapter 33 exists,
watch every system call turn into a general protection fault.

🟡 **16.3** Register a handler for vector 0 that prints a message and *skips the faulting
instruction* by advancing `regs->eip`. You will need to know the instruction's length. Then explain
why this is a terrible idea in general and what it teaches about `regs` being a pointer.

🟡 **16.4** Reorder two fields in `registers_t` without changing `isr.asm`. Predict what the register
dump will show, then run it.

🟡 **16.5** Deliberately cause a double fault by pointing `esp0` at an unmapped address and taking an
interrupt from ring 3 (needs Chapter 32). Use `-d int` to find the first fault in the cascade.

🔴 **16.6** Implement a stack backtrace: walk the `EBP` chain from `regs->ebp`, printing each return
address, and stop at zero. Then use the ELF symbol table to turn each address into a function name.
This is Chapter 47's tool and it is about sixty lines.

---

## What we covered

- Three kinds of event, one delivery mechanism, and the seven steps the CPU takes.
- Gate descriptors: the split offset, the selector that causes the privilege switch, and the one bit
  that distinguishes an interrupt gate from a trap gate.
- Why DPL 0 on every vector except the syscall gate, and the fabricated-page-fault attack it prevents.
- Why the handler entry must be assembly: `iret` and the unsaved general-purpose registers.
- Forty-nine stubs from two macros, the error-code inconsistency they paper over, and a stub table
  the assembler generates so it cannot drift.
- `registers_t` as a description of the stack rather than a convenience struct.
- `push esp` — passing the frame by pointer, and the three features that depend on it.
- The `isr_return` label, which Chapter 34 will jump into from a stack it built by hand.
- Double faults, triple faults, and the fact that the first fault is the one you want.
- Killing the process for a ring 3 fault and the machine for a ring 0 one — and not inventing
  evidence in the register dump.

[Chapter 17](17-pic-irqs.md) brings in the hardware: two 1981 interrupt controllers, a remap that is
not optional, and the end-of-interrupt signal whose absence silently kills the keyboard.

---

[← The GDT](15-gdt.md) · [Contents](README.md) · [Next: The PIC and IRQs →](17-pic-irqs.md)
