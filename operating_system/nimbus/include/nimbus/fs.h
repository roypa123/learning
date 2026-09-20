/* ===========================================================================
 *  nimbus/include/nimbus/fs.h  --  the filesystem drivers and the fd layer
 * ===========================================================================
 *  Explained in: docs/40-initrd.md, docs/41-fat16-read.md,
 *                docs/42-fat16-write.md, docs/43-fds-and-pipes.md
 * =========================================================================== */
#ifndef NIMBUS_FS_H
#define NIMBUS_FS_H

#include <nimbus/types.h>
#include <nimbus/vfs.h>
#include <nimbus/ata.h>

/* ---------------------------------------------------------------------------
 *  initrd: a tar archive loaded into RAM by the bootloader
 *
 *  tar is a wonderful bootstrap format. There is no index, no compression and
 *  no alignment cleverness: a 512-byte header of plain ASCII fields, then the
 *  file contents rounded up to 512 bytes, then the next header. You can write
 *  a reader in eighty lines and a writer in forty, and every machine on earth
 *  already has a tool that produces one.
 * ------------------------------------------------------------------------- */
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

vfs_node_t *initrd_init(paddr_t start, size_t length);

/* ---------------------------------------------------------------------------
 *  MBR partitions
 * ------------------------------------------------------------------------- */
typedef struct mbr_partition {
    uint8_t  status;       /* 0x80 = bootable                                 */
    uint8_t  chs_first[3];
    uint8_t  type;         /* 0x04/0x06 = FAT16, 0x0B/0x0C = FAT32, 0x83 = ext2 */
    uint8_t  chs_last[3];
    uint32_t lba_first;    /* the only field anyone has used since 1995       */
    uint32_t sectors;
} PACKED mbr_partition_t;

/* ---------------------------------------------------------------------------
 *  FAT16
 *
 *  The boot sector doubles as the filesystem superblock, which is why this
 *  structure starts with a jump instruction: bytes 0..2 are real x86 code, and
 *  the fields the driver cares about start at offset 11 so that the boot code
 *  can jump over them.
 * ------------------------------------------------------------------------- */
typedef struct fat_bpb {
    uint8_t  jump[3];
    char     oem[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  fat_count;            /* almost always 2: the FAT is mirrored    */
    uint16_t root_entries;         /* FAT16 has a fixed-size root directory   */
    uint16_t total_sectors_16;
    uint8_t  media_type;
    uint16_t sectors_per_fat;
    uint16_t sectors_per_track;
    uint16_t heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    uint8_t  drive_number;
    uint8_t  reserved;
    uint8_t  boot_signature;
    uint32_t volume_id;
    char     volume_label[11];
    char     fs_type[8];           /* "FAT16   " -- documentation, not truth  */
} PACKED fat_bpb_t;

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

#define FAT_ATTR_READONLY  0x01
#define FAT_ATTR_HIDDEN    0x02
#define FAT_ATTR_SYSTEM    0x04
#define FAT_ATTR_VOLUME_ID 0x08
#define FAT_ATTR_DIRECTORY 0x10
#define FAT_ATTR_ARCHIVE   0x20
#define FAT_ATTR_LFN       0x0F    /* the long-filename hack: an entry whose
                                      attribute byte is this is not a file at
                                      all, and old DOS skipped it because no
                                      real file is read-only + hidden + system
                                      + volume-label at once.                 */

#define FAT_FREE_CLUSTER   0x0000
#define FAT_BAD_CLUSTER    0xFFF7
#define FAT_EOC            0xFFF8  /* >= this means end of chain              */

vfs_node_t *fat16_mount(ata_device_t *dev, uint32_t partition_lba);

/* ---------------------------------------------------------------------------
 *  File descriptors
 * ------------------------------------------------------------------------- */
int      fd_alloc(file_t *f);
file_t  *fd_get(int fd);
int      fd_close(int fd);
file_t  *file_open_node(vfs_node_t *node, uint32_t flags);
void     file_ref(file_t *f);
void     file_unref(file_t *f);

/* ---------------------------------------------------------------------------
 *  Pipes
 * ------------------------------------------------------------------------- */
#define PIPE_BUFFER_SIZE 4096
int      pipe_create(vfs_node_t **read_end, vfs_node_t **write_end);

#endif /* NIMBUS_FS_H */
