/* ===========================================================================
 *  nimbus/drivers/serial.c  --  the 16550 UART
 * ===========================================================================
 *
 *  Why a kernel writes a serial driver on day one, in 2026, for a port no
 *  laptop has had since 2008:
 *
 *    * It works before everything else. No paging, no interrupts, no memory
 *      allocator, no console. You can call serial_putc() from the third
 *      instruction of the kernel.
 *    * It survives. When the machine triple-faults and reboots, the screen is
 *      gone but the log file on the host still has every byte you sent.
 *    * It is a file. `-serial file:serial.log` makes the kernel's output
 *      greppable, diffable and attachable to a bug report.
 *    * It is bidirectional, which means a debug shell over a wire, on a
 *      machine whose keyboard driver is the thing you are debugging.
 *
 *  Explained in: docs/13-serial-port.md
 * =========================================================================== */

#include <nimbus/serial.h>
#include <nimbus/io.h>
#include <nimbus/string.h>

static uint16_t serial_port = 0;
static bool     serial_ok   = false;

/* ---------------------------------------------------------------------------
 *  serial_init
 *
 *  Nine port writes, in an order that matters, and then a self-test.
 * ------------------------------------------------------------------------- */
bool serial_init(uint16_t port)
{
    serial_port = port;
    serial_ok   = false;

    /*  1. Disable interrupts from the UART while we reconfigure it. If a
     *     character arrives mid-setup and IRQ 4 fires into an IDT that does
     *     not exist yet, the machine triple-faults during boot. */
    outb(port + UART_IER, 0x00);

    /*  2. Set the divisor latch access bit, which re-points the first two
     *     registers at the baud rate divisor. This is why the next two writes
     *     go to offsets 0 and 1, which are normally data and interrupt-enable. */
    outb(port + UART_LCR, 0x80);

    /*  3. The divisor. The UART's clock is 115200 Hz, and the baud rate is
     *     115200 / divisor. Divisor 1 gives 115200 baud, which is what QEMU
     *     and every terminal program default to. (Divisor 12 would be 9600,
     *     the speed of every modem in 1992.) */
    outb(port + UART_DIVISOR_LO, 0x01);
    outb(port + UART_DIVISOR_HI, 0x00);

    /*  4. Clear DLAB and set the line format: 8 data bits, no parity, 1 stop
     *     bit. "8N1" -- the three numbers every terminal program asks for. */
    outb(port + UART_LCR, 0x03);

    /*  5. Enable and clear the 16-byte FIFOs, with an interrupt trigger level
     *     of 14 bytes. The FIFO is what makes a 16550 a 16550 rather than an
     *     8250: without it the CPU must collect every byte before the next
     *     arrives or the byte is lost. */
    outb(port + UART_FCR, 0xC7);

    /*  6. Modem control: DTR and RTS asserted (tell the other end we are here
     *     and ready), plus OUT2, which on a PC is wired to the interrupt
     *     enable gate -- without it, IRQ 4 never reaches the PIC no matter
     *     what the IER says. This is a PC wiring quirk, not a UART feature,
     *     and it is documented in exactly one sentence of the IBM technical
     *     reference. */
    outb(port + UART_MCR, 0x0B);

    /*  7. Self-test. Put the UART in loopback mode, where its transmitter is
     *     wired to its own receiver, send a byte and see whether it comes
     *     back. If the port does not exist, reads return 0xFF and the test
     *     fails cleanly instead of hanging forever in serial_putc waiting for
     *     a transmitter that will never be empty. */
    outb(port + UART_MCR, 0x1E);          /* loopback on */
    outb(port + UART_DATA, 0xAE);

    if (inb(port + UART_DATA) != 0xAE) {
        serial_port = 0;
        return false;
    }

    /*  8. Out of loopback, into normal operation. */
    outb(port + UART_MCR, 0x0F);

    serial_ok = true;
    return true;
}

bool serial_present(void) { return serial_ok; }

/* ---------------------------------------------------------------------------
 *  Transmitting
 *
 *  Bit 5 of the line status register is THRE -- Transmitter Holding Register
 *  Empty. Spinning on it is a busy wait, and at 115200 baud a byte takes
 *  87 microseconds, which is an eternity. But the alternative -- an interrupt
 *  driven transmit queue -- cannot be used from panic(), from an interrupt
 *  handler, or before the scheduler exists, which are the three places we most
 *  need the log to work. So we spin, and accept that heavy logging slows the
 *  kernel down. That is a feature the first time a race condition disappears
 *  when you add a printf.
 * ------------------------------------------------------------------------- */
static inline void serial_wait_tx(void)
{
    /* A bounded wait. An unbounded one turns a missing UART into a hang, and a
     * hang during boot with no output is the worst failure mode there is. */
    for (int spins = 0; spins < 100000; spins++)
        if (inb(serial_port + UART_LSR) & 0x20)
            return;
}

void serial_putc(char c)
{
    if (!serial_ok) return;

    /* Translate bare newlines into CRLF. A terminal emulator moves the cursor
     * down on \n but does not return it to column 0 -- that is what \r is for
     * -- so a log written with bare newlines comes out as a diagonal
     * staircase. Every serial driver does this and none of them mention it. */
    if (c == '\n') {
        serial_wait_tx();
        outb(serial_port + UART_DATA, '\r');
    }

    serial_wait_tx();
    outb(serial_port + UART_DATA, (uint8_t)c);
}

void serial_write(const char *s, size_t len)
{
    for (size_t i = 0; i < len; i++) serial_putc(s[i]);
}

void serial_puts(const char *s)
{
    while (*s) serial_putc(*s++);
}

/* ---------------------------------------------------------------------------
 *  Receiving
 *
 *  Bit 0 of the line status register is "data ready". Polling it is fine for
 *  our purposes; an interrupt-driven version is a twenty-line exercise at the
 *  end of Chapter 13 once IRQ 4 is wired up.
 * ------------------------------------------------------------------------- */
int serial_getc_nonblock(void)
{
    if (!serial_ok) return -1;
    if (!(inb(serial_port + UART_LSR) & 0x01)) return -1;
    return (int)inb(serial_port + UART_DATA);
}
