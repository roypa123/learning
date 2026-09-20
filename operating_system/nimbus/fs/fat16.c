/* ===========================================================================
 *  nimbus/fs/fat16.c  --  a real filesystem, read and write
 * ===========================================================================
 *
 *  FAT is the simplest filesystem that is genuinely in use, and its central
 *  idea fits in one sentence: the disk is divided into fixed-size clusters,
 *  and a single array -- the File Allocation Table -- says, for each cluster,
 *  which cluster comes next.
 *
 *      FAT[5] = 6      cluster 5 is followed by cluster 6
 *      FAT[6] = 9      then cluster 9
 *      FAT[9] = 0xFFFF end of file
 *      FAT[7] = 0      cluster 7 is free
 *
 *  A file is therefore a linked list, and the list's "next" pointers all live
 *  together in one array near the front of the disk instead of being scattered
 *  with the data. That is the whole design. Its consequences are worth
 *  noticing as you read the code:
 *
 *    * Seeking to byte 1,000,000 of a file means walking the chain from the
 *      beginning -- O(n). Every modern filesystem uses extents or a tree
 *      instead, for exactly this reason.
 *    * Losing the FAT loses every file, which is why there are two copies.
 *    * There is no journal, so a power cut between "allocate a cluster" and
 *      "write the directory entry" leaks the cluster permanently. That is what
 *      `chkdsk` spends its time finding.
 *
 *  Explained in: docs/41-fat16-read.md and docs/42-fat16-write.md
 *  Line by line: docs/line-by-line/nimbus-fat16.md
 * =========================================================================== */

#include <nimbus/fs.h>
#include <nimbus/vfs.h>
#include <nimbus/ata.h>
#include <nimbus/heap.h>
#include <nimbus/kernel.h>
#include <nimbus/string.h>
#include <nimbus/syscall.h>

typedef struct fat_fs {
    ata_device_t *dev;
    uint32_t      part_lba;        /* where the partition starts on the disk  */

    uint16_t      bytes_per_sector;
    uint8_t       sectors_per_cluster;
    uint16_t      reserved_sectors;
    uint8_t       fat_count;
    uint16_t      root_entries;
    uint16_t      sectors_per_fat;
    uint32_t      total_sectors;

    uint32_t      fat_lba;         /* first sector of the first FAT           */
    uint32_t      root_lba;        /* first sector of the root directory      */
    uint32_t      data_lba;        /* first sector of cluster 2               */
    uint32_t      cluster_count;

    uint16_t     *fat;             /* the whole FAT, cached in memory         */
    bool          fat_dirty;
} fat_fs_t;

/*  The root directory of FAT16 is not a normal directory: it is a fixed-size
 *  region at a fixed place, it cannot grow, and it has no cluster number. We
 *  mark it with this sentinel in a node's `impl` field, and every directory
 *  operation has one branch for it. FAT32 removed the special case by making
 *  the root an ordinary cluster chain -- which is most of what FAT32 changed. */
#define ROOT_DIR_SENTINEL 0xFFFFFFFFu

static dirent_t shared_dirent;

/* ===========================================================================
 *  Sector and cluster I/O
 * =========================================================================== */

static int read_sector(fat_fs_t *fs, uint32_t lba, void *buf)
{
    return ata_read_sectors(fs->dev, fs->part_lba + lba, 1, buf);
}

static int write_sector(fat_fs_t *fs, uint32_t lba, const void *buf)
{
    return ata_write_sectors(fs->dev, fs->part_lba + lba, 1, buf);
}

/*  Cluster numbering starts at 2. Clusters 0 and 1 do not exist on the disk;
 *  their FAT entries hold the media descriptor and a dirty flag instead. So
 *  the data area's first sector is cluster *2*, and forgetting the subtraction
 *  reads every file two clusters too late -- a bug that produces plausible
 *  looking garbage rather than an obvious failure.                            */
static uint32_t cluster_to_lba(fat_fs_t *fs, uint16_t cluster)
{
    return fs->data_lba + (uint32_t)(cluster - 2) * fs->sectors_per_cluster;
}

static uint16_t fat_get(fat_fs_t *fs, uint16_t cluster)
{
    if (cluster >= fs->cluster_count + 2) return FAT_EOC;
    return fs->fat[cluster];
}

static void fat_set(fat_fs_t *fs, uint16_t cluster, uint16_t value)
{
    if (cluster >= fs->cluster_count + 2) return;
    fs->fat[cluster] = value;
    fs->fat_dirty = true;
}

/*  Write the in-memory FAT back to the disk -- to *both* copies.
 *
 *  Writing only the first copy is a bug that nothing will notice until a real
 *  DOS or Windows machine reads the disk, decides the two FATs disagree, and
 *  either repairs from the wrong one or refuses to mount.                     */
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

/*  Find a free cluster, claim it, and return it. Linear search from the start
 *  every time: fine for a small disk, and the reason a heavily used FAT volume
 *  gets slow to write to. Real implementations cache a "next free" hint,
 *  exactly like our physical frame allocator does.                            */
static uint16_t fat_alloc_cluster(fat_fs_t *fs)
{
    for (uint16_t c = 2; c < fs->cluster_count + 2; c++) {
        if (fs->fat[c] == FAT_FREE_CLUSTER) {
            fat_set(fs, c, FAT_EOC);

            /*  Zero it. A freshly allocated cluster otherwise contains the
             *  previous file's data, which would be visible to whoever
             *  allocates it next -- a genuine information leak, and the reason
             *  every filesystem zeroes on allocate rather than on free.       */
            uint8_t zeros[512];
            memset(zeros, 0, sizeof(zeros));
            for (uint8_t s = 0; s < fs->sectors_per_cluster; s++)
                write_sector(fs, cluster_to_lba(fs, c) + s, zeros);

            return c;
        }
    }
    return 0;      /* 0 is never a valid data cluster, so it works as "full" */
}

static void fat_free_chain(fat_fs_t *fs, uint16_t start)
{
    uint16_t c = start;

    while (c >= 2 && c < FAT_BAD_CLUSTER) {
        uint16_t next = fat_get(fs, c);
        fat_set(fs, c, FAT_FREE_CLUSTER);
        c = next;
    }
    fat_flush(fs);
}

/* ===========================================================================
 *  8.3 names
 *
 *  On disk: "README  TXT" -- eight characters of name, three of extension,
 *  space padded, upper case, and no dot. In the world: "readme.txt".
 * =========================================================================== */

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

static bool name_to_dirent(const char *name, char out_name[8], char out_ext[3])
{
    memset(out_name, ' ', 8);
    memset(out_ext,  ' ', 3);

    int i = 0;
    for (; name[i] && name[i] != '.'; i++) {
        if (i >= 8) return false;              /* too long for 8.3 */
        out_name[i] = (char)toupper((unsigned char)name[i]);
    }

    if (name[i] == '.') {
        i++;
        for (int j = 0; name[i]; i++, j++) {
            if (j >= 3) return false;
            out_ext[j] = (char)toupper((unsigned char)name[i]);
        }
    }
    return true;
}

/* ===========================================================================
 *  Walking a directory
 *
 *  One function serves the root directory and every subdirectory, because the
 *  only difference is where the entries live. It returns the entry *and* the
 *  sector and offset it came from, so that a caller who wants to modify it can
 *  write it straight back.
 * =========================================================================== */
static bool dir_get_entry(fat_fs_t *fs, uint32_t dir_cluster, uint32_t index,
                          fat_dirent_t *out, uint32_t *out_lba, uint32_t *out_off)
{
    uint32_t per_sector = fs->bytes_per_sector / sizeof(fat_dirent_t);
    uint8_t  sector[512];

    if (dir_cluster == ROOT_DIR_SENTINEL) {
        uint32_t root_sectors =
            ((uint32_t)fs->root_entries * sizeof(fat_dirent_t) + fs->bytes_per_sector - 1)
            / fs->bytes_per_sector;

        if (index >= fs->root_entries) return false;

        uint32_t s   = index / per_sector;
        uint32_t off = index % per_sector;
        if (s >= root_sectors) return false;

        if (read_sector(fs, fs->root_lba + s, sector) < 0) return false;

        memcpy(out, sector + off * sizeof(fat_dirent_t), sizeof(fat_dirent_t));
        if (out_lba) *out_lba = fs->root_lba + s;
        if (out_off) *out_off = off * sizeof(fat_dirent_t);
        return true;
    }

    /*  A subdirectory: walk the cluster chain, counting entries.              */
    uint32_t per_cluster = per_sector * fs->sectors_per_cluster;
    uint16_t cluster     = (uint16_t)dir_cluster;
    uint32_t skip        = index;

    while (cluster >= 2 && cluster < FAT_EOC) {
        if (skip < per_cluster) {
            uint32_t s   = skip / per_sector;
            uint32_t off = skip % per_sector;
            uint32_t lba = cluster_to_lba(fs, cluster) + s;

            if (read_sector(fs, lba, sector) < 0) return false;

            memcpy(out, sector + off * sizeof(fat_dirent_t), sizeof(fat_dirent_t));
            if (out_lba) *out_lba = lba;
            if (out_off) *out_off = off * sizeof(fat_dirent_t);
            return true;
        }
        skip -= per_cluster;
        cluster = fat_get(fs, cluster);
    }
    return false;
}

static bool dir_put_entry(fat_fs_t *fs, uint32_t lba, uint32_t off,
                          const fat_dirent_t *e)
{
    uint8_t sector[512];

    /*  Read, modify, write. A directory entry is 32 bytes and the smallest
     *  thing a disk can write is 512, so there is no alternative -- and this
     *  is exactly the read-modify-write window that makes a filesystem need a
     *  journal to survive a power cut.                                        */
    if (read_sector(fs, lba, sector) < 0) return false;
    memcpy(sector + off, e, sizeof(fat_dirent_t));
    return write_sector(fs, lba, sector) >= 0;
}

/* ===========================================================================
 *  VFS node operations
 * =========================================================================== */

static vfs_node_t *make_node(fat_fs_t *fs, const fat_dirent_t *e, const char *name);

static ssize_t fat_read(vfs_node_t *node, off_t offset, size_t size, uint8_t *buf)
{
    fat_fs_t *fs = (fat_fs_t *)node->device;

    if (offset >= node->length) return 0;
    if (offset + size > node->length) size = node->length - offset;
    if (size == 0) return 0;

    uint32_t cluster_bytes = (uint32_t)fs->sectors_per_cluster * fs->bytes_per_sector;
    uint16_t cluster = (uint16_t)node->impl;

    /*  Skip to the cluster containing `offset`. This is the O(n) seek that the
     *  FAT design forces: to reach byte 1,000,000 we follow a thousand links.
     *  A cache of the last (offset, cluster) pair turns sequential reading
     *  back into O(1) per call, and is the single highest-value optimisation
     *  you can make to this driver.                                           */
    uint32_t skip = (uint32_t)offset / cluster_bytes;
    while (skip-- && cluster >= 2 && cluster < FAT_EOC)
        cluster = fat_get(fs, cluster);

    uint32_t within = (uint32_t)offset % cluster_bytes;
    size_t   done   = 0;
    uint8_t  sector[512];

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

    return (ssize_t)done;
}

static ssize_t fat_write(vfs_node_t *node, off_t offset, size_t size, const uint8_t *buf)
{
    fat_fs_t *fs = (fat_fs_t *)node->device;

    if (size == 0) return 0;

    uint32_t cluster_bytes = (uint32_t)fs->sectors_per_cluster * fs->bytes_per_sector;

    /*  An empty file has no clusters at all -- cluster 0 in its directory
     *  entry. The first write has to allocate the first one, which is a
     *  special case because there is no previous cluster to link from.        */
    uint16_t cluster = (uint16_t)node->impl;
    if (cluster < 2) {
        cluster = fat_alloc_cluster(fs);
        if (!cluster) return -ENOSPC;
        node->impl = cluster;
    }

    /*  Walk to the cluster holding `offset`, extending the chain if the write
     *  starts past the end of the allocated space.                            */
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

    uint32_t within = (uint32_t)offset % cluster_bytes;
    size_t   done   = 0;
    uint8_t  sector[512];

    while (done < size) {
        uint32_t s   = within / fs->bytes_per_sector;
        uint32_t off = within % fs->bytes_per_sector;

        for (; s < fs->sectors_per_cluster && done < size; s++, off = 0) {
            uint32_t lba   = cluster_to_lba(fs, cluster) + s;
            uint32_t chunk = fs->bytes_per_sector - off;
            if (chunk > size - done) chunk = (uint32_t)(size - done);

            /*  A partial sector must be read before it is written, or the
             *  bytes we are not changing become zeros.                        */
            if (chunk != fs->bytes_per_sector) {
                if (read_sector(fs, lba, sector) < 0) return done ? (ssize_t)done : -EIO;
            }

            memcpy(sector + off, buf + done, chunk);

            if (write_sector(fs, lba, sector) < 0) return done ? (ssize_t)done : -EIO;
            done += chunk;
        }

        if (done >= size) break;

        uint16_t next = fat_get(fs, cluster);
        if (next < 2 || next >= FAT_EOC) {
            next = fat_alloc_cluster(fs);
            if (!next) break;
            fat_set(fs, cluster, next);
        }
        cluster = next;
        within  = 0;
    }

    if (offset + done > node->length)
        node->length = (uint32_t)(offset + done);

    /*  Update the on-disk directory entry: the size and the starting cluster
     *  both live there, not in the FAT, and a file whose data is written but
     *  whose entry is not updated appears to be zero bytes long.              */
    fat_dirent_t e;
    uint32_t lba, off;
    if (dir_get_entry(fs, node->inode, node->permissions, &e, &lba, &off)) {
        e.size       = node->length;
        e.cluster_lo = (uint16_t)node->impl;
        dir_put_entry(fs, lba, off, &e);
    }

    fat_flush(fs);
    return (ssize_t)done;
}

static dirent_t *fat_readdir(vfs_node_t *node, uint32_t index)
{
    fat_fs_t *fs = (fat_fs_t *)node->device;

    fat_dirent_t e;
    uint32_t     slot = 0;

    /*  `index` counts *visible* entries; `slot` counts raw 32-byte records.
     *  They differ because of deleted entries, long-filename records and the
     *  volume label, all of which we skip.                                    */
    for (uint32_t i = 0; dir_get_entry(fs, node->impl, i, &e, NULL, NULL); i++) {
        if ((uint8_t)e.name[0] == 0x00) break;        /* no more entries ever  */
        if ((uint8_t)e.name[0] == 0xE5) continue;     /* deleted               */
        if (e.attr == FAT_ATTR_LFN) continue;         /* long filename record  */
        if (e.attr & FAT_ATTR_VOLUME_ID) continue;    /* the volume label      */

        if (slot++ != index) continue;

        name_from_dirent(&e, shared_dirent.name);
        shared_dirent.inode = e.cluster_lo;
        shared_dirent.type  = (e.attr & FAT_ATTR_DIRECTORY) ? VFS_DIRECTORY : VFS_FILE;
        return &shared_dirent;
    }
    return NULL;
}

static vfs_node_t *fat_finddir(vfs_node_t *node, const char *name)
{
    fat_fs_t *fs = (fat_fs_t *)node->device;

    fat_dirent_t e;
    char         candidate[VFS_NAME_MAX];

    for (uint32_t i = 0; dir_get_entry(fs, node->impl, i, &e, NULL, NULL); i++) {
        if ((uint8_t)e.name[0] == 0x00) break;
        if ((uint8_t)e.name[0] == 0xE5) continue;
        if (e.attr == FAT_ATTR_LFN) continue;
        if (e.attr & FAT_ATTR_VOLUME_ID) continue;

        name_from_dirent(&e, candidate);

        /*  Case-insensitive, because FAT upper-cases everything on disk. That
         *  makes "Makefile" and "MAKEFILE" the same file, which is correct for
         *  FAT and a constant source of surprise for anyone who develops on it
         *  and deploys to a case-sensitive filesystem.                        */
        if (strcasecmp(candidate, name) != 0) continue;

        vfs_node_t *found = make_node(fs, &e, candidate);
        if (found) found->permissions = i;   /* remember which slot, for write */
        return found;
    }
    return NULL;
}

static int fat_create(vfs_node_t *dir, const char *name, uint32_t flags UNUSED)
{
    fat_fs_t *fs = (fat_fs_t *)dir->device;

    char n[8], x[3];
    if (!name_to_dirent(name, n, x)) return -ENAMETOOLONG;

    fat_dirent_t e;
    uint32_t     lba, off;

    /*  Find a free slot: either one marked deleted (0xE5) or the first
     *  never-used one (0x00). Reusing deleted slots is what stops a directory
     *  growing forever in a create/delete loop.                               */
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

    /*  The root directory cannot grow -- it is a fixed region. This is the
     *  most visible way FAT16's root differs from a normal directory, and the
     *  reason "cannot create file in root directory" was a real DOS error.    */
    return -ENOSPC;
}

static int fat_unlink(vfs_node_t *dir, const char *name)
{
    fat_fs_t *fs = (fat_fs_t *)dir->device;

    fat_dirent_t e;
    uint32_t     lba, off;
    char         candidate[VFS_NAME_MAX];

    for (uint32_t i = 0; dir_get_entry(fs, dir->impl, i, &e, &lba, &off); i++) {
        if ((uint8_t)e.name[0] == 0x00) break;
        if ((uint8_t)e.name[0] == 0xE5) continue;
        if (e.attr == FAT_ATTR_LFN) continue;

        name_from_dirent(&e, candidate);
        if (strcasecmp(candidate, name) != 0) continue;

        if (e.attr & FAT_ATTR_DIRECTORY) return -EISDIR;

        if (e.cluster_lo >= 2) fat_free_chain(fs, e.cluster_lo);

        /*  Deletion is one byte: replace the first character of the name with
         *  0xE5. Everything else -- the size, the starting cluster, the
         *  timestamps -- stays on disk, which is precisely why undelete tools
         *  worked on DOS and why "deleted" has never meant "gone".            */
        e.name[0] = (char)0xE5;
        if (!dir_put_entry(fs, lba, off, &e)) return -EIO;

        fat_flush(fs);
        return 0;
    }
    return -ENOENT;
}

static vfs_node_t *make_node(fat_fs_t *fs, const fat_dirent_t *e, const char *name)
{
    vfs_node_t *n = (vfs_node_t *)kcalloc(1, sizeof(vfs_node_t));
    if (!n) return NULL;

    strlcpy(n->name, name, VFS_NAME_MAX);
    n->device   = fs;
    n->refcount = 1;
    n->length   = e->size;

    if (e->attr & FAT_ATTR_DIRECTORY) {
        n->flags   = VFS_DIRECTORY;
        n->impl    = e->cluster_lo;
        n->readdir = fat_readdir;
        n->finddir = fat_finddir;
        n->create  = fat_create;
        n->unlink  = fat_unlink;
    } else {
        n->flags = VFS_FILE;
        n->impl  = e->cluster_lo;
        n->read  = fat_read;
        n->write = fat_write;
    }
    return n;
}

/* ===========================================================================
 *  Mounting
 * =========================================================================== */
vfs_node_t *fat16_mount(ata_device_t *dev, uint32_t partition_lba)
{
    uint8_t boot[512];

    if (ata_read_sectors(dev, partition_lba, 1, boot) < 0) {
        LOG_ERR("fat16: could not read the boot sector at LBA %u", partition_lba);
        return NULL;
    }

    fat_bpb_t *bpb = (fat_bpb_t *)boot;

    /*  Sanity checks before we trust a single field. A disk that is not FAT16
     *  will happily produce a bytes_per_sector of 0 and a sectors_per_cluster
     *  of 0, and the first division by either is a divide-by-zero exception in
     *  the kernel -- from mounting an unformatted disk.                       */
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

    fat_fs_t *fs = (fat_fs_t *)kcalloc(1, sizeof(fat_fs_t));
    if (!fs) return NULL;

    fs->dev              = dev;
    fs->part_lba         = partition_lba;
    fs->bytes_per_sector = bpb->bytes_per_sector;
    fs->sectors_per_cluster = bpb->sectors_per_cluster;
    fs->reserved_sectors = bpb->reserved_sectors;
    fs->fat_count        = bpb->fat_count;
    fs->root_entries     = bpb->root_entries;
    fs->sectors_per_fat  = bpb->sectors_per_fat;
    fs->total_sectors    = bpb->total_sectors_16 ? bpb->total_sectors_16
                                                 : bpb->total_sectors_32;

    /*  The four regions of a FAT volume, in order, each starting where the
     *  previous one ends. Everything in the driver is derived from these.
     *
     *      [reserved][FAT 1][FAT 2][root directory][data...]
     */
    fs->fat_lba  = fs->reserved_sectors;
    fs->root_lba = fs->fat_lba + (uint32_t)fs->fat_count * fs->sectors_per_fat;

    uint32_t root_sectors =
        ((uint32_t)fs->root_entries * sizeof(fat_dirent_t) + 511) / 512;

    fs->data_lba = fs->root_lba + root_sectors;
    fs->cluster_count = (fs->total_sectors - fs->data_lba) / fs->sectors_per_cluster;

    /*  Cache the entire FAT. For a 64 MiB volume with 2 KiB clusters that is
     *  32,768 entries = 64 KiB of RAM, which is a bargain: without it, every
     *  single chain step would be a disk read.                                */
    size_t fat_bytes = (size_t)fs->sectors_per_fat * fs->bytes_per_sector;
    fs->fat = (uint16_t *)kmalloc(fat_bytes);
    if (!fs->fat) { kfree(fs); return NULL; }

    for (uint16_t s = 0; s < fs->sectors_per_fat; s++) {
        if (read_sector(fs, fs->fat_lba + s,
                        (uint8_t *)fs->fat + (size_t)s * fs->bytes_per_sector) < 0) {
            LOG_ERR("fat16: failed reading the FAT");
            kfree(fs->fat); kfree(fs);
            return NULL;
        }
    }

    vfs_node_t *root = (vfs_node_t *)kcalloc(1, sizeof(vfs_node_t));
    if (!root) { kfree(fs->fat); kfree(fs); return NULL; }

    strlcpy(root->name, "fat16", VFS_NAME_MAX);
    root->flags    = VFS_DIRECTORY;
    root->impl     = ROOT_DIR_SENTINEL;
    root->device   = fs;
    root->readdir  = fat_readdir;
    root->finddir  = fat_finddir;
    root->create   = fat_create;
    root->unlink   = fat_unlink;
    root->refcount = 1;

    kprintf("fat16: %u KiB volume, %u clusters of %u bytes, %u root entries\n",
            (fs->total_sectors * 512) / 1024,
            fs->cluster_count,
            (uint32_t)fs->sectors_per_cluster * 512,
            fs->root_entries);

    return root;
}
