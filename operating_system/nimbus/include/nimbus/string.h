/* ===========================================================================
 *  nimbus/include/nimbus/string.h  --  the parts of <string.h> we need
 * ===========================================================================
 *
 *  GCC assumes these exist even with -ffreestanding. It will happily turn a
 *  struct assignment into a call to memcpy(), or a loop that fills an array
 *  into a call to memset(), and then the link fails with "undefined reference
 *  to memcpy" pointing at a line of code containing no call at all. So four of
 *  these functions -- memcpy, memset, memmove, memcmp -- are not optional even
 *  if we never call them ourselves.
 * =========================================================================== */
#ifndef NIMBUS_STRING_H
#define NIMBUS_STRING_H

#include <nimbus/types.h>

void   *memcpy(void *dst, const void *src, size_t n);
void   *memmove(void *dst, const void *src, size_t n);
void   *memset(void *dst, int c, size_t n);
int     memcmp(const void *a, const void *b, size_t n);
void   *memchr(const void *s, int c, size_t n);

size_t  strlen(const char *s);
size_t  strnlen(const char *s, size_t max);
int     strcmp(const char *a, const char *b);
int     strncmp(const char *a, const char *b, size_t n);
char   *strcpy(char *dst, const char *src);
char   *strncpy(char *dst, const char *src, size_t n);
size_t  strlcpy(char *dst, const char *src, size_t size);
char   *strcat(char *dst, const char *src);
char   *strchr(const char *s, int c);
char   *strrchr(const char *s, int c);
char   *strstr(const char *haystack, const char *needle);

/*  Case-insensitive compare, needed by FAT16: the filesystem upper-cases every
 *  name on disk, so "README.TXT" and "readme.txt" must match.                 */
int     strcasecmp(const char *a, const char *b);
int     toupper(int c);
int     tolower(int c);
bool    isdigit(int c);
bool    isspace(int c);
bool    isprint(int c);

/*  Parse a decimal or 0x-prefixed hex integer. Returns 0 on garbage, like the
 *  C library's atoi -- which is a bad interface, but the shell's argument
 *  handling does not need better and a kernel is not the place to invent a
 *  new error convention.                                                       */
int     atoi(const char *s);

#endif /* NIMBUS_STRING_H */
