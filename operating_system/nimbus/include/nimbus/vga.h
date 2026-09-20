/* ===========================================================================
 *  nimbus/include/nimbus/vga.h  --  80x25 text mode
 * ===========================================================================
 *  Explained in: docs/12-vga-driver.md
 * =========================================================================== */
#ifndef NIMBUS_VGA_H
#define NIMBUS_VGA_H

#include <nimbus/types.h>

#define VGA_WIDTH   80
#define VGA_HEIGHT  25

/*  Physical 0xB8000. After paging is on (Chapter 24) the kernel reaches it
 *  through the higher-half window at 0xC00B8000, and vga.c uses this constant
 *  via the P2V() macro rather than hard-coding either number.                 */
#define VGA_PHYS    0x000B8000u

enum vga_color {
    VGA_BLACK = 0,   VGA_BLUE,          VGA_GREEN,       VGA_CYAN,
    VGA_RED,         VGA_MAGENTA,       VGA_BROWN,       VGA_LIGHT_GREY,
    VGA_DARK_GREY,   VGA_LIGHT_BLUE,    VGA_LIGHT_GREEN, VGA_LIGHT_CYAN,
    VGA_LIGHT_RED,   VGA_LIGHT_MAGENTA, VGA_YELLOW,      VGA_WHITE
};

void    vga_init(void);
void    vga_clear(void);
void    vga_putc(char c);
void    vga_write(const char *s, size_t len);
void    vga_puts(const char *s);

void    vga_set_color(enum vga_color fg, enum vga_color bg);
uint8_t vga_get_color(void);
void    vga_set_raw_color(uint8_t attr);

void    vga_move_cursor(int row, int col);
void    vga_get_cursor(int *row, int *col);
void    vga_hide_cursor(void);
void    vga_show_cursor(void);

/*  Write one cell directly, bypassing the cursor. Used by the panic screen,
 *  which paints a full-screen background and cannot afford to scroll.         */
void    vga_put_at(int row, int col, char c, uint8_t attr);

#endif /* NIMBUS_VGA_H */
