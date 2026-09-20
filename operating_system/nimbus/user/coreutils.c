/* ===========================================================================
 *  nimbus/user/coreutils.c  --  ls, cat, echo, hexdump, sleep, true, false
 * ===========================================================================
 *
 *  Six programs in one file. Each is compiled separately, with -D to select
 *  which main() is built -- see the Makefile. That keeps six near-identical
 *  build rules from cluttering the tree while still producing six independent
 *  ELF binaries, which is the point: every one of them is a real process with
 *  its own address space, started by fork and exec like any other.
 *
 *  They are deliberately small and deliberately honest about it. `ls` does not
 *  sort. `cat` does not take flags. What they demonstrate is that once the
 *  kernel is right, userland is just programs.
 *
 *  Explained in: docs/46-shell.md
 * =========================================================================== */

#include "libc.h"

/* ===========================================================================
 *  ls
 * =========================================================================== */
#ifdef BUILD_LS

int main(int argc, char *argv[])
{
    const char *path = (argc > 1) ? argv[1] : "/";

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        fprintf(STDERR_FILENO, "ls: cannot open %s\n", path);
        return 1;
    }

    dirent_t entry;
    unsigned index = 0;
    int      shown = 0;

    /*  readdir returns 1 for an entry, 0 at the end. The index-based interface
     *  is ours; POSIX uses an opaque DIR* with internal state, which is nicer
     *  to use and harder to implement across a syscall boundary.             */
    while (readdir_fd(fd, &entry, index++) == 1) {
        if (entry.type & VFS_DIRECTORY)
            printf("%s/  ", entry.name);
        else
            printf("%s  ", entry.name);

        if (++shown % 5 == 0) printf("\n");
    }

    if (shown % 5 != 0) printf("\n");
    if (shown == 0)     printf("(empty)\n");

    close(fd);
    return 0;
}

#endif

/* ===========================================================================
 *  cat
 * =========================================================================== */
#ifdef BUILD_CAT

static int cat_fd(int fd)
{
    char buf[512];
    ssize_t n;

    /*  Read until read() returns 0, which means end of file. Not until it
     *  returns less than we asked for -- a short read is normal and says
     *  nothing about the end. Getting that wrong truncates output from a pipe
     *  every time, because a pipe returns whatever is buffered.               */
    while ((n = read(fd, buf, sizeof(buf))) > 0)
        write(STDOUT_FILENO, buf, (size_t)n);

    return n < 0 ? 1 : 0;
}

int main(int argc, char *argv[])
{
    /*  No arguments: read standard input. That one line is what makes `cat`
     *  usable in a pipeline, and it is the convention every Unix filter
     *  follows.                                                               */
    if (argc < 2) return cat_fd(STDIN_FILENO);

    int rc = 0;
    for (int i = 1; i < argc; i++) {
        int fd = open(argv[i], O_RDONLY);
        if (fd < 0) {
            fprintf(STDERR_FILENO, "cat: %s: no such file\n", argv[i]);
            rc = 1;
            continue;
        }
        if (cat_fd(fd)) rc = 1;
        close(fd);
    }
    return rc;
}

#endif

/* ===========================================================================
 *  echo
 * =========================================================================== */
#ifdef BUILD_ECHO

int main(int argc, char *argv[])
{
    for (int i = 1; i < argc; i++) {
        write(STDOUT_FILENO, argv[i], strlen(argv[i]));
        if (i + 1 < argc) write(STDOUT_FILENO, " ", 1);
    }
    write(STDOUT_FILENO, "\n", 1);
    return 0;
}

#endif

/* ===========================================================================
 *  hexdump
 * =========================================================================== */
#ifdef BUILD_HEXDUMP

static void dump(int fd)
{
    unsigned char buf[16];
    unsigned      offset = 0;
    ssize_t       n;

    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        printf("%08x  ", offset);

        for (int i = 0; i < 16; i++) {
            if (i < n) printf("%02x ", buf[i]);
            else       printf("   ");
            if (i == 7) printf(" ");
        }

        printf(" |");
        for (int i = 0; i < n; i++)
            putchar(isprint(buf[i]) ? buf[i] : '.');
        printf("|\n");

        offset += (unsigned)n;
    }
}

int main(int argc, char *argv[])
{
    if (argc < 2) { dump(STDIN_FILENO); return 0; }

    int fd = open(argv[1], O_RDONLY);
    if (fd < 0) {
        fprintf(STDERR_FILENO, "hexdump: %s: no such file\n", argv[1]);
        return 1;
    }
    dump(fd);
    close(fd);
    return 0;
}

#endif

/* ===========================================================================
 *  sleep -- and a demonstration that preemption is real
 * =========================================================================== */
#ifdef BUILD_SLEEP

int main(int argc, char *argv[])
{
    unsigned ms = (argc > 1) ? (unsigned)atoi(argv[1]) : 1000;

    unsigned before = uptime_ms();
    sleep_ms(ms);
    unsigned after = uptime_ms();

    /*  Printing the actual elapsed time makes the point that sleep is not a
     *  busy wait: during those milliseconds this process was not on the CPU at
     *  all, and anything else that was runnable ran instead.                  */
    printf("slept %u ms (asked for %u)\n", after - before, ms);
    return 0;
}

#endif

/* ===========================================================================
 *  true and false -- the two smallest useful programs
 * =========================================================================== */
#ifdef BUILD_TRUE
int main(void) { return 0; }
#endif

#ifdef BUILD_FALSE
int main(void) { return 1; }
#endif

/* ===========================================================================
 *  forktest -- proof that two processes really are running
 * =========================================================================== */
#ifdef BUILD_FORKTEST

int main(void)
{
    printf("parent pid is %d\n", getpid());

    pid_t pid = fork();

    if (pid == 0) {
        /*  Both processes execute this same line of the same program, at the
         *  same address, with different page tables behind it.                */
        for (int i = 0; i < 5; i++) {
            printf("  child  %d: %d\n", getpid(), i);
            sleep_ms(120);
        }
        exit(7);
    }

    for (int i = 0; i < 5; i++) {
        printf("parent %d: %d\n", getpid(), i);
        sleep_ms(100);
    }

    int status = 0;
    pid_t reaped = wait(&status);
    printf("child %d exited with %d\n", reaped, status);
    return 0;
}

#endif
