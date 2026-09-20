# Line by line: `spark/kernel/entry.asm` and `kernel.c`

[Index](README.md) · [Chapter 9](../09-freestanding-c.md) · [Chapter 10](../10-vga-hello.md)

---

# `entry.asm`

```nasm
[BITS 32]
global _start
extern kmain
extern __bss_start
extern __bss_end
```
`__bss_start` and `__bss_end` are defined by `link.ld`, not by any source file.

```nasm
section .text.entry
```
⚠️ A section name that exists for one purpose: so `link.ld` can place it first. Stage 2 does
`jmp 0x100000` with no ELF parsing, so the first byte of the image must be `_start`. A normal
`.text` gives no ordering guarantee, and the failure is a triple fault with no obvious cause.

```nasm
_start:
    cli
```
Stage 2 already cleared `IF`. Repeated because this file must be correct alone.

```nasm
    mov edi, __bss_start
    mov ecx, __bss_end
    sub ecx, edi
    xor eax, eax
    cld
    rep stosb
```
`.bss` is "block started by symbol": globals that are zero at startup. The linker reserves address
space and stores no bytes, which is why a kernel with a 4 MiB buffer is not a 4 MiB binary.

Nobody zeroes it for us — on a hosted system the ELF loader does, and we are the loader.

🔧 QEMU's RAM starts zeroed, so a kernel that skips this works in the emulator and fails on metal.

`rep stosb` needs no stack, which is fortunate: the stack is inside the region being cleared.

```nasm
    mov esp, stack_top
```
⚠️ `stack_top`, the label *after* the reservation, because stacks grow down. `stack_bottom` works for
a surprising number of calls before destroying whatever is below it.

```nasm
    xor ebp, ebp
```
Terminates the frame-pointer chain, so a stack walker stops here rather than following garbage.

```nasm
    call kmain
.halt:
    cli
    hlt
    jmp .halt
```
`kmain` must not return. If it does, stop rather than executing whatever bytes follow.

```nasm
section .bss
align 16
stack_bottom:
    resb 16384
stack_top:
```
`resb` reserves without storing, so 16 KiB costs nothing in the binary.

`align 16` because the System V ABI wants 16-byte stack alignment at call boundaries; GCC may emit
SSE for struct copies, and `movaps` faults on a misaligned address.

No guard page. 16 KiB is enough for a kernel with no recursion.

---

# `kernel.c`

## Types

```c
typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;
```
`<stdint.h>` is freestanding and would work. Writing them out once is worth doing because each is a
promise about the machine.

⚠️ On 32-bit x86, `long` is **32 bits**, not 64. Code from a 64-bit host that uses `long` for a
pointer-sized integer breaks here.

## The framebuffer

```c
#define VGA_MEMORY  ((volatile uint16_t *)0xB8000)
```
Three decisions.

`uint16_t *` so one store writes a complete cell — and cannot be interrupted between the character
and the colour.

⚠️ `volatile` — without it the compiler may delete stores nothing reads, hoist them out of loops, or
merge 2000 identical ones. All legal for ordinary memory, all wrong here.

The bare address works because Spark has no paging. Nimbus needs `P2V(VGA_PHYS)`.

## `vga_cell`

```c
static inline uint16_t vga_cell(char c, uint8_t attr)
{
    return (uint16_t)(uint8_t)c | ((uint16_t)attr << 8);
}
```
⚠️ **The double cast.** `char` is signed on x86 GCC, so `0xDB` is −37, and casting straight to
`uint16_t` sign-extends to `0xFFDB` — the colour nibbles become `0xFF` and you get a blinking white
block.

`(uint8_t)` truncates, `(uint16_t)` zero-extends. Invisible for every character below 128.

## `vga_clear`

```c
    for (int i = 0; i < VGA_COLS * VGA_ROWS; i++)
        VGA_MEMORY[i] = vga_cell(' ', color);
```
2000 stores. There is no clear-screen command in text mode; the BIOS's `int 0x10, AH=0x06` does the
same loop.

Clears with the *current* colour, not black — which is what a blue panic screen wants.

## `vga_scroll`

```c
    for (int row = 1; row < VGA_ROWS; row++)
        for (int col = 0; col < VGA_COLS; col++)
            VGA_MEMORY[(row - 1) * VGA_COLS + col] = VGA_MEMORY[row * VGA_COLS + col];
```
3840 bytes per newline at the bottom. Genuinely how DOS did it, and invisible even on a 386.

A `memmove` would be clearer, and `memmove` does not exist yet — Spark has no `lib/string.c`.

## `vga_putc`

```c
    } else if (c == '\t') {
        cursor_col = (cursor_col + 8) & ~7;
```
Round up to a multiple of 8, branchlessly. Same trick as `ALIGN_UP`.

```c
    } else if (c == '\b') {
        if (cursor_col > 0) cursor_col--;
```
Backspace **moves**; it does not erase. To delete visibly you send `\b`, space, `\b` — which is what
Nimbus's console does.

```c
    if (cursor_col >= VGA_COLS) { cursor_col = 0; cursor_row++; }
    if (cursor_row >= VGA_ROWS) vga_scroll();
```
Wrap, then scroll. Both checked after every character.

## `vga_puthex`

```c
    for (int shift = 28; shift >= 0; shift -= 4)
        vga_putc(digits[(value >> shift) & 0xF]);
```
Top down, four bits at a time. No division, no buffer, no reversal.

Fixed width — always eight digits — deliberately: a ragged column of addresses is one you cannot
scan.

## `vga_putdec`

```c
    char buf[11];
```
⚠️ 4294967295 is ten digits plus a NUL. Sizing this at 10 overflows a stack array for exactly one
input.

```c
    if (value == 0) { vga_putc('0'); return; }
```
⚠️ The loop is `while (value > 0)`, which produces nothing for zero. Every hand-written
integer-to-string has this bug once.

```c
    while (value > 0) {
        buf[i++] = (char)('0' + (value % 10));
        value /= 10;
    }
    while (i > 0)
        vga_putc(buf[--i]);
```
Division produces digits backwards, so fill forwards and print in reverse.

## `banner`

```c
    for (int col = 0; col < VGA_COLS; col++)
        VGA_MEMORY[col] = vga_cell(' ', color);
```
Writes the buffer directly rather than through `vga_putc`, because it needs absolute positions with
no cursor movement and no scroll.

That is exactly what hardware scrolling would break — Nimbus keeps `vga_put_at` for the same reason.

## `kmain`

```c
    vga_puthex((uint32_t)(void *)kmain);
```
A function pointer through `void *` to an integer. The intermediate cast silences a warning; the
result should be close to `0x100000`, which confirms the linker script.

```c
    int here;
    vga_puthex((uint32_t)&here);
```
The address of a local tells you roughly where `ESP` is — the only way to see the stack pointer from
C without inline assembly. It should be just below `stack_top`.

Two numbers on screen confirming two different parts of the build.

```c
    for (;;)
        __asm__ volatile ("hlt");
```
Nothing left to do. No scheduler, no idle task, and no interrupt will ever fire — `entry.asm` cleared
`IF` and there is no IDT.

`hlt` rather than a spin loop: on a laptop that is the difference between silence and a fan.

---

[Index](README.md) · [Chapter 9](../09-freestanding-c.md) · [Chapter 10](../10-vga-hello.md)
