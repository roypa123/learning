# Appendix F — Reading the Intel manuals without drowning

[Contents](README.md)

---

The *Intel 64 and IA-32 Architectures Software Developer's Manual* is about 5,000 pages across four
volumes. It is the authority for everything in this book, and reading it front to back is not a plan.

This appendix is how to use it.

---

## 1. The four volumes

| Volume | Title | Use it for |
|---|---|---|
| **1** | Basic Architecture | Registers, data types, the programming model. Read once, early. |
| **2** | Instruction Set Reference (A–Z) | What one instruction does. **Look things up here.** |
| **3** | System Programming Guide | Everything in this book: descriptors, paging, interrupts, privilege. |
| **4** | Model-Specific Registers | MSRs. Rarely, and always for one specific register. |

**Volume 3 is the one.** If you buy or print one, that.

Get the combined PDF from
[intel.com/sdm](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html).
It is searchable and the internal links work, which the separate volumes do not do as well.

---

## 2. The thirty pages that matter

Volume 3, in the order this book uses them:

| Section | Pages | Chapter |
|---|---|---|
| 2.1, 2.5 — System architecture, control registers | 8 | 2 |
| 3.4.5 — Segment descriptors | 6 | 7, 15 |
| 5.5 — Privilege levels | 4 | 32 |
| 6.3–6.5 — Interrupts and gates | 8 | 16 |
| 6.15 — Exception reference | 30 | 16 |
| **4.3 — 32-bit paging** | 12 | 23, 24 |
| **4.6 — Access rights** | 4 | 23, 32 |
| **4.10 — Caching translation information (the TLB)** | 8 | 23 |
| 7.2 — Task state segment | 6 | 15, 32 |
| 8.1–8.3 — Locked operations, memory ordering | 10 | 36 |
| 10.x — APIC | 60 | 48 |

**If you read three things:** 4.3 (paging), 6.3–6.5 (interrupts), and 5.5 (privilege). About twenty
pages, and they are the authoritative version of Parts III and IV.

---

## 3. How to look something up

### An instruction

Volume 2, alphabetical. Each entry has:

- **Opcode table** — every encoding. Useful when reading a hex dump.
- **Description** — prose.
- **Operation** — pseudocode. **This is the authoritative part.** Read it when the prose is ambiguous.
- **Flags Affected**
- **Protected Mode Exceptions** — which fault, and why. Invaluable when something raises `#GP` and
  you do not know why.

Start with Operation and Exceptions. The prose is for orientation.

### A bit in a register

Volume 3's index, or search the PDF for the bit's *name* (`WP`, `PSE`, `PG`) rather than its number.
Register diagrams are in Volume 3 Chapter 2 and near each feature's description.

### Why an exception fired

Volume 3, section 6.15 — the exception reference. Each entry lists every cause of that exception.

`#GP` has about twenty causes. The list is the fastest way to find yours.

---

## 4. Reading conventions

**Bit numbering** is from 0 = least significant. "Bit 31" is the top bit of a 32-bit value.

**`#XX`** is an exception mnemonic: `#GP` general protection, `#PF` page fault, `#DF` double fault,
`#UD` invalid opcode, `#DE` divide error.

**"Reserved"** means: write 0, do not assume it reads 0, and expect a future CPU to use it. Writing 1
to a reserved page table bit raises `#PF` with bit 3 of the error code set (Ch. 23, §6).

**"Undefined"** means the CPU may do anything, including different things on different models.
Genuinely undefined, not "probably zero".

**"Implementation specific"** means it varies by model and is documented in Volume 4 or a
model-specific datasheet.

**Pseudocode** uses `←` for assignment, `:` for concatenation (`EDX:EAX` is a 64-bit pair), and
`IF/THEN/FI` blocks. It is precise and worth learning to read — for `iret` it is the only complete
description of the two-stack-frame behaviour.

---

## 5. Where the book leans on it

| Chapter | Section |
|---|---|
| 7, 15 — GDT descriptors | Vol 3, 3.4.5, Figure 3-8 |
| 16 — Gate descriptors | Vol 3, 6.11, Figure 6-2 |
| 16 — Which exceptions push an error code | Vol 3, **Table 6-1** |
| 16 — Error code format | Vol 3, 6.13 |
| 23 — Page table entry bits | Vol 3, **Table 4-6** |
| 23 — The translation walk | Vol 3, Figure 4-2 |
| 23 — TLB invalidation | Vol 3, 4.10.4 |
| 26 — Page fault error code | Vol 3, **Figure 4-12** |
| 30 — Privilege checks on `iret` | Vol 2, `IRET` Operation |
| 32 — Stack switching on an interrupt | Vol 3, 6.12.1, Figure 6-4 |
| 36 — `lock` and atomicity | Vol 3, 8.1 |
| 36 — Memory ordering | Vol 3, 8.2 |

The three in bold are the tables worth printing.

---

## 6. Other specifications

| Specification | Pages | For |
|---|---|---|
| Multiboot 1 | 20 | Ch. 11. Short and complete. |
| ELF gABI | 60 | Ch. 44. Read chapters 1–2 only. |
| Microsoft FAT32 File System Specification | 34 | Ch. 41–42. Covers FAT12/16/32. **The best specification on this list.** |
| ATA/ATAPI-6 | 500 | Ch. 37. Look up commands; do not read. |
| PC/AT Technical Reference (IBM, 1984) | — | The 8259, 8253 and 8042, from the source |
| PCI Local Bus 3.0 | 300 | Ch. 48. Chapter 6 (configuration space) only. |
| UEFI | 2,500 | Ch. 4. Not needed for this book. |

The FAT specification is genuinely excellent: 34 pages, complete, with worked examples and correct
pseudocode for cluster arithmetic. If you only read one non-Intel document, that.

---

## 7. Other references

**[OSDev wiki](https://wiki.osdev.org)** — the reference everyone uses. Accurate on hardware,
occasionally out of date on toolchains. The "Bare Bones" and "Meaty Skeleton" pages are where most
hobby kernels start.

Pages worth reading: *Global Descriptor Table*, *Interrupt Descriptor Table*, *Paging*, *PS/2
Keyboard*, *ATA PIO Mode*, *FAT*. Each is a good summary with the manual references attached.

**Ralf Brown's Interrupt List** — an exhaustive catalogue of every BIOS and DOS interrupt, compiled
over a decade. The authority for `int 0x10`, `int 0x13` and `int 0x15`.

**[Linux source](https://elixir.bootlin.com)** — cross-referenced and searchable. When you want to
know how a real kernel handles something, `arch/x86/kernel/` is surprisingly readable, and version
0.11 (about 10,000 lines) is readable in full.

**xv6** — MIT's teaching kernel, 6,000 lines, with a book. The closest thing to Nimbus in the world,
done differently. Read it *after* this one; the differences are the interesting part.

---

## 8. A method

When something does not work and you do not know why:

1. **What does the CPU think happened?** `-d int` gives the vector and the error code.
2. **What does the exception mean?** Volume 3, section 6.15.
3. **What does the error code say?** Volume 3, 6.13 for selector errors; Figure 4-12 for page faults.
4. **What are the *rules* for what I was doing?** The relevant section — 4.3 for paging, 5.5 for
   privilege.
5. **What does the instruction actually do?** Volume 2, the Operation pseudocode.

Steps 1–3 take two minutes and answer most questions. Step 4 is where you find out that the rule you
assumed is not the rule.

**Read the pseudocode, not the prose**, when they seem to disagree. The prose is a summary; the
pseudocode is the specification.

---

## 9. What the manual will not tell you

The manual documents the *architecture*. It does not document:

**The PC.** The 8259 at `0x20`, the PIT at `0x40`, the framebuffer at `0xB8000`, IRQ 1 being the
keyboard — none of that is Intel's. It is IBM's, from 1981, and the references are the PC/AT
Technical Reference and the OSDev wiki.

**What is actually fast.** The manual says what instructions *do*, not what they cost. That is the
*Optimization Reference Manual*, and Agner Fog's tables are better.

**What real firmware does.** Whether your BIOS enables A20, leaves IRQs unmasked, or puts the EBDA at
`0x9FC00` — all machine-specific, all discovered by testing.

**What other operating systems assume.** That a FAT volume's two FATs agree, that an ELF entry point
is in the first `PT_LOAD`, that a partition starts at a 1 MiB boundary — conventions with real
consequences and no specification.

That last category is the one this book has tried to write down, because it is the part that is
nowhere else.

---

[Contents](README.md)
