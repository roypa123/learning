/* ===========================================================================
 *  nimbus/include/nimbus/syscall.h  --  the kernel's public interface
 * ===========================================================================
 *
 *  This is the only header shared verbatim between the kernel and userland,
 *  because it is the only thing the two agree on. Everything else in
 *  include/nimbus/ is kernel-private; everything in user/libc.h is
 *  user-private; these numbers are the contract in the middle.
 *
 *  Explained in: docs/33-syscalls.md
 * =========================================================================== */
#ifndef NIMBUS_SYSCALL_H
#define NIMBUS_SYSCALL_H

/* ---------------------------------------------------------------------------
 *  Call numbers
 *
 *  Once a number is published it can never be reused for anything else: an old
 *  binary will keep calling it. Linux still has a `stat` at number 106 that
 *  nothing has called since 1999. We are not shipping to anyone, but the habit
 *  is worth forming, so numbers here are append-only and gaps stay gaps.
 * ------------------------------------------------------------------------- */
#define SYS_EXIT        0
#define SYS_WRITE       1
#define SYS_READ        2
#define SYS_OPEN        3
#define SYS_CLOSE       4
#define SYS_FORK        5
#define SYS_EXEC        6
#define SYS_WAIT        7
#define SYS_GETPID      8
#define SYS_SBRK        9
#define SYS_SLEEP       10
#define SYS_YIELD       11
#define SYS_LSEEK       12
#define SYS_READDIR     13
#define SYS_STAT        14
#define SYS_PIPE        15
#define SYS_DUP2        16
#define SYS_CHDIR       17
#define SYS_GETCWD      18
#define SYS_UNLINK      19
#define SYS_MKDIR       20
#define SYS_UPTIME      21
#define SYS_PS          22       /* not POSIX; a teaching convenience         */
#define SYS_REBOOT      23
#define SYS_MAX         24

/* ---------------------------------------------------------------------------
 *  The calling convention
 *
 *      eax = call number
 *      ebx, ecx, edx, esi, edi = arguments 1..5
 *      return value in eax; negative values are -errno
 *
 *  This is Linux's 32-bit convention, chosen for the same reason Linux chose
 *  it: those are the registers `int 0x80` does not already have a use for, and
 *  passing in registers avoids the kernel having to read the user stack --
 *  which it would have to validate, page by page, before touching.
 *
 *  Errors come back as small negative numbers rather than through a separate
 *  channel because there is no separate channel: one register comes back, and
 *  no syscall of ours legitimately returns a value in [-4095, -1].
 * ------------------------------------------------------------------------- */

/* ---------------------------------------------------------------------------
 *  Error numbers
 * ------------------------------------------------------------------------- */
#define EPERM        1   /* operation not permitted                           */
#define ENOENT       2   /* no such file or directory                         */
#define ESRCH        3   /* no such process                                   */
#define EINTR        4   /* interrupted                                       */
#define EIO          5   /* I/O error                                         */
#define E2BIG        7   /* argument list too long                            */
#define ENOEXEC      8   /* not an executable                                 */
#define EBADF        9   /* bad file descriptor                               */
#define ECHILD      10   /* no child processes                                */
#define EAGAIN      11   /* try again                                         */
#define ENOMEM      12   /* out of memory                                     */
#define EACCES      13   /* permission denied                                 */
#define EFAULT      14   /* bad address -- a user pointer we refused          */
#define EBUSY       16
#define EEXIST      17
#define ENODEV      19
#define ENOTDIR     20
#define EISDIR      21
#define EINVAL      22   /* invalid argument                                  */
#define ENFILE      23
#define EMFILE      24   /* too many open files                               */
#define ENOSPC      28   /* no space left on device                           */
#define ESPIPE      29   /* illegal seek                                      */
#define EROFS       30
#define EPIPE       32   /* broken pipe                                       */
#define ENOSYS      38   /* function not implemented                          */
#define ENAMETOOLONG 36
#define ENOTEMPTY   39

/*  Flags for open(). Deliberately the same numbers as Linux, so that the
 *  constants look familiar and so that you can read a strace without
 *  translating.                                                              */
#define O_RDONLY    0x0000
#define O_WRONLY    0x0001
#define O_RDWR      0x0002
#define O_CREAT     0x0040
#define O_TRUNC     0x0200
#define O_APPEND    0x0400

#define SEEK_SET    0
#define SEEK_CUR    1
#define SEEK_END    2

#ifdef NIMBUS_KERNEL
#include <nimbus/isr.h>
void syscall_init(void);
void syscall_dispatch(registers_t *regs);

/*  Copying across the user/kernel boundary.
 *
 *  A pointer that arrived from ring 3 is not a pointer. It is a number chosen
 *  by code that may be actively trying to break us, and it may point at kernel
 *  memory, at an unmapped page, or at a page that another thread unmaps
 *  halfway through our copy. These four functions are the only legal way to
 *  touch user memory from the kernel, and Chapter 33 is largely about why.    */
bool copy_from_user(void *dst, const void *user_src, size_t n);
bool copy_to_user(void *user_dst, const void *src, size_t n);
bool user_string_copy(char *dst, const char *user_src, size_t max);
bool user_range_ok(const void *user_ptr, size_t n, bool need_write);
#endif

#endif /* NIMBUS_SYSCALL_H */
