# Appendix A — The x86 instruction subset we use

[Contents](README.md)

---

Every instruction that appears in this project, what it does, and where it is used. Intel syntax
throughout: **destination first**.

---

## Data movement

| Instruction | Effect | Notes |
|---|---|---|
| `mov dst, src` | `dst = src` | Cannot move memory to memory. Cannot load `CS`. |
| `movzx dst, src` | zero-extend | `movzx eax, bl` → `eax = (uint32_t)bl` |
| `movsx dst, src` | sign-extend | `movsx eax, bl` → `eax = (int32_t)(int8_t)bl` |
| `lea dst, [expr]` | `dst = &expr` | Computes the address, does not dereference. A free 3-operand add. |
| `xchg a, b` | swap | Implicitly `lock`ed when one operand is memory |
| `push src` | `esp -= 4; [esp] = src` | |
| `pop dst` | `dst = [esp]; esp += 4` | |
| `pusha` / `popa` | all eight GPRs | Order: `EAX ECX EDX EBX ESP EBP ESI EDI`. `popa` discards the saved `ESP`. |
| `pushf` / `popf` | `EFLAGS` | The only way to read or write it |

**Where:** `mov` everywhere. `lea` in `boot.asm`'s higher-half jump (Ch. 25, §4.3). `pusha`/`popa`
in every interrupt stub (Ch. 16, §6.1) and in Spark's `print`/`disk_read`.

---

## Arithmetic and logic

| Instruction | Effect | Flags set |
|---|---|---|
| `add dst, src` | `dst += src` | CF ZF SF OF AF PF |
| `sub dst, src` | `dst -= src` | all |
| `inc dst` / `dec dst` | ±1 | all **except CF** |
| `neg dst` | `dst = -dst` | all |
| `mul src` | `EDX:EAX = EAX * src` | unsigned |
| `imul` | signed multiply | has 1-, 2- and 3-operand forms |
| `div src` | `EAX = EDX:EAX / src`, `EDX = remainder` | **zero `EDX` first** |
| `idiv` | signed divide | |
| `and` / `or` / `xor` / `not` | bitwise | CF and OF cleared |
| `shl` / `shr` | logical shift | CF = last bit out |
| `sar` | arithmetic right shift | sign extends |
| `test a, b` | `a & b`, discard result | ZF SF PF |
| `cmp a, b` | `a - b`, discard result | all |

**The `div` trap:** `div cx` divides `DX:AX`, not `AX`. `xor dx, dx` first or the quotient overflows
and the CPU raises `#DE` (Ch. 6, §3.1).

**`xor eax, eax`** is the idiomatic zero: two bytes, and every CPU since the Pentium Pro
special-cases it with no dependency on the old value.

---

## Control flow

| Instruction | Condition |
|---|---|
| `jmp label` | unconditional (relative) |
| `jmp reg` | unconditional (absolute, indirect) |
| `jmp seg:off` | far — also loads `CS` |
| `je` / `jz` | ZF = 1 |
| `jne` / `jnz` | ZF = 0 |
| `jc` / `jnc` | CF |
| `js` / `jns` | SF |
| `jl` / `jge` | **signed** less / greater-or-equal |
| `jb` / `jae` | **unsigned** below / above-or-equal |
| `jg` / `jle` | signed greater / less-or-equal |
| `ja` / `jbe` | unsigned above / below-or-equal |
| `call label` | push return address, jump |
| `ret` | pop into `EIP` |
| `ret n` | pop into `EIP`, then `esp += n` |
| `loop label` | `ecx--`; jump if `ecx != 0` |

**Signed versus unsigned matters.** `jl` and `jb` test different flags; using the wrong one makes
`0x80000000` compare as negative when you meant two billion.

**`loop`** is one byte and slower than `dec ecx; jnz` on every CPU since ~1995. Used in the boot
sector, where size beats speed.

---

## String instructions

| Instruction | Effect |
|---|---|
| `lodsb` / `lodsw` / `lodsd` | `AL/AX/EAX = [DS:ESI]`, advance `ESI` |
| `stosb` / `stosw` / `stosd` | `[ES:EDI] = AL/AX/EAX`, advance `EDI` |
| `movsb` / `movsw` / `movsd` | `[ES:EDI] = [DS:ESI]`, advance both |
| `scasb` | compare `AL` with `[ES:EDI]`, advance |
| `cmpsb` | compare `[DS:ESI]` with `[ES:EDI]`, advance both |
| `rep prefix` | repeat `ECX` times |
| `repe` / `repne` | repeat while equal / not equal |
| `insw` / `outsw` | port to memory / memory to port |
| `cld` / `std` | `DF = 0` (up) / `DF = 1` (down) |

**Always `cld` first.** `DF` is a global that a called routine might have left set, and the BIOS is
not bound by the System V ABI.

**Where:** `rep movsd` copies Spark's kernel to 1 MiB (Ch. 8, §4). `rep stosb` zeroes `.bss`
(Ch. 9, §2.2 and Ch. 25, §4). `rep insw` moves 256 words per ATA sector (Ch. 37, §6.2). `lodsb`
drives the boot sector's `print`.

---

## Privileged and system

| Instruction | Effect | Faults in ring 3 |
|---|---|---|
| `cli` / `sti` | clear / set `EFLAGS.IF` | yes |
| `hlt` | stop until an interrupt | yes |
| `in` / `out` | I/O port access | yes (unless IOPL or the TSS bitmap permits) |
| `lgdt [m]` | load the GDT register | yes |
| `lidt [m]` | load the IDT register | yes |
| `ltr reg` | load the task register | yes |
| `invlpg [m]` | invalidate one TLB entry | yes |
| `mov reg, cr0` | read a control register | yes |
| `mov cr0, reg` | write one | yes |
| `wrmsr` / `rdmsr` | model-specific registers | yes |
| `iret` | return from an interrupt | no (but only pops what it was given) |
| `int n` | software interrupt | only if the gate's DPL ≥ 3 |
| `rdtsc` | read the timestamp counter | no, unless `CR4.TSD` |
| `cpuid` | feature identification | no |
| `pause` | spin-loop hint | no |

**`hlt` with `IF` clear is a permanent stop.** Hence `sti; hlt` in every idle loop.

**`iret`** pops `EIP`, `CS`, `EFLAGS` — and `ESP`, `SS` too if the popped `CS` is less privileged.
One opcode, two behaviours (Ch. 3, §4).

---

## Instruction encodings worth recognising

When you are staring at a hex dump of memory:

| Bytes | Instruction |
|---|---|
| `90` | `nop` |
| `c3` | `ret` |
| `cb` | `retf` |
| `cf` | `iret` |
| `cc` | `int3` |
| `cd 80` | `int 0x80` |
| `55` | `push ebp` |
| `5d` | `pop ebp` |
| `89 e5` | `mov ebp, esp` |
| `fa` | `cli` |
| `fb` | `sti` |
| `f4` | `hlt` |
| `eb fe` | `jmp $` — a deliberate stop |
| `e8 xx xx xx xx` | `call rel32` |
| `ea xx xx ss ss` | `jmp far` |
| `31 c0` | `xor eax, eax` |
| `00 00` | `add [eax], al` — **zeroed memory; you are lost** |

That last row is the useful one. A disassembly full of `add [eax],al` means the CPU went somewhere it
should not have.

---

## NASM directives

```nasm
[BITS 16] / [BITS 32]       assemble 16- or 32-bit encodings
[ORG addr]                  assume this load address for label arithmetic

section .text               which section follows
global sym                  export to the linker
extern sym                  defined elsewhere

db / dw / dd / dq           define 1 / 2 / 4 / 8 bytes
resb / resw / resd          reserve uninitialised space (.bss)
times n expr                repeat n times
align n                     pad to an n-byte boundary
equ                         assembly-time constant

$                           the current address
$$                          the start of the current section
%macro / %endmacro          a macro
%rep / %endrep              repeat
%assign                     an assembly-time variable
%+                          token concatenation
```

`times 510 - ($ - $$) db 0` — pad to offset 510. A negative count means the boot sector is too big.

---

## Inline assembly constraints

```c
__asm__ volatile ("instruction" : outputs : inputs : clobbers);
```

| Constraint | Meaning |
|---|---|
| `"a"` `"b"` `"c"` `"d"` | `EAX` `EBX` `ECX` `EDX` |
| `"S"` `"D"` | `ESI` `EDI` |
| `"r"` | any general-purpose register |
| `"q"` | a register with an addressable low byte (`a`, `b`, `c`, `d`) |
| `"m"` | a memory operand |
| `"i"` | an immediate constant |
| `"Nd"` | in `DX`, or an immediate 0–255 |
| `"=x"` | output |
| `"+x"` | read and written |
| `"memory"` | (clobber) may read or write any memory |
| `"cc"` | (clobber) modifies the flags |

**`volatile`** stops GCC deleting, hoisting or reusing the block. Mandatory for anything with a side
effect the compiler cannot see — which is every port access.

**`"memory"`** stops GCC caching a value across the instruction. Mandatory for `rep insw`, for
`int 0x80`, and for `cli`/`sti`.

---

## The calling convention (System V cdecl, 32-bit)

1. Arguments pushed **right to left**, so the first is at the lowest address.
2. The **caller** removes them.
3. Return value in `EAX` (or `EDX:EAX` for 64 bits).
4. **Caller-saved:** `EAX`, `ECX`, `EDX` — a function may destroy them.
5. **Callee-saved:** `EBX`, `ESI`, `EDI`, `EBP` — a function must preserve them.
6. `DF` must be clear on entry and exit.

```nasm
    ; int f(int a, int b) called as f(1, 2)
    push 2
    push 1
    call f
    add esp, 8

f:
    push ebp
    mov  ebp, esp
    ; [ebp+8]  = a
    ; [ebp+12] = b
    mov  esp, ebp
    pop  ebp
    ret
```

Rule 4 is what makes the context switch five registers instead of sixteen (Ch. 30, §2).

---

[Contents](README.md)
