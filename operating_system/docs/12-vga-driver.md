# Chapter 12 — A real VGA text driver

[← Multiboot](11-multiboot.md) · [Contents](README.md) · [Next: The serial port →](13-serial-port.md)

---

## Goal

Turn Chapter 10's 80 lines into a driver: a hardware cursor, correct scrolling, a batched write path,
and the port protocol that talks to the CRT controller. Along the way, the first real encounter with
memory-mapped I/O and with the fact that Nimbus has paging on before any C runs.

---

## 1. Ports and MMIO, properly

Chapter 2, §6 introduced the two address spaces. This is the first driver that uses both, so it is
worth being precise.

**Memory-mapped I/O** is the framebuffer at `0xB8000`. A device's memory appears at a physical
address, and you read and write it with ordinary `mov`. It is the modern way, and it is better in
every respect: pointers work, the compiler understands it, structs describe it, and there is no
64 KiB limit.

**Port I/O** is the CRT controller at `0x3D4`/`0x3D5`. A separate 64 KiB address space, reachable
only with `in` and `out`, and unreachable from C without inline assembly.

The VGA adapter uses both at once, which is normal for hardware of that era: the *data* is large and
gets a memory window, the *control registers* are small and live in port space.

### 1.1 The two rules for MMIO

**`volatile`, always.** Covered in Chapter 10, §2. Without it the compiler may delete, reorder,
combine or hoist accesses, all of which are legal for ordinary memory and wrong for a device.

**No caching, for real devices.** The CPU caches memory reads. A device register that the CPU has
cached is a register you read once and then never see change again.

The VGA framebuffer is an exception — it is RAM on the card, and caching it is fine and much faster.
But for a device with status registers in its memory window, the pages must be mapped with the
cache-disable bit. That is what `PTE_NOCACHE` in
[`paging.h`](../nimbus/include/nimbus/paging.h) is for, and Chapter 23 covers when to use it.

### 1.2 The `outb` wrapper, again

```c
static ALWAYS_INLINE void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile ("outb %0, %1" :: "a"(value), "Nd"(port));
}
```

`"Nd"` means "in `DX`, or an immediate constant 0–255". The assembler picks the short form
`out 0x20, al` when the port is a small constant, which is one byte smaller and one cycle faster.
Since most of our ports are above 255, it usually ends up in `DX` anyway.

---

## 2. Paging changes the address

```c
void vga_init(void)
{
    vga_buffer   = (volatile uint16_t *)P2V(VGA_PHYS);
    ...
}
```

This is the first place the higher half bites.

[`boot.asm`](../nimbus/boot/boot.asm) enables paging before any C runs, and removes the identity
mapping immediately afterwards. So physical `0xB8000` is **not reachable by that number**. The
higher-half window maps it at `0xC00B8000`, which is what `P2V(0xB8000)` computes.

Writing the constant `0xB8000` here is a page fault, and it is a page fault in `vga_init`, which
means before there is any way to print the fault. The machine simply stops.

Chapter 25 covers the window properly. The rule for now: **in Nimbus, any fixed physical address a
driver needs goes through `P2V`.** Chapter 25, exercise 25.3 suggests making this mistake on purpose
once, because the failure mode is instructive and you will recognise it instantly thereafter.

---

## 3. The CRT controller

```c
#define CRTC_INDEX 0x3D4
#define CRTC_DATA  0x3D5
```

Two ports, and about twenty-five internal registers behind them. You write an *index* to `0x3D4` to
select a register, then read or write its value at `0x3D5`.

That is called an index/data pair, and it is how a chip with two dozen registers fits into an I/O
space that was already crowded in 1987. You will meet the same pattern in the PIC (Chapter 17), the
PIT (Chapter 18) and the CMOS clock.

The registers we use:

| Index | Register |
|---|---|
| `0x0A` | Cursor start scanline, plus the disable bit |
| `0x0B` | Cursor end scanline |
| `0x0C` | Start address high — for hardware scrolling |
| `0x0D` | Start address low |
| `0x0E` | Cursor position high |
| `0x0F` | Cursor position low |

> 🔧 **Monochrome adapters use `0x3B4`/`0x3B5`.** Bit 0 of the BIOS "equipment word" at `0x0410`
> says which. Every machine since about 1990 is colour, and a driver that reads the equipment word
> is being conscientious about hardware that no longer exists. We hardcode `0x3D4`.

---

## 4. The hardware cursor

```c
void vga_move_cursor(int row, int col)
{
    if (row < 0) row = 0;
    if (col < 0) col = 0;
    if (row >= VGA_HEIGHT) row = VGA_HEIGHT - 1;
    if (col >= VGA_WIDTH)  col = VGA_WIDTH - 1;

    cursor_row = row;
    cursor_col = col;

    uint16_t pos = (uint16_t)(row * VGA_WIDTH + col);

    outb(CRTC_INDEX, CRTC_CURSOR_HIGH);
    outb(CRTC_DATA,  (uint8_t)(pos >> 8));
    outb(CRTC_INDEX, CRTC_CURSOR_LOW);
    outb(CRTC_DATA,  (uint8_t)(pos & 0xFF));
}
```

The blinking underscore is drawn by the card, not by us. Its position is a *linear cell index* —
`row × 80 + col`, not a coordinate pair — held in two 8-bit registers.

Four port writes per move. At roughly a microsecond per I/O bus cycle, that is four microseconds, and
§6 is about not doing it once per character.

### 4.1 The clamping

Those four `if`s are not defensive programming for its own sake. A cursor position past the end of
the buffer makes the card display its cursor over whatever memory follows the framebuffer, which on
some adapters is the second video page and on others is nothing. It is a visual glitch rather than a
crash, but it is the kind that makes you doubt code that is working.

### 4.2 The flicker nobody mentions

Between the two writes, the cursor is at a nonsense position — the high byte of the new location with
the low byte of the old. At 60 Hz it is invisible.

It is, however, why some old code disables the cursor, moves it, and re-enables it. If you ever see
that pattern, that is what it is for.

### 4.3 Shape and visibility

```c
void vga_hide_cursor(void)
{
    outb(CRTC_INDEX, CRTC_CURSOR_START);
    outb(CRTC_DATA, 0x20);
}

void vga_show_cursor(void)
{
    outb(CRTC_INDEX, CRTC_CURSOR_START);
    outb(CRTC_DATA, 14);
    outb(CRTC_INDEX, CRTC_CURSOR_END);
    outb(CRTC_DATA, 15);
}
```

A character cell is 16 scanlines tall. The cursor occupies the range from the start scanline to the
end scanline, so 14–15 is a two-pixel underscore at the bottom and 0–15 is a full block.

Bit 5 of the start register means "disabled". Setting `0x20` is start-scanline 0 with the disable bit
set, which turns it off.

---

## 5. Scrolling, and the decision not to do it in hardware

```c
static void vga_scroll(void)
{
    memmove((void *)vga_buffer,
            (const void *)(vga_buffer + VGA_WIDTH),
            (VGA_HEIGHT - 1) * VGA_WIDTH * sizeof(uint16_t));

    uint16_t blank = cell(' ', current_attr);
    for (int col = 0; col < VGA_WIDTH; col++)
        vga_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + col] = blank;

    cursor_row = VGA_HEIGHT - 1;
}
```

`memmove`, not `memcpy`. The regions overlap. Copying upwards in the forward direction happens to be
safe, and relying on that is relying on an implementation detail of our own `memcpy` — which
Chapter 14's version might change. `memmove` says what we mean and, for this direction, compiles to
the same loop.

This also is the first use of `memmove` in Nimbus, which means [`lib/string.c`](../nimbus/lib/string.c)
has to exist before this driver links. Chapter 9, §5: GCC assumes those four functions exist whether
or not you call them.

### 5.1 The hardware alternative

The CRTC's start address register says which cell of the buffer appears at the top-left. Add 80 and
the screen scrolls, in two port writes instead of 3840 bytes of memory traffic.

The source explains why we do not:

> it makes the buffer a ring, so every coordinate calculation in the driver needs a modulo, and
> every caller that pokes the buffer directly (the panic screen does) breaks.

Concretely, after eight scrolls the top-left of the screen is at buffer offset 640, and
`vga_put_at(0, 0, ...)` writes to the wrong place. Every function in the file needs
`(start + row * 80 + col) % 2048`, and the panic screen — which paints absolute positions — needs to
know about `start` too.

The buffer is 32 KiB, so there is room for eight screens. Real DOS-era code scrolled in hardware
until it ran out and then did one big memmove to reset. It is about forty lines and Exercise 12.6
implements it.

**The real lesson** is that the obvious optimisation (3840 bytes → 4 port writes) is not the
expensive part of the decision. The expensive part is the complexity it pushes into every other
function.

---

## 6. Batching the cursor update

```c
static void vga_putc_raw(char c)
{
    ... /* all the character logic, no cursor update */
}

void vga_putc(char c)
{
    vga_putc_raw(c);
    vga_move_cursor(cursor_row, cursor_col);
}

void vga_write(const char *s, size_t len)
{
    for (size_t i = 0; i < len; i++)
        vga_putc_raw(s[i]);

    vga_move_cursor(cursor_row, cursor_col);
}
```

Splitting `vga_putc_raw` out of `vga_putc` is the whole optimisation, and it is worth doing because
the numbers are not marginal.

Four port writes per character, at roughly a microsecond each. An 80-character line costs 320
microseconds of I/O bus traffic to draw 80 characters of text — the cursor updates dominate the
actual work by an order of magnitude.

Batching makes an 80-character line cost 4 microseconds of port traffic plus 160 bytes of memory
stores. The cursor is only observable when it stops moving, so updating it once at the end is not a
compromise.

This matters more than it sounds once `kprintf` exists: a kernel that logs a line per subsystem at
boot spends a real fraction of its startup time talking to the CRT controller.

---

## 7. The control characters, and one that is missing

```c
static void vga_putc_raw(char c)
{
    switch (c) {
    case '\n':  cursor_col = 0; cursor_row++;           break;
    case '\r':  cursor_col = 0;                          break;
    case '\t':  cursor_col = (cursor_col + 8) & ~7;      break;
    case '\b':
        if (cursor_col > 0) {
            cursor_col--;
        } else if (cursor_row > 0) {
            cursor_row--;
            cursor_col = VGA_WIDTH - 1;
        }
        break;
    default:
        if ((unsigned char)c < 0x20) return;
        vga_buffer[cursor_row * VGA_WIDTH + cursor_col] = cell(c, current_attr);
        cursor_col++;
        break;
    }

    if (cursor_col >= VGA_WIDTH) { cursor_col = 0; cursor_row++; }
    while (cursor_row >= VGA_HEIGHT) vga_scroll();
}
```

Two differences from Spark's version.

**Backspace wraps to the previous line.** Spark's stops at column 0. This matters once the console
has line editing (Chapter 20): a typed line that wrapped needs backspace to go back up, or the
erasure visibly desynchronises from the buffer.

**Other control characters are dropped.** `if ((unsigned char)c < 0x20) return;`. Without this, a
`\x07` (bell) would print as a code-page-437 bullet, and — more to the point — a binary file
accidentally `cat`ed to the console would paint the screen with garbage including characters that
look like box-drawing.

The `(unsigned char)` cast is the signed-`char` trap again: without it, `c = 0xDB` is negative and
`< 0x20` is true, so every high character would be silently dropped.

**`while` rather than `if` on the scroll.** A single `\n` can only push one row past the end, so `if`
would be enough — but `\t` at the bottom-right corner can advance the column past 80, which
increments the row, and defensive code elsewhere might set `cursor_row` directly. A `while` costs
nothing and removes a class of "the screen scrolled once but the cursor is off the bottom".

---

## 8. What is not here: escape sequences

A real terminal understands `\x1b[2J` (clear screen), `\x1b[31m` (red), `\x1b[10;20H` (move cursor).
Those are ANSI escape sequences and they are how every Unix program does colour.

Adding them means a small state machine in `vga_putc_raw`: normal, saw-escape, saw-bracket,
collecting-parameters. About eighty lines for the useful subset.

We do not, for one reason: **nothing generates them.** Our userland is our own, our `kprintf` calls
`vga_set_color` directly, and there is no `ncurses`. Adding escape parsing would be writing a
decoder for an encoder that does not exist.

The moment it becomes worth doing is when you want to run a program written for a real terminal, or
when the serial console needs colour — and that is Exercise 12.7.

---

## 9. Running it

At this point `kmain` initialises serial and VGA and prints a banner:

```bat
build nimbus
run nimbus
```

```
+------------------------------------------------------------------------------+
| NIMBUS       an operating system you wrote                                   |
+------------------------------------------------------------------------------+
```

and in the terminal, from `-serial stdio`:

```
=== Nimbus starting ===
```

Two output paths, both working, before anything else exists. That is deliberate and Chapter 13
explains why the serial one matters more.

### 9.1 Testing the driver

A quick exercise of every path:

```c
    vga_clear();
    for (int i = 0; i < 16; i++) {
        vga_set_color(i, VGA_BLACK);
        kprintf("colour %2d ", i);
    }
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    kprintf("\n\ntab:\tafter\nbackspace: abcXX\b\b  \b\b\n");

    for (int i = 0; i < 30; i++)
        kprintf("line %d\n", i);
```

Sixteen colours, a tab that lands on a multiple of 8, a backspace that erases, and thirty lines that
force five scrolls. If all four look right, the driver is done.

---

## 10. Exercises

🟢 **12.1** Make the cursor a full block by changing the start and end scanlines. Then make it
invisible and confirm output still appears.

🟢 **12.2** Remove the `(unsigned char)` cast from the control-character check and print a string
containing `\xDB`. Explain what you see.

🟢 **12.3** Change `vga_write` to call `vga_putc` instead of `vga_putc_raw`, then print 2000
characters and compare the time in QEMU (`uptime_ms` before and after, once Chapter 18 exists — or
just watch).

🟡 **12.4** Add `vga_set_cursor_shape(int start, int end)` and use it to make the cursor change shape
when a modifier key is held, once Chapter 19 exists.

🟡 **12.5** The BIOS left a cursor position in the CRTC registers before we started. Read it back
(`CRTC_CURSOR_HIGH`/`LOW` are readable) and initialise `cursor_row`/`cursor_col` from it instead of
zeroing, so that boot messages continue from where the BIOS stopped.

🟡 **12.6** Implement hardware scrolling. Keep a `start_offset`, bump it by 80, wrap it at 2048, and
add the modulo to every coordinate calculation. Handle the wrap-around case where the visible screen
straddles the end of the buffer. Then explain what `vga_put_at` now has to do.

🔴 **12.7** Write an ANSI escape sequence parser handling `ESC [ n ; m H` (cursor position),
`ESC [ 2 J` (clear), and `ESC [ n m` (colour, for `n` in 30–37 and 40–47). Make `kprintf("\x1b[31mred\x1b[0m")`
work.

🔴 **12.8** Switch to VGA mode 13h (320×200, 256 colours) by writing the register sequence directly
— you cannot use `int 0x10` any more — and draw a pixel. This is about 60 register writes and is the
gateway to a graphical kernel.

---

## What we covered

- MMIO versus port I/O on one device, and the two rules MMIO needs that memory does not.
- `P2V` on every fixed physical address, because Nimbus has paging on before C runs — and why getting
  it wrong fails invisibly early.
- The index/data register pair, a pattern you will meet in four more drivers.
- The hardware cursor: a linear index in two 8-bit registers, the clamping, the flicker, and the
  scanline-based shape.
- `memmove` over `memcpy`, and why that forces `lib/string.c` to exist.
- Hardware scrolling, and why the expensive part of the decision is the complexity it pushes into
  every other function rather than the cycles it saves.
- Batching the cursor update: 320 microseconds per line down to 4.
- Backspace across a line boundary, dropping control characters, and the `unsigned char` cast that
  makes the check correct.
- Why there is no escape sequence parser, and the moment at which there should be.

[Chapter 13](13-serial-port.md) writes the driver that matters most: the one whose output survives a
crash.

---

[← Multiboot](11-multiboot.md) · [Contents](README.md) · [Next: The serial port →](13-serial-port.md)
