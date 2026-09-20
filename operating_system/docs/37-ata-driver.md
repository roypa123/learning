# Chapter 37 — The ATA PIO driver

[← Synchronisation](36-synchronisation.md) · [Contents](README.md) · [Next: Partitions →](38-block-layer.md)

> 📖 **Line by line:** [ata.c](line-by-line/nimbus-ata.md)

---

## Goal

Read a sector off a disk. Six port writes, a handshake, and 256 words through an I/O port — plus the
error handling that separates a driver from a demonstration.

---

## 1. What ATA is

ATA — also called IDE, and renamed PATA when SATA arrived — is the interface every PC hard disk used
from 1986 to about 2005. It is what QEMU emulates for `-hda`, what every chipset still provides in
compatibility mode, and the simplest disk interface that exists.

The whole protocol:

```
    1. Select the drive, and wait 400 ns for it to notice.
    2. Write the sector count and the 28-bit LBA into four registers.
    3. Write the command.
    4. Wait for BSY to clear and DRQ to set.
    5. Move 256 16-bit words through the data port.
    6. Repeat 4-5 for each remaining sector.
```

> That is genuinely all of it, and it has not changed since 1986. The difficulty is entirely in the
> waiting: every one of those steps can fail, hang, or return a status register full of 0xFF because
> there is no disk there at all, and a driver that does not bound its waits turns a missing disk into
> a hung boot with no message.

### 1.1 PIO versus DMA

**PIO** — Programmed I/O — means the CPU moves every byte itself through an I/O port. That is what we
do.

**DMA** means the disk writes directly into memory and interrupts you when it is done. The CPU is
free for the whole transfer, and at 100 MB/s that is the difference between a usable system and one
that spends all its time copying.

Why we do not:

> DMA means bus mastering, which means PCI configuration, which means a PCI driver — three chapters of
> prerequisites for a speedup nobody will notice inside an emulator.

PIO at 16 bits per `insw`, with `rep`, gets perhaps 10–20 MB/s on real hardware. Enough for a kernel
that reads a few hundred kilobytes at boot.

---

## 2. The registers

```c
#define ATA_PRIMARY_IO      0x1F0
#define ATA_PRIMARY_CTRL    0x3F6
#define ATA_SECONDARY_IO    0x170
#define ATA_SECONDARY_CTRL  0x376
```

Two channels, each carrying two drives — master and slave, selected by one bit of one register.

> which is why a PC had exactly four IDE drives for twenty years.

```c
#define ATA_REG_DATA        0
#define ATA_REG_ERROR       1   /* read                                       */
#define ATA_REG_FEATURES    1   /* write                                      */
#define ATA_REG_SECCOUNT    2
#define ATA_REG_LBA_LO      3
#define ATA_REG_LBA_MID     4
#define ATA_REG_LBA_HI      5
#define ATA_REG_DRIVE       6
#define ATA_REG_STATUS      7   /* read                                       */
#define ATA_REG_COMMAND     7   /* write                                      */
```

> Several of them are read/write pairs with completely different meanings in each direction, which is
> normal for hardware of this vintage and merciless if you misread the datasheet.

Writing to offset 7 issues a command; reading it returns status. Offset 1 is features on write and
the error code on read.

### 2.1 The status register

```c
#define ATA_SR_BSY  0x80   /* busy: every other bit is meaningless while set  */
#define ATA_SR_DRDY 0x40
#define ATA_SR_DF   0x20   /* device fault                                    */
#define ATA_SR_DSC  0x10
#define ATA_SR_DRQ  0x08   /* data request: a sector is waiting               */
#define ATA_SR_CORR 0x04
#define ATA_SR_IDX  0x02
#define ATA_SR_ERR  0x01   /* error: read the error register for why          */
```

**BSY and DRQ are the whole protocol.** Wait for BSY to clear, then for DRQ to set, then transfer.

And the sentence that matters most in the entire ATA specification:

> While BSY is set, *every other bit of the status register is meaningless*. That is the single most
> important sentence in the ATA specification and the source of most driver bugs: code that checks
> DRQ without first waiting for BSY is reading a bit that the drive has not written yet.

---

## 3. Waiting

### 3.1 The 400 nanosecond delay

```c
static inline void ata_delay(ata_device_t *dev)
{
    for (int i = 0; i < 4; i++)
        (void)inb(dev->ctrl_base);
}
```

After selecting a drive or issuing a command, the status register is not valid immediately — the
drive needs time to put its answer on the bus.

The specification says 400 ns. The traditional way to wait that long without a timer is four reads of
the **alternate** status register, each an ISA bus cycle of roughly 100 ns.

> reading the alternate register rather than the normal one matters: reading the normal status
> register clears the pending interrupt, which we do not want to do yet.

Same register contents, different side effect. That distinction exists in the hardware precisely so
that a driver can poll without acknowledging.

### 3.2 Bounded waits

```c
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
```

A million iterations at roughly a microsecond per I/O read is about a second.

Unbounded would turn a wedged drive into a hung kernel. The same rule as the serial driver
(Chapter 13, §5.2), for the same reason: a hang during boot with no output is the worst failure mode
there is.

A spin count is a poor timeout — it varies with CPU speed and with bus contention. Exercise 18.8's
TSC calibration would give a real one, and Exercise 37.6 wires it in.

### 3.3 Waiting for data, or an error

```c
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
```

BSY first, always. Then poll for one of three outcomes: error, device fault, or data ready.

Checking ERR and DF *before* DRQ matters: a failed command may set both DRQ and ERR, and a driver
that checks DRQ first will happily transfer 512 bytes of nothing.

---

## 4. Drive selection

```c
static void ata_select(ata_device_t *dev, uint32_t lba)
{
    uint8_t value = (uint8_t)(0xE0
                              | (dev->is_slave ? 0x10 : 0x00)
                              | ((lba >> 24) & 0x0F));
    outb(dev->io_base + ATA_REG_DRIVE, value);
    ata_delay(dev);
}
```

One register carrying four things:

```
    bit 7      always 1  )  the "obsolete" bits, which must still be set
    bit 5      always 1  )  or the drive ignores you
    bit 6      1 = LBA addressing, 0 = CHS
    bit 4      0 = master, 1 = slave
    bits 3..0  LBA bits 24..27
```

Hence `0xE0` = `1110 0000`.

Bits 7 and 5 were the "sector size" field on pre-1986 drives and are documented as obsolete — but
they must still be 1 or the command is ignored, which is the single most common cause of "my ATA
driver does nothing at all".

Bits 3–0 holding the top nibble of the LBA is why LBA28 is 28 bits: 24 in the three LBA registers,
four here.

---

## 5. IDENTIFY

```c
static bool ata_identify(ata_device_t *dev)
{
    outb(dev->io_base + ATA_REG_DRIVE,
         (uint8_t)(dev->is_slave ? 0xB0 : 0xA0));
    ata_delay(dev);

    outb(dev->io_base + ATA_REG_SECCOUNT, 0);
    outb(dev->io_base + ATA_REG_LBA_LO,   0);
    outb(dev->io_base + ATA_REG_LBA_MID,  0);
    outb(dev->io_base + ATA_REG_LBA_HI,   0);

    outb(dev->io_base + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
```

Ask the drive what it is. The response is 256 words of everything about it.

### 5.1 Two ways to be absent

```c
    uint8_t status = inb(dev->io_base + ATA_REG_STATUS);
    if (status == 0 || status == 0xFF) return false;
```

> Status 0 means there is no drive on this channel at all. An empty ATA bus floats high and reads as
> 0xFF, but a *present* controller with no drive attached drives it to 0 — so both values mean
> "nothing here", and checking only one of them is why some kernels hang on hardware that works fine
> in QEMU.

QEMU produces 0 for an absent drive; real hardware often produces `0xFF`. A driver tested only in an
emulator checks one.

### 5.2 ATAPI

```c
    if (inb(dev->io_base + ATA_REG_LBA_MID) != 0 ||
        inb(dev->io_base + ATA_REG_LBA_HI)  != 0) {
        LOG_INFO("ata: found an ATAPI device; we do not drive those");
        return false;
    }
```

We zeroed those registers before the command. If they come back non-zero, the device is signalling
that it speaks the ATAPI packet interface — a CD-ROM or a tape drive.

The signature is `0x14 0xEB` for ATAPI, `0x3C 0xC3` for SATA. Either way it is not a disk we can
drive, and continuing would hang waiting for a DRQ that never arrives.

### 5.3 The byte-swapped model string

```c
    for (int i = 0; i < 20; i++) {
        dev->model[i * 2]     = (char)(identify[27 + i] >> 8);
        dev->model[i * 2 + 1] = (char)(identify[27 + i] & 0xFF);
    }
    dev->model[40] = '\0';
```

> Words 27-46: the model string, 40 characters, with every *pair* of bytes swapped. ATA transfers
> 16-bit words and the string was defined big-endian, so "QEMU HARDDISK" arrives as "EQUM AHDRIDKS".

An endianness mismatch baked into a 1986 interface, preserved for compatibility, and a reliable way
to tell whether a driver author read the specification or guessed.

```c
    for (int i = 39; i >= 0 && dev->model[i] == ' '; i--)
        dev->model[i] = '\0';
```

The standard requires space padding, not NUL termination, so the trailing spaces have to be trimmed.

### 5.4 The size

```c
    dev->sectors = (uint32_t)identify[60] | ((uint32_t)identify[61] << 16);
```

Words 60–61: the LBA28 sector count, low word first.

2²⁸ sectors × 512 bytes = **128 GiB**, the LBA28 limit. LBA48 uses the same registers written twice
and a different command byte; Exercise 37.5.

---

## 6. Reading

```c
int ata_read_sectors(ata_device_t *dev, uint32_t lba, uint8_t count, void *buffer)
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
    outb(dev->io_base + ATA_REG_COMMAND, ATA_CMD_READ_PIO);

    uint16_t *out = (uint16_t *)buffer;

    for (uint8_t s = 0; s < count; s++) {
        int rc = ata_wait_drq(dev);
        if (rc < 0) return rc;

        insw(dev->io_base + ATA_REG_DATA, out, 256);
        out += 256;
    }

    return 0;
}
```

### 6.1 One handshake per sector

The loop is the part people get wrong:

> The drive does not hand over 8 sectors in one go — it fills its 512-byte buffer, raises DRQ, waits
> for us to drain it, then does the next. Reading `count * 256` words after a single wait happens to
> work in QEMU and fails on hardware.

QEMU's emulated drive has the whole transfer ready immediately, so a single `insw` of
`count * 256` words succeeds. A real drive has one sector's worth of buffer.

This is the canonical example of a bug that only appears on metal.

### 6.2 `insw`

```c
static ALWAYS_INLINE void insw(uint16_t port, void *buffer, uint32_t count)
{
    __asm__ volatile ("cld; rep insw"
                      : "+D"(buffer), "+c"(count)
                      : "d"(port)
                      : "memory");
}
```

> `rep insw` reads `count` words from `port` into memory. The ATA PIO driver moves 256 words per
> sector this way, which is roughly five times faster than a C loop around `inw()` because the CPU
> does not re-decode the instruction each time.

`"+D"` and `"+c"` are read-write constraints: `rep insw` advances `EDI` and decrements `ECX`, so GCC
must know both are modified.

`cld` first, because `DF` decides the direction (Chapter 3, §3.5).

`"memory"` because GCC cannot see that the buffer was written.

### 6.3 A count of 0 means 256

```c
    outb(dev->io_base + ATA_REG_SECCOUNT, count);
```

`count` is a `uint8_t`, so the maximum is 255 — except that the hardware treats 0 as 256.

We reject 0 earlier (`if (count == 0) return 0;`), which means 256-sector transfers are not
expressible. That is fine for us and it is worth knowing, because a caller that computes a count and
happens to get 0 would otherwise transfer 128 KiB by accident.

---

## 7. Writing

```c
    outb(dev->io_base + ATA_REG_COMMAND, ATA_CMD_WRITE_PIO);

    for (uint8_t s = 0; s < count; s++) {
        int rc = ata_wait_drq(dev);
        if (rc < 0) return rc;

        outsw(dev->io_base + ATA_REG_DATA, in, 256);
        in += 256;
    }

    outb(dev->io_base + ATA_REG_COMMAND, ATA_CMD_CACHE_FLUSH);
    ata_wait_busy(dev);
```

The mirror image, plus one thing.

### 7.1 The cache flush

```c
 *  Flush the drive's write cache. Without it the data is acknowledged but
 *  may still be in volatile memory on the drive -- and the difference
 *  shows up exactly once, when the power goes out.
```

Every modern drive has a write cache of a few megabytes. A write command returns as soon as the data
is in that cache, not when it is on the platter.

`0xE7` (FLUSH CACHE) does not return until it is durable.

This is the mechanism behind `fsync()`, and it is why a filesystem that does not issue it can lose
data that the application believed was written. It is also why disabling the flush makes benchmarks
look dramatically better, which is why it is occasionally done by people who should not.

### 7.2 The old-drive caveat

> `rep outsw` is legal here and we use it, but note the caveat the specification gives: some very old
> drives require a short delay between words on write. Nothing made after 1995 does.

Worth a comment rather than a workaround: a per-word delay would cost a factor of five on every write
for hardware nobody has.

---

## 8. Initialisation, and turning interrupts off

```c
    for (int i = 0; i < 4; i++) {
        devices[i].io_base   = io[i];
        devices[i].ctrl_base = ctrl[i];
        devices[i].is_slave  = (i & 1) != 0;

        outb(devices[i].ctrl_base, 0x02);

        devices[i].present = ata_identify(&devices[i]);
        ...
    }
```

Bit 1 of the control register is `nIEN` — "not interrupt enable". Setting it stops the drive
asserting IRQ 14 or 15.

> We poll, and an interrupt we do not handle would just be noise on IRQ 14 — or worse, an IRQ storm.

That is the right call for a polling driver. The interrupt-driven version clears this bit and blocks
in `ata_wait_drq` instead of spinning:

```c
    irq_register(IRQ_ATA_PRIMARY, ata_irq_handler);
    ...
    sched_block(dev);
```

The whole transfer then costs the CPU nothing while the disk seeks, which on real hardware is
milliseconds. Exercise 37.7.

---

## 9. Running it

```bash
dd if=/dev/zero of=bin/disk.img bs=1M count=16
mkfs.fat -F 16 -n NIMBUS bin/disk.img
```

```
run nimbus
```

```
ata0: QEMU HARDDISK, 16 MiB (32768 sectors)
```

The model string is readable, which means the byte swap is right. The size matches the image, which
means words 60–61 were read correctly.

### 9.1 Reading sector 0 by hand

```c
    ata_device_t *d = ata_get(0);
    if (d) {
        uint8_t buf[512];
        if (ata_read_sectors(d, 0, 1, buf) == 0) {
            for (int i = 0; i < 64; i++) {
                kprintf("%02x ", buf[i]);
                if (i % 16 == 15) kprintf("\n");
            }
        }
    }
```

```
eb 3c 90 6d 6b 66 73 2e 66 61 74 00 02 04 01 00
02 00 02 00 80 f8 20 00 20 00 40 00 00 00 00 00
00 00 00 00 80 00 29 ...
```

`eb 3c 90` is a jump instruction — the FAT boot record starts with one (Chapter 41, §2).
`6d 6b 66 73 2e 66 61 74` is "mkfs.fat", the OEM name. `02 00` at offset 11 is 512 bytes per sector.

Reading those bytes and recognising them is the proof that the driver works.

### 9.2 A round trip

```c
    uint8_t out[512], in[512];
    memset(out, 0xA5, sizeof(out));

    ata_write_sectors(d, 100, 1, out);
    memset(in, 0, sizeof(in));
    ata_read_sectors(d, 100, 1, in);

    kprintf("round trip: %s\n", memcmp(in, out, 512) == 0 ? "ok" : "FAILED");
```

```
round trip: ok
```

Write, read back, compare. Use a sector well past anything important — sector 100 on a freshly
formatted 16 MiB volume is in the data area and unused.

---

## 10. What could go wrong

| Symptom | Cause |
|---|---|
| No drives detected | `0xE0` bits 7 and 5 not set in the drive register |
| Detected, but reads hang | Checking DRQ without waiting for BSY first |
| Works in QEMU, fails on hardware | One `insw` for the whole transfer instead of per sector |
| Model string is gibberish | Byte pairs not swapped |
| Boot hangs with no message | Unbounded wait on an absent drive |
| Hangs on a CD-ROM | ATAPI signature not checked |
| Data lost on power failure | No cache flush after writes |
| Reads return zeros | LBA registers written in the wrong order, or the drive not selected |
| 128 GiB limit hit | LBA28; needs LBA48 |

---

## 11. Exercises

🟢 **37.1** Dump sector 0 of `disk.img` with the code in §9.1 and identify the OEM name and the
bytes-per-sector field.

🟢 **37.2** Remove bits 7 and 5 from `ata_select`'s `0xE0` and see what the drive does.

🟢 **37.3** Do the round trip from §9.2 with a multi-sector transfer and confirm all of it.

🟡 **37.4** Move the `ata_wait_drq` call outside the per-sector loop so there is one wait for the
whole transfer. Confirm it still works in QEMU, then write down why it would not on hardware.

🟡 **37.5** Implement LBA48: command `0x24`, the high bytes written first then the low, and a 16-bit
sector count. Test with a disk image larger than 128 GiB (sparse files make this cheap).

🟡 **37.6** Replace the spin counts with real timeouts using the TSC from Exercise 18.8.

🔴 **37.7** Make the driver interrupt-driven: clear `nIEN`, register a handler on IRQ 14, and block
in `ata_wait_drq` instead of spinning. Measure the CPU time spent reading a megabyte before and
after.

🔴 **37.8** Implement UDMA. You will need to find the IDE controller on the PCI bus, read its bus
master base address, build a physical region descriptor table, and handle the completion interrupt.
This is the single biggest performance change available and it is a weekend.

---

## What we covered

- The six-step ATA protocol, unchanged since 1986, and why all the difficulty is in the waiting.
- PIO versus DMA, and the three chapters of prerequisites DMA would need.
- Registers whose meaning depends on the direction, and the sentence about BSY that causes most
  driver bugs.
- The 400 ns delay via four alternate-status reads, and why the alternate register specifically.
- Bounded waits, because an absent drive must not hang the boot.
- Checking ERR and DF before DRQ.
- A drive register carrying four fields, including two obsolete bits that must still be set.
- Two different values that both mean "no drive", and why checking one passes in QEMU.
- The ATAPI signature, the byte-swapped model string, and the LBA28 limit.
- One DRQ handshake per sector — the canonical bug that only appears on real hardware.
- `rep insw` with read-write constraints, and the sector count where 0 means 256.
- The cache flush, and its relationship to `fsync`.
- `nIEN`, and what the interrupt-driven version would change.

[Chapter 38](38-block-layer.md) puts structure on the disk: partition tables, and the block cache we
do not have.

---

[← Synchronisation](36-synchronisation.md) · [Contents](README.md) · [Next: Partitions →](38-block-layer.md)
