# Chapter 41 — FAT16, read

[← The initrd](40-initrd.md) · [Contents](README.md) · [Next: FAT16, write →](42-fat16-write.md)

> 📖 **Line by line:** [fat16.c](line-by-line/nimbus-fat16.md)

---

## Goal

Read a filesystem somebody else designed, off a real disk, with a driver that has to match the
specification exactly — because the test of correctness is that `mdir` on the host agrees with us.

---

## 1. The central idea

> The disk is divided into fixed-size clusters, and a single array — the File Allocation Table —
> says, for each cluster, which cluster comes next.

```
    FAT[5] = 6      cluster 5 is followed by cluster 6
    FAT[6] = 9      then cluster 9
    FAT[9] = 0xFFFF end of file
    FAT[7] = 0      cluster 7 is free
```

A file is a **linked list**, and the list's "next" pointers all live together in one array near the
front of the disk instead of being scattered with the data.

That is the whole design. Its consequences are the interesting part:

> * Seeking to byte 1,000,000 of a file means walking the chain from the beginning — O(n). Every
>   modern filesystem uses extents or a tree instead, for exactly this reason.
> * Losing the FAT loses every file, which is why there are two copies.
> * There is no journal, so a power cut between "allocate a cluster" and "write the directory entry"
>   leaks the cluster permanently. That is what `chkdsk` spends its time finding.

---

## 2. The four regions

```
    [reserved][FAT 1][FAT 2][root directory][data...]
```

```c
    fs->fat_lba  = fs->reserved_sectors;
    fs->root_lba = fs->fat_lba + (uint32_t)fs->fat_count * fs->sectors_per_fat;

    uint32_t root_sectors =
        ((uint32_t)fs->root_entries * sizeof(fat_dirent_t) + 511) / 512;

    fs->data_lba = fs->root_lba + root_sectors;
    fs->cluster_count = (fs->total_sectors - fs->data_lba) / fs->sectors_per_cluster;
```

Each region starts where the previous one ends. Everything in the driver derives from these four
numbers, and getting one wrong shifts everything after it.

### 2.1 The boot record is the superblock

```c
typedef struct fat_bpb {
    uint8_t  jump[3];
    char     oem[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  fat_count;
    uint16_t root_entries;
    ...
} PACKED fat_bpb_t;
```

> The boot sector doubles as the filesystem superblock, which is why this structure starts with a
> jump instruction: bytes 0..2 are real x86 code, and the fields the driver cares about start at
> offset 11 so that the boot code can jump over them.

`eb 3c 90` is `jmp short 0x3e; nop` — over the 59 bytes of BPB, into the boot code at offset 0x3E.

One sector, two purposes, and the layout of one constrains the other. That is why `bytes_per_sector`
is at offset 11 rather than 0.

### 2.2 Two FATs

```c
    uint8_t  fat_count;            /* almost always 2: the FAT is mirrored    */
```

Lose the FAT and every file becomes an unreachable chain of clusters. So there are two.

Nothing in the format says which is authoritative when they disagree — that is `chkdsk`'s problem —
and nothing keeps them in sync except every driver writing both:

```c
    for (uint8_t copy = 0; copy < fs->fat_count; copy++) {
```

> Writing only the first copy is a bug that nothing will notice until a real DOS or Windows machine
> reads the disk, decides the two FATs disagree, and either repairs from the wrong one or refuses to
> mount.

### 2.3 The root directory is special

```c
    uint16_t root_entries;         /* FAT16 has a fixed-size root directory   */
```

On FAT16 the root is **not a normal directory**. It is a fixed-size region at a fixed place, it
cannot grow, and it has no cluster number.

```c
#define ROOT_DIR_SENTINEL 0xFFFFFFFFu
```

> We mark it with this sentinel in a node's `impl` field, and every directory operation has one
> branch for it. FAT32 removed the special case by making the root an ordinary cluster chain — which
> is most of what FAT32 changed.

512 entries is the usual value, which is why "cannot create file in root directory" was a real DOS
error and why the limit felt arbitrary.

---

## 3. Cluster arithmetic

```c
static uint32_t cluster_to_lba(fat_fs_t *fs, uint16_t cluster)
{
    return fs->data_lba + (uint32_t)(cluster - 2) * fs->sectors_per_cluster;
}
```

The `- 2` is the thing to get right:

> Cluster numbering starts at 2. Clusters 0 and 1 do not exist on the disk; their FAT entries hold
> the media descriptor and a dirty flag instead. So the data area's first sector is cluster *2*, and
> forgetting the subtraction reads every file two clusters too late — a bug that produces plausible
> looking garbage rather than an obvious failure.

FAT[0] holds the media descriptor (`0xFFF8` for a hard disk) and FAT[1] holds two status bits — one
saying whether the volume was cleanly unmounted, one saying whether an I/O error occurred.

So the first two entries are metadata, and the first *usable* cluster is 2.

### 3.1 Cluster values

```c
#define FAT_FREE_CLUSTER   0x0000
#define FAT_BAD_CLUSTER    0xFFF7
#define FAT_EOC            0xFFF8  /* >= this means end of chain              */
```

| Value | Meaning |
|---|---|
| `0x0000` | free |
| `0x0002`–`0xFFEF` | the next cluster |
| `0xFFF0`–`0xFFF6` | reserved |
| `0xFFF7` | bad cluster, do not use |
| `0xFFF8`–`0xFFFF` | end of chain |

End of chain is a **range**, not a value, which is why the tests are `>=`:

```c
    while (cluster >= 2 && cluster < FAT_EOC)
```

A driver that compares against `0xFFFF` exactly works on most volumes and fails on ones formatted by
tools that write `0xFFF8`.

---

## 4. Caching the FAT

```c
    size_t fat_bytes = (size_t)fs->sectors_per_fat * fs->bytes_per_sector;
    fs->fat = (uint16_t *)kmalloc(fat_bytes);
    if (!fs->fat) { kfree(fs); return NULL; }

    for (uint16_t s = 0; s < fs->sectors_per_fat; s++) {
        if (read_sector(fs, fs->fat_lba + s,
                        (uint8_t *)fs->fat + (size_t)s * fs->bytes_per_sector) < 0) {
            ...
        }
    }
```

> For a 64 MiB volume with 2 KiB clusters that is 32,768 entries = 64 KiB of RAM, which is a bargain:
> without it, every single chain step would be a disk read.

A 1 MiB file with 2 KiB clusters is 512 clusters. Reading it sequentially means 512 chain lookups; at
300 µs per sector read that would be 150 ms of pure FAT traffic, on top of the data.

With the FAT in memory, a chain lookup is an array index.

```c
static uint16_t fat_get(fat_fs_t *fs, uint16_t cluster)
{
    if (cluster >= fs->cluster_count + 2) return FAT_EOC;
    return fs->fat[cluster];
}
```

The bounds check returns `FAT_EOC` rather than panicking, because a corrupt chain pointing off the
end of the volume should terminate the read, not the kernel. A filesystem driver reading damaged
media must not be a denial of service.

**`uint16_t *`** works because x86 is little-endian and FAT16 entries are little-endian 16-bit
values. On a big-endian machine every access would need a byte swap.

---

## 5. Reading a file

```c
static ssize_t fat_read(vfs_node_t *node, off_t offset, size_t size, uint8_t *buf)
{
    fat_fs_t *fs = (fat_fs_t *)node->device;

    if (offset >= node->length) return 0;
    if (offset + size > node->length) size = node->length - offset;
    if (size == 0) return 0;

    uint32_t cluster_bytes = (uint32_t)fs->sectors_per_cluster * fs->bytes_per_sector;
    uint16_t cluster = (uint16_t)node->impl;

    uint32_t skip = (uint32_t)offset / cluster_bytes;
    while (skip-- && cluster >= 2 && cluster < FAT_EOC)
        cluster = fat_get(fs, cluster);
    ...
```

### 5.1 The O(n) seek

```c
    uint32_t skip = (uint32_t)offset / cluster_bytes;
    while (skip-- && cluster >= 2 && cluster < FAT_EOC)
        cluster = fat_get(fs, cluster);
```

> This is the O(n) seek that the FAT design forces: to reach byte 1,000,000 we follow a thousand
> links. A cache of the last (offset, cluster) pair turns sequential reading back into O(1) per call,
> and is the single highest-value optimisation you can make to this driver.

Sequential reading is the common case and it is quadratic: read 1 (walk 0 links), read 2 (walk 1),
read 3 (walk 2)... For a 1 MiB file read in 512-byte chunks that is 2048 reads walking an average of
256 links — half a million array accesses that a two-field cache would eliminate.

Exercise 41.5.

### 5.2 The nested loop

```c
    while (done < size && cluster >= 2 && cluster < FAT_EOC) {
        uint32_t s   = within / fs->bytes_per_sector;
        uint32_t off = within % fs->bytes_per_sector;

        for (; s < fs->sectors_per_cluster && done < size; s++, off = 0) {
            if (read_sector(fs, cluster_to_lba(fs, cluster) + s, sector) < 0)
                return done ? (ssize_t)done : -EIO;

            uint32_t chunk = fs->bytes_per_sector - off;
            if (chunk > size - done) chunk = (uint32_t)(size - done);

            memcpy(buf + done, sector + off, chunk);
            done += chunk;
        }

        within  = 0;
        cluster = fat_get(fs, cluster);
    }
```

Outer loop over clusters, inner over sectors within a cluster.

**`off = 0` in the increment** — the first sector may start mid-way (a read from offset 100), every
subsequent one starts at 0.

**`within = 0` after the first cluster** — same idea one level up.

**A partial error returns the partial count.** `return done ? (ssize_t)done : -EIO;` — if we read
1000 bytes and then the disk failed, the caller gets 1000. That is what `read()` promises and it is
why `cat` loops until 0 rather than until short.

### 5.3 The 512-byte stack buffer

```c
    uint8_t  sector[512];
```

On the kernel stack, which is 8 KiB (Chapter 29, §4.2). This is the deepest stack user in the kernel
and it is why 8 KiB rather than 4.

A block cache (Chapter 38, §4.1) would remove it entirely — `read_sector` would return a pointer into
the cache and the `memcpy` would come straight from there.

---

## 6. Directories

A directory is a file whose contents are 32-byte entries.

```c
typedef struct fat_dirent {
    char     name[8];              /* space padded, upper case, no dot        */
    char     ext[3];
    uint8_t  attr;
    uint8_t  reserved;
    uint8_t  create_tenths;
    uint16_t create_time;
    uint16_t create_date;
    uint16_t access_date;
    uint16_t cluster_hi;           /* always 0 on FAT16                       */
    uint16_t modify_time;
    uint16_t modify_date;
    uint16_t cluster_lo;
    uint32_t size;
} PACKED fat_dirent_t;
```

Exactly 32 bytes, so 16 entries per sector.

`cluster_hi` is always 0 on FAT16 and holds the high half of a 32-bit cluster number on FAT32 — the
field was added by overwriting part of what used to be reserved, which is why FAT32's cluster numbers
are split across two non-adjacent fields.

### 6.1 One walker for both kinds of directory

```c
static bool dir_get_entry(fat_fs_t *fs, uint32_t dir_cluster, uint32_t index,
                          fat_dirent_t *out, uint32_t *out_lba, uint32_t *out_off)
{
    uint32_t per_sector = fs->bytes_per_sector / sizeof(fat_dirent_t);
    uint8_t  sector[512];

    if (dir_cluster == ROOT_DIR_SENTINEL) {
        ...
    }

    /*  A subdirectory: walk the cluster chain, counting entries.              */
    ...
}
```

> One function serves the root directory and every subdirectory, because the only difference is where
> the entries live. It returns the entry *and* the sector and offset it came from, so that a caller
> who wants to modify it can write it straight back.

Those two out-parameters are what makes Chapter 42 possible: `fat_write` updates a file's size and
starting cluster, and `fat_unlink` marks an entry deleted, and both need to know which 32 bytes of
which sector to change.

### 6.2 Three kinds of entry to skip

```c
    for (uint32_t i = 0; dir_get_entry(fs, node->impl, i, &e, NULL, NULL); i++) {
        if ((uint8_t)e.name[0] == 0x00) break;        /* no more entries ever  */
        if ((uint8_t)e.name[0] == 0xE5) continue;     /* deleted               */
        if (e.attr == FAT_ATTR_LFN) continue;         /* long filename record  */
        if (e.attr & FAT_ATTR_VOLUME_ID) continue;    /* the volume label      */
```

**`0x00` means stop**, not skip. It marks the first never-used entry, and everything after it is
guaranteed unused — so scanning can stop. Treating it as "skip" makes every directory scan read the
whole region.

**`0xE5` means deleted**, and scanning continues. §6.4.

**The volume label** is an entry with the volume-ID attribute, holding the disk's name. It is not a
file.

And the index bookkeeping:

```c
    fat_dirent_t e;
    uint32_t     slot = 0;

    for (uint32_t i = 0; dir_get_entry(...); i++) {
        ...
        if (slot++ != index) continue;
```

> `index` counts *visible* entries; `slot` counts raw 32-byte records. They differ because of deleted
> entries, long-filename records and the volume label, all of which we skip.

Conflating them makes `readdir(n)` skip files whenever a directory has ever had one deleted.

### 6.3 Long filenames, and the attribute that hides them

```c
#define FAT_ATTR_LFN       0x0F    /* the long-filename hack */
```

> an entry whose attribute byte is this is not a file at all, and old DOS skipped it because no real
> file is read-only + hidden + system + volume-label at once.

`0x0F` is `READONLY | HIDDEN | SYSTEM | VOLUME_ID`. Microsoft needed to store long names in a format
with no room for them, and found a combination of attribute bits that every existing DOS version
would ignore.

A long name is stored in a chain of these pseudo-entries *before* the real 8.3 entry, 13 UTF-16
characters each, in reverse order, with a checksum tying them to the short name.

We skip them and show the 8.3 name. `README.TXT` reads as `readme.txt`; a file called
`My Long Document.txt` reads as `MYLONG~1.TXT`.

Implementing them: reverse the chain, decode UTF-16 to something, verify the checksum, and handle the
case where the chain is broken. About 150 lines. Exercise 41.7.

### 6.4 8.3 names

```c
static void name_from_dirent(const fat_dirent_t *e, char *out)
{
    int o = 0;

    for (int i = 0; i < 8 && e->name[i] != ' '; i++)
        out[o++] = (char)tolower((unsigned char)e->name[i]);

    if (e->ext[0] != ' ') {
        out[o++] = '.';
        for (int i = 0; i < 3 && e->ext[i] != ' '; i++)
            out[o++] = (char)tolower((unsigned char)e->ext[i]);
    }
    out[o] = '\0';
}
```

On disk: `"README  TXT"` — eight characters of name, three of extension, space padded, upper case, no
dot.

In the world: `readme.txt`.

We lower-case on the way out, which is a presentation choice — FAT has no case information at all in
the 8.3 name. (FAT32 added two bits in the `reserved` byte to remember "the name was all lowercase",
which is how `readme.txt` round-trips on Windows.)

### 6.5 Case-insensitive matching

```c
        if (strcasecmp(candidate, name) != 0) continue;
```

> Case-insensitive, because FAT upper-cases everything on disk. That makes "Makefile" and "MAKEFILE"
> the same file, which is correct for FAT and a constant source of surprise for anyone who develops
> on it and deploys to a case-sensitive filesystem.

This is a real and ongoing source of bugs: a project that builds on Windows and fails on Linux
because a `#include "Foo.h"` refers to a file called `foo.h`.

---

## 7. Mounting

```c
    if (bpb->bytes_per_sector != 512) {
        LOG_ERR("fat16: bytes_per_sector is %u, we only handle 512",
                bpb->bytes_per_sector);
        return NULL;
    }
    if (bpb->sectors_per_cluster == 0 || bpb->fat_count == 0 ||
        bpb->sectors_per_fat == 0 || bpb->root_entries == 0) {
        LOG_ERR("fat16: the boot sector at LBA %u is not a FAT16 BPB", partition_lba);
        return NULL;
    }
```

> Sanity checks before we trust a single field. A disk that is not FAT16 will happily produce a
> `bytes_per_sector` of 0 and a `sectors_per_cluster` of 0, and the first division by either is a
> divide-by-zero exception in the kernel — from mounting an unformatted disk.

Chapter 38, §2.2: the partition type byte is advisory. These four checks are what actually decides.

They are not sufficient to prove the volume is FAT16 — a determined adversary could craft a boot
sector that passes them and then produces nonsense — but they turn "the kernel crashes on an
unformatted disk" into "the mount fails with a message".

### 7.1 Detecting FAT12 versus FAT16 versus FAT32

We do not, and a real driver must.

The rule is **not** the `fs_type` string:

```c
    char     fs_type[8];           /* "FAT16   " -- documentation, not truth  */
```

Microsoft's own documentation says that field is informational and must not be used to determine the
type.

The actual rule is the **cluster count**:

| Clusters | Type |
|---|---|
| < 4085 | FAT12 |
| 4085–65524 | FAT16 |
| ≥ 65525 | FAT32 |

Which means the type depends on the volume size and the cluster size together, and a 16 MiB volume
formatted with 512-byte clusters is FAT12 while the same volume with 2 KiB clusters is FAT16.

That is why `mkfs.fat -F 16` exists as an explicit flag. Exercise 41.6.

---

## 8. Running it

```
fat16: 16384 KiB volume, 8154 clusters of 2048 bytes, 512 root entries
[    0.430] inf  vfs: mounted fat16 at /mnt
```

```bash
$ mcopy -i bin/disk.img README.md ::
$ mcopy -i bin/disk.img /bin/ls ::LS.BIN
```

```
nimbus> ls /mnt
readme.md  ls.bin
nimbus> cat /mnt/readme.md
# Operating Systems From Scratch
...
nimbus> hexdump /mnt/ls.bin | cat
00000000  7f 45 4c 46 01 01 01 00  ...
```

Files put there by a host tool, read by our driver. And the reverse — Chapter 38, §6 — files written
by us and read by `mdir`.

**Both directions agreeing is the test.** A driver that can only read what it wrote has not
implemented FAT16; it has implemented something.

### 8.1 Checking the geometry by hand

```c
    kprintf("fat_lba=%u root_lba=%u data_lba=%u clusters=%u\n",
            fs->fat_lba, fs->root_lba, fs->data_lba, fs->cluster_count);
```

```
fat_lba=4 root_lba=68 data_lba=100 clusters=8154
```

Check against `mkfs.fat`'s output:

```bash
$ minfo -i bin/disk.img ::
sector size: 512 bytes
cluster size: 4 sectors
reserved (boot) sectors: 4
fats: 2
max available root directory slots: 512
sectors per fat: 32
```

`fat_lba` = 4 reserved. ✓
`root_lba` = 4 + 2 × 32 = 68. ✓
`root_sectors` = 512 × 32 / 512 = 32, so `data_lba` = 100. ✓
`clusters` = (32768 − 100) / 4 = 8167 — close to 8154, the difference being total sectors counted
from the BPB rather than the image size.

Five numbers, checked against an independent implementation. That is the right way to debug a
filesystem driver.

---

## 9. What could go wrong

| Symptom | Cause |
|---|---|
| Every file reads garbage | `- 2` missing in `cluster_to_lba` |
| Files read correctly until cluster 2 | End-of-chain compared `== 0xFFFF` |
| Divide by zero on mount | BPB not validated |
| `readdir` skips files | `index` and `slot` conflated |
| Directory scan is very slow | `0x00` treated as skip rather than stop |
| Long-named files appear twice | LFN entries not skipped |
| Volume label appears as a file | `FAT_ATTR_VOLUME_ID` not skipped |
| Case-sensitive lookups fail | `strcmp` instead of `strcasecmp` |
| Windows says the disk is damaged | Only one FAT written (Chapter 42) |

---

## 10. Exercises

🟢 **41.1** Print `fat_get(fs, n)` for the first 16 clusters of a volume with one file on it. Follow
the chain by hand.

🟢 **41.2** Remove the `- 2` from `cluster_to_lba` and `cat` a file.

🟢 **41.3** Compare `== 0xFFFF` against `>= 0xFFF8` for end-of-chain on a volume formatted by
`mkfs.fat`. Which value does it write?

🟡 **41.4** Print every raw directory entry including deleted ones and LFN records. Delete a file
from the host and look again.

🟡 **41.5** Add the (offset, cluster) seek cache from §5.1. Measure the time to `cat` a 1 MiB file
before and after.

🟡 **41.6** Implement proper type detection from the cluster count, and refuse to mount FAT12 or
FAT32 with a clear message.

🔴 **41.7** Implement long filename reading: collect the LFN chain, verify the checksum against the
8.3 name, decode UTF-16 to code page 437 where possible, and present the long name.

🔴 **41.8** Implement FAT32: 32-bit FAT entries, a root directory that is an ordinary cluster chain,
and the `FSInfo` sector. Note how much of the driver is unchanged.

---

## What we covered

- A file as a linked list whose pointers all live in one array, and the three consequences that
  follow.
- Four regions, each starting where the last ends, and a boot sector that is also the superblock —
  which is why it begins with a jump.
- Two FATs, kept in sync only by convention.
- A root directory that is not a directory, and the sentinel that gives it one branch everywhere.
- Clusters numbered from 2, and the subtraction that produces plausible garbage when forgotten.
- End-of-chain as a range, not a value.
- Caching the whole FAT for 64 KiB, and the bounds check that terminates a corrupt chain instead of
  the kernel.
- The O(n) seek that sequential reading turns quadratic, and the two-field cache that fixes it.
- A nested read loop with two partial-start cases, and a partial error returning a partial count.
- Directory entries: what `0x00` and `0xE5` mean, why `index` and `slot` differ, and the attribute
  combination that hid long filenames from DOS.
- 8.3 names, case insensitivity, and the cross-platform bug it causes.
- Four validation checks that turn a kernel crash into a mount failure, and the cluster-count rule
  that actually distinguishes FAT12/16/32.
- Both directions agreeing with `mtools` as the real test.

[Chapter 42](42-fat16-write.md) makes it writable, which is where the absence of a journal starts to
matter.

---

[← The initrd](40-initrd.md) · [Contents](README.md) · [Next: FAT16, write →](42-fat16-write.md)
