/* ===========================================================================
 *  nimbus/kernel/console.c  --  the line discipline
 * ===========================================================================
 *
 *  Between "the keyboard driver produced the letter k" and "the shell's read()
 *  returned a line of text" sits a layer that every Unix has and nobody enjoys
 *  writing. It owns three behaviours that feel like they belong somewhere else
 *  and do not:
 *
 *  Echo. The characters you type appear on screen because the *kernel* prints
 *  them. The program has not seen them yet -- it is blocked in read() -- so it
 *  cannot be the one echoing. That is why a password prompt has to explicitly
 *  ask the kernel to stop.
 *
 *  Line buffering. read() does not return until Enter, because until Enter you
 *  might still press backspace, and a character already handed to the program
 *  cannot be taken back. Everything about cooked-mode terminals follows from
 *  that one constraint.
 *
 *  Editing. Backspace, and Ctrl-U to kill the line, are implemented here,
 *  inside the kernel, because they operate on the buffer that has not been
 *  delivered yet.
 *
 *  Explained in: docs/20-kernel-console.md
 * =========================================================================== */

#include <nimbus/console.h>
#include <nimbus/keyboard.h>
#include <nimbus/vga.h>
#include <nimbus/serial.h>
#include <nimbus/kernel.h>
#include <nimbus/sched.h>
#include <nimbus/string.h>
#include <nimbus/io.h>
#include <nimbus/vfs.h>
#include <nimbus/heap.h>

static char     line[CONSOLE_LINE_MAX];
static size_t   line_len   = 0;
static bool     line_ready = false;
static bool     echo       = true;

/*  The wait channel. Its *address* is the token; the value is never read. Any
 *  unique address would do, and using the buffer it protects makes the pairing
 *  obvious at both ends.                                                      */
static int console_wait_channel;

void console_putc(char c)
{
    vga_putc(c);
    serial_putc(c);
}

void console_write(const char *s, size_t len)
{
    vga_write(s, len);
    serial_write(s, len);
}

/* ---------------------------------------------------------------------------
 *  console_input -- called from the keyboard IRQ, once per key
 *
 *  Runs in interrupt context. That is a hard constraint, not a style note: it
 *  must not block, must not call kmalloc, and must not take any lock that
 *  task-context code holds while interrupts are enabled. All it does is edit a
 *  fixed buffer and wake a sleeper.
 * ------------------------------------------------------------------------- */
void console_input(int key)
{
    /*  Non-character keys -- arrows, function keys -- are dropped here. A real
     *  terminal turns them into escape sequences ("\x1b[A" for up), and
     *  Chapter 20's exercises add that. Doing it properly means the shell's
     *  line editor has to parse escape sequences, which is a surprising amount
     *  of work for an up arrow.                                               */
    if (key > 0xFF) return;

    char c = (char)key;

    switch (c) {
    case '\n':
    case '\r':
        /*  Terminate the line and hand it over. The newline is *included* in
         *  the buffer, because that is what read() on a terminal returns and
         *  programs like `cat` rely on it. */
        if (line_len < CONSOLE_LINE_MAX - 1)
            line[line_len++] = '\n';
        line[line_len] = '\0';

        if (echo) console_putc('\n');

        line_ready = true;
        sched_wake(&console_wait_channel);
        break;

    case '\b':
    case 0x7F:            /* some keyboards send DEL for the backspace key */
        if (line_len > 0) {
            line_len--;
            /*  Erasing on screen takes three characters: move left, overwrite
             *  with a space, move left again. A bare backspace only moves the
             *  cursor, leaving the old character visible underneath -- which
             *  is why a mis-implemented backspace looks like it does nothing. */
            if (echo) {
                console_putc('\b');
                console_putc(' ');
                console_putc('\b');
            }
        }
        break;

    case 21:              /* Ctrl-U: kill the whole line */
        while (line_len > 0) {
            line_len--;
            if (echo) { console_putc('\b'); console_putc(' '); console_putc('\b'); }
        }
        break;

    case 3:               /* Ctrl-C */
        /*  A real kernel delivers SIGINT to the foreground process group here.
         *  We have no signals (Chapter 46 discusses what adding them costs),
         *  so all this does is abandon the current line, which at least makes
         *  Ctrl-C feel like it does something.                                */
        line_len = 0;
        if (echo) console_write("^C\n", 3);
        line_ready = true;
        line[0] = '\0';
        sched_wake(&console_wait_channel);
        break;

    default:
        if ((unsigned char)c < 0x20) return;      /* other control codes */
        if (line_len >= CONSOLE_LINE_MAX - 2) {
            /*  Buffer full. Refuse further input rather than silently
             *  truncating in the middle -- and beep, in the sense that we
             *  would if we drove the PC speaker. */
            return;
        }
        line[line_len++] = c;
        if (echo) console_putc(c);
        break;
    }
}

/* ---------------------------------------------------------------------------
 *  console_read_line -- called from task context, blocks
 * ------------------------------------------------------------------------- */
ssize_t console_read_line(char *buf, size_t max)
{
    /*  Wait for a complete line. The flag is set by an interrupt handler, so
     *  it must be re-checked with interrupts disabled: the classic lost-wakeup
     *  race is (1) we test the flag and find it false, (2) the interrupt fires
     *  and sets it and calls wake, (3) we go to sleep and nobody is left to
     *  wake us. Disabling interrupts across the test-and-sleep closes it.
     *
     *  sched_block() re-enables interrupts as part of switching away, so the
     *  window really is closed rather than merely narrowed.                   */
    for (;;) {
        uint32_t flags = irq_save();
        if (line_ready) { irq_restore(flags); break; }
        irq_restore(flags);

        sched_block(&console_wait_channel);
    }

    uint32_t flags = irq_save();

    size_t n = line_len;
    if (n > max - 1) n = max - 1;
    memcpy(buf, line, n);
    buf[n] = '\0';

    line_len   = 0;
    line_ready = false;

    irq_restore(flags);
    return (ssize_t)n;
}

/* ---------------------------------------------------------------------------
 *  /dev/console as a VFS node
 *
 *  Once this exists, the shell does not need a special case: its stdin is fd
 *  0, fd 0 is an open file, the open file points at this node, and read() on
 *  it calls console_node_read. Exactly the same code path as reading a file
 *  off the disk. That uniformity is the entire argument for a VFS.
 * ------------------------------------------------------------------------- */
static ssize_t console_node_read(vfs_node_t *node UNUSED, off_t offset UNUSED,
                                 size_t size, uint8_t *buf)
{
    return console_read_line((char *)buf, size);
}

static ssize_t console_node_write(vfs_node_t *node UNUSED, off_t offset UNUSED,
                                  size_t size, const uint8_t *buf)
{
    console_write((const char *)buf, size);
    return (ssize_t)size;
}

static vfs_node_t *console_node = NULL;

vfs_node_t *console_device_node(void)
{
    return console_node;
}

void console_init(void)
{
    line_len   = 0;
    line_ready = false;
    echo       = true;

    console_node = (vfs_node_t *)kcalloc(1, sizeof(vfs_node_t));
    if (!console_node) panic("console: out of memory creating /dev/console");

    strlcpy(console_node->name, "console", VFS_NAME_MAX);
    console_node->flags = VFS_CHARDEVICE;
    console_node->read  = console_node_read;
    console_node->write = console_node_write;
    console_node->refcount = 1;

    LOG_INFO("console: line discipline ready");
}
