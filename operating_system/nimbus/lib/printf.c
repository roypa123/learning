/* ===========================================================================
 *  nimbus/lib/printf.c  --  the formatter, shared by the kernel and userland
 * ===========================================================================
 *
 *  printf is the single most useful function in a kernel and the one you miss
 *  most in the first hour without it. It is also completely self-contained:
 *  parse a format string, convert some integers to digits, emit characters.
 *  No allocation, no floating point (we deliberately omit %f -- see below), no
 *  dependencies beyond string.c.
 *
 *  The design point worth copying is the output callback. The formatter never
 *  writes to a buffer; it calls `out(ctx, c)` for each character. That one
 *  indirection gives us snprintf (ctx = a buffer), kprintf (ctx = the console
 *  and the serial port at once) and userland printf (ctx = a write() syscall)
 *  from a single implementation, with no truncation-then-copy in the middle.
 *
 *  Explained in: docs/14-printf.md
 * =========================================================================== */

#include <nimbus/types.h>
#include <nimbus/string.h>
#include <nimbus/printf.h>

/* ---- flags ---------------------------------------------------------------- */
#define FLAG_LEFT   0x01   /* '-'  left align within the field                */
#define FLAG_ZERO   0x02   /* '0'  pad with zeros instead of spaces           */
#define FLAG_PLUS   0x04   /* '+'  always show a sign on signed conversions   */
#define FLAG_SPACE  0x08   /* ' '  a space where the '+' would be             */
#define FLAG_ALT    0x10   /* '#'  0x prefix on %x, 0 prefix on %o            */
#define FLAG_UPPER  0x20   /* set by %X, not by a flag character              */

typedef struct {
    fmt_out_fn out;
    void      *ctx;
    int        count;      /* how many characters we have emitted             */
} fmt_state_t;

static void emit(fmt_state_t *st, char c)
{
    st->out(st->ctx, c);
    st->count++;
}

static void emit_pad(fmt_state_t *st, char c, int n)
{
    while (n-- > 0) emit(st, c);
}

/* ---------------------------------------------------------------------------
 *  Integer to digits
 *
 *  Division generates digits least-significant first, so we fill a buffer
 *  backwards and return a pointer into it. 32 characters is enough for the
 *  longest thing we can produce: a 64-bit value in binary is 64 digits, so the
 *  buffer is sized for that in the one place %b can reach it.
 * ------------------------------------------------------------------------- */
static int utoa(uint64_t value, unsigned base, bool upper, char *buf, int bufsize)
{
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    int i = bufsize;

    if (value == 0) {
        buf[--i] = '0';
        return i;
    }
    while (value && i > 0) {
        buf[--i] = digits[value % base];
        value /= base;
    }
    return i;
}

/* ---------------------------------------------------------------------------
 *  The one conversion routine every numeric specifier funnels into.
 * ------------------------------------------------------------------------- */
static void format_number(fmt_state_t *st, uint64_t value, unsigned base,
                          bool negative, int width, int precision, unsigned flags)
{
    char digits[72];
    int  start = utoa(value, base, (flags & FLAG_UPPER) != 0, digits, (int)sizeof(digits));
    int  ndigits = (int)sizeof(digits) - start;

    /* "%.0d" of zero prints nothing at all. An obscure rule, and the reason
     * printf("%.*d", 0, 0) is the canonical way to print an empty field. */
    if (precision == 0 && value == 0) ndigits = 0;

    char sign = 0;
    if (negative)             sign = '-';
    else if (flags & FLAG_PLUS)  sign = '+';
    else if (flags & FLAG_SPACE) sign = ' ';

    const char *prefix = "";
    if (flags & FLAG_ALT) {
        if (base == 16) prefix = (flags & FLAG_UPPER) ? "0X" : "0x";
        else if (base == 8 && value != 0) prefix = "0";
        else if (base == 2) prefix = "0b";
    }
    int prefix_len = (int)strlen(prefix);

    /* Zeros demanded by an explicit precision, e.g. "%.8x". */
    int zeros = (precision > ndigits) ? precision - ndigits : 0;

    int body = (sign ? 1 : 0) + prefix_len + zeros + ndigits;
    int pad  = (width > body) ? width - body : 0;

    /* Zero padding goes *after* the sign and the 0x, space padding before.
     * "%08x" of 255 is 000000ff; "%8x" is "      ff". Getting this backwards
     * produces "0x000000ff" printed as "00000x ff", which is the kind of thing
     * you only notice at 2 a.m. An explicit precision cancels FLAG_ZERO,
     * because the standard says so. */
    bool zero_pad = (flags & FLAG_ZERO) && !(flags & FLAG_LEFT) && precision < 0;

    if (!zero_pad && !(flags & FLAG_LEFT)) emit_pad(st, ' ', pad);

    if (sign) emit(st, sign);
    for (int i = 0; i < prefix_len; i++) emit(st, prefix[i]);

    if (zero_pad) emit_pad(st, '0', pad);
    emit_pad(st, '0', zeros);

    for (int i = 0; i < ndigits; i++) emit(st, digits[start + i]);

    if (flags & FLAG_LEFT) emit_pad(st, ' ', pad);
}

/* ---------------------------------------------------------------------------
 *  format -- the state machine
 * ------------------------------------------------------------------------- */
int fmt_vformat(fmt_out_fn out, void *ctx, const char *fmt, va_list ap)
{
    fmt_state_t st = { out, ctx, 0 };

    while (*fmt) {
        if (*fmt != '%') { emit(&st, *fmt++); continue; }
        fmt++;

        /* ---- flags ------------------------------------------------------- */
        unsigned flags = 0;
        for (;;) {
            if      (*fmt == '-') flags |= FLAG_LEFT;
            else if (*fmt == '0') flags |= FLAG_ZERO;
            else if (*fmt == '+') flags |= FLAG_PLUS;
            else if (*fmt == ' ') flags |= FLAG_SPACE;
            else if (*fmt == '#') flags |= FLAG_ALT;
            else break;
            fmt++;
        }

        /* ---- width ------------------------------------------------------- */
        int width = 0;
        if (*fmt == '*') {
            width = va_arg(ap, int);
            if (width < 0) { flags |= FLAG_LEFT; width = -width; }
            fmt++;
        } else {
            while (isdigit((unsigned char)*fmt)) width = width * 10 + (*fmt++ - '0');
        }

        /* ---- precision --------------------------------------------------- */
        int precision = -1;                       /* -1 means "not specified" */
        if (*fmt == '.') {
            fmt++;
            precision = 0;
            if (*fmt == '*') { precision = va_arg(ap, int); fmt++; }
            else while (isdigit((unsigned char)*fmt)) precision = precision * 10 + (*fmt++ - '0');
            if (precision < 0) precision = -1;
        }

        /* ---- length modifier ---------------------------------------------
         * On 32-bit x86, int and long are both 32 bits, so `l` changes nothing
         * and we accept it purely so that code written for a 64-bit host
         * compiles unchanged. `ll` is real: it means the argument occupies two
         * stack slots and reading it as 32 bits would leave the other half in
         * place for the *next* conversion to pick up. */
        int longness = 0;
        while (*fmt == 'l') { longness++; fmt++; }
        if (*fmt == 'h') { fmt++; if (*fmt == 'h') fmt++; }   /* no-op: promoted */
        if (*fmt == 'z') { fmt++; }

        char conv = *fmt++;

        switch (conv) {
        case 'd':
        case 'i': {
            int64_t v = (longness >= 2) ? va_arg(ap, int64_t) : (int64_t)va_arg(ap, int32_t);
            bool neg = v < 0;
            /* Negating INT64_MIN overflows, so convert through unsigned. The
             * two's-complement trick `(uint64_t)-(v+1) + 1` is well defined
             * for every input, including the one that bites naive code. */
            uint64_t mag = neg ? ((uint64_t)(-(v + 1)) + 1) : (uint64_t)v;
            format_number(&st, mag, 10, neg, width, precision, flags);
            break;
        }
        case 'u': {
            uint64_t v = (longness >= 2) ? va_arg(ap, uint64_t) : (uint64_t)va_arg(ap, uint32_t);
            format_number(&st, v, 10, false, width, precision, flags);
            break;
        }
        case 'X': flags |= FLAG_UPPER;
            /* fall through */
        case 'x': {
            uint64_t v = (longness >= 2) ? va_arg(ap, uint64_t) : (uint64_t)va_arg(ap, uint32_t);
            format_number(&st, v, 16, false, width, precision, flags);
            break;
        }
        case 'o': {
            uint64_t v = (longness >= 2) ? va_arg(ap, uint64_t) : (uint64_t)va_arg(ap, uint32_t);
            format_number(&st, v, 8, false, width, precision, flags);
            break;
        }
        case 'b': {   /* not standard C; invaluable when you are staring at
                       * a descriptor or a page table entry */
            uint64_t v = (longness >= 2) ? va_arg(ap, uint64_t) : (uint64_t)va_arg(ap, uint32_t);
            format_number(&st, v, 2, false, width, precision, flags);
            break;
        }
        case 'p': {
            /* Always 0x + 8 digits. A pointer column that changes width is a
             * pointer column you cannot scan. */
            uintptr_t v = (uintptr_t)va_arg(ap, void *);
            format_number(&st, (uint64_t)v, 16, false, 8, 8, FLAG_ZERO | FLAG_ALT);
            break;
        }
        case 'c': {
            char c = (char)va_arg(ap, int);   /* char is promoted to int */
            int pad = width > 1 ? width - 1 : 0;
            if (!(flags & FLAG_LEFT)) emit_pad(&st, ' ', pad);
            emit(&st, c);
            if (flags & FLAG_LEFT) emit_pad(&st, ' ', pad);
            break;
        }
        case 's': {
            const char *s = va_arg(ap, const char *);
            /* A null pointer here is a bug, but printing "(null)" instead of
             * page-faulting inside the logger has saved more debugging
             * sessions than it has hidden bugs. */
            if (!s) s = "(null)";
            int len = (precision >= 0) ? (int)strnlen(s, (size_t)precision) : (int)strlen(s);
            int pad = width > len ? width - len : 0;
            if (!(flags & FLAG_LEFT)) emit_pad(&st, ' ', pad);
            for (int i = 0; i < len; i++) emit(&st, s[i]);
            if (flags & FLAG_LEFT) emit_pad(&st, ' ', pad);
            break;
        }
        case '%':
            emit(&st, '%');
            break;
        case '\0':
            /* A trailing '%' with nothing after it. Stop rather than run off
             * the end of the string. */
            return st.count;
        default:
            /* An unknown conversion. Echo it verbatim so the mistake is
             * visible in the output instead of silently swallowing an
             * argument and desynchronising every conversion after it. */
            emit(&st, '%');
            emit(&st, conv);
            break;
        }
    }
    return st.count;
}

/* ---------------------------------------------------------------------------
 *  snprintf, built on the callback
 * ------------------------------------------------------------------------- */
typedef struct {
    char  *buf;
    size_t size;      /* total capacity including the terminator */
    size_t pos;
} buf_sink_t;

static void buf_putc(void *ctx, char c)
{
    buf_sink_t *s = (buf_sink_t *)ctx;
    /* Keep counting past the end: snprintf must return the length it *would*
     * have produced, which is how callers detect truncation and how the
     * "measure then allocate" idiom works. */
    if (s->pos + 1 < s->size)
        s->buf[s->pos] = c;
    s->pos++;
}

int vsnprintf(char *buf, size_t size, const char *fmt, va_list ap)
{
    buf_sink_t sink = { buf, size, 0 };
    int n = fmt_vformat(buf_putc, &sink, fmt, ap);

    if (size > 0)
        buf[(sink.pos < size - 1) ? sink.pos : size - 1] = '\0';
    return n;
}

int snprintf(char *buf, size_t size, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, size, fmt, ap);
    va_end(ap);
    return n;
}
