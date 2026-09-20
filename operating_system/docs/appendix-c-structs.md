# Appendix C — Every struct in Nimbus, with its layout

[Contents](README.md)

---

Structures whose layout is dictated by hardware or by a file format are marked **PACKED** and their
byte offsets are given. Get one wrong and the failure is silent.

---

## Hardware-defined

### `gdt_entry_t` — 8 bytes

```c
typedef struct gdt_entry {
    uint16_t limit_low;      /* 0-1  limit bits 0..15              */
    uint16_t base_low;       /* 2-3  base  bits 0..15              */
    uint8_t  base_mid;       /* 4    base  bits 16..23             */
    uint8_t  access;         /* 5    P | DPL(2) | S | Type(4)      */
    uint8_t  granularity;    /* 6    G | D/B | L | AVL | lim 16..19*/
    uint8_t  base_high;      /* 7    base  bits 24..31             */
} PACKED gdt_entry_t;
```

```
    access:       P  DPL DPL S  E  DC RW A
                  7  6   5   4  3  2  1  0

    granularity:  G  D/B L  AVL limit19..16
                  7  6   5  4   3..0
```

Split and out of order for 80286 compatibility (Ch. 15, §2). Our six descriptors:

| # | Selector | Base | Limit | Access | Gran |
|---|---|---|---|---|---|
| 0 | `0x00` | — | — | `0x00` | `0x00` |
| 1 | `0x08` | 0 | 4 GiB | `0x9A` | `0xCF` |
| 2 | `0x10` | 0 | 4 GiB | `0x92` | `0xCF` |
| 3 | `0x1B` | 0 | 4 GiB | `0xFA` | `0xCF` |
| 4 | `0x23` | 0 | 4 GiB | `0xF2` | `0xCF` |
| 5 | `0x2B` | `&tss` | 103 | `0xE9` | `0x00` |

### `gdt_ptr_t` / `idt_ptr_t` — 6 bytes

```c
    uint16_t limit;          /* size in bytes, MINUS ONE  */
    uint32_t base;           /* linear address            */
```

**Minus one.** The CPU stores the offset of the last valid byte.

### `idt_entry_t` — 8 bytes

```c
typedef struct idt_entry {
    uint16_t offset_low;     /* 0-1  handler bits 0..15  */
    uint16_t selector;       /* 2-3  GDT code selector   */
    uint8_t  zero;           /* 4    must be 0           */
    uint8_t  type_attr;      /* 5    P | DPL(2) | 0 | type(4) */
    uint16_t offset_high;    /* 6-7  handler bits 16..31 */
} PACKED idt_entry_t;
```

| `type_attr` | Meaning |
|---|---|
| `0x8E` | present, DPL 0, 32-bit interrupt gate |
| `0xEE` | present, DPL 3, 32-bit interrupt gate — the syscall |
| `0x8F` | present, DPL 0, 32-bit trap gate |

### `tss_entry_t` — 104 bytes

```c
    uint32_t prev_tss;       /* 0    unused                       */
    uint32_t esp0;           /* 4    <- the ring 0 stack pointer  */
    uint32_t ss0;            /* 8    <- the ring 0 stack segment  */
    ... 84 bytes of hardware task switching nobody uses ...
    uint16_t iomap_base;     /* 102  set past the end = no I/O    */
```

Two useful fields out of 104 (Ch. 15, §4).

### Page directory / table entry — 4 bytes

```
    31                              12 11  9 8 7 6 5 4 3 2 1 0
   +----------------------------------+-----+-+-+-+-+-+-+-+-+-+
   |         frame address            | AVL |G|S|D|A|C|W|U|R|P|
   +----------------------------------+-----+-+-+-+-+-+-+-+-+-+
```

| Bit | Name | Meaning |
|---|---|---|
| 0 | P | present; 0 = fault on any access |
| 1 | RW | writable (for ring 3, and ring 0 if `CR0.WP`) |
| 2 | U | user-accessible — **the wall** |
| 3 | PWT | write-through |
| 4 | PCD | cache disable — mandatory for MMIO |
| 5 | A | accessed; set by the CPU |
| 6 | D | dirty; set by the CPU (table entries) |
| 7 | PS | 4 MiB page (directory) / PAT (table) |
| 8 | G | global; survives a `CR3` reload |
| 9–11 | AVL | software — we use bit 9 for `PTE_COW` |

---

## Format-defined

### `multiboot_info_t`

```c
    uint32_t flags;          /* 0   which fields are valid       */
    uint32_t mem_lower;      /* 4   KiB below 1 MiB              */
    uint32_t mem_upper;      /* 8   KiB above 1 MiB, NOT total   */
    uint32_t boot_device;    /* 12                                */
    uint32_t cmdline;        /* 16  physical                      */
    uint32_t mods_count;     /* 20                                */
    uint32_t mods_addr;      /* 24  physical                      */
    uint32_t syms[4];        /* 28                                */
    uint32_t mmap_length;    /* 44                                */
    uint32_t mmap_addr;      /* 48  physical                      */
    ...
```

**Every address in it is physical.** Every field must be checked against `flags` first
(Ch. 11, §5.1).

### `multiboot_mmap_entry_t` — variable

```c
    uint32_t size;           /* bytes in this entry, NOT counting size */
    uint64_t addr;
    uint64_t len;
    uint32_t type;           /* 1 = available                          */
```

Advance by `size + 4`, not `sizeof` (Ch. 21, §4.1).

### `elf_header_t` — 52 bytes

```c
    uint32_t magic;          /* 0   0x464C457F = "\x7FELF"  */
    uint8_t  class;          /* 4   1 = 32-bit              */
    uint8_t  endian;         /* 5   1 = little              */
    ...
    uint16_t type;           /* 16  2 = ET_EXEC             */
    uint16_t machine;        /* 18  3 = EM_386              */
    uint32_t entry;          /* 24                          */
    uint32_t phoff;          /* 28                          */
    uint32_t shoff;          /* 32                          */
    uint16_t phentsize;      /* 42  size of one prog header */
    uint16_t phnum;          /* 44                          */
```

### `elf_program_header_t` — 32 bytes

```c
    uint32_t type;           /* 0   1 = PT_LOAD           */
    uint32_t offset;         /* 4   where in the file     */
    uint32_t vaddr;          /* 8   where in memory       */
    uint32_t paddr;          /* 12  ignored with an MMU   */
    uint32_t filesz;         /* 16  bytes to copy         */
    uint32_t memsz;          /* 20  bytes to reserve      */
    uint32_t flags;          /* 24  1=X 2=W 4=R           */
    uint32_t align;          /* 28                        */
```

**`memsz > filesz` is `.bss`** (Ch. 44, §4).

### `tar_header_t` — 512 bytes

```c
    char name[100];          /* 0                              */
    char mode[8];            /* 100  OCTAL ASCII               */
    char uid[8];             /* 108                            */
    char gid[8];             /* 116                            */
    char size[12];           /* 124  OCTAL ASCII               */
    char mtime[12];          /* 136                            */
    char checksum[8];        /* 148                            */
    char typeflag;           /* 156  '0' file, '5' directory   */
    char linkname[100];      /* 157                            */
    char magic[6];           /* 257  "ustar"                   */
    ...
```

**Octal**, not decimal (Ch. 40, §3.1).

### `mbr_partition_t` — 16 bytes, at offset 446

```c
    uint8_t  status;         /* 0   0x80 = bootable       */
    uint8_t  chs_first[3];   /* 1   archaeology           */
    uint8_t  type;           /* 4   0x06 = FAT16          */
    uint8_t  chs_last[3];    /* 5   archaeology           */
    uint32_t lba_first;      /* 8                         */
    uint32_t sectors;        /* 12                        */
```

### `fat_bpb_t`

```c
    uint8_t  jump[3];        /* 0   real x86 code        */
    char     oem[8];         /* 3                         */
    uint16_t bytes_per_sector;      /* 11                */
    uint8_t  sectors_per_cluster;   /* 13                */
    uint16_t reserved_sectors;      /* 14                */
    uint8_t  fat_count;             /* 16                */
    uint16_t root_entries;          /* 17                */
    uint16_t total_sectors_16;      /* 19                */
    uint8_t  media_type;            /* 21                */
    uint16_t sectors_per_fat;       /* 22                */
    ...
    uint32_t total_sectors_32;      /* 32                */
```

The jump instruction at offset 0 is why the fields start at 11 (Ch. 41, §2.1).

### `fat_dirent_t` — 32 bytes

```c
    char     name[8];        /* 0   space padded, UPPER   */
    char     ext[3];         /* 8                         */
    uint8_t  attr;           /* 11  0x0F = an LFN record  */
    ...
    uint16_t cluster_hi;     /* 20  always 0 on FAT16     */
    uint16_t modify_time;    /* 22                        */
    uint16_t modify_date;    /* 24                        */
    uint16_t cluster_lo;     /* 26                        */
    uint32_t size;           /* 28                        */
```

Sixteen per sector. `name[0] == 0x00` means stop; `0xE5` means deleted.

---

## Kernel-internal

### `registers_t` — 76 bytes

**Not a convenience struct.** Its field order *is* the stack layout produced by
`isr_common_stub`.

```c
typedef struct registers {
    uint32_t ds;             /* pushed by our stub                     */
    uint32_t edi, esi, ebp, esp_dummy, ebx, edx, ecx, eax;   /* pusha  */
    uint32_t int_no, err_code;                     /* stub / CPU       */
    uint32_t eip, cs, eflags, useresp, ss;         /* the CPU          */
} registers_t;
```

`esp_dummy` is the pre-`pusha` `ESP` and is not useful. `useresp` and `ss` are only real when
`(cs & 3) == 3` (Ch. 16, §6).

### `context_t` — 20 bytes

```c
typedef struct context {
    uint32_t edi;            /* lowest address  */
    uint32_t esi;
    uint32_t ebx;
    uint32_t ebp;
    uint32_t eip;            /* highest address */
} context_t;
```

Must match the push order in `switch_context` (Ch. 30, §3). Lives on the task's own kernel stack; the
saved `ESP` is the pointer to it.

### `task_t` — ~200 bytes

```c
    pid_t             pid, ppid;
    char              name[32];
    task_state_t      state;

    context_t        *context;        /* the saved ESP                  */
    uint32_t          kernel_stack;   /* the TOP; kfree needs -SIZE     */
    uint32_t          priority, time_slice;
    uint64_t          wake_tick;
    void             *wait_channel;
    int               exit_status;

    page_directory_t *directory;
    vaddr_t           brk, user_stack_bottom;

    struct file      *fds[16];
    struct vfs_node  *cwd;

    uint64_t          ticks_used;
    struct task      *next;
```

### `page_directory_t` — 8 bytes

```c
    uint32_t *entries;       /* virtual — what we write   */
    paddr_t   phys;          /* physical — what CR3 takes */
```

Both, deliberately (Ch. 28, §1).

### `heap_block_t` — 16 bytes

```c
    uint32_t            magic;   /* 0xA110C8ED used / 0xF2EEB10C free */
    size_t              size;    /* payload, not counting this header */
    struct heap_block  *next;
    struct heap_block  *prev;
```

Immediately before the payload, so `kfree` finds it by subtraction.

### `vfs_node_t` — ~120 bytes

```c
    char          name[64];
    uint32_t      flags, inode, length, permissions, refcount;

    uint32_t      impl;           /* driver scratch */
    void         *device;         /* driver scratch */

    vfs_read_t    read;
    vfs_write_t   write;
    vfs_open_t    open;
    vfs_close_t   close;
    vfs_readdir_t readdir;
    vfs_finddir_t finddir;
    vfs_create_t  create;
    vfs_unlink_t  unlink;

    struct vfs_node *mounted;
```

What each driver puts in the scratch fields: Ch. 39, §2.1.

### `file_t` — 16 bytes

```c
    vfs_node_t *node;
    off_t       offset;
    uint32_t    flags;
    uint32_t    refcount;
```

The middle of three levels (Ch. 43, §1).

### `pipe_t` — ~4112 bytes

```c
    uint8_t  buffer[4096];
    uint32_t head, tail, count;
    uint32_t readers, writers;
```

Refcounts per **end**, not per node — a reader needs to know whether *any* writer exists.

### `fat_fs_t` — ~60 bytes plus the cached FAT

```c
    ata_device_t *dev;
    uint32_t      part_lba;       /* added to every sector number */
    ... geometry ...
    uint32_t      fat_lba, root_lba, data_lba, cluster_count;
    uint16_t     *fat;            /* the whole FAT, in memory     */
    bool          fat_dirty;
```

---

## Alignment and `PACKED`

`PACKED` is `__attribute__((packed))`: no padding, fields at their stated offsets.

**Use it** for anything a chip or a file format defines: GDT and IDT entries, the TSS, multiboot
structures, ELF headers, tar headers, MBR entries, FAT structures.

**Do not use it** for anything else:

```c
 *  It is exactly wrong for anything else: unaligned loads are slower, and
 *  taking the address of a packed field gives you a pointer GCC will not
 *  trust. Use it only where a chip or a file format dictated the layout.
```

A `PACKED` struct with a `uint32_t` at an odd offset makes `&s->field` a pointer GCC considers
misaligned; passing it to a function taking `uint32_t *` is a warning on x86 and a fault on ARM.

Default alignment on 32-bit x86: `char` 1, `short` 2, `int`/`long`/pointer 4, `long long` 8,
`double` 8 — and a struct is aligned to its largest member.

---

[Contents](README.md)
