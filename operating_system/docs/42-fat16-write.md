# Chapter 42 — FAT16, write

[← FAT16, read](41-fat16-read.md) · [Contents](README.md) · [Next: File descriptors and pipes →](43-fds-and-pipes.md)

---

## Goal

Make the filesystem writable: allocate clusters, grow files, create and delete directory entries —
and understand precisely what a power cut in the middle of each would leave behind.

Writing is where the absence of a journal stops being an abstraction.

---

## 1. What has to change, and in what order

Creating a one-cluster file touches four things:

```
    1. find a free cluster in the FAT          -> in-memory FAT
    2. mark it end-of-chain                    -> in-memory FAT
    3. write the file's data                   -> data area
    4. write the directory entry               -> directory region
    5. flush the FAT to disk (both copies)     -> FAT region
```

Five writes to three regions, and **there is no way to make them atomic**. A power cut between any
two leaves the filesystem in a state that is not a filesystem.

§7 goes through what each failure looks like. It is worth reading before the code.

---

## 2. Allocating a cluster

```c
static uint16_t fat_alloc_cluster(fat_fs_t *fs)
{
    for (uint16_t c = 2; c < fs->cluster_count + 2; c++) {
        if (fs->fat[c] == FAT_FREE_CLUSTER) {
            fat_set(fs, c, FAT_EOC);

            uint8_t zeros[512];
            memset(zeros, 0, sizeof(zeros));
            for (uint8_t s = 0; s < fs->sectors_per_cluster; s++)
                write_sector(fs, cluster_to_lba(fs, c) + s, zeros);

            return c;
        }
    }
    return 0;      /* 0 is never a valid data cluster, so it works as "full" */
}
```

### 2.1 Linear search from 2

> Linear search from the start every time: fine for a small disk, and the reason a heavily used FAT
> volume gets slow to write to. Real implementations cache a "next free" hint, exactly like our
> physical frame allocator does.

Chapter 22, §5.2 — the same optimisation, four lines, and FAT32 formalised it: the `FSInfo` sector
stores both a free-cluster count and a next-free hint, precisely so a driver does not have to scan.

Exercise 42.4.

### 2.2 Zeroing on allocate

```c
            uint8_t zeros[512];
            memset(zeros, 0, sizeof(zeros));
            for (uint8_t s = 0; s < fs->sectors_per_cluster; s++)
                write_sector(fs, cluster_to_lba(fs, c) + s, zeros);
```

> A freshly allocated cluster otherwise contains the previous file's data, which would be visible to
> whoever allocates it next — a genuine information leak, and the reason every filesystem zeroes on
> allocate rather than on free.

**On allocate, not on free**, and the distinction matters:

- Zeroing on *free* costs work every time a file is deleted, including at shutdown, and a crash
  between "mark free" and "zero" leaves data exposed anyway.
- Zeroing on *allocate* costs work only when the cluster is about to be used, and there is no window.

The cost here is real: a 2 KiB cluster is four sector writes before a single byte of data is stored.
That is why `mkfs` offers to skip it, and why secure-delete tools exist as separate programs.

### 2.3 The sentinel that is free

`0` means "full", and it works because cluster 0 is never a data cluster (Chapter 41, §3).

The same reasoning as `PMM_NO_FRAME` (Chapter 22, §4.1): pick a value that cannot be a valid result.

---

## 3. Writing a file

```c
static ssize_t fat_write(vfs_node_t *node, off_t offset, size_t size, const uint8_t *buf)
{
    ...
    uint16_t cluster = (uint16_t)node->impl;
    if (cluster < 2) {
        cluster = fat_alloc_cluster(fs);
        if (!cluster) return -ENOSPC;
        node->impl = cluster;
    }
```

### 3.1 The empty-file special case

> An empty file has no clusters at all — cluster 0 in its directory entry. The first write has to
> allocate the first one, which is a special case because there is no previous cluster to link from.

Every subsequent allocation links from a predecessor:

```c
        if (next < 2 || next >= FAT_EOC) {
            next = fat_alloc_cluster(fs);
            if (!next) return -ENOSPC;
            fat_set(fs, cluster, next);
        }
```

The first has nowhere to link from, so it goes in the directory entry instead. One branch, and
missing it means `echo hello > newfile` writes to cluster 0 — which is the media descriptor.

### 3.2 Extending the chain to reach the offset

```c
    uint32_t skip = (uint32_t)offset / cluster_bytes;
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

Unlike the read path (Chapter 41, §5.1), this one *allocates* as it walks — because a write past the
end of a file must extend it.

That is also how sparse writes work: `lseek(fd, 1000000, SEEK_SET); write(fd, "x", 1)` allocates
every cluster along the way and zeroes them (§2.2).

Real filesystems support **sparse files** — holes that occupy no disk space — by allowing a gap in
the block map. FAT cannot: a chain has no way to say "the next 500 clusters are zero".

### 3.3 Read-modify-write for a partial sector

```c
            if (chunk != fs->bytes_per_sector) {
                if (read_sector(fs, lba, sector) < 0) return done ? (ssize_t)done : -EIO;
            }

            memcpy(sector + off, buf + done, chunk);

            if (write_sector(fs, lba, sector) < 0) return done ? (ssize_t)done : -EIO;
```

> A partial sector must be read before it is written, or the bytes we are not changing become zeros.

A disk writes whole sectors. To change 10 bytes in the middle of one, you read 512, change 10, write
512.

The `if` skips the read when the whole sector is being replaced, which is the common case for
sequential writing and saves a disk read per sector.

This is also the read-modify-write window that makes the whole operation non-atomic: between the read
and the write, the sector on disk is the old version, and a crash after the write with a partial
buffer would leave it half-updated. On a drive with 4096-byte physical sectors (Chapter 38, §5) the
window is eight times larger.

### 3.4 Updating the directory entry

```c
    fat_dirent_t e;
    uint32_t lba, off;
    if (dir_get_entry(fs, node->inode, node->permissions, &e, &lba, &off)) {
        e.size       = node->length;
        e.cluster_lo = (uint16_t)node->impl;
        dir_put_entry(fs, lba, off, &e);
    }

    fat_flush(fs);
```

> The size and the starting cluster both live there, not in the FAT, and a file whose data is written
> but whose entry is not updated appears to be zero bytes long.

This is where the two out-parameters from `dir_get_entry` (Chapter 41, §6.1) pay off: we know exactly
which sector and which 32-byte offset to rewrite.

`node->inode` holds the directory's cluster and `node->permissions` holds the entry's slot index —
two VFS fields repurposed, because `vfs_node_t` has nowhere else to put them:

```c
        vfs_node_t *found = make_node(fs, &e, candidate);
        if (found) found->permissions = i;   /* remember which slot, for write */
```

That is an abuse of a field named `permissions` and the comment says so. The honest fix is a
FAT-specific node structure with a `void *private` in the VFS node, which is what Linux does
(Chapter 39, §2.1).

---

## 4. `dir_put_entry`

```c
static bool dir_put_entry(fat_fs_t *fs, uint32_t lba, uint32_t off,
                          const fat_dirent_t *e)
{
    uint8_t sector[512];

    if (read_sector(fs, lba, sector) < 0) return false;
    memcpy(sector + off, e, sizeof(fat_dirent_t));
    return write_sector(fs, lba, sector) >= 0;
}
```

> Read, modify, write. A directory entry is 32 bytes and the smallest thing a disk can write is 512,
> so there is no alternative — and this is exactly the read-modify-write window that makes a
> filesystem need a journal to survive a power cut.

Sixteen entries per sector. Changing one means rewriting all sixteen, and a crash mid-write can
corrupt the other fifteen.

That is not hypothetical — it is why `chkdsk` finds "cross-linked files" and why directories are the
first thing a repair tool checks.

---

## 5. Flushing the FAT

```c
static void fat_flush(fat_fs_t *fs)
{
    if (!fs->fat_dirty) return;

    const uint8_t *bytes = (const uint8_t *)fs->fat;

    for (uint8_t copy = 0; copy < fs->fat_count; copy++) {
        uint32_t base = fs->fat_lba + (uint32_t)copy * fs->sectors_per_fat;

        for (uint16_t s = 0; s < fs->sectors_per_fat; s++)
            write_sector(fs, base + s, bytes + (size_t)s * fs->bytes_per_sector);
    }

    fs->fat_dirty = false;
}
```

**This is the worst performance bug in Nimbus**, and it is worth stating plainly.

For a 16 MiB volume the FAT is 32 sectors, mirrored. So **every single cluster allocation writes 64
sectors** — 32 KiB of disk traffic to change two bytes.

At 300 µs per PIO sector write that is 19 milliseconds per allocation. Writing a 1 MiB file in 2 KiB
clusters is 512 allocations = ten seconds of FAT flushing.

The `fat_dirty` flag helps only in that consecutive operations with no allocation skip it entirely.

### 5.1 The fix

A block cache with write-back (Chapter 38, §4.2). `write_sector` marks a buffer dirty; the flush
writes only the sectors that actually changed — which for a single allocation is one, mirrored, so
two.

64 sectors down to 2, and the code above does not change at all: it still calls `write_sector` for
every sector, and the cache notices that 62 of them are identical to what it holds.

That is the argument for putting the cache in the block layer rather than in the filesystem.
Exercise 38.5 and 38.7.

### 5.2 Both copies, every time

Chapter 41, §2.2. Writing only the first is invisible until another operating system reads the disk.

Some drivers write the second FAT lazily, or only on unmount, and that is a defensible tradeoff — but
it means a crash leaves them disagreeing, and the repair tool has to pick one.

---

## 6. Creating and deleting

### 6.1 Create: find a slot

```c
    for (uint32_t i = 0; dir_get_entry(fs, dir->impl, i, &e, &lba, &off); i++) {
        if ((uint8_t)e.name[0] != 0x00 && (uint8_t)e.name[0] != 0xE5) continue;

        memset(&e, 0, sizeof(e));
        memcpy(e.name, n, 8);
        memcpy(e.ext,  x, 3);
        e.attr       = FAT_ATTR_ARCHIVE;
        e.cluster_lo = 0;                /* no data yet */
        e.size       = 0;

        if (!dir_put_entry(fs, lba, off, &e)) return -EIO;
        return 0;
    }

    return -ENOSPC;
```

> Find a free slot: either one marked deleted (`0xE5`) or the first never-used one (`0x00`). Reusing
> deleted slots is what stops a directory growing forever in a create/delete loop.

A file is created with **no clusters** — `cluster_lo = 0`, `size = 0`. The first write allocates
(§3.1).

That is correct and it is why an empty file costs one directory entry and zero clusters, which is why
`touch` is cheap.

### 6.2 The root cannot grow

```c
    /*  The root directory cannot grow -- it is a fixed region. This is the
     *  most visible way FAT16's root differs from a normal directory, and the
     *  reason "cannot create file in root directory" was a real DOS error.    */
    return -ENOSPC;
```

Chapter 41, §2.3. A subdirectory *could* grow by allocating another cluster and linking it; the root
cannot, because it has no chain.

Our `fat_create` does not grow subdirectories either — it returns `-ENOSPC` when the existing entries
are full. Exercise 42.5.

### 6.3 Names that do not fit

```c
static bool name_to_dirent(const char *name, char out_name[8], char out_ext[3])
{
    memset(out_name, ' ', 8);
    memset(out_ext,  ' ', 3);

    int i = 0;
    for (; name[i] && name[i] != '.'; i++) {
        if (i >= 8) return false;              /* too long for 8.3 */
        out_name[i] = (char)toupper((unsigned char)name[i]);
    }
    ...
}
```

`-ENAMETOOLONG` rather than truncation — the same principle as path components
(Chapter 39, §5.1). Truncating `mydocument.txt` to `MYDOCUME.TXT` creates a file the user did not ask
for and can collide with an existing one.

Windows generates `MYDOCU~1.TXT` and stores the real name in LFN entries. We refuse.

### 6.4 Delete is one byte

```c
        if (e.cluster_lo >= 2) fat_free_chain(fs, e.cluster_lo);

        e.name[0] = (char)0xE5;
        if (!dir_put_entry(fs, lba, off, &e)) return -EIO;
```

> Deletion is one byte: replace the first character of the name with `0xE5`. Everything else — the
> size, the starting cluster, the timestamps — stays on disk, which is precisely why undelete tools
> worked on DOS and why "deleted" has never meant "gone".

The data clusters are freed in the FAT but not overwritten. So an undelete tool can:

1. Find the `0xE5` entry.
2. Read `cluster_lo` and `size`, which are still there.
3. Check whether those clusters are still free.
4. Rebuild the chain — if the file was contiguous, which most small files are.

Step 4 is why it often works and sometimes does not: the chain itself was freed, so the tool has to
*guess* that the file occupied consecutive clusters.

The order matters, too: freeing the chain before marking the entry deleted means a crash in between
leaves an entry pointing at clusters marked free — which `chkdsk` reports as a cross-linked file. The
other order leaks the clusters instead. Neither is safe; §7.

---

## 7. What a power cut leaves behind

Going through the five steps from §1, and what a crash after each leaves.

| Crash after | State on disk | Symptom | `chkdsk` says |
|---|---|---|---|
| 1–2 (FAT marked in memory only) | nothing changed | none | — |
| FAT flushed, no directory entry | cluster marked used, nothing points at it | space missing | "lost cluster chain" |
| Directory entry written, FAT not flushed | entry points at a cluster still marked free | file may be overwritten | "cross-linked file" |
| Data written, entry says size 0 | file appears empty | data invisible | — |
| Delete: chain freed, entry not marked | entry points at free clusters | file may be overwritten | "cross-linked file" |
| Delete: entry marked, chain not freed | clusters used by nothing | space missing | "lost cluster chain" |

**Every row is a real failure mode**, and none of them is preventable in FAT.

That is what a journal is for. A journaling filesystem writes its *intentions* to a log first:

```
    BEGIN
    allocate cluster 47
    set FAT[47] = EOC
    set dir entry 3 cluster = 47, size = 100
    COMMIT
```

and only then performs them. After a crash, the log is replayed: either the whole transaction is
there and is redone, or `COMMIT` is missing and it is discarded. Either way the filesystem is
consistent — which is not the same as "no data was lost", only "no structure was corrupted".

ext3 added this to ext2, NTFS had it from the start, and it is the single biggest difference between
a 1980s filesystem and a modern one.

`chkdsk` and `fsck` exist because FAT does not have it. And the reason a FAT volume must be
*cleanly unmounted* — the "dirty" bit in FAT[1] (Chapter 41, §3) — is so the OS knows whether to run
the check.

---

## 8. Running it

```
nimbus> echo hello > /mnt/greeting.txt
nimbus> ls /mnt
greeting.txt
nimbus> cat /mnt/greeting.txt
hello
nimbus> reboot
...
nimbus> cat /mnt/greeting.txt
hello
```

And from the host:

```bash
$ mdir -i bin/disk.img
 Volume in drive : is NIMBUS
Directory for ::/

GREETING TXT         6 2026-01-01  00:00
        1 file                    6 bytes
                         16 764 928 bytes free

$ mtype -i bin/disk.img ::GREETING.TXT
hello
```

`mtools` agrees with us about the name, the size, and the contents. That is the test.

### 8.1 Checking consistency

```bash
$ fsck.fat -v bin/disk.img
fsck.fat 4.2 (2021-01-31)
Checking we can access the last sector of the filesystem
Boot sector contents:
System ID "mkfs.fat"
Media byte 0xf8 (hard disk)
       512 bytes per logical sector
      2048 bytes per cluster
...
Checking for unused clusters.
bin/disk.img: 1 files, 1/8154 clusters
```

No errors. One file, one cluster.

**Run this after every change to the write path.** A driver that produces a volume `fsck.fat`
complains about is a driver with a bug, even if it can read back what it wrote.

### 8.2 Watching the cost

```
nimbus> echo hello > /mnt/a.txt
```

With the sector counters from Exercise 38.2:

```
writes: 68
```

Sixty-eight sector writes for six bytes. Four zeroing the cluster, one for the data, one for the
directory entry, 64 for the two FAT copies (§5).

That number is the argument for the block cache, stated in one measurement.

---

## 9. What could go wrong

| Symptom | Cause |
|---|---|
| New files are zero bytes | Directory entry not updated after write |
| Writing to a new file corrupts the FAT | Empty-file special case missing; wrote to cluster 0 |
| Partial writes zero the rest of the sector | Missing read-modify-write |
| Windows reports errors after Nimbus writes | Only one FAT copy written |
| Space disappears after deletes | Chain not freed |
| `fsck.fat` reports cross-linked files | FAT and directory updated in the wrong order, or not flushed |
| Deleted files still visible | `0xE5` not written, or `readdir` not skipping it |
| Writes are extremely slow | §5 — the whole FAT rewritten per allocation |
| Previous file contents visible in a new file | Not zeroing on allocate |

---

## 10. Exercises

🟢 **42.1** Write a file, then use `fsck.fat -v` on the image and read the cluster count.

🟢 **42.2** Remove the cluster zeroing, delete a file, create a new one, and `hexdump` it.

🟢 **42.3** Add a write counter and reproduce §8.2's 68.

🟡 **42.4** Add a next-free hint to `fat_alloc_cluster`. Measure the time to create 100 files before
and after.

🟡 **42.5** Make `fat_create` grow a subdirectory by allocating another cluster when the entries are
full. Confirm the root still returns `-ENOSPC`.

🟡 **42.6** Implement `truncate`: shorten a file, freeing the clusters past the new end. Then make
`O_TRUNC` in `file_open_node` use it — currently it only sets `length` to 0 and leaks the chain.

🔴 **42.7** Implement a journal: a reserved region of the volume, a transaction log of intended
writes, a commit record, and replay at mount. Then test it by killing QEMU mid-write and confirming
`fsck.fat` is clean afterwards.

🔴 **42.8** Implement long filename *writing*: generate a `~1` short name, build the LFN chain with
the correct checksum, and confirm Windows and `mtools` both show the long name.

---

## What we covered

- Five writes to three regions, none of them atomic.
- Linear cluster search, and the `FSInfo` hint FAT32 added to avoid it.
- Zeroing on allocate rather than on free, and why the distinction closes a window rather than just
  moving work.
- The empty-file special case, and the cluster 0 write you get without it.
- Extending a chain while walking to an offset, and why FAT cannot do sparse files.
- Read-modify-write for partial sectors, and the window it opens.
- A directory entry holding the size and the start cluster, two VFS fields repurposed to find it, and
  the honest fix.
- Sixteen entries per sector, so changing one rewrites fifteen others.
- 64 sector writes per cluster allocation — the worst performance bug in Nimbus — and why the fix
  belongs in the block layer.
- Create with no clusters, a root that cannot grow, and refusing names rather than truncating.
- Delete as one byte, why undelete worked, and why it sometimes did not.
- Six distinct power-cut outcomes, what `chkdsk` calls each, and what a journal does about them.
- `fsck.fat` and `mtools` as the correctness test.

[Chapter 43](43-fds-and-pipes.md) finishes Part V with the layer between a small integer and a file,
and the object that has no file behind it at all.

---

[← FAT16, read](41-fat16-read.md) · [Contents](README.md) · [Next: File descriptors and pipes →](43-fds-and-pipes.md)
