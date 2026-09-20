/* ===========================================================================
 *  nimbus/user/libc.h  --  the C library, from the other side of the wall
 * ===========================================================================
 *
 *  Everything here is a system call, or built out of system calls. There is no
 *  magic in a C library: printf is vsnprintf plus write(), malloc is a free
 *  list plus sbrk(), and fopen is open() plus a buffer.
 *
 *  Explained in: docs/45-user-libc.md
 * =========================================================================== */
#ifndef NIMBUS_LIBC_H
#define NIMBUS_LIBC_H

#include <nimbus/types.h>
#include <nimbus/string.h>
#include <nimbus/printf.h>
#include <nimbus/syscall.h>
#include <nimbus/vfs.h>     /* dirent_t and stat_t: the shapes the kernel returns */

#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

/* ---- the raw system calls ------------------------------------------------- */
void     exit(int status) NORETURN;
ssize_t  write(int fd, const void *buf, size_t count);
ssize_t  read(int fd, void *buf, size_t count);
int      open(const char *path, int flags);
int      close(int fd);
pid_t    fork(void);
int      execv(const char *path, char *const argv[]);
pid_t    wait(int *status);
pid_t    getpid(void);
void    *sbrk(int increment);
int      sleep_ms(unsigned ms);
int      yield(void);
int      lseek(int fd, int offset, int whence);
int      readdir_fd(int fd, dirent_t *out, unsigned index);
int      stat(const char *path, stat_t *out);
int      pipe(int fds[2]);
int      dup2(int oldfd, int newfd);
int      chdir(const char *path);
int      unlink(const char *path);
unsigned uptime_ms(void);
int      ps(void);
int      reboot(void);

/* ---- built on top --------------------------------------------------------- */
int      printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
int      fprintf(int fd, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
int      puts(const char *s);
int      putchar(int c);

/*  Read one line from stdin into buf, without the trailing newline. Returns
 *  the length, or -1 at end of file.                                          */
int      getline(char *buf, size_t max);

void    *malloc(size_t size);
void    *calloc(size_t count, size_t size);
void     free(void *ptr);
void    *realloc(void *ptr, size_t size);

/*  Run a program and wait for it. The three-line idiom every shell is built
 *  on, wrapped up because it appears in every one of our utilities.           */
int      run(const char *path, char *const argv[]);

#endif /* NIMBUS_LIBC_H */
