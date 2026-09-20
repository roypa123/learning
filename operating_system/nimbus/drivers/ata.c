/* ===========================================================================
 *  nimbus/drivers/ata.c  --  PIO mode disk access
 * ===========================================================================
 *
 *  The protocol, in full:
 *
 *      1. Select the drive, and wait 400 ns for it to notice.
 *      2. Write the sector count and the 28-bit LBA into four registers.
 *      3. Write the command.
 *      4. Wait for BSY to clear and DRQ to set.
 *      5. Move 256 16-bit words through the data port.
 *      6. Repeat 4-5 for each remaining sector.
 *
 *  That is genuinely all of it, and it has not changed since 1986. The
 *  difficulty is entirely in the waiting: every one of those steps can fail,
 *  hang, or return a status register full of 0xFF because there is no disk
 *  there at all, and a driver that does not bound its waits turns a missing
 *  disk into a hung boot with no message.
 *
 *  Explained in: docs/37-ata-driver.md
 *  Line by line: docs/line-by-line/nimbus-ata.md
 * =========================================================================== */

#include <nimbus/ata.h>
#include <nimbus/io.h>
#include <nimbus/kernel.h>
#include <nimbus/string.h>
#include <nimbus/syscall.h>
#include <nimbus/timer.h>

static ata_device_t devices[4];

/* ---------------------------------------------------------------------------
 *  The 400 nanosecond delay
 *
 *  After selecting a drive or issuing a command, the status register is not
 *  valid immediately -- the drive needs time to put its answer on the bus.
 *  The specification says 400 ns, and the traditional way to wait that long
 *  without a timer is to read the *alternate* status register four times.
 *  Each read is an ISA bus cycle of roughly 100 ns, and reading the alternate
 *  register rather than the normal one matters: reading the normal status
 *  register clears the pending interrupt, which we do not want to do yet.
 * ------------------------------------------------------------------------- */
static inline void ata_delay(ata_device_t *dev)
{
    for (int i = 0; i < 4; i++)
        (void)inb(dev->ctrl_base);
}

/*  Wait for BSY to clear, with a bound. Returns false on timeout.
 *
 *  While BSY is set, *every other bit of the status register is meaningless*.
 *  That is the single most important sentence in the ATA specification and the
 *  source of most driver bugs: code that checks DRQ without first waiting for
 *  BSY is reading a bit that the drive has not written yet.                   */
static bool ata_wait_busy(ata_device_t *dev)
{
    for (int spins = 0; spins < 1000000; spins++) {
        uint8_t status = inb(dev->io_base + ATA_REG_STATUS);
        if (!(status & ATA_SR_BSY)) return true;
    }
    LOG_ERR("ata: timeout waiting for BSY to clear on %s",
            dev->is_slave ? "slave" : "master");
    return false;
}

/*  Wait until the drive has data for us (DRQ), or has given up (ERR/DF).     */
static int ata_wait_drq(ata_device_t *dev)
{
    if (!ata_wait_busy(dev)) return -EIO;

    for (int spins = 0; spins < 1000000; spins++) {
        uint8_t status = inb(dev->io_base + ATA_REG_STATUS);

        if (status & ATA_SR_ERR) {
            uint8_t err = inb(dev->io_base + ATA_REG_ERROR);
            LOG_ERR("ata: command failed, error register %02x", err);
            return -EIO;
        }
        if (status & ATA_SR_DF) {
            LOG_ERR("ata: device fault");
            return -EIO;
        }
        if (status & ATA_SR_DRQ) return 0;
    }

    LOG_ERR("ata: timeout waiting for DRQ");
    return -EIO;
}

/* ---------------------------------------------------------------------------
 *  Drive selection
 *
 *  The drive/head register carries four things at once:
 *
 *      bit 7      always 1  )  the "obsolete" bits, which must still be set
 *      bit 5      always 1  )  or the drive ignores you
 *      bit 6      1 = LBA addressing, 0 = CHS
 *      bit 4      0 = master, 1 = slave
 *      bits 3..0  LBA bits 24..27
 *
 *  Hence the magic 0xE0: 1110 0000.
 * ------------------------------------------------------------------------- */
static void ata_select(ata_device_t *dev, uint32_t lba)
{
    uint8_t value = (uint8_t)(0xE0
                              | (dev->is_slave ? 0x10 : 0x00)
                              | ((lba >> 24) & 0x0F));
    outb(dev->io_base + ATA_REG_DRIVE, value);
    ata_delay(dev);
}

/* ---------------------------------------------------------------------------
 *  IDENTIFY -- is there a drive, and what is it?
 * ------------------------------------------------------------------------- */
static bool ata_identify(ata_device_t *dev)
{
    outb(dev->io_base + ATA_REG_DRIVE,
         (uint8_t)(dev->is_slave ? 0xB0 : 0xA0));
    ata_delay(dev);

    /*  Zero the addressing registers. IDENTIFY requires them clear, and a
     *  non-zero value here is how a drive tells us it is an ATAPI device
     *  rather than a disk -- see the signature check below.                   */
    outb(dev->io_base + ATA_REG_SECCOUNT, 0);
    outb(dev->io_base + ATA_REG_LBA_LO,   0);
    outb(dev->io_base + ATA_REG_LBA_MID,  0);
    outb(dev->io_base + ATA_REG_LBA_HI,   0);

    outb(dev->io_base + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);

    /*  Status 0 means there is no drive on this channel at all. An empty ATA
     *  bus floats high and reads as 0xFF, but a *present* controller with no
     *  drive attached drives it to 0 -- so both values mean "nothing here",
     *  and checking only one of them is why some kernels hang on hardware that
     *  works fine in QEMU.                                                    */
    uint8_t status = inb(dev->io_base + ATA_REG_STATUS);
    if (status == 0 || status == 0xFF) return false;

    if (!ata_wait_busy(dev)) return false;

    /*  If these are non-zero, it is an ATAPI device (a CD-ROM), which speaks a
     *  different command set entirely and would hang forever waiting for DRQ.  */
    if (inb(dev->io_base + ATA_REG_LBA_MID) != 0 ||
        inb(dev->io_base + ATA_REG_LBA_HI)  != 0) {
        LOG_INFO("ata: found an ATAPI device; we do not drive those");
        return false;
    }

    if (ata_wait_drq(dev) < 0) return false;

    uint16_t identify[256];
    insw(dev->io_base + ATA_REG_DATA, identify, 256);

    /*  Word 60-61: the number of LBA28 sectors. Two 16-bit words holding one
     *  32-bit value, low word first.                                          */
    dev->sectors = (uint32_t)identify[60] | ((uint32_t)identify[61] << 16);

    /*  Words 27-46: the model string, 40 characters, with every *pair* of
     *  bytes swapped. ATA transfers 16-bit words and the string was defined
     *  big-endian, so "QEMU HARDDISK" arrives as "EQUM AHDRIDKS".             */
    for (int i = 0; i < 20; i++) {
        dev->model[i * 2]     = (char)(identify[27 + i] >> 8);
        dev->model[i * 2 + 1] = (char)(identify[27 + i] & 0xFF);
    }
    dev->model[40] = '\0';

    /*  Trim the trailing spaces the standard requires.                        */
    for (int i = 39; i >= 0 && dev->model[i] == ' '; i--)
        dev->model[i] = '\0';

    return true;
}

/* ---------------------------------------------------------------------------
 *  Reading
 * ------------------------------------------------------------------------- */
int ata_read_sectors(ata_device_t *dev, uint32_t lba, uint8_t count, void *buffer)
{
    if (!dev || !dev->present) return -ENODEV;
    if (count == 0) return 0;

    /*  LBA28 tops out here. Beyond it you need LBA48, which uses the same
     *  registers written twice -- once for the high bytes, once for the low --
     *  and a different command byte. Chapter 37's exercises add it.           */
    if (lba + count > (1u << 28)) return -EINVAL;

    if (!ata_wait_busy(dev)) return -EIO;

    ata_select(dev, lba);

    outb(dev->io_base + ATA_REG_SECCOUNT, count);
    outb(dev->io_base + ATA_REG_LBA_LO,  (uint8_t)(lba & 0xFF));
    outb(dev->io_base + ATA_REG_LBA_MID, (uint8_t)((lba >> 8) & 0xFF));
    outb(dev->io_base + ATA_REG_LBA_HI,  (uint8_t)((lba >> 16) & 0xFF));
    outb(dev->io_base + ATA_REG_COMMAND, ATA_CMD_READ_PIO);

    uint16_t *out = (uint16_t *)buffer;

    /*  One DRQ handshake per sector. The drive does not hand over 8 sectors in
     *  one go -- it fills its 512-byte buffer, raises DRQ, waits for us to
     *  drain it, then does the next. Reading `count * 256` words after a
     *  single wait happens to work in QEMU and fails on hardware.             */
    for (uint8_t s = 0; s < count; s++) {
        int rc = ata_wait_drq(dev);
        if (rc < 0) return rc;

        insw(dev->io_base + ATA_REG_DATA, out, 256);
        out += 256;
    }

    return 0;
}

int ata_write_sectors(ata_device_t *dev, uint32_t lba, uint8_t count, const void *buffer)
{
    if (!dev || !dev->present) return -ENODEV;
    if (count == 0) return 0;
    if (lba + count > (1u << 28)) return -EINVAL;

    if (!ata_wait_busy(dev)) return -EIO;

    ata_select(dev, lba);

    outb(dev->io_base + ATA_REG_SECCOUNT, count);
    outb(dev->io_base + ATA_REG_LBA_LO,  (uint8_t)(lba & 0xFF));
    outb(dev->io_base + ATA_REG_LBA_MID, (uint8_t)((lba >> 8) & 0xFF));
    outb(dev->io_base + ATA_REG_LBA_HI,  (uint8_t)((lba >> 16) & 0xFF));
    outb(dev->io_base + ATA_REG_COMMAND, ATA_CMD_WRITE_PIO);

    const uint16_t *in = (const uint16_t *)buffer;

    for (uint8_t s = 0; s < count; s++) {
        int rc = ata_wait_drq(dev);
        if (rc < 0) return rc;

        /*  `rep outsw` is legal here and we use it, but note the caveat the
         *  specification gives: some very old drives require a short delay
         *  between words on write. Nothing made after 1995 does.              */
        outsw(dev->io_base + ATA_REG_DATA, in, 256);
        in += 256;
    }

    /*  Flush the drive's write cache. Without it the data is acknowledged but
     *  may still be in volatile memory on the drive -- and the difference
     *  shows up exactly once, when the power goes out.                        */
    outb(dev->io_base + ATA_REG_COMMAND, ATA_CMD_CACHE_FLUSH);
    ata_wait_busy(dev);

    return 0;
}

ata_device_t *ata_get(int index)
{
    if (index < 0 || index > 3) return NULL;
    return devices[index].present ? &devices[index] : NULL;
}

void ata_init(void)
{
    static const uint16_t io[4]   = { ATA_PRIMARY_IO,   ATA_PRIMARY_IO,
                                      ATA_SECONDARY_IO, ATA_SECONDARY_IO };
    static const uint16_t ctrl[4] = { ATA_PRIMARY_CTRL, ATA_PRIMARY_CTRL,
                                      ATA_SECONDARY_CTRL, ATA_SECONDARY_CTRL };

    for (int i = 0; i < 4; i++) {
        devices[i].io_base   = io[i];
        devices[i].ctrl_base = ctrl[i];
        devices[i].is_slave  = (i & 1) != 0;

        /*  Disable interrupts from this channel (bit 1 of the control
         *  register, nIEN). We poll, and an interrupt we do not handle would
         *  just be noise on IRQ 14 -- or worse, an IRQ storm. Chapter 37's
         *  last section converts the driver to interrupt-driven and has to
         *  clear this bit again.                                              */
        outb(devices[i].ctrl_base, 0x02);

        devices[i].present = ata_identify(&devices[i]);

        if (devices[i].present) {
            uint32_t mb = (uint32_t)(((uint64_t)devices[i].sectors * 512) / MiB);
            kprintf("ata%d: %s, %u MiB (%u sectors)\n",
                    i, devices[i].model, mb, devices[i].sectors);
        }
    }
}
