/* ===========================================================================
 *  nimbus/lib/string.c
 * ===========================================================================
 *  Compiled into both the kernel and the user C library. Nothing in here
 *  touches hardware, allocates, or calls anything outside this file -- which
 *  is exactly what makes it shareable.
 * =========================================================================== */

#include <nimbus/string.h>

/* ---------------------------------------------------------------------------
 *  memcpy
 *
 *  The dword loop is not premature optimisation: memcpy is called on every
 *  fork, every ELF segment load and every disk block, and a byte loop is four
 *  times slower for no readability gain. The tail handles the 0-3 bytes a
 *  dword loop cannot.
 *
 *  What it does *not* do is handle overlap. C says the behaviour is undefined
 *  and we take the language at its word; if you need overlap, memmove exists
 *  four lines down and knows the difference.
 * ------------------------------------------------------------------------- */
void *memcpy(void *dst, const void *src, size_t n)
{
    uint8_t       *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;

    /* Only use the wide path when both pointers have the same alignment;
     * otherwise every dword access is a misaligned one, which on x86 is legal
     * but slower than the byte loop we were trying to beat. */
    if (((uintptr_t)d & 3) == ((uintptr_t)s & 3)) {
        while (n && ((uintptr_t)d & 3)) { *d++ = *s++; n--; }

        uint32_t       *d32 = (uint32_t *)d;
        const uint32_t *s32 = (const uint32_t *)s;
        while (n >= 4) { *d32++ = *s32++; n -= 4; }

        d = (uint8_t *)d32;
        s = (const uint8_t *)s32;
    }

    while (n--) *d++ = *s++;
    return dst;
}

/* ---------------------------------------------------------------------------
 *  memmove -- memcpy that survives overlap
 *
 *  If the destination is above the source and the regions overlap, copying
 *  forwards overwrites bytes we have not read yet. Copying backwards fixes
 *  that case and breaks the other one, so we pick a direction.
 * ------------------------------------------------------------------------- */
void *memmove(void *dst, const void *src, size_t n)
{
    uint8_t       *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;

    if (d == s || n == 0) return dst;

    if (d < s) {
        while (n--) *d++ = *s++;
    } else {
        d += n;
        s += n;
        while (n--) *--d = *--s;
    }
    return dst;
}

void *memset(void *dst, int c, size_t n)
{
    uint8_t *d = (uint8_t *)dst;
    uint8_t  b = (uint8_t)c;

    /* Splat the byte across a dword once, then store 4 bytes at a time. */
    uint32_t pattern = ((uint32_t)b << 24) | ((uint32_t)b << 16) |
                       ((uint32_t)b << 8)  |  (uint32_t)b;

    while (n && ((uintptr_t)d & 3)) { *d++ = b; n--; }

    uint32_t *d32 = (uint32_t *)d;
    while (n >= 4) { *d32++ = pattern; n -= 4; }

    d = (uint8_t *)d32;
    while (n--) *d++ = b;
    return dst;
}

int memcmp(const void *a, const void *b, size_t n)
{
    const uint8_t *x = (const uint8_t *)a;
    const uint8_t *y = (const uint8_t *)b;

    while (n--) {
        if (*x != *y)
            return (int)*x - (int)*y;   /* unsigned compare, signed result */
        x++; y++;
    }
    return 0;
}

void *memchr(const void *s, int c, size_t n)
{
    const uint8_t *p = (const uint8_t *)s;
    while (n--) {
        if (*p == (uint8_t)c) return (void *)p;
        p++;
    }
    return NULL;
}

size_t strlen(const char *s)
{
    const char *p = s;
    while (*p) p++;
    return (size_t)(p - s);
}

size_t strnlen(const char *s, size_t max)
{
    size_t n = 0;
    while (n < max && s[n]) n++;
    return n;
}

int strcmp(const char *a, const char *b)
{
    /* The casts to unsigned char matter. `char` is signed on x86, so a byte
     * 0x80 compares as -128, and "\x80" would sort *before* "a". Every real
     * libc does this; every hand-written strcmp forgets it. */
    while (*a && (*a == *b)) { a++; b++; }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

int strncmp(const char *a, const char *b, size_t n)
{
    while (n && *a && (*a == *b)) { a++; b++; n--; }
    if (n == 0) return 0;
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

char *strcpy(char *dst, const char *src)
{
    char *d = dst;
    while ((*d++ = *src++)) { }
    return dst;
}

/*  strncpy is a trap and we implement it only because code expects it: if src
 *  is exactly n characters, the result is NOT terminated. It also pads with
 *  zeros to the full n, which is O(n) even for a 3-byte name in a 256-byte
 *  buffer. Use strlcpy.                                                       */
char *strncpy(char *dst, const char *src, size_t n)
{
    size_t i = 0;
    for (; i < n && src[i]; i++) dst[i] = src[i];
    for (; i < n; i++) dst[i] = '\0';
    return dst;
}

/*  strlcpy: always terminates, returns the length it *wanted*, so that
 *  truncation is detectable with `if (strlcpy(...) >= size)`. From OpenBSD,
 *  and the right default.                                                     */
size_t strlcpy(char *dst, const char *src, size_t size)
{
    size_t srclen = strlen(src);

    if (size) {
        size_t copy = (srclen >= size) ? size - 1 : srclen;
        memcpy(dst, src, copy);
        dst[copy] = '\0';
    }
    return srclen;
}

char *strcat(char *dst, const char *src)
{
    char *d = dst;
    while (*d) d++;
    while ((*d++ = *src++)) { }
    return dst;
}

char *strchr(const char *s, int c)
{
    /* The NUL is part of the string as far as strchr is concerned:
     * strchr(s, '\0') legitimately returns a pointer to the terminator, which
     * is why the test is after the loop body and not before it. */
    for (;; s++) {
        if (*s == (char)c) return (char *)s;
        if (!*s) return NULL;
    }
}

char *strrchr(const char *s, int c)
{
    const char *last = NULL;
    for (;; s++) {
        if (*s == (char)c) last = s;
        if (!*s) return (char *)last;
    }
}

char *strstr(const char *haystack, const char *needle)
{
    if (!*needle) return (char *)haystack;

    /* Naive O(n*m). For the string lengths a shell deals with, the constant
     * factor of anything cleverer costs more than it saves. */
    for (; *haystack; haystack++) {
        const char *h = haystack, *n = needle;
        while (*h && *n && *h == *n) { h++; n++; }
        if (!*n) return (char *)haystack;
    }
    return NULL;
}

int toupper(int c) { return (c >= 'a' && c <= 'z') ? c - 'a' + 'A' : c; }
int tolower(int c) { return (c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c; }

bool isdigit(int c) { return c >= '0' && c <= '9'; }
bool isspace(int c) { return c == ' ' || c == '\t' || c == '\n' ||
                             c == '\r' || c == '\v' || c == '\f'; }
bool isprint(int c) { return c >= 0x20 && c < 0x7F; }

int strcasecmp(const char *a, const char *b)
{
    while (*a && tolower((unsigned char)*a) == tolower((unsigned char)*b)) { a++; b++; }
    return tolower((unsigned char)*a) - tolower((unsigned char)*b);
}

int atoi(const char *s)
{
    int sign = 1;
    int value = 0;

    while (isspace((unsigned char)*s)) s++;

    if (*s == '-') { sign = -1; s++; }
    else if (*s == '+') { s++; }

    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        s += 2;
        for (;;) {
            int digit;
            if (isdigit((unsigned char)*s))               digit = *s - '0';
            else if (*s >= 'a' && *s <= 'f')              digit = *s - 'a' + 10;
            else if (*s >= 'A' && *s <= 'F')              digit = *s - 'A' + 10;
            else break;
            value = value * 16 + digit;
            s++;
        }
    } else {
        while (isdigit((unsigned char)*s))
            value = value * 10 + (*s++ - '0');
    }
    return value * sign;
}
