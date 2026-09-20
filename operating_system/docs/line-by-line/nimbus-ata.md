# Line by line: `nimbus/drivers/ata.c`

[Index](README.md) · [Chapter 37](../37-ata-driver.md)

---

## `ata_delay`

```c
static inline void ata_delay(ata_device_t *dev)
{
    for (int i = 0; i < 4; i++)
        (void)inb(dev->ctrl_base);
}
```
After selecting a drive or issuing a command the status register is not valid immediately — the drive
needs ~400 ns to put its answer on the bus.

Four reads of the **alternate** status register, each an ISA bus cycle of roughly 100 ns.

⚠️ The alternate specifically:

> reading the normal status register clears the pending interrupt, which we do not want to do yet.

Same contents, different side effect. The distinction exists in the hardware precisely so a driver
can poll without acknowledging.

---

## `ata_wait_busy`

```c
    for (int spins = 0; spins < 1000000; spins++) {
        uint8_t status = inb(dev->io_base + ATA_REG_STATUS);
        if (!(status & ATA_SR_BSY)) return true;
    }
    LOG_ERR("ata: timeout waiting for BSY to clear on %s", ...);
    return false;
```
⚠️ **Bounded.** An unbounded wait turns a wedged drive into a hung kernel — and a hang during boot
with no output is the worst failure mode there is.

A million iterations at ~1 µs per I/O read is about a second.

🔧 A spin count is a poor timeout: it varies with CPU speed and bus contention. Exercise 37.6 wires in
a TSC-based one.

**Why BSY first, always:**

> While BSY is set, *every other bit of the status register is meaningless*. That is the single most
> important sentence in the ATA specification and the source of most driver bugs: code that checks
> DRQ without first waiting for BSY is reading a bit that the drive has not written yet.

---

## `ata_wait_drq`

```c
    if (!ata_wait_busy(dev)) return -EIO;

    for (int spins = 0; spins < 1000000; spins++) {
        uint8_t status = inb(dev->io_base + ATA_REG_STATUS);

        if (status & ATA_SR_ERR) {
            uint8_t err = inb(dev->io_base + ATA_REG_ERROR);
            LOG_ERR("ata: command failed, error register %02x", err);
            return -EIO;
        }
        if (status & ATA_SR_DF) { ... return -EIO; }
        if (status & ATA_SR_DRQ) return 0;
    }
```
⚠️ **ERR and DF checked before DRQ.** A failed command may set both DRQ and ERR, and a driver that
checks DRQ first will happily transfer 512 bytes of nothing.

---

## `ata_select`

```c
    uint8_t value = (uint8_t)(0xE0
                              | (dev->is_slave ? 0x10 : 0x00)
                              | ((lba >> 24) & 0x0F));
    outb(dev->io_base + ATA_REG_DRIVE, value);
    ata_delay(dev);
```
One register, four fields:

```
    bit 7      always 1  )  the "obsolete" bits, which must still be set
    bit 5      always 1  )  or the drive ignores you
    bit 6      1 = LBA addressing
    bit 4      0 = master, 1 = slave
    bits 3..0  LBA bits 24..27
```

⚠️ Bits 7 and 5 were the sector-size field on pre-1986 drives and are documented as obsolete — but
must still be 1, or the command is ignored. The single most common cause of "my ATA driver does
nothing at all".

Bits 3–0 holding the top nibble is why LBA28 is 28 bits: 24 in three registers, four here.

---

## `ata_identify`

```c
    outb(dev->io_base + ATA_REG_DRIVE,
         (uint8_t)(dev->is_slave ? 0xB0 : 0xA0));
    ata_delay(dev);

    outb(dev->io_base + ATA_REG_SECCOUNT, 0);
    outb(dev->io_base + ATA_REG_LBA_LO,   0);
    outb(dev->io_base + ATA_REG_LBA_MID,  0);
    outb(dev->io_base + ATA_REG_LBA_HI,   0);
```
IDENTIFY requires the addressing registers clear. Zeroing them is also what makes the ATAPI check
below work.

```c
    uint8_t status = inb(dev->io_base + ATA_REG_STATUS);
    if (status == 0 || status == 0xFF) return false;
```
⚠️ **Two different values both mean "no drive".**

> An empty ATA bus floats high and reads as 0xFF, but a *present* controller with no drive attached
> drives it to 0 — so both values mean "nothing here", and checking only one of them is why some
> kernels hang on hardware that works fine in QEMU.

🔧 QEMU produces 0; real hardware often produces `0xFF`.

```c
    if (inb(dev->io_base + ATA_REG_LBA_MID) != 0 ||
        inb(dev->io_base + ATA_REG_LBA_HI)  != 0) {
        LOG_INFO("ata: found an ATAPI device; we do not drive those");
        return false;
    }
```
⚠️ We zeroed those. Non-zero means the device is signalling ATAPI (`0x14 0xEB`) or SATA
(`0x3C 0xC3`).

Continuing would hang waiting for a DRQ that never arrives.

```c
    dev->sectors = (uint32_t)identify[60] | ((uint32_t)identify[61] << 16);
```
Words 60–61, low word first. 2²⁸ sectors × 512 = **128 GiB**, the LBA28 limit.

```c
    for (int i = 0; i < 20; i++) {
        dev->model[i * 2]     = (char)(identify[27 + i] >> 8);
        dev->model[i * 2 + 1] = (char)(identify[27 + i] & 0xFF);
    }
```
⚠️ **Every pair of bytes swapped.**

> ATA transfers 16-bit words and the string was defined big-endian, so "QEMU HARDDISK" arrives as
> "EQUM AHDRIDKS".

An endianness mismatch baked into a 1986 interface, and a reliable way to tell whether a driver
author read the specification or guessed.

```c
    for (int i = 39; i >= 0 && dev->model[i] == ' '; i--)
        dev->model[i] = '\0';
```
The standard requires space padding, not NUL termination.

---

## `ata_read_sectors`

```c
    if (lba + count > (1u << 28)) return -EINVAL;
```
LBA28. LBA48 uses the same registers written twice and a different command byte.

```c
    if (!ata_wait_busy(dev)) return -EIO;

    ata_select(dev, lba);

    outb(dev->io_base + ATA_REG_SECCOUNT, count);
    outb(dev->io_base + ATA_REG_LBA_LO,  (uint8_t)(lba & 0xFF));
    outb(dev->io_base + ATA_REG_LBA_MID, (uint8_t)((lba >> 8) & 0xFF));
    outb(dev->io_base + ATA_REG_LBA_HI,  (uint8_t)((lba >> 16) & 0xFF));
    outb(dev->io_base + ATA_REG_COMMAND, ATA_CMD_READ_PIO);
```
Wait, select, address, command. The command write is what starts the operation, so it must be last.

⚠️ `count` is a `uint8_t`, so the maximum is 255 — except the hardware treats **0 as 256**. We reject
0 earlier, which means 256-sector transfers are not expressible. Worth knowing, because a caller that
computes a count and happens to get 0 would otherwise move 128 KiB by accident.

```c
    for (uint8_t s = 0; s < count; s++) {
        int rc = ata_wait_drq(dev);
        if (rc < 0) return rc;

        insw(dev->io_base + ATA_REG_DATA, out, 256);
        out += 256;
    }
```
⚠️🔧 **One DRQ handshake per sector.** The canonical bug that only appears on real hardware:

> The drive does not hand over 8 sectors in one go — it fills its 512-byte buffer, raises DRQ, waits
> for us to drain it, then does the next. Reading `count * 256` words after a single wait happens to
> work in QEMU and fails on hardware.

QEMU's emulated drive has the whole transfer ready immediately. A real drive has one sector of
buffer.

### `insw`

```c
static ALWAYS_INLINE void insw(uint16_t port, void *buffer, uint32_t count)
{
    __asm__ volatile ("cld; rep insw"
                      : "+D"(buffer), "+c"(count)
                      : "d"(port)
                      : "memory");
}
```
`"+D"` and `"+c"` are read-write: `rep insw` advances `EDI` and decrements `ECX`.

`cld` because `DF` decides the direction.

⚠️ `"memory"` because GCC cannot see that the buffer was written — without it, code reading the
buffer afterwards may use stale values.

Roughly five times faster than a C loop around `inw()`, because the CPU does not re-decode the
instruction each time.

---

## `ata_write_sectors`

```c
        outsw(dev->io_base + ATA_REG_DATA, in, 256);
```
The mirror image.

> `rep outsw` is legal here and we use it, but note the caveat the specification gives: some very old
> drives require a short delay between words on write. Nothing made after 1995 does.

```c
    outb(dev->io_base + ATA_REG_COMMAND, ATA_CMD_CACHE_FLUSH);
    ata_wait_busy(dev);
```
⚠️ **The flush.**

> Without it the data is acknowledged but may still be in volatile memory on the drive — and the
> difference shows up exactly once, when the power goes out.

Every modern drive has a write cache of a few megabytes; a write command returns as soon as the data
is in it. `0xE7` does not return until it is durable.

This is the mechanism behind `fsync()`, and why disabling the flush makes benchmarks look
dramatically better.

---

## `ata_init`

```c
        outb(devices[i].ctrl_base, 0x02);
```
⚠️ Bit 1 is `nIEN` — "not interrupt enable". Setting it stops the drive asserting IRQ 14 or 15.

> We poll, and an interrupt we do not handle would just be noise on IRQ 14 — or worse, an IRQ storm.

The interrupt-driven version clears this bit, registers a handler, and blocks in `ata_wait_drq`
instead of spinning — which on real hardware frees the CPU for milliseconds per seek.

```c
        if (devices[i].present) {
            uint32_t mb = (uint32_t)(((uint64_t)devices[i].sectors * 512) / MiB);
            kprintf("ata%d: %s, %u MiB (%u sectors)\n", ...);
        }
```
The size computed in 64 bits: `sectors * 512` overflows 32 bits above 8 GiB.

```
ata0: QEMU HARDDISK, 16 MiB (32768 sectors)
```

A readable model string means the byte swap is right; a size matching the image means words 60–61
were read correctly.

---

[Index](README.md) · [Chapter 37](../37-ata-driver.md)
