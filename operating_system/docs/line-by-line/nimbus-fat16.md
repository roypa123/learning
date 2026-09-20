# Line by line: `nimbus/fs/fat16.c`

[Index](README.md) · [Chapter 41](../41-fat16-read.md) · [Chapter 42](../42-fat16-write.md)

---

## `fat_fs_t`

```c
typedef struct fat_fs {
    ata_device_t *dev;
    uint32_t      part_lba;
    ...
    uint32_t      fat_lba, root_lba, data_lba, cluster_count;
    uint16_t     *fat;
    bool          fat_dirty;
} fat_fs_t;
```
The four derived LBAs and the cached FAT.

```c
#define ROOT_DIR_SENTINEL 0xFFFFFFFFu
```
⚠️ The FAT16 root directory is **not a directory**: a fixed-size region at a fixed place, with no
cluster number and no ability to grow.

> FAT32 removed the special case by making the root an ordinary cluster chain — which is most of what
> FAT32 changed.

---

## Sector I/O

```c
static int read_sector(fat_fs_t *fs, uint32_t lba, void *buf)
{
    return ata_read_sectors(fs->dev, fs->part_lba + lba, 1, buf);
}
```
⚠️ `fs->part_lba +` in one place, and the rest of the driver never knows partitions exist. That
addition is the entirety of our block layer.

⚠️ **Every call is a real disk read.** No cache. A directory scan reads the same sector repeatedly,
and at ~300 µs per PIO read a lookup that should be microseconds takes milliseconds.

---

## `cluster_to_lba`

```c
    return fs->data_lba + (uint32_t)(cluster - 2) * fs->sectors_per_cluster;
```
⚠️ **The `- 2`.**

> Clusters 0 and 1 do not exist on the disk; their FAT entries hold the media descriptor and a dirty
> flag instead. [...] forgetting the subtraction reads every file two clusters too late — a bug that
> produces plausible looking garbage rather than an obvious failure.

---

## `fat_get` / `fat_set`

```c
static uint16_t fat_get(fat_fs_t *fs, uint16_t cluster)
{
    if (cluster >= fs->cluster_count + 2) return FAT_EOC;
    return fs->fat[cluster];
}
```
⚠️ Returns `FAT_EOC` rather than panicking. A corrupt chain pointing off the end should terminate the
read, not the kernel — a filesystem driver reading damaged media must not be a denial of service.

`uint16_t *` works because x86 is little-endian and FAT16 entries are little-endian 16-bit values. On
big-endian every access would need a swap.

⚠️ End of chain is a **range**: `>= 0xFFF8`. Comparing `== 0xFFFF` works on most volumes and fails on
ones formatted by tools that write `0xFFF8`.

---

## `fat_flush`

```c
    for (uint8_t copy = 0; copy < fs->fat_count; copy++) {
        uint32_t base = fs->fat_lba + (uint32_t)copy * fs->sectors_per_fat;

        for (uint16_t s = 0; s < fs->sectors_per_fat; s++)
            write_sector(fs, base + s, bytes + (size_t)s * fs->bytes_per_sector);
    }
```
⚠️ **The worst performance bug in Nimbus.**

For a 16 MiB volume the FAT is 32 sectors, mirrored, so **every cluster allocation writes 64
sectors** — 32 KiB of disk traffic to change two bytes. At 300 µs per write that is 19 ms per
allocation.

The fix is a block cache with write-back: `write_sector` marks a buffer dirty, and only the sectors
that actually changed reach the disk. 64 down to 2, **with no change to this code** — which is the
argument for putting the cache in the block layer.

⚠️ **Both copies.**

> Writing only the first copy is a bug that nothing will notice until a real DOS or Windows machine
> reads the disk, decides the two FATs disagree, and either repairs from the wrong one or refuses to
> mount.

---

## `fat_alloc_cluster`

```c
    for (uint16_t c = 2; c < fs->cluster_count + 2; c++) {
        if (fs->fat[c] == FAT_FREE_CLUSTER) {
            fat_set(fs, c, FAT_EOC);
```
Linear search from 2 every time. FAT32's `FSInfo` sector formalised a next-free hint precisely to
avoid this.

```c
            uint8_t zeros[512];
            memset(zeros, 0, sizeof(zeros));
            for (uint8_t s = 0; s < fs->sectors_per_cluster; s++)
                write_sector(fs, cluster_to_lba(fs, c) + s, zeros);
```
⚠️ **Zeroing on allocate, not on free.**

> A freshly allocated cluster otherwise contains the previous file's data, which would be visible to
> whoever allocates it next — a genuine information leak, and the reason every filesystem zeroes on
> allocate rather than on free.

On *free* would cost work on every delete and leave a window: a crash between "mark free" and "zero"
exposes the data anyway.

```c
    return 0;      /* 0 is never a valid data cluster, so it works as "full" */
```
Same reasoning as `PMM_NO_FRAME`: a value that cannot be a valid result.

---

## `name_from_dirent` / `name_to_dirent`

```c
    for (int i = 0; i < 8 && e->name[i] != ' '; i++)
        out[o++] = (char)tolower((unsigned char)e->name[i]);
```
On disk `"README  TXT"`; in the world `readme.txt`.

Lower-casing is a presentation choice — FAT has no case information in the 8.3 name at all.

```c
    int i = 0;
    for (; name[i] && name[i] != '.'; i++) {
        if (i >= 8) return false;              /* too long for 8.3 */
```
⚠️ Refuses rather than truncating, the same principle as path components. Truncating
`mydocument.txt` to `MYDOCUME.TXT` creates a file the user did not ask for and can collide with an
existing one.

---

## `dir_get_entry`

```c
static bool dir_get_entry(fat_fs_t *fs, uint32_t dir_cluster, uint32_t index,
                          fat_dirent_t *out, uint32_t *out_lba, uint32_t *out_off)
```
⚠️ **The two out-parameters are what make Chapter 42 possible.** `fat_write` updates a file's size and
start cluster; `fat_unlink` marks an entry deleted. Both need to know which 32 bytes of which sector
to rewrite.

```c
    if (dir_cluster == ROOT_DIR_SENTINEL) {
        uint32_t root_sectors =
            ((uint32_t)fs->root_entries * sizeof(fat_dirent_t) + fs->bytes_per_sector - 1)
            / fs->bytes_per_sector;
        ...
    }
```
One branch for the root; the rest walks a cluster chain. One function for both, because the only
difference is where the entries live.

---

## `dir_put_entry`

```c
    if (read_sector(fs, lba, sector) < 0) return false;
    memcpy(sector + off, e, sizeof(fat_dirent_t));
    return write_sector(fs, lba, sector) >= 0;
```
⚠️ **Read, modify, write.**

> A directory entry is 32 bytes and the smallest thing a disk can write is 512, so there is no
> alternative — and this is exactly the read-modify-write window that makes a filesystem need a
> journal to survive a power cut.

Sixteen entries per sector. Changing one rewrites all sixteen, and a crash mid-write can corrupt the
other fifteen. That is why `chkdsk` finds cross-linked files.

---

## `fat_read`

```c
    if (offset >= node->length) return 0;
    if (offset + size > node->length) size = node->length - offset;
```
EOF returns 0; a read spanning the end is clamped.

```c
    uint32_t skip = (uint32_t)offset / cluster_bytes;
    while (skip-- && cluster >= 2 && cluster < FAT_EOC)
        cluster = fat_get(fs, cluster);
```
⚠️ **The O(n) seek the FAT design forces.**

> A cache of the last (offset, cluster) pair turns sequential reading back into O(1) per call, and is
> the single highest-value optimisation you can make to this driver.

Sequential reading is quadratic: read 1 walks 0 links, read 2 walks 1, read 3 walks 2… For a 1 MiB
file in 512-byte chunks that is 2048 reads walking an average of 256 links.

```c
        for (; s < fs->sectors_per_cluster && done < size; s++, off = 0) {
```
⚠️ `off = 0` in the increment: the first sector may start mid-way, every subsequent one starts at 0.
Same idea one level up with `within = 0` after the first cluster.

```c
            if (read_sector(...) < 0)
                return done ? (ssize_t)done : -EIO;
```
A partial error returns the partial count. That is what `read()` promises, and why `cat` loops until
0 rather than until short.

⚠️ `uint8_t sector[512]` on the kernel stack. The deepest stack user in the kernel, and why
`KERNEL_STACK_SIZE` is 8 KiB rather than 4.

---

## `fat_write`

```c
    uint16_t cluster = (uint16_t)node->impl;
    if (cluster < 2) {
        cluster = fat_alloc_cluster(fs);
        if (!cluster) return -ENOSPC;
        node->impl = cluster;
    }
```
⚠️ **The empty-file case.** A new file has cluster 0 in its directory entry, and the first write has
nowhere to link from — so the cluster goes in the directory entry instead.

Missing this branch means `echo hello > newfile` writes to cluster 0, which is the media descriptor.

```c
    while (skip--) {
        uint16_t next = fat_get(fs, cluster);

        if (next < 2 || next >= FAT_EOC) {
            next = fat_alloc_cluster(fs);
            if (!next) return -ENOSPC;
            fat_set(fs, cluster, next);
        }
        cluster = next;
    }
```
Unlike the read path, this one **allocates as it walks** — a write past the end must extend.

That is also how sparse writes work, and why FAT cannot do sparse *files*: a chain has no way to say
"the next 500 clusters are zero".

```c
            if (chunk != fs->bytes_per_sector) {
                if (read_sector(fs, lba, sector) < 0) ...
            }
```
⚠️ **A partial sector must be read before it is written**, or the bytes we are not changing become
zeros.

The `if` skips the read when the whole sector is replaced — the common case for sequential writing.

```c
    fat_dirent_t e;
    uint32_t lba, off;
    if (dir_get_entry(fs, node->inode, node->permissions, &e, &lba, &off)) {
        e.size       = node->length;
        e.cluster_lo = (uint16_t)node->impl;
        dir_put_entry(fs, lba, off, &e);
    }
```
⚠️ **The size and start cluster live in the directory entry, not the FAT.** A file whose data is
written but whose entry is not updated appears to be zero bytes long.

`node->inode` holds the directory's cluster; `node->permissions` holds the entry's slot index — two
VFS fields repurposed, which is an abuse of a field named `permissions`. The honest fix is a
FAT-specific node with a `void *private`.

---

## `fat_readdir` / `fat_finddir`

```c
        if ((uint8_t)e.name[0] == 0x00) break;        /* no more entries ever  */
        if ((uint8_t)e.name[0] == 0xE5) continue;     /* deleted               */
        if (e.attr == FAT_ATTR_LFN) continue;         /* long filename record  */
        if (e.attr & FAT_ATTR_VOLUME_ID) continue;    /* the volume label      */
```
⚠️ **`0x00` means stop, not skip.** Everything after the first never-used entry is guaranteed unused.
Treating it as "skip" makes every scan read the whole region.

`FAT_ATTR_LFN` is `0x0F` = readonly + hidden + system + volume-ID:

> no real file is read-only + hidden + system + volume-label at once

— a combination Microsoft found that every existing DOS version would ignore.

```c
        if (slot++ != index) continue;
```
⚠️ `index` counts **visible** entries; `slot` counts raw records. Conflating them makes `readdir(n)`
skip files whenever a directory has ever had one deleted.

```c
        if (strcasecmp(candidate, name) != 0) continue;
```
⚠️ Case-insensitive, because FAT upper-cases everything on disk.

> That makes "Makefile" and "MAKEFILE" the same file, which is correct for FAT and a constant source
> of surprise for anyone who develops on it and deploys to a case-sensitive filesystem.

---

## `fat_unlink`

```c
        if (e.cluster_lo >= 2) fat_free_chain(fs, e.cluster_lo);

        e.name[0] = (char)0xE5;
        if (!dir_put_entry(fs, lba, off, &e)) return -EIO;
```
⚠️ **Deletion is one byte.**

> Everything else — the size, the starting cluster, the timestamps — stays on disk, which is
> precisely why undelete tools worked on DOS and why "deleted" has never meant "gone".

An undelete tool reads `cluster_lo` and `size` from the `0xE5` entry and *guesses* the file was
contiguous — because the chain itself was freed.

⚠️ The order matters and neither is safe: chain first leaves an entry pointing at free clusters (a
cross-linked file); entry first leaks the clusters (a lost chain). That is what a journal is for.

---

## `fat16_mount`

```c
    if (bpb->bytes_per_sector != 512) { ... return NULL; }
    if (bpb->sectors_per_cluster == 0 || bpb->fat_count == 0 ||
        bpb->sectors_per_fat == 0 || bpb->root_entries == 0) { ... return NULL; }
```
⚠️ **Before trusting a single field.**

> A disk that is not FAT16 will happily produce a `bytes_per_sector` of 0 and a
> `sectors_per_cluster` of 0, and the first division by either is a divide-by-zero exception in the
> kernel — from mounting an unformatted disk.

The partition type byte is advisory; these four checks are what actually decides.

```c
    char     fs_type[8];           /* "FAT16   " -- documentation, not truth  */
```
Microsoft's own documentation says that field is informational and must not be used to determine the
type. The real rule is the **cluster count**: < 4085 is FAT12, < 65525 is FAT16, else FAT32.

---

[Index](README.md) · [Chapter 41](../41-fat16-read.md) · [Chapter 42](../42-fat16-write.md)
