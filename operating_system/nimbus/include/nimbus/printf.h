/* ===========================================================================
 *  nimbus/include/nimbus/printf.h  --  the formatter's interface
 * ===========================================================================
 *  Shared by the kernel and by the userland C library. The kernel's kprintf
 *  and userland's printf are both four lines wrapped around fmt_vformat().
 * =========================================================================== */
#ifndef NIMBUS_PRINTF_H
#define NIMBUS_PRINTF_H

#include <nimbus/types.h>

/*  Called once per output character. `ctx` is whatever the caller passed, and
 *  the formatter never looks inside it.                                       */
typedef void (*fmt_out_fn)(void *ctx, char c);

/*  Returns the number of characters produced -- which, for a sink that drops
 *  output past a limit, is the number it *would* have produced.               */
int fmt_vformat(fmt_out_fn out, void *ctx, const char *fmt, va_list ap);

int vsnprintf(char *buf, size_t size, const char *fmt, va_list ap);
int snprintf(char *buf, size_t size, const char *fmt, ...)
        __attribute__((format(printf, 3, 4)));

#endif /* NIMBUS_PRINTF_H */
