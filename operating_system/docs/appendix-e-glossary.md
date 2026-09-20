# Appendix E — Glossary

[Contents](README.md)

---

**A20 gate** — A hardware gate that forces address line 20 to zero, added to the PC/AT in 1984 so
that software relying on the 8086's 1 MiB address wrap kept working. Closed at power-on; must be
opened before any address above 1 MiB is usable. *Ch. 7, §1*

**APIC** — Advanced Programmable Interrupt Controller. The successor to the 8259, with per-CPU local
APICs and an I/O APIC for device routing. Required for SMP; requires ACPI to find. *Ch. 17, §1.2*

**ATA / IDE / PATA** — The disk interface every PC used from 1986 to about 2005, and what QEMU
emulates for `-hda`. Three names for the same thing at different times. *Ch. 37*

**BPB** — BIOS Parameter Block. The part of a FAT boot sector that describes the filesystem geometry.
Starts at offset 11 because bytes 0–2 are a jump instruction. *Ch. 41, §2.1*

**BSY / DRQ** — Busy and Data Request, the two ATA status bits that are the entire transfer protocol.
While BSY is set, every other bit is meaningless. *Ch. 37, §2.1*

**Caller-saved / callee-saved** — The calling convention's division of registers. `EAX`, `ECX`, `EDX`
may be destroyed by a function; `EBX`, `ESI`, `EDI`, `EBP` must be preserved. This split is why a
context switch saves five registers and not sixteen. *Ch. 30, §2*

**CHS** — Cylinder/Head/Sector addressing. How disks were named before LBA; still what `int 0x13`
AH=0x02 speaks. Sectors count from 1, everything else from 0. *Ch. 6, §2*

**Cluster** — FAT's unit of allocation: a fixed number of sectors. Numbered from 2, because FAT
entries 0 and 1 hold metadata. *Ch. 41, §3*

**Context switch** — Saving one task's registers and restoring another's. Fourteen instructions, of
which the load-bearing two are `mov [eax], esp` and `mov esp, edx`. *Ch. 30*

**Copy-on-write (COW)** — Sharing pages read-only between two address spaces and copying only when
one writes. What makes `fork` cheap. *Ch. 28, §4.1*

**CPL / DPL / RPL** — Current, Descriptor and Requested Privilege Level. CPL is the bottom two bits
of `CS`; DPL is in a descriptor; RPL is in a selector. *Ch. 32, §1*

**CR0, CR2, CR3, CR4** — Control registers. `CR0.PE` enables protected mode, `CR0.PG` paging,
`CR0.WP` write protection for ring 0. `CR2` holds the faulting address after a page fault. `CR3`
holds the page directory's physical address. *Ch. 2, §2.5*

**crt0** — The object file containing `_start`, which turns the kernel's process-startup convention
into C's and turns `main`'s return into `exit`. *Ch. 45, §1*

**Demand paging** — Mapping pages not-present and filling them in the fault handler on first touch.
What makes a 200 MB binary start instantly. *Ch. 44, §9*

**Dentry cache** — A cache from (parent, name) to node, so repeated path lookups cost nothing. The
single biggest performance feature of the Linux VFS. *Ch. 39, §7*

**Direct map** — A permanent mapping of physical memory into the kernel's address space, so any frame
has a kernel virtual address. `P2V` and `V2P` convert. *Ch. 24, §2.1*

**DLAB** — Divisor Latch Access Bit. Bit 7 of a UART's line control register; setting it re-points
the first two registers at the baud divisor. Bank switching, and the most confusing thing about the
16550. *Ch. 13, §3*

**Double fault** — Exception 8, raised when the CPU cannot deliver another exception. Its error code
is always zero and carries no information; the *first* fault is the one you want. *Ch. 16, §8.1*

**E820** — The BIOS call (`int 0x15, AX=0xE820`) that reports the memory map. Multiboot relays the
same data. *Ch. 21, §3*

**EBDA** — Extended BIOS Data Area, near the top of conventional memory. Its base is stored at
`0x040E` and varies by machine. *Ch. 21, §2*

**ELF** — Executable and Linkable Format. Two header tables describing the same bytes: *section*
headers for the linker, *program* headers for the loader. *Ch. 44, §1*

**EOI** — End Of Interrupt. The signal that tells the PIC a handler has finished; without it the PIC
blocks every interrupt of equal or lower priority. *Ch. 17, §5*

**Exception** — A fault raised by the CPU: divide error, page fault, general protection. Vectors
0–31. *Ch. 16, §8*

**Extent** — A contiguous run of blocks, described by a start and a length. What modern filesystems
use instead of FAT's linked list. *Ch. 41, §1*

**Fixup table** — A table mapping "if a fault happens at this instruction" to "resume here instead",
used so that kernel copies from user memory can fail gracefully. What real kernels use instead of
validating up front. *Ch. 33, §5.5*

**Frame** — A 4 KiB unit of *physical* memory. A *page* is the virtual counterpart. *Ch. 22, §2*

**Freestanding** — A C environment with no standard library. `-ffreestanding` tells GCC so — but does
**not** stop it emitting calls to `memcpy`, `memmove`, `memset` and `memcmp`, which the standard
requires a freestanding implementation to provide. *Ch. 9, §5*

**GDT** — Global Descriptor Table. An array of 8-byte descriptors that segment registers index into.
Ours has six: null, kernel code and data, user code and data, and a TSS. *Ch. 15*

**Guard page** — An unmapped page placed where an overflow would go, turning silent corruption into a
clean fault. *Ch. 26, §3.3*

**Higher half** — Placing the kernel at a high virtual address (`0xC0000000` here) so that every
process can have the low addresses. Requires a kernel that is *loaded* at one address and *runs* at
another. *Ch. 25*

**IDT** — Interrupt Descriptor Table. 256 gate descriptors; entry *N* says where to jump when vector
*N* fires. *Ch. 16*

**Initrd / initramfs** — A filesystem image loaded into memory by the bootloader, mounted before any
disk driver works. Breaks the bootstrapping cycle. *Ch. 40, §1*

**Interrupt gate vs trap gate** — One bit. An interrupt gate clears `IF` on entry; a trap gate does
not. *Ch. 16, §3.1*

**IOPL** — I/O Privilege Level, two bits of `EFLAGS`. The privilege required to use `in` and `out`.
Ours is 0, so ring 3 cannot touch ports. *Ch. 32, §2.2*

**`iret`** — Return from interrupt. Pops `EIP`, `CS`, `EFLAGS` — and `ESP`, `SS` too if the popped
`CS` is less privileged. The only instruction that can lower privilege. *Ch. 3, §4*

**Journal** — A log of a filesystem's intended writes, committed before they are performed, so that a
crash leaves the structure consistent. What FAT does not have, and what `chkdsk` exists because of.
*Ch. 42, §7*

**LBA** — Logical Block Addressing. Sectors numbered 0, 1, 2… LBA28 tops out at 128 GiB; LBA48 goes
further. *Ch. 6, §2*

**LFN** — Long File Name. FAT's mechanism for names over 8.3, stored in pseudo-entries whose
attribute byte is `0x0F` — a combination no real file has, so old DOS skipped them. *Ch. 41, §6.3*

**LMA / VMA** — Load and Virtual Memory Address: where the bytes are *put* and where the code expects
to *be*. Equal for most programs; they differ by 3 GiB for a higher-half kernel. *Ch. 25, §3.1*

**Lost wakeup** — A wakeup delivered between a waiter's condition test and its sleep, leaving it
asleep forever. Closed by disabling interrupts across test-and-sleep, and by the act of sleeping
being what re-enables them. *Ch. 35, §3.2*

**MBR** — Master Boot Record. Sector 0: 446 bytes of boot code, four 16-byte partition entries, and
`0x55 0xAA`. *Ch. 38, §1*

**MMIO** — Memory-Mapped I/O. Device registers appearing at physical addresses, reached with ordinary
`mov`. Needs `volatile` and, for real devices, cache disabling. *Ch. 12, §1*

**Multiboot** — A 1995 contract between bootloaders and kernels: a 12-byte header in the image, and a
defined machine state plus an info structure on entry. *Ch. 11*

**NX** — No-eXecute, a page table bit marking a page non-executable. Requires PAE or long mode; not
available with plain 32-bit paging. *Ch. 44, §5.2*

**PAE** — Physical Address Extension. 64-bit page table entries and a third level, giving 64 GiB of
physical address space on a 32-bit CPU. Also where `NX` lives. *Ch. 23, §7*

**Page directory / page table** — The two levels of 32-bit x86 paging. 1024 entries each, 10 bits of
virtual address each, 12 bits of offset. *Ch. 23, §2*

**Page fault** — Exception 14. Not necessarily an error: demand paging, stack growth and
copy-on-write all arrive as faults. `CR2` holds the address; the error code says what happened.
*Ch. 26*

**PIC (8259)** — Programmable Interrupt Controller. Two cascaded chips multiplexing sixteen device
lines onto one CPU pin. Must be remapped away from vectors 8–15, which are CPU exceptions.
*Ch. 17*

**PIO** — Programmed I/O: the CPU moves every byte through a port. Slow, simple, and needs no PCI
driver. *Ch. 37, §1.1*

**PIT (8253/8254)** — Programmable Interval Timer. Three counters at 1,193,182 Hz; channel 0 drives
IRQ 0. *Ch. 18*

**Protected mode** — The 32-bit mode entered by setting `CR0.PE`. Segment registers become selectors
into the GDT. *Ch. 7, §3*

**Real mode** — The 16-bit mode every x86 boots in. Addresses are `segment × 16 + offset`; 1 MiB
total; wraps. *Ch. 2, §3*

**Ring** — A privilege level, 0 (kernel) to 3 (user). Rings 1 and 2 are unused because paging has
only one bit to distinguish them. *Ch. 32, §1*

**Scancode** — What a keyboard sends: a number identifying a physical switch, not a character.
Set 1 uses bit 7 for release and `0xE0` as a prefix for the keys added in 1986. *Ch. 19, §1*

**Sector** — 512 bytes, the unit a disk reads and writes. Drives since ~2010 use 4096-byte physical
sectors while reporting 512. *Ch. 38, §5*

**Selector** — A 16-bit value in a segment register: an index into a descriptor table, plus a table
bit and two RPL bits. `0x1B` is descriptor 3 at ring 3. *Ch. 2, §4*

**Slab allocator** — A pool per object type, with free slots chained through their own memory. O(1),
no per-object header, and objects can stay constructed. *Ch. 27, §9*

**SMP** — Symmetric Multiprocessing. Needs the APIC, ACPI parsing, per-CPU data, real spinlocks, TLB
shootdowns, and an audit of every global. *Ch. 48, §2.2*

**Spurious interrupt** — An interrupt the PIC delivers because a device dropped its request line too
late. Appears as IRQ 7 or 15, and must **not** be acknowledged with an EOI. *Ch. 17, §4*

**System call** — A controlled entry into the kernel, through one IDT gate with DPL 3. *Ch. 33*

**TLB** — Translation Lookaside Buffer, the MMU's cache of translations. Does **not** notice page
table changes; `invlpg` or a `CR3` reload is required. *Ch. 23, §5*

**TLB shootdown** — On a multiprocessor, an inter-processor interrupt telling other CPUs to
invalidate a translation, plus waiting for them. Why unmapping is expensive on a large machine.
*Ch. 23, §5.4*

**TOCTOU** — Time Of Check To Time Of Use: a value validated and then changed before it is used. The
class of bug that fixup tables and `O_NOFOLLOW` exist to prevent. *Ch. 33, §5.4*

**Triple fault** — A fault while handling a double fault. The CPU asserts the reset line; the machine
reboots instantly with no output. *Ch. 47, §3*

**TSS** — Task State Segment. 104 bytes designed for hardware task switching nobody uses, of which
two fields — `ss0` and `esp0` — make userland possible. *Ch. 15, §4*

**UART (16550)** — The serial port chip. Eight registers, two of which change meaning with DLAB.
*Ch. 13, §2*

**ustar** — The POSIX tar format: a 512-byte header of ASCII fields, all numbers in octal, then the
data padded to 512 bytes. *Ch. 40, §3*

**VFS** — Virtual File System. Seven function pointers that make a FAT16 file, a device and a pipe
interchangeable. *Ch. 39*

**Wait channel** — An address used as a token for blocking. Nothing is stored at it; it is compared
for equality. From Unix Sixth Edition. *Ch. 35, §2.1*

**Write-back / write-through** — Whether a cache defers writes or performs them immediately.
Write-back is faster and loses data on a crash, which is why `fsync` exists. *Ch. 38, §4.2*

**Zombie** — A process that has exited but whose parent has not collected its status. Necessary
because a process cannot free the stack it is standing on. *Ch. 29, §3*

---

[Contents](README.md)
