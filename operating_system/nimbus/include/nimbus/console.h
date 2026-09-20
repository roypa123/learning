/* ===========================================================================
 *  nimbus/include/nimbus/console.h  --  the terminal
 * ===========================================================================
 *
 *  The console is the layer between "a keyboard produces keys" and "a program
 *  calls read() and gets a line". It owns three things that belong together
 *  and nowhere else:
 *
 *    echo        typed characters appear on screen, because the *console*
 *                prints them -- the keyboard does not, and the program has
 *                not seen them yet
 *    line mode   read() does not return until Enter, so that backspace can
 *                still take a character back
 *    the buffer  where the half-typed line lives in the meantime
 *
 *  On Unix this layer is the tty line discipline, and it is famously the most
 *  confusing part of the system. Ours is 150 lines, which is roughly what it
 *  takes to do the job honestly.
 *
 *  Explained in: docs/20-kernel-console.md
 * =========================================================================== */
#ifndef NIMBUS_CONSOLE_H
#define NIMBUS_CONSOLE_H

#include <nimbus/types.h>
#include <nimbus/vfs.h>

#define CONSOLE_LINE_MAX 256

void    console_init(void);

/*  Called by the keyboard IRQ for every key. Runs in interrupt context, so it
 *  must not block, must not allocate, and must finish fast.                   */
void    console_input(int key);

/*  Read one line. Blocks until Enter. Returns the number of bytes written,
 *  not counting the terminating newline, which is included in the buffer.     */
ssize_t console_read_line(char *buf, size_t max);

void    console_write(const char *s, size_t len);
void    console_putc(char c);

/*  The /dev/console node, so that userland can open() and read() it like any
 *  other file -- which is the whole point of having a VFS.                    */
vfs_node_t *console_device_node(void);

#endif /* NIMBUS_CONSOLE_H */
