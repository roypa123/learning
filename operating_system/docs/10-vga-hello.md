# Chapter 10 — Hello, VGA: Spark is finished

[← Freestanding C](09-freestanding-c.md) · [Contents](README.md) · [Next: Multiboot →](11-multiboot.md)

> 📖 **Line by line:** [kernel.c](line-by-line/spark-kernel.md)

---

## Goal

Finish Spark. Write to the screen by storing bytes at a fixed address, build the handful of routines
that make that usable — `putc`, `puts`, scrolling, hex and decimal — and look at what we have built.

This is the last chapter of Part I. At the end of it you will have a complete, bootable operating
system that you wrote every byte of, and the next chapter starts a different one.

---

## 1. The VGA text buffer

On every PC, a 4000-byte region of physical memory at `0xB8000` is wired directly to the display
hardware. Store a word there and a character appears. No driver, no initialisation, no handshake —
the video card's option ROM already put the adapter in mode 3 (80×25 colour text) before the BIOS
handed us control (Chapter 4, §2).

```
    0xB8000 + (row * 80 + col) * 2
```

80 columns, 25 rows, two bytes per cell. 80 × 25 × 2 = 4000 bytes.

### 1.1 The cell format

```
    bit  15 14 13 12  11 10  9  8   7  6  5  4  3  2  1  0
        +---+--------+-----------+  +------------------------+
        |blk|   bg   |    fg     |  |      code point        |
        +---+--------+-----------+  +------------------------+
```

The low byte is the character. The high byte is the attribute: four bits of foreground colour, three
of background, and one blink bit.

Three bits of background means eight colours, not sixteen. Bit 15 was originally the fourth
background bit on the MDA, and IBM reassigned it to "blink" on the CGA. You can get it back by
clearing bit 0 of the Attribute Mode Control register, which is the kind of fact that is useless
until the one day you want a bright background.

The sixteen colours:

```c
enum vga_color {
    VGA_BLACK = 0,   VGA_BLUE,          VGA_GREEN,       VGA_CYAN,
    VGA_RED,         VGA_MAGENTA,       VGA_BROWN,       VGA_LIGHT_GREY,
    VGA_DARK_GREY,   VGA_LIGHT_BLUE,    VGA_LIGHT_GREEN, VGA_LIGHT_CYAN,
    VGA_LIGHT_RED,   VGA_LIGHT_MAGENTA, VGA_YELLOW,      VGA_WHITE
};
```

The order is not arbitrary: bits 0–2 are blue, green, red, and bit 3 is intensity. So
`VGA_LIGHT_BLUE` is `VGA_BLUE | 8`. "Brown" is dark yellow, which on a CRT looked brown, and the name
stuck.

### 1.2 It is not ASCII

The character byte is **code page 437**, the IBM PC character set. For the 95 printable ASCII
characters the two agree, which is why you can pretend it is ASCII until the day you print a `£`.

Outside that range it is a different world: `0x01` is a smiley face, `0xB0`–`0xB2` are shaded blocks,
`0xC0`–`0xDA` are box-drawing characters, and `0xDB` is a solid block. Every DOS program that drew a
window used those box characters, and they are still there.

---

## 2. The buffer pointer

```c
#define VGA_MEMORY  ((volatile uint16_t *)0xB8000)
```

Three decisions in one line.

**`uint16_t *`**, so that one store writes a complete cell. Writing the character and the attribute
separately as bytes works and takes two bus cycles instead of one; more importantly, a single 16-bit
store cannot be interrupted between the two halves, so the screen never shows a character with the
wrong colour.

**`volatile`.** This is the one that matters. Without it, the compiler is entitled to observe that
nothing ever *reads* these locations and delete the stores entirely. Or to hoist a store out of a
loop. Or to notice that `vga_clear` writes the same value 2000 times and merge them.

All of those are valid optimisations for ordinary memory and all of them are wrong here, because the
side effect — light arriving at your eye — is not visible to the compiler. `volatile` means "this
location may change or matter in ways you cannot see; do exactly what I wrote, in the order I wrote
it".

**The bare address.** Spark has no paging, so `0xB8000` is both the physical and the virtual address.
Nimbus enables paging before any C runs, so [`vga.c`](../nimbus/drivers/vga.c) has to write
`P2V(VGA_PHYS)` instead — Chapter 25 covers that, and writing the constant there is a page fault.

---

## 3. Building a cell

```c
static inline uint16_t vga_cell(char c, uint8_t attr)
{
    return (uint16_t)(uint8_t)c | ((uint16_t)attr << 8);
}
```

The double cast is the point, and Chapter 9, §6.3 flagged it: `char` is signed on x86 GCC, so a byte
like `0xDB` is the negative number −37. Casting `char` straight to `uint16_t` sign-extends, giving
`0xFFDB` — the colour nibbles become `0xFF`, and instead of a solid block you get a blinking white
block on a white background.

`(uint8_t)` first truncates to an unsigned byte; `(uint16_t)` then zero-extends. `0x00DB`, correct.

This bug is invisible for every character below 128, which is to say for all of your testing until
the day you draw a box.

---

## 4. Clearing and writing

```c
void vga_clear(void)
{
    for (int i = 0; i < VGA_COLS * VGA_ROWS; i++)
        VGA_MEMORY[i] = vga_cell(' ', color);
    cursor_row = 0;
    cursor_col = 0;
}
```

2000 stores. There is no "clear screen" command in text mode — the BIOS's `int 0x10, AH=0x06` does
exactly this loop.

Note that it clears with the *current* colour, not with black. That means `vga_set_color(WHITE, BLUE)`
followed by `vga_clear()` gives a blue screen, which is what the panic handler in Nimbus wants.

```c
void vga_putc(char c)
{
    if (c == '\n') {
        cursor_col = 0;
        cursor_row++;
    } else if (c == '\r') {
        cursor_col = 0;
    } else if (c == '\t') {
        cursor_col = (cursor_col + 8) & ~7;
    } else if (c == '\b') {
        if (cursor_col > 0) cursor_col--;
    } else {
        VGA_MEMORY[cursor_row * VGA_COLS + cursor_col] = vga_cell(c, color);
        cursor_col++;
    }

    if (cursor_col >= VGA_COLS) { cursor_col = 0; cursor_row++; }
    if (cursor_row >= VGA_ROWS) vga_scroll();
}
```

Four control characters, handled by hand, because nothing else will.

**`\n` moves down and to column 0.** On a real terminal `\n` is *only* line feed — it moves down and
leaves the column alone — and `\r` is what returns to column 0. Unix decided that `\n` means both,
and every Unix terminal driver translates. We are the terminal driver, so we make the same choice.

This is why [`serial.c`](../nimbus/drivers/serial.c) translates `\n` into `\r\n`: a terminal emulator
on the other end of a serial line does *not* make that assumption, and a log written with bare
newlines comes out as a diagonal staircase.

**`\t` rounds up to a multiple of 8.** `(col + 8) & ~7` — add 8, then clear the low three bits. The
same trick as `ALIGN_UP` in [`types.h`](../nimbus/include/nimbus/types.h), and it avoids a division
in a function called once per character.

**`\b` moves, it does not erase.** This surprises people. Backspace is defined as "move the cursor
left one position" and nothing more. To actually erase, you send three characters: `\b`, a space,
`\b` again. That is exactly what [`console.c`](../nimbus/kernel/console.c) does, and it is why a
mis-implemented backspace appears to do nothing — the cursor moves, then the next character
overwrites, so it looks right until you backspace over the end of a line.

---

## 5. Scrolling

```c
static void vga_scroll(void)
{
    for (int row = 1; row < VGA_ROWS; row++)
        for (int col = 0; col < VGA_COLS; col++)
            VGA_MEMORY[(row - 1) * VGA_COLS + col] = VGA_MEMORY[row * VGA_COLS + col];

    for (int col = 0; col < VGA_COLS; col++)
        VGA_MEMORY[(VGA_ROWS - 1) * VGA_COLS + col] = vga_cell(' ', color);

    cursor_row = VGA_ROWS - 1;
}
```

Copy rows 1–24 up to rows 0–23, blank row 24.

That is 24 × 80 × 2 = 3840 bytes of memory traffic on every newline at the bottom of the screen. It
is genuinely how DOS did it, and at 3840 bytes it is invisible even on a 386.

### The alternative: hardware scrolling

The CRT controller has a *start address* register — which cell of the buffer to display at the
top-left. Bump it by 80 and the whole screen scrolls in two port writes instead of 3840 bytes.

Nimbus does not do it either, and [`vga.c`](../nimbus/drivers/vga.c) says why:

> it makes the buffer a ring, so every coordinate calculation in the driver needs a modulo, and
> every caller that pokes the buffer directly (the panic screen does) breaks.

That is the real cost. It is not the two port writes versus the memcpy; it is that every other
function in the file becomes harder, and the one place that legitimately writes to the buffer
directly stops working. Chapter 12, exercise 12.6 implements it anyway, because it is worth doing
once to feel the difference.

---

## 6. Numbers

No `printf` yet — that is Chapter 14, and it is 200 lines. Two specialised routines do the job.

```c
void vga_puthex(uint32_t value)
{
    const char *digits = "0123456789ABCDEF";
    vga_puts("0x");
    for (int shift = 28; shift >= 0; shift -= 4)
        vga_putc(digits[(value >> shift) & 0xF]);
}
```

Fixed width — always eight digits — rather than minimal width. Deliberately: when you are scanning a
column of addresses trying to spot the one that is wrong, ragged alignment costs you more than the
leading zeros do.

Shift from the top down, four bits at a time, masking off the rest. No division, no buffer, no
reversal.

```c
void vga_putdec(uint32_t value)
{
    char buf[11];
    int  i = 0;

    if (value == 0) { vga_putc('0'); return; }

    while (value > 0) {
        buf[i++] = (char)('0' + (value % 10));
        value /= 10;
    }
    while (i > 0)
        vga_putc(buf[--i]);
}
```

Decimal needs a buffer because repeated division produces digits *backwards* — least significant
first. Fill forwards, print in reverse.

`char buf[11]`: 4294967295 is ten digits, plus room. Sizing that buffer at 10 is an off-by-one that
overflows a stack array for exactly one input value.

The `value == 0` special case exists because the loop is `while (value > 0)`, which produces nothing
for zero. Every hand-written integer-to-string has this bug once.

---

## 7. The banner, and writing without the cursor

```c
static void banner(void)
{
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLUE);
    for (int col = 0; col < VGA_COLS; col++)
        VGA_MEMORY[col] = vga_cell(' ', color);

    const char *title = " SPARK -- a 600-line operating system ";
    int start = (VGA_COLS - 38) / 2;
    for (int i = 0; title[i]; i++)
        VGA_MEMORY[start + i] = vga_cell(title[i], color);

    cursor_row = 2;
    cursor_col = 0;
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
}
```

This writes to the buffer directly rather than through `vga_putc`, because it needs to place
characters at absolute positions without moving the cursor or triggering a scroll.

That is a legitimate thing to do and it is exactly what hardware scrolling would break. Nimbus keeps
`vga_put_at` in the header for the same reason — the panic screen paints a full-screen background and
cannot afford the cursor logic.

---

## 8. `kmain`

```c
void kmain(void)
{
    vga_clear();
    banner();

    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_puts("The CPU is in 32-bit protected mode.\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);

    vga_puts("Everything that got us here, you wrote:\n\n");
    vga_puts("  - the BIOS loaded 512 bytes from sector 0 to 0x7C00\n");
    vga_puts("  - stage 1 read four more sectors and jumped to 0x7E00\n");
    vga_puts("  - stage 2 loaded this kernel, opened the A20 gate,\n");
    vga_puts("    installed a GDT and set CR0.PE\n");
    vga_puts("  - it copied us to 1 MiB and jumped here\n\n");

    vga_set_color(VGA_YELLOW, VGA_BLACK);
    vga_puts("kmain() is at ");
    vga_puthex((uint32_t)(void *)kmain);
    ...

    int here;
    vga_puthex((uint32_t)&here);
    ...

    for (;;)
        __asm__ volatile ("hlt");
}
```

Two of those lines are doing something worth pointing out.

**`(uint32_t)(void *)kmain`** — casting a function pointer to an integer, via `void *`. The
intermediate cast silences a warning; the result is the address the linker assigned, and it should be
close to `0x100000`. Seeing that number on screen is a direct confirmation that the linker script
did what §3 of Chapter 9 said it would.

**`int here; vga_puthex((uint32_t)&here);`** — taking the address of a local tells us roughly where
`ESP` is, which is the only way to observe the stack pointer from C without inline assembly. It
should be a little below `stack_top`, which is in `.bss`, which `readelf -S` said is at about
`0x103000`.

Two numbers on screen that confirm two entirely different parts of the build. That is what the
chapter's output is for.

---

## 9. Running it

```bat
build spark
run spark
```

```
+------------------------------------------------------------------------------+
|                    SPARK -- a 600-line operating system                      |
+------------------------------------------------------------------------------+

The CPU is in 32-bit protected mode.
Everything that got us here, you wrote:

  - the BIOS loaded 512 bytes from sector 0 to 0x7C00
  - stage 1 read four more sectors and jumped to 0x7E00
  - stage 2 loaded this kernel, opened the A20 gate,
    installed a GDT and set CR0.PE
  - it copied us to 1 MiB and jumped here

kmain() is at 0x00100034
video memory is at 0x000B8000
the stack is around 0x00106FE8

this screen is 2000 cells of 2 bytes = 4000 bytes at 0xB8000

Spark is finished. Nimbus starts in Chapter 11.
```

Check the two numbers against the build:

```bash
$ i686-elf-readelf -s bin/spark/kernel.elf | grep -E "kmain|stack_top"
    21: 00100034    ...  kmain
    24: 00107010    ...  stack_top
```

`kmain` at `0x00100034` — matching the screen. The stack at `0x00106FE8`, a little below
`stack_top = 0x00107010`, which is exactly right: `call kmain` pushed a return address, the prologue
pushed `EBP`, and `here` is a local below both.

---

## 10. What Spark is, and is not

Take stock. In about 600 lines:

| Have | Do not have |
|---|---|
| Our own bootloader, two stages | Interrupts — `IF` has been clear since Chapter 7 |
| 32-bit protected mode | Any way to read input |
| A GDT | Memory management of any kind |
| Disk reading (in real mode only) | Processes, or the concept of one |
| Screen output | A filesystem |
| C running on bare metal | Any way to run a program |

Spark cannot do anything useful and that was never the point. The point is that there is now nothing
between you and the machine that you do not understand. When Chapter 15 builds a GDT properly, you
have already built one. When Chapter 24 turns on paging, you have already set `CR0.PE` and felt what
a mode switch is.

That is the difference between reading about an operating system and having written one.

---

## 11. Exercises

🟢 **10.1** Add a `vga_puts_at(int row, int col, const char *s)` and use it to put a status line at
the bottom of the screen that `vga_putc` never scrolls away.

🟢 **10.2** Print `0xDB` (a solid block) 80 times in different colours to make a colour bar. Then
remove the `(uint8_t)` cast from `vga_cell` and run it again.

🟢 **10.3** `vga_putdec(0)` works because of a special case. Delete it and explain what you see.

🟡 **10.4** Write `vga_putbin(uint32_t)` that prints 32 binary digits with a space every 4. Use it to
print the GDT access byte from Chapter 7 and confirm it matches the table there.

🟡 **10.5** Implement `vga_scroll` with `memmove` instead of the double loop — which means writing
`memmove` first, since there is no libc. Compare the generated assembly.

🟡 **10.6** Fill the whole screen with `A`, then time 100 scrolls using a counting loop. Now do the
same with a `rep movsd` version. How much faster is it, and does it matter?

🔴 **10.7** Enable bright backgrounds. You will need to read from port `0x3C0` (the Attribute
Controller), which has an unusual protocol: reading port `0x3DA` resets its flip-flop, then writes to
`0x3C0` alternate between index and data. Clear bit 3 of Attribute Mode Control (index `0x10`), then
print white-on-bright-blue.

🔴 **10.8** Add a second video page. The text buffer is 32 KiB but a screen is only 4000 bytes, so
there is room for eight pages. Use the CRTC start address register to flip between two, and write an
alternating splash screen.

---

## What we covered

- `0xB8000`: 4000 bytes wired to the display, no driver required, already in mode 3 thanks to the
  video BIOS.
- The cell format, the sixteen colours and why there are only eight backgrounds, and the fact that
  it is code page 437 rather than ASCII.
- Why the pointer is `volatile uint16_t *`, and what the compiler would otherwise be entitled to do.
- The signed-`char` trap and the two-cast idiom that fixes it.
- `\n`, `\r`, `\t` and `\b` handled by hand, including why backspace does not erase.
- Software scrolling, what hardware scrolling would cost, and why neither Spark nor Nimbus does it.
- Hex fixed-width for scannability; decimal backwards into a buffer, with the two bugs that always
  appear.
- Writing to the buffer directly for absolute positioning, and why that habit constrains the
  scrolling decision.
- Two numbers on screen that independently confirm the linker script and the stack setup.

**Part I is finished.** [Chapter 11](11-multiboot.md) starts Nimbus, and the first thing it does is
explain why we are not going to use the bootloader we just spent six chapters writing.

---

[← Freestanding C](09-freestanding-c.md) · [Contents](README.md) · [Next: Multiboot →](11-multiboot.md)
