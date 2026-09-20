/* ===========================================================================
 *  nimbus/include/nimbus/serial.h  --  COM1, the debugger you always have
 * ===========================================================================
 *
 *  A UART is the simplest useful device on a PC: eight registers, no DMA, no
 *  interrupts required, and QEMU will pipe it straight to a file or to the
 *  terminal you launched from with `-serial stdio`. It works before paging,
 *  before interrupts, before the console, and it keeps working while the
 *  screen is full of a panic message.
 *
 *  Explained in: docs/13-serial-port.md
 * =========================================================================== */
#ifndef NIMBUS_SERIAL_H
#define NIMBUS_SERIAL_H

#include <nimbus/types.h>

#define COM1 0x3F8
#define COM2 0x2F8
#define COM3 0x3E8
#define COM4 0x2E8

/*  Register offsets from the base port. Two of them change meaning depending
 *  on bit 7 of the Line Control Register (the "divisor latch access bit"),
 *  which is the single most confusing thing about the 16550 and the reason
 *  serial_init() looks like it writes to the same port twice for no reason.   */
#define UART_DATA        0   /* R: receive buffer   W: transmit buffer   (DLAB=0) */
#define UART_IER         1   /* interrupt enable                        (DLAB=0) */
#define UART_DIVISOR_LO  0   /* baud divisor low byte                   (DLAB=1) */
#define UART_DIVISOR_HI  1   /* baud divisor high byte                  (DLAB=1) */
#define UART_FCR         2   /* W: FIFO control     R: interrupt identification  */
#define UART_LCR         3   /* line control: word length, parity, stop, DLAB    */
#define UART_MCR         4   /* modem control: DTR, RTS, loopback                */
#define UART_LSR         5   /* line status: is the transmitter empty?           */
#define UART_MSR         6   /* modem status                                     */
#define UART_SCRATCH     7   /* one byte of scratch RAM -- used to probe presence */

bool serial_init(uint16_t port);
void serial_putc(char c);
void serial_write(const char *s, size_t len);
void serial_puts(const char *s);
int  serial_getc_nonblock(void);   /* -1 if nothing waiting */
bool serial_present(void);

#endif /* NIMBUS_SERIAL_H */
