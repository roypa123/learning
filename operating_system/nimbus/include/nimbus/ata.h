/* ===========================================================================
 *  nimbus/include/nimbus/ata.h  --  the parallel ATA disk interface
 * ===========================================================================
 *
 *  ATA -- also called IDE, and later renamed PATA when SATA arrived -- is the
 *  interface every PC hard disk used from 1986 until about 2005, and it is
 *  what QEMU emulates by default for `-hda`. We drive it in PIO mode: the CPU
 *  moves every byte itself through an I/O port, 512 bytes at a time.
 *
 *  PIO is slow. A real driver uses DMA, where the disk writes into memory
 *  directly and interrupts you when it is done. But DMA means bus mastering,
 *  which means PCI configuration, which means a PCI driver -- three chapters
 *  of prerequisites for a speedup nobody will notice inside an emulator.
 *  Chapter 37 explains exactly what the upgrade would involve.
 *
 *  Explained in: docs/37-ata-driver.md
 * =========================================================================== */
#ifndef NIMBUS_ATA_H
#define NIMBUS_ATA_H

#include <nimbus/types.h>

/*  The two legacy channels, at the port addresses IBM fixed in 1984. Each can
 *  carry two drives, "master" and "slave", selected by one bit of one register
 *  -- which is why a PC had exactly four IDE drives for twenty years.         */
#define ATA_PRIMARY_IO      0x1F0
#define ATA_PRIMARY_CTRL    0x3F6
#define ATA_SECONDARY_IO    0x170
#define ATA_SECONDARY_CTRL  0x376

/*  Register offsets from the I/O base. Several of them are read/write pairs
 *  with completely different meanings in each direction, which is normal for
 *  hardware of this vintage and merciless if you misread the datasheet.       */
#define ATA_REG_DATA        0
#define ATA_REG_ERROR       1   /* read                                       */
#define ATA_REG_FEATURES    1   /* write                                      */
#define ATA_REG_SECCOUNT    2
#define ATA_REG_LBA_LO      3
#define ATA_REG_LBA_MID     4
#define ATA_REG_LBA_HI      5
#define ATA_REG_DRIVE       6   /* drive select + LBA bits 24..27             */
#define ATA_REG_STATUS      7   /* read                                       */
#define ATA_REG_COMMAND     7   /* write                                      */

/*  Status register bits. BSY and DRQ are the whole protocol: you wait for BSY
 *  to clear, then for DRQ to set, then you transfer.                          */
#define ATA_SR_BSY  0x80   /* busy: every other bit is meaningless while set  */
#define ATA_SR_DRDY 0x40   /* drive ready                                     */
#define ATA_SR_DF   0x20   /* device fault                                    */
#define ATA_SR_DSC  0x10
#define ATA_SR_DRQ  0x08   /* data request: a sector is waiting in the buffer */
#define ATA_SR_CORR 0x04
#define ATA_SR_IDX  0x02
#define ATA_SR_ERR  0x01   /* error: read the error register for why          */

#define ATA_CMD_READ_PIO    0x20
#define ATA_CMD_WRITE_PIO   0x30
#define ATA_CMD_CACHE_FLUSH 0xE7
#define ATA_CMD_IDENTIFY    0xEC

#define ATA_SECTOR_SIZE     512

typedef struct ata_device {
    bool     present;
    uint16_t io_base;
    uint16_t ctrl_base;
    bool     is_slave;
    uint32_t sectors;        /* total LBA28 sectors                           */
    char     model[41];      /* from IDENTIFY, byte-swapped and NUL-terminated */
} ata_device_t;

void    ata_init(void);
ata_device_t *ata_get(int index);       /* 0..3, or NULL                      */

/*  Read/write `count` sectors starting at LBA. Returns 0 on success or a
 *  negative errno. LBA28 only: 2^28 sectors * 512 bytes = 128 GiB, which is
 *  more disk than this kernel will ever be pointed at.                        */
int     ata_read_sectors(ata_device_t *dev, uint32_t lba, uint8_t count, void *buffer);
int     ata_write_sectors(ata_device_t *dev, uint32_t lba, uint8_t count, const void *buffer);

#endif /* NIMBUS_ATA_H */
