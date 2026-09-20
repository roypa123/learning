# Chapter 40 — The initrd

[← The VFS](39-vfs.md) · [Contents](README.md) · [Next: FAT16, read →](41-fat16-read.md)

---

## Goal

Mount a filesystem before any filesystem driver works. A tar archive, loaded into memory by the
bootloader, presented as a read-only VFS tree.

Eighty lines, and it breaks a bootstrapping cycle.

---

## 1. The cycle

To run `/bin/sh` you need a filesystem driver. To develop and test a filesystem driver you would
quite like to be able to run a program.

The initrd breaks it: the bootloader loads an archive into memory, tells us where it is, and we
present it as a filesystem. The shell runs. FAT16 and the disk driver can then be developed with a
working system around them.

Real systems use it for a related reason: the driver needed to mount the root filesystem might itself
be a loadable module, or on an encrypted volume, or on a device that needs firmware. Linux's
`initramfs` holds enough userland to find and mount the real root, then pivots to it and disappears.

---

## 2. Why tar

tar is a wonderful bootstrap format, and the reasons are the *opposite* of what makes a good on-disk
format:

> It has no index (so you cannot seek to a file — but we scan once at mount and build our own). It
> has no compression (so we need no decompressor). Its header is ASCII (so you can read it in a hex
> dump). It is 512-byte aligned (so it matches sectors). And every machine already has a tool that
> produces one.

An archive is a flat sequence:

```
    [512-byte header][file contents, padded to 512][512-byte header][...]
    ...
    [512 zero bytes][512 zero bytes]
```

No seeking, no central directory, no back-pointers. You read it forwards or not at all — which is
exactly right for something that was designed to be written to a *tape*.

---

## 3. The header

```c
typedef struct tar_header {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];        /* the file size, in OCTAL, as ASCII digits         */
    char mtime[12];
    char checksum[8];
    char typeflag;        /* '0' or '\0' = file, '5' = directory              */
    char linkname[100];
    char magic[6];        /* "ustar"                                          */
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
    char pad[12];
} PACKED tar_header_t;
```

Every field is ASCII. You can read a tar archive in a hex dump without a tool:

```
00000000: 7368 0000 0000 0000 0000 0000 0000 0000  sh..............
00000060: 0000 0000 3030 3030 3634 3400 3030 3030  ....0000644.0000
000000...: 3030 3000 3030 3030 3030 3000 3030 3030  000.0000000.0000
00000080: 3030 3134 3434 3400 ...                   00014444.
```

`sh`, mode `0000644`, size `00014444` octal = 6436 bytes.

### 3.1 Octal, and the bug it causes

```c
static uint32_t parse_octal(const char *s, size_t len)
{
    uint32_t value = 0;

    for (size_t i = 0; i < len; i++) {
        if (s[i] < '0' || s[i] > '7') break;      /* stops at NUL or space */
        value = value * 8 + (uint32_t)(s[i] - '0');
    }
    return value;
}
```

> Octal, in 1979, because it made the fields readable on a teletype and because PDP-11 programmers
> thought in octal. It is the single most surprising thing about the format and the cause of every
> "my 8-byte file appears to be 8 bytes but my 9-byte file appears to be 11" bug.

`"11"` parsed as decimal is 11; as octal it is 9. Sizes below 8 are identical in both bases, so a
decimal parser passes every small test and fails on the ninth byte.

The loop stops at the first non-octal character, which handles both terminations the format allows:
NUL and space. Different tar implementations use different ones, sometimes within the same archive.

### 3.2 The checksum

```powershell
    for ($i = 148; $i -lt 156; $i++) { $header[$i] = 0x20 }

    $sum = 0
    foreach ($b in $header) { $sum += $b }

    Write-Field $header 148 ("{0}`0 " -f [Convert]::ToString($sum, 8).PadLeft(6, '0')) 8
```

From [`mkinitrd.ps1`](../tools/mkinitrd.ps1), with the comment:

> The checksum is computed with the checksum field itself treated as eight spaces, then written back
> into that field. A self-referential definition that every tar implementation gets right and every
> first attempt gets wrong — and a wrong checksum is why GNU tar says "invalid header".

And the terminator is `NUL` then *space*, which is the convention GNU tar expects. Some
implementations write space-NUL. Both are accepted by most readers and neither is accepted by all.

Our reader does not verify it:

```c
        if (memcmp(h->magic, "ustar", 5) != 0) {
            LOG_WARN("initrd: entry at offset %u is not ustar, stopping", (uint32_t)pos);
            break;
        }
```

The magic is checked; the checksum is not. That is a deliberate omission for an archive we built
ourselves in the same build, and it would be wrong for anything from outside. Exercise 40.5.

### 3.3 `ustar` versus the old format

The original tar had no magic and no `typeflag` — a directory was a name ending in `/`. POSIX
standardised `ustar` in 1988, adding the magic, the type flag, and `prefix` for names over 100
characters.

GNU tar and BSD tar each added their own extensions on top, and POSIX then standardised a *third*
format, `pax`, which stores long names and large sizes in special extension records.

> Windows has `tar.exe` these days, but it produces archives with PAX extension records that our
> eighty-line reader would have to learn to skip.

Which is why [`mkinitrd.ps1`](../tools/mkinitrd.ps1) exists rather than shelling out to `tar`:
writing the format ourselves keeps it to exactly what `initrd.c` parses, and lets you read both
halves side by side.

---

## 4. Scanning

```c
vfs_node_t *initrd_init(paddr_t start, size_t length)
{
    vfs_node_t *root = vfs_make_directory("initrd");
    if (!root) return NULL;

    uint8_t *base = (uint8_t *)P2V(start);
    size_t   pos  = 0;
    int      count = 0;

    while (pos + sizeof(tar_header_t) <= length) {
        tar_header_t *h = (tar_header_t *)(base + pos);

        if (h->name[0] == '\0') break;
        ...
        pos += 512 + ALIGN_UP(size, 512);
    }
```

One pass at mount, building VFS nodes. Nothing is copied — the nodes point into the archive where it
already sits in memory.

### 4.1 `P2V`

```c
    uint8_t *base = (uint8_t *)P2V(start);
```

`start` comes from the multiboot module list and is a **physical** address (Chapter 11, §5.3).

Chapter 25, §5: every fixed physical address goes through `P2V`. Writing `(uint8_t *)start` here is
a page fault.

And the frames must have been reserved, or the allocator will hand out the archive:

```c
        for (uint32_t i = 0; i < mbi->mods_count; i++)
            pmm_reserve_region(mods[i].mod_start,
                               mods[i].mod_end - mods[i].mod_start);
```

Forgetting that produces an initrd whose contents become garbage a few seconds after boot, which is
a memorable afternoon.

### 4.2 The advance

```c
        pos += 512 + ALIGN_UP(size, 512);
```

Header, then data rounded up to the next 512-byte boundary.

**Including for a 1-byte file**, which occupies a 512-byte header and a 512-byte data block. tar is
not a compact format and was never meant to be — it is a *tape* format, and a tape block is 512
bytes whether you fill it or not.

### 4.3 The end

```c
        if (h->name[0] == '\0') break;
```

> Two consecutive zero blocks mark the end of an archive. In practice one is enough to detect,
> because a real header always begins with a filename character.

The two-block rule exists because tape drives wrote in blocks and a single zero block could be
padding. We check one, which is correct for any archive that is not deliberately adversarial.

---

## 5. The flat namespace

```c
            const char *name = h->name;
            const char *slash = strrchr(name, '/');
            if (slash) name = slash + 1;
```

> tar stores "./bin/sh" or "bin/sh"; we want the basename for a flat filesystem. A real initrd driver
> reconstructs the directory tree, which is twenty more lines and one more struct.

So `bin/sh` and `usr/bin/sh` would collide. For nine files with distinct names it does not matter.

Building the tree properly: for each entry, split the path, walk or create each intermediate
directory (using `vfs_make_directory` and `vfs_dir_add`, which already exist), and add the file to
the last one. Exercise 40.4.

Note that the archive's *order* matters for that: tar does not guarantee a directory appears before
its contents, so a tree builder must create parents on demand rather than expecting them.

---

## 6. Reading

```c
static ssize_t initrd_read(vfs_node_t *node, off_t offset, size_t size, uint8_t *buf)
{
    initrd_file_t *f = (initrd_file_t *)node->device;

    if (offset >= f->length) return 0;                 /* EOF */
    if (offset + size > f->length) size = f->length - offset;

    memcpy(buf, f->data + offset, size);
    return (ssize_t)size;
}
```

Six lines. It is a `memcpy` from memory the bootloader already placed.

Three things worth noticing:

**Reading past the end returns 0**, which is EOF, which is how `cat` knows to stop
(Chapter 43, §4.1).

**A read spanning the end is clamped**, not rejected. `read(fd, buf, 4096)` on a 100-byte file
returns 100.

**`f->data` points into the archive.** No copy, no allocation per file. The whole initrd costs one
`vfs_node_t` and one `initrd_file_t` per file — about 140 bytes — regardless of file size.

### 6.1 Deliberately read-only

```c
static ssize_t initrd_write(vfs_node_t *node UNUSED, off_t offset UNUSED,
                            size_t size UNUSED, const uint8_t *buf UNUSED)
{
    return -EROFS;
}
```

> A ramdisk *could* be writable — the memory is right there. It is not, because an initrd that can be
> modified is an initrd whose contents differ from the image you built, and the whole value of it is
> being the one part of the system you know is exactly what you shipped.

That is a real argument and it is worth taking seriously. When something is broken, the question "is
the binary on disk the one I built?" should have an obvious answer.

Note the function exists and returns an error, rather than the pointer being NULL. Both work — the
dispatch layer checks (Chapter 39, §3) — and having it here means the error is `-EROFS`
("read-only filesystem") rather than `-EINVAL`, which is a better diagnostic.

---

## 7. Mounting it at `/bin`

```c
    if ((mbi->flags & MB_INFO_MODS) && mbi->mods_count > 0) {
        multiboot_module_t *mods = (multiboot_module_t *)P2V(mbi->mods_addr);

        vfs_node_t *initrd = initrd_init(mods[0].mod_start,
                                         mods[0].mod_end - mods[0].mod_start);
        if (initrd) {
            vfs_node_t *bin = vfs_make_directory("bin");
            vfs_dir_add(vfs_root, bin);
            bin->mounted = initrd;
            bin->flags  |= VFS_MOUNTPOINT;
        }
    } else {
        LOG_WARN("no initrd module: there will be nothing to run.\n"
                 "       Pass -initrd initrd.tar to QEMU.");
    }
```

Create an empty `/bin`, mount the initrd over it (Chapter 39, §6).

The warning names the fix, which is worth doing for any "the system will not work" message. Someone
who forgot the flag gets the answer rather than a mystery.

---

## 8. Building it

```make
bin/initrd.tar: $(USER_BINS)
	powershell -ExecutionPolicy Bypass -File tools/mkinitrd.ps1 \
	  -Out $@ -Files $(subst $(space),$(comma),$(USER_BINS))
```

Nine binaries in, one archive out.

```
sh                       6436 bytes
ls                       5120 bytes
cat                      5008 bytes
echo                     4896 bytes
hexdump                  5312 bytes
sleep                    4992 bytes
true                     4688 bytes
false                    4688 bytes
forktest                 5040 bytes
-> bin\initrd.tar (56320 bytes)
```

Nine files totalling 46 KiB become a 55 KiB archive — 9 KiB of headers and padding, which is 16%
overhead for small files.

That overhead is the price of a format with no index. For an initrd of a few dozen files it is
irrelevant; for a million-file archive it is why `cpio` (which has a 110-byte header) is what Linux
actually uses for `initramfs`.

---

## 9. Running it

```
initrd: 9 files, 55 KiB at 00110000
[    0.418] dbg  initrd: sh                    6436 bytes
[    0.418] dbg  initrd: ls                    5120 bytes
...
```

```
nimbus> ls /bin
sh  ls  cat  echo  hexdump
sleep  true  false  forktest
nimbus> hexdump /bin/echo | cat
00000000  7f 45 4c 46 01 01 01 00  00 00 00 00 00 00 00 00  |.ELF............|
00000010  02 00 03 00 01 00 00 00  80 80 04 08 34 00 00 00  |............4...|
```

`7f 45 4c 46` is `\x7FELF`. `02 00` is `ET_EXEC`. `03 00` is `EM_386`. `80 80 04 08` little-endian is
`0x08048080` — the entry point, which matches `user.ld`'s base plus the header size.

Reading an ELF header by eye out of a tar archive read out of memory placed by a bootloader is a
reasonable moment to notice how many layers are working.

### 9.1 Checking against the host

```bash
$ tar -tvf bin/initrd.tar
-rw-r--r-- 0/0            6436 1970-01-01 00:00 sh
-rw-r--r-- 0/0            5120 1970-01-01 00:00 ls
...
```

GNU tar reads our archive. That is the proof that the format was implemented rather than approximated
— the same check as `mdir` on the FAT16 volume (Chapter 38, §6).

If the checksum were wrong, this is where you would find out:

```
tar: This does not look like a tar archive
```

---

## 10. What could go wrong

| Symptom | Cause |
|---|---|
| "no initrd module" | `-initrd` not passed to QEMU |
| Page fault in `initrd_init` | Missing `P2V` |
| Files are garbage after a few seconds | Module frames not reserved in `pmm_init` |
| Sizes wrong for files over 8 bytes | Octal parsed as decimal |
| Only the first file found | Advance not rounded up to 512 |
| GNU tar rejects the archive | Checksum wrong, or magic missing |
| Two files with the same basename collide | Flat namespace (§5) |
| `exec` fails with `-EIO` | Short read; check `node->length` against the octal size |

---

## 11. Exercises

🟢 **40.1** Hex-dump the first 512 bytes of `initrd.tar` and identify the name, mode, size and magic
by eye.

🟢 **40.2** Change `parse_octal` to parse decimal and find the first file whose size is wrong.

🟢 **40.3** Add a tenth file to the initrd and confirm it appears in `ls /bin`.

🟡 **40.4** Build the directory tree: split each entry's path, create intermediate directories on
demand, and mount the initrd at `/` instead of `/bin`. Handle entries arriving before their parent
directory.

🟡 **40.5** Verify the checksum on load and refuse a corrupted archive. Test it by flipping a byte.

🟡 **40.6** Make the initrd writable in memory, then add a `sync` that writes it back — and then
argue with §6.1 about whether that was a good idea.

🔴 **40.7** Implement `initramfs` properly: instead of mounting the archive as a filesystem, *unpack*
it into an in-memory read-write filesystem at boot, then free the archive's frames. This is what
Linux does and it is why `initramfs` replaced `initrd`.

🔴 **40.8** Add gzip decompression so the archive can be compressed. Inflate is about 400 lines and
it is the same algorithm as PNG's — if you did the `render-from-scratch` course, you have already
written it.

---

## What we covered

- The bootstrapping cycle, and what real systems use an initrd for.
- Why tar is ideal for this and terrible as an on-disk format — the same properties, differently
  valued.
- A header that is entirely ASCII, readable in a hex dump without tools.
- Octal sizes, and the off-by-base bug that passes every test below 8 bytes.
- A self-referential checksum, and why our writer computes it and our reader does not check it.
- `ustar` versus the older format versus PAX, and why we write the archive ourselves.
- One scan at mount, nodes pointing into memory the bootloader placed, and the two things that must
  have happened first: `P2V` and frame reservation.
- 512-byte alignment even for a 1-byte file, and the two-zero-block terminator.
- A flat namespace, and what building the tree would take.
- Read that clamps and returns 0 at EOF; write that refuses on purpose, with a diagnostic rather than
  a NULL pointer.
- 16% overhead for small files, and why Linux uses cpio instead.
- GNU tar reading our archive as the proof of correctness.

[Chapter 41](41-fat16-read.md) reads a filesystem that was not designed by us, off a disk, with a
driver that has to be exactly right.

---

[← The VFS](39-vfs.md) · [Contents](README.md) · [Next: FAT16, read →](41-fat16-read.md)
