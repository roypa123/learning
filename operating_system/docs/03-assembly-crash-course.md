# Chapter 3 — x86 assembly for OS writers

[← The x86 machine](02-the-x86-machine.md) · [Contents](README.md) · [Next: How a PC boots →](04-how-a-pc-boots.md)

---

## Goal

Learn the roughly forty instructions this book actually uses, the NASM syntax we write them in, and
the calling convention that lets assembly and C call each other. That is enough to read every `.asm`
file in the repository and to write the four that have to be assembly.

This is not a general assembly tutorial. It is deliberately narrow: we cover what the kernel needs
and skip the rest, which is most of the instruction set.

---

## 1. Why any assembly at all?

Almost all of both operating systems is C. There are exactly four categories of thing that cannot be
written in C, and every line of assembly in this project is in one of them.

**1. Instructions with no C expression.** There is no way to write `lgdt`, `lidt`, `ltr`, `in`,
`out`, `invlpg`, `hlt`, `cli`, `sti`, or a write to `CR3` as a C statement. Most of these we wrap in
a one-line inline-assembly function — [`io.h`](../nimbus/include/nimbus/io.h) is nine of them — and
the rest live in [`cpu.asm`](../nimbus/boot/cpu.asm).

**2. Code that runs before C can.** C requires a stack, a zeroed `.bss`, and — after Chapter 25 — a
page table that maps the addresses it was linked for. Something has to establish those, and that
something cannot itself be C. This is [`boot.asm`](../nimbus/boot/boot.asm) and
[`entry.asm`](../spark/kernel/entry.asm).

**3. Code with a non-C control flow.** An interrupt handler must end with `iret`, not `ret`. A
context switch must change `ESP` in the middle of a function and return somewhere else. `enter_usermode`
must not return at all. GCC will not emit any of these. This is
[`isr.asm`](../nimbus/boot/isr.asm) and the rest of `cpu.asm`.

**4. Code in a mode the compiler does not target.** Our cross-compiler emits 32-bit protected mode
code. The boot sector runs in 16-bit real mode. There is no `-m16`worth using.

Note what is *not* on the list: performance. Not one line of assembly in this project exists because
it is faster than C would be.

---

## 2. NASM syntax

NASM uses Intel syntax, which matches the Intel manuals. GAS, the GNU assembler, uses AT&T syntax by
default, which does not. Since we will be checking things against Volume 3 constantly, Intel syntax
is the right choice.

### 2.1 Destination first

```nasm
    mov eax, ebx        ; eax = ebx
```

The destination is on the left. In AT&T syntax it is `movl %ebx, %eax` — the other way round, with
sigils and a size suffix. Every operand order in this book is destination-first.

### 2.2 Brackets mean dereference

```nasm
    mov eax, ebx        ; eax = ebx                    (register to register)
    mov eax, [ebx]      ; eax = *(uint32_t *)ebx       (load from memory)
    mov [eax], ebx      ; *(uint32_t *)eax = ebx       (store to memory)
    mov eax, label      ; eax = &label                 (the ADDRESS)
    mov eax, [label]    ; eax = *(uint32_t *)&label    (the CONTENTS)
```

The last two are the pair that catches everyone. `mov eax, label` loads the address; `mov eax,
[label]` loads what is stored there. In [`boot.asm`](../spark/boot/boot.asm):

```nasm
    mov si, msg_stage1      ; si = the address of the string
    call print              ; print walks it with lodsb
```

and

```nasm
    mov dl, [boot_drive]    ; dl = the byte stored at boot_drive
```

### 2.3 Sizes

When the assembler cannot infer the operand size, you say it:

```nasm
    mov byte  [eax], 5      ; store one byte
    mov word  [eax], 5      ; two bytes
    mov dword [eax], 5      ; four bytes
```

`mov eax, [ebx]` needs no annotation because `eax` is 32 bits. `mov [ebx], 5` does, because `5` could
be any width — and NASM will refuse to assemble it rather than guess.

### 2.4 Addressing modes

The general form is `[base + index*scale + displacement]`, with any part optional:

```nasm
    mov eax, [ebx]                  ; *ebx
    mov eax, [ebx + 4]              ; *(ebx + 4)
    mov eax, [ebx + esi]            ; *(ebx + esi)
    mov eax, [ebx + esi*4]          ; *(ebx + esi*4)  -- an int array index
    mov eax, [ebx + esi*4 + 8]      ; struct field of an array element
```

`scale` may only be 1, 2, 4 or 8. This single addressing mode is why array indexing compiles to one
instruction on x86.

### 2.5 Labels, locals and directives

```nasm
global _start               ; export this symbol to the linker
extern kmain                ; this symbol is defined elsewhere

section .text               ; which section the following code goes in

_start:                     ; a global label
.loop:                      ; a LOCAL label: belongs to _start
    dec ecx
    jnz .loop               ; can be reused in another function
```

A label starting with `.` is local to the preceding non-local label. That is why every routine in
this repository can have its own `.done` without colliding.

Data definitions:

```nasm
msg:    db "hello", 13, 10, 0   ; define bytes
count:  dw 0x1234               ; define word (2 bytes)
addr:   dd 0x12345678           ; define doubleword (4)
big:    dq 0                    ; define quadword (8)

buffer: resb 512                ; reserve 512 bytes, uninitialised (.bss)
stack:  resd 1024               ; reserve 1024 doublewords
```

`db`/`dw`/`dd` put bytes in the file. `resb`/`resd` reserve space without storing anything, which is
what `.bss` is — [`entry.asm`](../spark/kernel/entry.asm)'s 16 KiB stack costs zero bytes in the
binary.

And the two that only make sense in a boot sector:

```nasm
[BITS 16]                   ; assemble 16-bit instructions
[ORG 0x7C00]                ; labels are addresses relative to 0x7C00

times 510 - ($ - $$) db 0   ; pad with zeros to offset 510
dw 0xAA55
```

`$` is the current address; `$$` is the start of the section. `$ - $$` is "how many bytes so far".
If the boot sector is too big this line fails with *times value is negative*, which is the assembler
telling you the truth in an unhelpful way.

---

## 3. The instructions, by what they do

### 3.1 Moving data

```nasm
    mov  dst, src           ; dst = src
    movzx eax, bl           ; zero-extend: eax = (uint32_t)bl
    movsx eax, bl           ; sign-extend: eax = (int32_t)(int8_t)bl
    lea  eax, [ebx + 8]     ; eax = ebx + 8   -- computes the address, no load
    xchg eax, ebx           ; swap
```

`lea` is worth noticing: it runs the address calculation but does not dereference, so it is a free
three-operand add. `lea eax, [ebx + esi*4 + 8]` is one instruction.

`mov` cannot move memory to memory. `mov [eax], [ebx]` does not assemble. One operand must be a
register.

### 3.2 Arithmetic and logic

```nasm
    add  eax, ebx           ; eax += ebx          sets CF, ZF, SF, OF
    sub  eax, ebx           ; eax -= ebx
    inc  eax                ; eax++               does NOT set CF
    dec  eax                ; eax--
    neg  eax                ; eax = -eax
    and  eax, 0xFF          ; bitwise
    or   eax, 0x200
    xor  eax, eax           ; eax = 0  -- two bytes, and the idiomatic zero
    not  eax
    shl  eax, 4             ; eax <<= 4
    shr  eax, 12            ; eax >>= 12   (logical, zeros in)
    sar  eax, 2             ; arithmetic right shift (sign in)
    test eax, eax           ; AND, discard result, keep flags
    cmp  eax, ebx           ; SUB, discard result, keep flags
```

`xor eax, eax` rather than `mov eax, 0`: two bytes instead of five, and every CPU since the Pentium
Pro special-cases it as a zeroing idiom with no dependency on the old value.

`test eax, eax` followed by `jz` is "if (eax == 0)". `cmp` followed by a conditional jump is
everything else. Both exist because the conditional jumps read flags, and flags are set by
arithmetic.

Multiply and divide are awkward and worth reading carefully, because
[`boot.asm`](../spark/boot/boot.asm)'s CHS conversion depends on the details:

```nasm
    mul  ebx                ; EDX:EAX = EAX * EBX     (unsigned, 64-bit result)
    div  ebx                ; EAX = EDX:EAX / EBX,  EDX = remainder
```

Both use `EDX:EAX` as an implicit 64-bit pair. For `div` that means **`EDX` must be zeroed first**
unless you genuinely have a 64-bit dividend — and if the quotient does not fit in `EAX`, the CPU
raises a divide error. Forgetting `xor dx, dx` before a 16-bit `div` is the single most common boot
sector bug:

```nasm
    xor dx, dx              ; DX:AX = LBA, zero-extended
    mov cx, SECTORS_PER_TRACK
    div cx                  ; AX = LBA / 18, DX = LBA % 18
```

### 3.3 Control flow

```nasm
    jmp  label              ; unconditional
    jmp  eax                ; indirect, through a register
    jmp  0x08:label         ; FAR: also loads CS

    je / jz    label        ; jump if equal / zero          (ZF = 1)
    jne / jnz  label        ; jump if not equal             (ZF = 0)
    jc / jnc   label        ; jump if carry / not carry     (CF)
    jl / jge   label        ; signed less / greater-or-equal
    jb / jae   label        ; unsigned below / above-or-equal
    jg / jle   label        ; signed greater / less-or-equal

    call label              ; push the return address, then jump
    ret                     ; pop into EIP
    ret 8                   ; pop into EIP, then add 8 to ESP

    loop label              ; ECX--, jump if ECX != 0
```

The signed/unsigned distinction matters. `jl` and `jb` test different flags, and using the wrong one
makes `0x80000000` compare as negative when you meant it as two billion.

`loop` is one byte and is slower than `dec ecx; jnz` on every CPU since about 1995. We use it in the
boot sector, where size matters more than speed, and nowhere else.

### 3.4 The stack

```nasm
    push eax                ; ESP -= 4; [ESP] = eax
    pop  eax                ; eax = [ESP]; ESP += 4
    pusha                   ; push EAX ECX EDX EBX ESP EBP ESI EDI
    popa                    ; pop them back (discarding the saved ESP)
    pushfd / popfd          ; push / pop EFLAGS
```

`pusha` pushes in a fixed order, which means the registers appear on the stack in the *reverse* order
— `EDI` at the lowest address. That is exactly why
[`isr.h`](../nimbus/include/nimbus/isr.h)'s `registers_t` lists them that way:

```c
typedef struct registers {
    uint32_t ds;
    uint32_t edi, esi, ebp, esp_dummy, ebx, edx, ecx, eax;
    ...
```

`popa` deliberately discards the slot where `ESP` was saved — restoring it would undo the pops in
progress. The struct names it `esp_dummy` so that nobody trusts it.

### 3.5 String instructions

```nasm
    lodsb                   ; AL = [DS:ESI]; ESI++   (or -- if DF set)
    stosb                   ; [ES:EDI] = AL; EDI++
    movsb                   ; [ES:EDI] = [DS:ESI]; both advance
    movsd                   ; the same, four bytes at a time

    rep movsd               ; while (ECX--) movsd
    rep stosb               ; while (ECX--) stosb
    cld                     ; DF = 0, count upwards
    std                     ; DF = 1, count downwards
```

`rep movsd` is the fastest memory copy on x86 and is what
[`stage2.asm`](../spark/boot/stage2.asm) uses to move the kernel to 1 MiB:

```nasm
    mov esi, KERNEL_SEG * 16
    mov edi, KERNEL_PHYS
    mov ecx, KERNEL_SECTORS * 512 / 4
    cld
    rep movsd
```

`rep stosb` is `memset`, and [`boot.asm`](../nimbus/boot/boot.asm) uses it to zero `.bss` — which it
can do before setting up a stack, because `rep stosb` touches only `EDI`, `ECX` and `EAX`.

**Always `cld` first.** The direction flag is a global that a called routine might have left set, and
a `rep movsd` in the wrong direction copies from the wrong end. The System V ABI requires `DF` to be
clear at function boundaries, but the BIOS is not bound by the System V ABI.

### 3.6 Privileged and system instructions

```nasm
    cli / sti               ; clear / set EFLAGS.IF
    hlt                     ; stop until the next interrupt
    in  al, dx              ; read a byte from I/O port DX
    out dx, al              ; write a byte to I/O port DX
    lgdt [ptr]              ; load the GDT register
    lidt [ptr]              ; load the IDT register
    ltr  ax                 ; load the task register (the TSS selector)
    invlpg [eax]            ; invalidate one TLB entry
    iret                    ; return from an interrupt
    int  0x80               ; raise a software interrupt
    mov  eax, cr0           ; read a control register
    mov  cr0, eax           ; write one
```

All of these except `int` fault if executed in ring 3. That is the privilege boundary in practice: a
user program that executes `cli` gets a general protection fault, and
[`isr.c`](../nimbus/kernel/isr.c)'s handler kills it.

`hlt` deserves a note. It stops the CPU until an interrupt arrives, drawing almost no power. `hlt`
with `IF` clear is a permanent stop, which is what `panic()` wants and what an idle loop very much
does not — hence the `sti` before the `hlt` in [`main.c`](../nimbus/kernel/main.c)'s idle loop.

---

## 4. `iret`: the densest instruction on the machine

Worth its own section, because two different behaviours hide in one opcode.

When an interrupt occurs, the CPU pushes:

```
    EFLAGS
    CS
    EIP           <- ESP points here
```

and if the interrupt crossed from ring 3 to ring 0, it *first* switches to the kernel stack named in
the TSS and pushes two more:

```
    SS
    ESP           <- the user stack pointer
    EFLAGS
    CS
    EIP           <- ESP points here
```

`iret` pops `EIP`, `CS` and `EFLAGS`. Then it examines the privilege level in the `CS` it just
popped: if it is *less* privileged than the current level, it also pops `ESP` and `SS` and switches
stacks.

Two consequences that the whole of Part IV rests on:

**You can fake it.** Build that five-value frame by hand, execute `iret`, and the CPU enters ring 3.
That is exactly what [`cpu.asm`](../nimbus/boot/cpu.asm)'s `enter_usermode` does, and it is the only
way down from ring 0.

**The frame shape depends on where the interrupt came from.** A fault in kernel mode pushes three
values; a fault in user mode pushes five. `registers_t` describes the five-value case, and
[`printk.c`](../nimbus/kernel/printk.c)'s register dump checks `(regs->cs & 3) == 3` before printing
the last two, because otherwise it would be inventing evidence.

---

## 5. The calling convention

Assembly and C call each other constantly, so the rules matter. We use System V cdecl, which is what
our cross-compiler emits.

### 5.1 The rules

1. **Arguments are pushed right to left**, so the first argument ends up at the lowest address.
2. **The caller removes them** after the call.
3. **The return value is in `EAX`** (or `EDX:EAX` for 64 bits).
4. **`EAX`, `ECX`, `EDX` are caller-saved**: a function may destroy them.
5. **`EBX`, `ESI`, `EDI`, `EBP` are callee-saved**: a function must preserve them.
6. `DF` must be clear on entry and exit.

So for `switch_context(&prev->context, next->context)`:

```nasm
switch_context:
    mov eax, [esp + 4]      ; first argument
    mov edx, [esp + 8]      ; second argument
```

`[esp]` is the return address, `[esp+4]` is argument 1, `[esp+8]` is argument 2.

### 5.2 Why the context switch is only five registers

Rule 5 is what makes [`cpu.asm`](../nimbus/boot/cpu.asm)'s `switch_context` fourteen instructions
instead of forty:

```nasm
    push ebp
    push ebx
    push esi
    push edi
    mov [eax], esp          ; *old = esp
    mov esp, edx            ; esp = new
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret
```

Whoever called `switch_context` is a C function, and rule 4 says it has already spilled anything it
cared about in `EAX`, `ECX` and `EDX`. We only have to preserve the callee-saved four — plus `ESP`,
which is the switch itself, and `EIP`, which is already on the stack because we were called.

Chapter 30 goes through it instruction by instruction. The short version: **a saved context is just a
saved `ESP`.**

### 5.3 Stack frames

The standard prologue and epilogue:

```nasm
    push ebp
    mov  ebp, esp           ; EBP now points at the saved EBP
    sub  esp, 16            ; room for locals
    ...
    mov  esp, ebp
    pop  ebp
    ret
```

With `EBP` set up this way, `[ebp+8]` is the first argument and `[ebp-4]` is the first local,
regardless of how much has been pushed since. Each saved `EBP` points at the previous one, so a
debugger can walk the chain — which is why `-fno-omit-frame-pointer` is in our `CFLAGS` and why
[`entry.asm`](../spark/kernel/entry.asm) zeroes `EBP` before calling `kmain`, to terminate the chain.

### 5.4 Inline assembly

For one or two instructions, GCC's extended inline assembly is better than a separate file. The form:

```c
__asm__ volatile ("instruction" : outputs : inputs : clobbers);
```

From [`io.h`](../nimbus/include/nimbus/io.h):

```c
static ALWAYS_INLINE void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile ("outb %0, %1" :: "a"(value), "Nd"(port));
}

static ALWAYS_INLINE uint8_t inb(uint16_t port)
{
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}
```

The constraint letters:

| Constraint | Meaning |
|---|---|
| `"a"` | put it in `EAX`/`AX`/`AL` |
| `"=a"` | an output, delivered in `EAX` |
| `"+r"` | any register, read *and* written |
| `"Nd"` | in `DX`, or an immediate constant 0–255 |
| `"m"` | a memory operand |
| `"memory"` | (clobber) this may read or write any memory |
| `"cc"` | (clobber) this modifies the flags |

Two things you must get right or the bug will be invisible:

**`volatile`.** Without it, GCC treats the block as a pure function of its inputs and may delete it,
hoist it out of a loop, or reuse a previous result. Reading port `0x60` *consumes* a keyboard byte;
every one of those transformations is wrong.

**The `"memory"` clobber.** It tells GCC that memory may have changed, which stops it caching a value
across the instruction. `insw` writes to a buffer through `EDI` and GCC has no way to know, so
without `"memory"` the code that reads that buffer afterwards may use stale values.

---

## 6. Macros

NASM's preprocessor is doing real work in [`isr.asm`](../nimbus/boot/isr.asm) — without it the file
would be 49 nearly identical stubs written by hand.

```nasm
%macro ISR_NOERR 1
global isr%1
isr%1:
    push dword 0
    push dword %1
    jmp  isr_common_stub
%endmacro

ISR_NOERR 0
ISR_NOERR 1
```

`%1` is the first argument, and `isr%1` concatenates, producing labels `isr0`, `isr1` and so on.

`%rep` repeats, and `%assign` gives a compile-time variable:

```nasm
isr_stub_table:
%assign vector 0
%rep 48
    dd isr %+ vector
%assign vector vector + 1
%endrep
```

`%+` is explicit concatenation, needed here because `isr` and `vector` are separate tokens. This
emits 48 addresses and — the actual point — guarantees the table cannot drift out of step with the
stubs, since both come from the same numbers.

[`boot.asm`](../nimbus/boot/boot.asm) uses the same construct to build a page directory at assembly
time:

```nasm
    %assign i 0
    %rep BOOT_PAGES
        dd (i << 22) | 0x83
    %assign i i+1
    %endrep
```

Four page directory entries, each mapping a 4 MiB page, computed by the assembler. No runtime code at
all.

---

## 7. Reading a disassembly

You will spend real time in `objdump`. `make disasm` runs:

```bash
i686-elf-objdump -d -M intel bin/nimbus.elf
```

`-M intel` is important — without it you get AT&T syntax and everything is backwards from the manual.

```
c0100034 <switch_context>:
c0100034:  8b 44 24 04    mov    eax,DWORD PTR [esp+0x4]
c0100038:  8b 54 24 08    mov    edx,DWORD PTR [esp+0x8]
c010003c:  55             push   ebp
c010003d:  53             push   ebx
```

Left to right: the address, the actual bytes, and the instruction. The bytes matter more than you
would expect — when you are staring at a hex dump of memory trying to work out whether the CPU
jumped somewhere sensible, recognising `55` as `push ebp` and `c3` as `ret` is a real skill.

A few worth memorising:

| Byte | Instruction |
|---|---|
| `90` | `nop` |
| `c3` | `ret` |
| `cf` | `iret` |
| `cd 80` | `int 0x80` |
| `55` | `push ebp` |
| `eb fe` | `jmp $` — an infinite loop, and the classic way to stop and look |
| `00 00` | `add [eax], al` — what a run of zeros disassembles to, i.e. you are lost |

That last one is the useful one. If a disassembly is full of `add [eax],al`, you are looking at
zeroed memory and the CPU went somewhere it should not have.

---

## 8. Exercises

🟢 **3.1** What is the difference between `mov eax, msg` and `mov eax, [msg]`? Write the C equivalent
of each.

🟢 **3.2** Why does `xor eax, eax` appear so much more often than `mov eax, 0`? Give two reasons.

🟢 **3.3** In `disk_read`, why must `xor dx, dx` come before `div cx`? What happens if you leave it
out and `DX` happens to hold 3?

🟡 **3.4** Write an assembly routine `strlen(const char *s)` following cdecl: argument on the stack,
result in `EAX`, `EBX`/`ESI`/`EDI`/`EBP` preserved. Use `lodsb`.

🟡 **3.5** Read [`cpu.asm`](../nimbus/boot/cpu.asm)'s `switch_context` and write down, for each of the
five values `push`ed, which field of `struct context` it lands in. Then check against
[`task.h`](../nimbus/include/nimbus/task.h).

🟡 **3.6** Disassemble `bin/spark/kernel.elf` and find `kmain`. Identify the prologue, the first
call, and the `hlt` at the end.

🔴 **3.7** Write the `ISR_ERR` macro from scratch without looking at
[`isr.asm`](../nimbus/boot/isr.asm), then compare. Explain why the two macros differ by exactly one
`push`, and what would go wrong if they did not.

---

## What we covered

- The four categories of code that cannot be C, and the fact that performance is not one of them.
- NASM syntax: destination first, brackets dereference, `[base + index*scale + disp]`, local labels,
  `times`/`$`/`$$`.
- The instruction set we use: moves, arithmetic (including the `EDX:EAX` trap in `div`), control
  flow, the stack, string instructions, and the privileged set.
- `iret`, and the two different stack frames it can consume.
- cdecl: arguments right to left, `EAX` returns, and the caller-saved/callee-saved split that makes a
  context switch five registers instead of sixteen.
- Inline assembly constraints, and the two ways to get them wrong invisibly.
- NASM macros, and how `isr.asm` generates 49 stubs and a table that cannot disagree with them.

[Chapter 4](04-how-a-pc-boots.md) traces what happens between pressing the power button and the first
instruction of our code: the reset vector, what the BIOS does, where UEFI differs, and why the number
`0xAA55` matters.

---

[← The x86 machine](02-the-x86-machine.md) · [Contents](README.md) · [Next: How a PC boots →](04-how-a-pc-boots.md)
