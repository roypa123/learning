/* ===========================================================================
 *  spark/kernel/kernel.c  --  the entire Spark kernel
 * ===========================================================================
 *
 *  This is C, but not C as you know it. There is no libc: no printf, no
 *  malloc, no memcpy, no assert, no errno. There is no operating system to
 *  make a system call to -- we are the operating system. There is no `main`;
 *  entry.asm calls `kmain` because that is the name it was told to call.
 *
 *  What we *do* have is the C language itself, and one piece of hardware that
 *  is trivially easy to drive: the VGA text buffer. On every PC, a 4000-byte
 *  region of physical memory at 0xB8000 is wired directly to the display. Two
 *  bytes per character cell, 80 columns by 25 rows. Store a byte there and the
 *  character appears. No driver, no initialisation, no handshake.
 *
 *  Explained in: docs/10-vga-hello.md
 *  Line by line: docs/line-by-line/spark-kernel.md
 * =========================================================================== */

/* ---------------------------------------------------------------------------
 *  Fixed-width types
 *
 *  <stdint.h> is a *freestanding* header: the C standard says a compiler must
 *  provide it even with no operating system, and our cross-compiler does. But
 *  writing them out once is worth doing at least once in your life, because
 *  every one of these sizes is a promise about the machine, and on the machine
 *  we are targeting the promises are:
 *
 *      char       8 bits       short     16 bits
 *      int       32 bits       long      32 bits (32-bit x86! not 64)
 *      long long 64 bits       pointer   32 bits
 * ------------------------------------------------------------------------- */
typedef unsigned char      uint8_t;
typedef signed   char      int8_t;
typedef unsigned short     uint16_t;
typedef signed   short     int16_t;
typedef unsigned int       uint32_t;
typedef signed   int       int32_t;
typedef unsigned long long uint64_t;
typedef unsigned int       size_t;

/* ---------------------------------------------------------------------------
 *  The VGA text buffer
 * ------------------------------------------------------------------------- */

#define VGA_MEMORY  ((volatile uint16_t *)0xB8000)
#define VGA_COLS    80
#define VGA_ROWS    25

/*  Each cell is one 16-bit word:
 *
 *      bit  15 14 13 12  11 10 9 8   7 6 5 4 3 2 1 0
 *           |  \______/  \_______/   \_____________/
 *           |    bg         fg          code point
 *           blink
 *
 *  The "code point" is not Unicode and not quite ASCII -- it is code page 437,
 *  the IBM PC character set, where 0x01 is a smiley face and 0xDB is a solid
 *  block. For the 95 printable ASCII characters the two agree, which is why
 *  you can pretend it is ASCII until the day you print a `£`.
 */
enum vga_color {
    VGA_BLACK = 0, VGA_BLUE, VGA_GREEN, VGA_CYAN,
    VGA_RED, VGA_MAGENTA, VGA_BROWN, VGA_LIGHT_GREY,
    VGA_DARK_GREY, VGA_LIGHT_BLUE, VGA_LIGHT_GREEN, VGA_LIGHT_CYAN,
    VGA_LIGHT_RED, VGA_LIGHT_MAGENTA, VGA_YELLOW, VGA_WHITE
};

static int      cursor_row = 0;
static int      cursor_col = 0;
static uint8_t  color      = (VGA_BLACK << 4) | VGA_LIGHT_GREY;

/*  Pack a character and the current colour into one cell word.
 *
 *  The cast to uint16_t on `c` matters: `char` is signed on x86 GCC, so a
 *  byte like 0xDB is the negative number -37, and sign-extending that into 16
 *  bits gives 0xFFDB -- which would overwrite the colour nibbles with 0xFF and
 *  print a blinking white block. Casting through uint8_t first keeps it 0x00DB.
 */
static inline uint16_t vga_cell(char c, uint8_t attr)
{
    return (uint16_t)(uint8_t)c | ((uint16_t)attr << 8);
}

void vga_set_color(enum vga_color fg, enum vga_color bg)
{
    color = (uint8_t)((bg << 4) | fg);
}

void vga_clear(void)
{
    for (int i = 0; i < VGA_COLS * VGA_ROWS; i++)
        VGA_MEMORY[i] = vga_cell(' ', color);
    cursor_row = 0;
    cursor_col = 0;
}

/*  Scroll by one line: move rows 1..24 up to rows 0..23, then blank row 24.
 *
 *  This is a memmove of 24 * 80 * 2 = 3840 bytes on every newline at the
 *  bottom of the screen. It is genuinely how DOS did it, and at 3840 bytes it
 *  is invisible even on a 386. Chapter 12 replaces it with hardware scrolling,
 *  which costs two port writes instead, and explains why that is harder than
 *  it sounds.
 */
static void vga_scroll(void)
{
    for (int row = 1; row < VGA_ROWS; row++)
        for (int col = 0; col < VGA_COLS; col++)
            VGA_MEMORY[(row - 1) * VGA_COLS + col] = VGA_MEMORY[row * VGA_COLS + col];

    for (int col = 0; col < VGA_COLS; col++)
        VGA_MEMORY[(VGA_ROWS - 1) * VGA_COLS + col] = vga_cell(' ', color);

    cursor_row = VGA_ROWS - 1;
}

void vga_putc(char c)
{
    if (c == '\n') {
        cursor_col = 0;
        cursor_row++;
    } else if (c == '\r') {
        cursor_col = 0;
    } else if (c == '\t') {
        cursor_col = (cursor_col + 8) & ~7;      /* round up to a multiple of 8 */
    } else if (c == '\b') {
        if (cursor_col > 0) cursor_col--;
    } else {
        VGA_MEMORY[cursor_row * VGA_COLS + cursor_col] = vga_cell(c, color);
        cursor_col++;
    }

    if (cursor_col >= VGA_COLS) {                /* wrap */
        cursor_col = 0;
        cursor_row++;
    }
    if (cursor_row >= VGA_ROWS)                  /* scroll */
        vga_scroll();
}

void vga_puts(const char *s)
{
    while (*s) vga_putc(*s++);
}

/*  Print an unsigned 32-bit value in hex, always 8 digits, with a 0x prefix.
 *
 *  Fixed width rather than minimal width on purpose: when you are staring at a
 *  column of addresses trying to spot which one is wrong, ragged alignment
 *  costs you more than the leading zeros do. Chapter 14 writes a real printf.
 */
void vga_puthex(uint32_t value)
{
    const char *digits = "0123456789ABCDEF";
    vga_puts("0x");
    for (int shift = 28; shift >= 0; shift -= 4)
        vga_putc(digits[(value >> shift) & 0xF]);
}

void vga_putdec(uint32_t value)
{
    char buf[11];                 /* 4294967295 is 10 digits, plus a NUL */
    int  i = 0;

    if (value == 0) { vga_putc('0'); return; }

    /* Generating digits by repeated division produces them backwards, so we
     * fill a buffer from the front and then walk it in reverse. */
    while (value > 0) {
        buf[i++] = (char)('0' + (value % 10));
        value /= 10;
    }
    while (i > 0)
        vga_putc(buf[--i]);
}

/* ---------------------------------------------------------------------------
 *  A drawing helper, purely so the screen looks like something was built
 * ------------------------------------------------------------------------- */
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

/* ---------------------------------------------------------------------------
 *  kmain -- called by entry.asm, never returns
 * ------------------------------------------------------------------------- */
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
    vga_puts("\nvideo memory is at ");
    vga_puthex(0xB8000);
    vga_puts("\nthe stack is around ");

    /* Taking the address of a local tells us roughly where ESP is. It is a
     * legitimate trick and the only way to see the stack pointer from C
     * without inline assembly. */
    int here;
    vga_puthex((uint32_t)&here);

    vga_puts("\n\nthis screen is ");
    vga_putdec(VGA_COLS * VGA_ROWS);
    vga_puts(" cells of ");
    vga_putdec(2);
    vga_puts(" bytes = ");
    vga_putdec(VGA_COLS * VGA_ROWS * 2);
    vga_puts(" bytes at 0xB8000\n");

    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_puts("\nSpark is finished. Nimbus starts in Chapter 11.\n");

    /* Nothing left to do. There is no scheduler to yield to, no idle task, and
     * no interrupt that will ever fire, because entry.asm cleared IF and we
     * never installed an IDT. `hlt` in a loop is the correct way to stop: it
     * drops the CPU into a low-power state instead of spinning a core at 100%,
     * which on a laptop is the difference between silence and a fan. */
    for (;;)
        __asm__ volatile ("hlt");
}
