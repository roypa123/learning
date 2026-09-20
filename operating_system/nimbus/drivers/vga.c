/* ===========================================================================
 *  nimbus/drivers/vga.c  --  80x25 text mode
 * ===========================================================================
 *
 *  The simplest device on the machine, and therefore the one to write first:
 *  a 4000-byte window of physical memory that the video hardware scans out
 *  sixty times a second. Store a word, see a character. There is no
 *  initialisation sequence, no command register and no interrupt -- the BIOS
 *  already put the card in mode 3 before it handed control to the bootloader.
 *
 *  One address subtlety runs through the whole file. Paging is on before any C
 *  in this kernel executes, and the identity mapping is gone, so physical
 *  0xB8000 is not reachable by that number. The higher-half window maps it at
 *  0xC00B8000, which is what P2V() computes. Writing the constant 0xB8000 here
 *  produces a page fault, and it is a mistake worth making once on purpose --
 *  Chapter 25 suggests doing exactly that.
 *
 *  Explained in: docs/12-vga-driver.md
 * =========================================================================== */

#include <nimbus/vga.h>
#include <nimbus/io.h>
#include <nimbus/paging.h>
#include <nimbus/string.h>

static volatile uint16_t *vga_buffer;
static int      cursor_row;
static int      cursor_col;
static uint8_t  current_attr;

/*  The CRT controller. Two ports: you write an *index* to 0x3D4 to select one
 *  of about 25 internal registers, then read or write its value at 0x3D5. The
 *  whole card is driven through that keyhole, which is how a 1987 graphics
 *  adapter fitted into an I/O space that was already crowded.                 */
#define CRTC_INDEX 0x3D4
#define CRTC_DATA  0x3D5

#define CRTC_CURSOR_HIGH  0x0E
#define CRTC_CURSOR_LOW   0x0F
#define CRTC_CURSOR_START 0x0A
#define CRTC_CURSOR_END   0x0B

static inline uint16_t cell(char c, uint8_t attr)
{
    return (uint16_t)(uint8_t)c | ((uint16_t)attr << 8);
}

void vga_init(void)
{
    vga_buffer   = (volatile uint16_t *)P2V(VGA_PHYS);
    current_attr = (VGA_BLACK << 4) | VGA_LIGHT_GREY;
    cursor_row   = 0;
    cursor_col   = 0;
    vga_clear();
    vga_show_cursor();
}

void vga_set_color(enum vga_color fg, enum vga_color bg)
{
    current_attr = (uint8_t)((bg << 4) | fg);
}

void vga_set_raw_color(uint8_t attr) { current_attr = attr; }
uint8_t vga_get_color(void)          { return current_attr; }

void vga_clear(void)
{
    uint16_t blank = cell(' ', current_attr);
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
        vga_buffer[i] = blank;
    cursor_row = cursor_col = 0;
    vga_move_cursor(0, 0);
}

void vga_put_at(int row, int col, char c, uint8_t attr)
{
    if (row < 0 || row >= VGA_HEIGHT || col < 0 || col >= VGA_WIDTH) return;
    vga_buffer[row * VGA_WIDTH + col] = cell(c, attr);
}

/* ---------------------------------------------------------------------------
 *  The hardware cursor
 *
 *  The blinking underscore is drawn by the card, not by us, and it lives at a
 *  linear cell index the CRTC holds in two 8-bit registers. Splitting a 16-bit
 *  value across two registers means two port pairs and a shift, and it means
 *  that between the two writes the cursor is briefly at a nonsense position --
 *  invisible at 60 Hz, and the reason some old code disables the cursor while
 *  moving it.
 * ------------------------------------------------------------------------- */
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

void vga_get_cursor(int *row, int *col)
{
    if (row) *row = cursor_row;
    if (col) *col = cursor_col;
}

void vga_hide_cursor(void)
{
    /* Bit 5 of the cursor start register means "disable". The other bits are
     * the scanline the cursor starts on, which is how you get a block cursor
     * instead of an underscore. */
    outb(CRTC_INDEX, CRTC_CURSOR_START);
    outb(CRTC_DATA, 0x20);
}

void vga_show_cursor(void)
{
    outb(CRTC_INDEX, CRTC_CURSOR_START);
    outb(CRTC_DATA, 14);      /* start scanline: near the bottom of the cell */
    outb(CRTC_INDEX, CRTC_CURSOR_END);
    outb(CRTC_DATA, 15);      /* end scanline: the last one                  */
}

/* ---------------------------------------------------------------------------
 *  Scrolling
 *
 *  Move every row up one and blank the last. memmove, not memcpy: the regions
 *  overlap, and while copying upwards happens to be safe in the forward
 *  direction, relying on that is relying on an implementation detail of our
 *  own memcpy. memmove says what we mean and costs the same.
 *
 *  The alternative is hardware scrolling: the CRTC has a "start address"
 *  register, and bumping it by 80 scrolls the whole screen in two port writes
 *  instead of 3840 bytes of memory traffic. We do not, for one reason -- it
 *  makes the buffer a ring, so every coordinate calculation in the driver
 *  needs a modulo, and every caller that pokes the buffer directly (the panic
 *  screen does) breaks. Chapter 12 has the version that does it anyway.
 * ------------------------------------------------------------------------- */
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

/*  The character logic, with no cursor update. Split out so that vga_write()
 *  can emit a whole line and touch the CRTC once.                             */
static void vga_putc_raw(char c)
{
    switch (c) {
    case '\n':
        cursor_col = 0;
        cursor_row++;
        break;

    case '\r':
        cursor_col = 0;
        break;

    case '\t':
        /* Advance to the next multiple of 8. The bit trick is the same as
         * ALIGN_UP and avoids a division in a function called once per
         * character. */
        cursor_col = (cursor_col + 8) & ~7;
        break;

    case '\b':
        /* Backspace *moves*; it does not erase. The console layer erases by
         * sending "\b \b", which is the same sequence a real terminal uses and
         * the reason backspace behaves oddly over a bad serial line. */
        if (cursor_col > 0) {
            cursor_col--;
        } else if (cursor_row > 0) {
            cursor_row--;
            cursor_col = VGA_WIDTH - 1;
        }
        break;

    default:
        if ((unsigned char)c < 0x20) return;    /* drop other control codes */
        vga_buffer[cursor_row * VGA_WIDTH + cursor_col] = cell(c, current_attr);
        cursor_col++;
        break;
    }

    if (cursor_col >= VGA_WIDTH) {
        cursor_col = 0;
        cursor_row++;
    }
    while (cursor_row >= VGA_HEIGHT)
        vga_scroll();
}

void vga_putc(char c)
{
    vga_putc_raw(c);
    vga_move_cursor(cursor_row, cursor_col);
}

void vga_write(const char *s, size_t len)
{
    /* Move the cursor once at the end rather than once per character. Each
     * move is four I/O port writes and an I/O bus cycle costs roughly a
     * microsecond, so an 80-character line would otherwise spend 320
     * microseconds talking to the CRTC to draw 80 characters of text. */
    for (size_t i = 0; i < len; i++)
        vga_putc_raw(s[i]);

    vga_move_cursor(cursor_row, cursor_col);
}

void vga_puts(const char *s)
{
    while (*s) vga_putc_raw(*s++);
    vga_move_cursor(cursor_row, cursor_col);
}
