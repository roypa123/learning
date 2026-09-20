# Chapter 38 — Partitions and the block layer

[← The ATA driver](37-ata-driver.md) · [Contents](README.md) · [Next: The VFS →](39-vfs.md)

---

## Goal

Put structure on a disk. Parse a partition table, understand what a block layer is for, and be honest
about the one we do not have — because the cache Nimbus is missing is the single largest performance
decision in this part of the book.

---

## 1. Why a disk has partitions

A disk is a flat array of sectors. A filesystem wants to own a range of them starting at zero and
ending somewhere.

Two filesystems on one disk therefore need an agreement about who owns what, and that agreement lives
in sector 0. It has to, because sector 0 is the only sector whose location everyone knows.

The Master Boot Record does two jobs at once:

```
    offset 0    446 bytes   boot code
    offset 446   64 bytes   four 16-byte partition entries
    offset 510    2 bytes   0x55 0xAA
```

Chapter 5 wrote code that lives in the first 446 bytes. This chapter reads the next 64.

The signature is the same one the BIOS checks (Chapter 4, §3), which is why a partitioned disk is
also bootable and why the two roles share a sector.

---

## 2. A partition entry

```c
typedef struct mbr_partition {
    uint8_t  status;       /* 0x80 = bootable                                 */
    uint8_t  chs_first[3];
    uint8_t  type;         /* 0x04/0x06 = FAT16, 0x0B/0x0C = FAT32, 0x83 = ext2 */
    uint8_t  chs_last[3];
    uint32_t lba_first;    /* the only field anyone has used since 1995       */
    uint32_t sectors;
} PACKED mbr_partition_t;
```

Sixteen bytes, of which four are archaeology.

**`chs_first` and `chs_last`** are the Cylinder/Head/Sector addresses from Chapter 6. Six bytes,
capped at 8.4 GB, and meaningless on any disk made in the last twenty-five years. They are still
written — usually as `0xFE 0xFF 0xFF`, the "too big to express" value — because some very old
software reads them.

**`lba_first` and `sectors`** are what everyone uses. 32 bits of sector number × 512 bytes = **2 TiB**,
which is the limit that pushed the world to GPT around 2010.

**`status`** with bit 7 set means "boot from this one". An MBR boot sector reads it to decide which
partition's boot sector to chain-load.

### 2.1 Four partitions, and the extended partition hack

There is room for exactly four entries.

When that stopped being enough, the fix was an **extended partition**: type `0x05` or `0x0F`, whose
first sector contains *another* partition table, forming a linked list. Each entry in the chain
describes one logical partition and points at the next.

It is ugly, it is fragile, and it worked for twenty years. GPT replaced the whole scheme with a
table of 128 entries, 64-bit LBAs, a CRC, and a backup copy at the end of the disk.

We parse the four primary entries and stop.

### 2.2 Type codes

```c
                if (parts[i].type != 0x04 && parts[i].type != 0x06 &&
                    parts[i].type != 0x0E) continue;
```

| Code | Meaning |
|---|---|
| `0x00` | unused |
| `0x01` | FAT12 |
| `0x04` | FAT16, under 32 MiB |
| `0x05` | extended, CHS |
| `0x06` | FAT16, over 32 MiB |
| `0x07` | NTFS or exFAT |
| `0x0B` | FAT32, CHS |
| `0x0C` | FAT32, LBA |
| `0x0E` | FAT16, LBA |
| `0x83` | Linux |
| `0x82` | Linux swap |
| `0xEE` | GPT protective |

The type is **advisory**. It says what the creator intended; it does not guarantee what is there. A
driver should check the filesystem's own superblock, which is exactly what `fat16_mount` does:

```c
    if (bpb->bytes_per_sector != 512) { ... return NULL; }
    if (bpb->sectors_per_cluster == 0 || bpb->fat_count == 0 ||
        bpb->sectors_per_fat == 0 || bpb->root_entries == 0) { ... return NULL; }
```

Chapter 41, §8. Trusting the type byte and dividing by a zero `sectors_per_cluster` is a
divide-by-zero exception in the kernel, from mounting an unformatted disk.

---

## 3. Reading the table

```c
    ata_device_t *disk = ata_get(0);
    if (disk) {
        uint8_t mbr[512];

        if (ata_read_sectors(disk, 0, 1, mbr) == 0 &&
            mbr[510] == 0x55 && mbr[511] == 0xAA) {

            mbr_partition_t *parts = (mbr_partition_t *)(mbr + 446);

            for (int i = 0; i < 4; i++) {
                if (parts[i].type != 0x04 && parts[i].type != 0x06 &&
                    parts[i].type != 0x0E) continue;

                kprintf("mbr: partition %d, type %02x, %u sectors at LBA %u\n",
                        i, parts[i].type, parts[i].sectors, parts[i].lba_first);

                vfs_node_t *fat = fat16_mount(disk, parts[i].lba_first);
                ...
                break;
            }
        } else {
            vfs_node_t *fat = fat16_mount(disk, 0);
            ...
        }
    }
```

### 3.1 The cast at offset 446

```c
    mbr_partition_t *parts = (mbr_partition_t *)(mbr + 446);
```

This works because `mbr_partition_t` is `PACKED` (16 bytes, no padding) and because x86 permits
unaligned access — 446 is not a multiple of 4, so `lba_first` at offset 446+8 = 454 is 2-byte
aligned at best.

On ARM this would fault. The portable version reads the bytes individually:

```c
    uint32_t lba = mbr[454] | (mbr[455] << 8) | (mbr[456] << 16) | (mbr[457] << 24);
```

Worth knowing which shortcut you are taking. Chapter 2, §8.

### 3.2 The unpartitioned fallback

```c
        } else {
            /*  No partition table. Try the whole disk as one volume, which is
             *  how a floppy is formatted and how `mkfs.fat image.img` leaves
             *  a raw image.                                                   */
            vfs_node_t *fat = fat16_mount(disk, 0);
```

`mkfs.fat` on a raw file produces a filesystem starting at sector 0 with no partition table. So does
formatting a floppy.

The signature check distinguishes them — a FAT boot record also ends in `0x55 0xAA`, so the
distinction is not perfect, and the real answer is to try the partition table and fall back if
nothing plausible is found. Ours is the cheap version and it works for both cases we care about.

### 3.3 `part_lba` as an offset on every access

```c
static int read_sector(fat_fs_t *fs, uint32_t lba, void *buf)
{
    return ata_read_sectors(fs->dev, fs->part_lba + lba, 1, buf);
}
```

The filesystem thinks in sectors from 0; the disk thinks in absolute LBAs. One addition, in one
place, and the rest of the FAT16 driver never knows partitions exist.

That is what a block layer is *for*, and it is the whole of ours.

---

## 4. What a real block layer does

Ours is one function. A real one sits between filesystems and drivers and does five things.

### 4.1 Caching

**The big one.** Every `read_sector` in `fat16.c` is a real disk read:

```c
    for (uint32_t i = 0; dir_get_entry(fs, node->impl, i, &e, NULL, NULL); i++) {
```

`dir_get_entry` reads a sector. Scanning a 32-entry directory reads the *same* sector twice per
entry — once for the name check, once when the caller re-reads it.

With PIO at a microsecond per word, one sector read is about 300 µs. A directory scan that should
take microseconds takes milliseconds.

A cache of recently used sectors, keyed by LBA, would collapse almost all of it. Thirty lines:

```c
typedef struct buf {
    uint32_t lba;
    bool     valid;
    bool     dirty;
    uint32_t last_used;
    uint8_t  data[512];
} buf_t;

static buf_t cache[64];
```

with LRU replacement. 32 KiB of memory, and directory operations become memory-speed.

Exercise 38.5, and it is the single highest-value change available in Part V.

### 4.2 Write-back

A cache that writes through on every store is still doing one disk write per change. Write-*back*
marks the buffer dirty and flushes later — on eviction, on `sync`, or on a timer.

That turns the FAT update pattern in `fat_flush` from:

```c
    for (uint8_t copy = 0; copy < fs->fat_count; copy++) {
        uint32_t base = fs->fat_lba + (uint32_t)copy * fs->sectors_per_fat;
        for (uint16_t s = 0; s < fs->sectors_per_fat; s++)
            write_sector(fs, base + s, bytes + (size_t)s * fs->bytes_per_sector);
    }
```

— which writes the *entire* FAT, both copies, on every allocation — into writing the one sector that
changed.

For a 16 MiB volume the FAT is 64 sectors, mirrored, so every single-cluster allocation currently
costs 128 sector writes. That is the worst performance bug in Nimbus and the cache is what fixes it.

The cost of write-back is that data can be lost on a crash, which is why `sync()` exists and why
filesystems need barriers.

### 4.3 Request merging and reordering

Consecutive reads of LBA 100, 101, 102 are three commands. One command for three sectors is faster,
because the per-command overhead dominates.

And on a spinning disk, servicing requests in *track order* rather than arrival order can be an order
of magnitude faster — which is what elevator algorithms are for, and why Linux has a pluggable I/O
scheduler.

On an SSD neither matters much, which is why the modern default scheduler is close to "do nothing".

### 4.4 A uniform device interface

`fat16_mount` takes an `ata_device_t *`. It should take a "block device": something with
`read(lba, count, buf)` and `write(...)`, whatever is behind it.

Then FAT16 would work on a RAM disk, a floppy, a network block device, or a file on another
filesystem — with no change.

That is the same argument as the VFS (Chapter 39), one layer down. Exercise 38.6.

### 4.5 Partition abstraction

Ours is `part_lba +`. A real one presents each partition as its own block device with its own sector
numbering, so the filesystem driver never sees an offset at all.

---

## 5. Sector size, and the 4K transition

```c
    if (bpb->bytes_per_sector != 512) {
        LOG_ERR("fat16: bytes_per_sector is %u, we only handle 512", ...);
        return NULL;
    }
```

512 bytes has been the sector size since 1956. Around 2010, drives moved to **4096-byte physical
sectors** — better error correction overhead, more usable capacity — while continuing to *report*
512-byte logical sectors for compatibility.

That is "512e", and it has a consequence: a 512-byte write to such a drive is a
read-modify-write of a 4096-byte physical sector. If a filesystem's structures are not 4096-aligned,
every write costs an extra read.

This is why partition tools since about 2011 align partitions to 1 MiB rather than to a cylinder
boundary, and why an old tool creating a partition at LBA 63 produces a volume that is measurably
slower forever.

Nimbus hardcodes 512 everywhere. Making it variable is mostly replacing constants; making it
*correct* on 512e hardware means aligning the filesystem, which is Exercise 38.7.

---

## 6. Running it

```bash
make disk
run nimbus
```

```
ata0: QEMU HARDDISK, 16 MiB (32768 sectors)
fat16: 16384 KiB volume, 8154 clusters of 2048 bytes, 512 root entries
[    0.430] inf  vfs: mounted fat16 at /mnt
```

```
nimbus> ls /mnt
(empty)
nimbus> echo hello > /mnt/greeting.txt
nimbus> ls /mnt
greeting.txt
nimbus> cat /mnt/greeting.txt
hello
```

A file, on a disk, that survives a reboot:

```
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
```

The file is readable by a tool that knows nothing about Nimbus. That is the proof that the format was
implemented rather than invented.

### 6.1 With a partition table

```bash
$ fdisk bin/disk.img
  n, p, 1, <enter>, <enter>, t, 6, w
$ losetup -o $((2048*512)) /dev/loop0 bin/disk.img
$ mkfs.fat -F 16 /dev/loop0
```

```
mbr: partition 0, type 06, 30720 sectors at LBA 2048
fat16: 15360 KiB volume, 7640 clusters of 2048 bytes, 512 root entries
```

LBA 2048 is the 1 MiB alignment from §5.

### 6.2 Measuring the missing cache

```c
    uint32_t reads_before = ata_read_count;
    vfs_node_t *n = vfs_lookup("/mnt/greeting.txt");
    kprintf("lookup cost %u sector reads\n", ata_read_count - reads_before);
```

```
lookup cost 14 sector reads
```

Fourteen reads to find one file in a root directory with one entry. At 300 µs each that is 4 ms —
for a lookup that should be instant.

That number is the argument for §4.1, and watching it drop to 1 after implementing the cache is the
most satisfying exercise in Part V.

---

## 7. What could go wrong

| Symptom | Cause |
|---|---|
| Partition not found | Type code not in the accepted list |
| Mount fails on a valid partition | `part_lba` not added in `read_sector` |
| Garbage filesystem data | Read from LBA 0 instead of the partition start |
| Divide by zero in the kernel | Trusted the type byte without validating the BPB |
| Works on a raw image, not a partitioned one | Fallback path taken when it should not be |
| Unaligned access fault (non-x86) | The cast at offset 446 |
| Writes are extremely slow | No block cache; the whole FAT is rewritten per allocation |
| Data lost on reboot | No cache flush in the ATA driver (Ch. 37, §7.1) |

---

## 8. Exercises

🟢 **38.1** Print all four partition entries of a partitioned image, including the CHS fields, and
confirm they are `0xFE 0xFF 0xFF`.

🟢 **38.2** Add an `ata_read_count` and `ata_write_count`, print them in the `mem` command, and watch
them during `ls /mnt`.

🟢 **38.3** Read the partition table with individual byte loads instead of the struct cast.

🟡 **38.4** Handle extended partitions: follow the chain from a type `0x05` entry and list the
logical partitions.

🟡 **38.5** Implement the block cache from §4.1: 64 buffers, LRU, write-through first. Measure the
lookup cost from §6.2 before and after.

🟡 **38.6** Introduce a `block_device_t` with function pointers, make `fat16_mount` take one, and
write a RAM-disk implementation. Then mount a FAT16 image from the initrd.

🔴 **38.7** Add write-back to the cache, plus a `sync()` syscall and a flush on a timer. Then work out
what guarantees FAT16 needs about ordering, and whether write-back breaks any of them.

🔴 **38.8** Implement GPT: read the header at LBA 1, verify the CRC32, walk the entry array, and
handle the protective MBR. Then make the mount code prefer GPT when the protective MBR is present.

---

## What we covered

- Why partitions exist, and why the table lives in the sector that also holds boot code.
- A 16-byte entry of which six bytes are archaeology, and the 2 TiB limit in the four that are not.
- Four entries, the extended-partition linked list that got round it, and what GPT replaced.
- Type codes as advisory, and the divide-by-zero you get from trusting one.
- A `PACKED` cast at an unaligned offset, and the portable version.
- The unpartitioned fallback, and why `mkfs.fat` on a raw file needs it.
- `part_lba +` in one function as the entirety of our block layer.
- The five things a real block layer does, and the measured cost of not having the first two — 128
  sector writes per cluster allocation, and 14 reads for one lookup.
- 512e drives, 1 MiB partition alignment, and why a partition at LBA 63 is permanently slower.
- A file written by Nimbus, read by `mdir`, surviving a reboot.

[Chapter 39](39-vfs.md) builds the layer that makes all of this interchangeable.

---

[← The ATA driver](37-ata-driver.md) · [Contents](README.md) · [Next: The VFS →](39-vfs.md)
