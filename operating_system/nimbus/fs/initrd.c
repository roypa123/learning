/* ===========================================================================
 *  nimbus/fs/initrd.c  --  a tar archive as a read-only filesystem
 * ===========================================================================
 *
 *  The bootstrap problem: to run /bin/sh you need a filesystem driver, and to
 *  test a filesystem driver you would quite like to be able to run a program.
 *  The initrd breaks the cycle. The bootloader loads an archive into memory
 *  and tells us where it is; we present it as a filesystem; the shell runs.
 *  The disk driver and FAT16 can then be developed with a working system
 *  around them.
 *
 *  tar is the ideal format for this and it is worth saying why, because the
 *  reasons are the opposite of what makes a good on-disk format. It has no
 *  index (so you cannot seek to a file -- but we scan once at mount and build
 *  our own). It has no compression (so we need no decompressor). Its header is
 *  ASCII (so you can read it in a hex dump). It is 512-byte aligned (so it
 *  matches sectors). And every machine already has a tool that writes one.
 *
 *  Explained in: docs/40-initrd.md
 * =========================================================================== */

#include <nimbus/fs.h>
#include <nimbus/vfs.h>
#include <nimbus/paging.h>
#include <nimbus/heap.h>
#include <nimbus/kernel.h>
#include <nimbus/string.h>
#include <nimbus/syscall.h>

/*  One file in the archive: where its bytes are, and how many.               */
typedef struct initrd_file {
    uint8_t *data;
    uint32_t length;
} initrd_file_t;

/* ---------------------------------------------------------------------------
 *  tar stores its numbers as NUL- or space-terminated ASCII octal.
 *
 *  Octal, in 1979, because it made the fields readable on a teletype and
 *  because PDP-11 programmers thought in octal. It is the single most
 *  surprising thing about the format and the cause of every "my 8-byte file
 *  appears to be 8 bytes but my 9-byte file appears to be 11" bug.
 * ------------------------------------------------------------------------- */
static uint32_t parse_octal(const char *s, size_t len)
{
    uint32_t value = 0;

    for (size_t i = 0; i < len; i++) {
        if (s[i] < '0' || s[i] > '7') break;      /* stops at NUL or space */
        value = value * 8 + (uint32_t)(s[i] - '0');
    }
    return value;
}

static ssize_t initrd_read(vfs_node_t *node, off_t offset, size_t size, uint8_t *buf)
{
    initrd_file_t *f = (initrd_file_t *)node->device;

    if (offset >= f->length) return 0;                 /* EOF */
    if (offset + size > f->length) size = f->length - offset;

    memcpy(buf, f->data + offset, size);
    return (ssize_t)size;
}

static ssize_t initrd_write(vfs_node_t *node UNUSED, off_t offset UNUSED,
                            size_t size UNUSED, const uint8_t *buf UNUSED)
{
    /*  A ramdisk *could* be writable -- the memory is right there. It is not,
     *  because an initrd that can be modified is an initrd whose contents
     *  differ from the image you built, and the whole value of it is being the
     *  one part of the system you know is exactly what you shipped.           */
    return -EROFS;
}

/* ---------------------------------------------------------------------------
 *  initrd_init -- scan the archive once, build nodes
 *
 *  `start` is a *physical* address, because that is what the bootloader gave
 *  us. Everything after the P2V() is a kernel virtual address.
 * ------------------------------------------------------------------------- */
vfs_node_t *initrd_init(paddr_t start, size_t length)
{
    vfs_node_t *root = vfs_make_directory("initrd");
    if (!root) return NULL;

    uint8_t *base = (uint8_t *)P2V(start);
    size_t   pos  = 0;
    int      count = 0;

    while (pos + sizeof(tar_header_t) <= length) {
        tar_header_t *h = (tar_header_t *)(base + pos);

        /*  Two consecutive zero blocks mark the end of an archive. In practice
         *  one is enough to detect, because a real header always begins with a
         *  filename character.                                                */
        if (h->name[0] == '\0') break;

        if (memcmp(h->magic, "ustar", 5) != 0) {
            LOG_WARN("initrd: entry at offset %u is not ustar, stopping", (uint32_t)pos);
            break;
        }

        uint32_t size = parse_octal(h->size, sizeof(h->size));

        /*  The file's data begins in the block right after the header, and
         *  occupies ceil(size/512) blocks. Everything in tar is rounded up to
         *  512 bytes, including the last block of a 1-byte file.              */
        uint8_t *data = base + pos + 512;

        if (h->typeflag == '0' || h->typeflag == '\0') {
            initrd_file_t *f = (initrd_file_t *)kmalloc(sizeof(initrd_file_t));
            vfs_node_t    *n = (vfs_node_t *)kcalloc(1, sizeof(vfs_node_t));
            if (!f || !n) { LOG_ERR("initrd: out of memory"); break; }

            f->data   = data;
            f->length = size;

            /*  tar stores "./bin/sh" or "bin/sh"; we want the basename for a
             *  flat filesystem. A real initrd driver reconstructs the
             *  directory tree, which is twenty more lines and one more struct;
             *  Chapter 40 does it as an exercise.                             */
            const char *name = h->name;
            const char *slash = strrchr(name, '/');
            if (slash) name = slash + 1;

            strlcpy(n->name, name, VFS_NAME_MAX);
            n->flags    = VFS_FILE;
            n->length   = size;
            n->inode    = (uint32_t)count;
            n->device   = f;
            n->read     = initrd_read;
            n->write    = initrd_write;
            n->refcount = 1;

            vfs_dir_add(root, n);
            count++;

            LOG_DEBUG("initrd: %-20s %6u bytes", n->name, size);
        }

        pos += 512 + ALIGN_UP(size, 512);
    }

    kprintf("initrd: %d files, %u KiB at %08x\n",
            count, (uint32_t)(length / KiB), start);
    return root;
}
