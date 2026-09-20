# Chapter 44 — The ELF loader

[← Descriptors and pipes](43-fds-and-pipes.md) · [Contents](README.md) · [Next: A C library →](45-user-libc.md)

> 📖 **Line by line:** [elf.c](line-by-line/nimbus-elf.md)

---

## Goal

Turn a file into a running program. Walk the program headers, map each segment, zero the difference
between file size and memory size, and return an entry point.

A hundred and twenty lines, and the interesting parts are the three bounds checks.

---

## 1. Two tables describing the same bytes

> An ELF file has two tables of headers describing the same bytes from two points of view.
> *Section* headers describe the file for the linker: `.text`, `.rodata`, `.symtab`, relocation info.
> *Program* headers describe it for the loader: "map 0x1a40 bytes of file offset 0x1000 at virtual
> address 0x08049000, readable and executable". A loader reads only the program headers, and a
> stripped binary has no section headers at all.

```
    +---------------------+
    | ELF header          |  where everything else is
    +---------------------+
    | program headers     |  <- the LOADER reads these
    +---------------------+
    |                     |
    | .text .rodata       |  the actual bytes
    | .data .bss(no bytes)|
    |                     |
    +---------------------+
    | section headers     |  <- the LINKER and DEBUGGER read these
    | .symtab .strtab     |
    +---------------------+
```

That split is why `strip` works: removing the symbol table and section headers makes the file
smaller and still loadable.

```bash
$ i686-elf-readelf -l bin/user/sh

Program Headers:
  Type   Offset   VirtAddr   PhysAddr   FileSiz MemSiz  Flg Align
  LOAD   0x001000 0x08048000 0x08048000 0x023c0 0x023c0 R E 0x1000
  LOAD   0x004000 0x0804b000 0x0804b000 0x000e0 0x008e0 RW  0x1000
```

Two `PT_LOAD` segments: code read-execute, data read-write. Note the second one's `MemSiz` is larger
than its `FileSiz` — §4.

---

## 2. The header

```c
typedef struct elf_header {
    uint32_t magic;          /* 0x7F 'E' 'L' 'F'                              */
    uint8_t  class;          /* 1 = 32-bit                                    */
    uint8_t  endian;         /* 1 = little                                    */
    uint8_t  version;
    uint8_t  abi;
    uint8_t  abi_version;
    uint8_t  pad[7];
    uint16_t type;           /* ET_EXEC, ET_DYN, ...                          */
    uint16_t machine;        /* EM_386                                        */
    uint32_t version2;
    uint32_t entry;          /* virtual address of the first instruction      */
    uint32_t phoff;          /* file offset of the program header table       */
    uint32_t shoff;
    uint32_t flags;
    uint16_t ehsize;
    uint16_t phentsize;      /* size of one program header                    */
    uint16_t phnum;          /* how many program headers                      */
    ...
} PACKED elf_header_t;
```

### 2.1 Validate everything, and say which check failed

```c
    if (eh->magic != ELF_MAGIC) {
        LOG_WARN("elf: bad magic %08x (expected %08x)", eh->magic, ELF_MAGIC);
        return false;
    }
    if (eh->class != ELFCLASS32) {
        LOG_WARN("elf: class %u -- this is a 64-bit binary, we are a 32-bit kernel",
                 eh->class);
        return false;
    }
    ...
    if (eh->type != ET_EXEC) {
        LOG_WARN("elf: type %u -- we load only ET_EXEC. A PIE (ET_DYN) needs "
                 "relocation, which this loader does not do.", eh->type);
        return false;
    }
    if (eh->phoff == 0 || eh->phnum == 0) {
        LOG_WARN("elf: no program headers -- this is an object file, not an "
                 "executable. Did you forget to link it?");
        return false;
    }
```

> Check everything, and say which check failed. "not an executable" as the only diagnostic turns a
> five-second problem — you built for the wrong architecture — into an afternoon.

Six checks, six distinct messages, and each one names the likely cause. That is the difference
between `-ENOEXEC` and a log line that fixes the problem.

### 2.2 `phentsize`, and the loop that uses it

```c
        const elf_program_header_t *ph =
            (const elf_program_header_t *)(image + eh->phoff + (size_t)i * eh->phentsize);
```

`phentsize` rather than `sizeof(elf_program_header_t)`.

The format allows the header size to vary — a future revision could add fields — and reading the
size from the file rather than assuming it is what makes a loader forward-compatible.

The same pattern as the Multiboot memory map's variable-length entries (Chapter 21, §4.1), for the
same reason.

### 2.3 `ET_EXEC` versus `ET_DYN`

**`ET_EXEC`** is a fixed-address executable. Every address in it is final; the loader puts the bytes
where the headers say and jumps.

**`ET_DYN`** is position independent. It can be loaded anywhere, which is what ASLR needs, and it
requires *relocation*: a table of places in the image that hold addresses, each of which must be
adjusted by the load offset.

Modern toolchains default to PIE, which is why `gcc -o prog prog.c` on a current Linux produces an
`ET_DYN` that a naive loader rejects. Our `user.ld` produces `ET_EXEC` because nothing passes
`-pie` and the linker script fixes the base.

Adding relocation: parse `.rela.dyn` (or `.rel.dyn` on 32-bit), apply `R_386_RELATIVE` entries by
adding the load bias. About forty lines for the static-PIE case. Exercise 44.6.

---

## 3. The bounds checks

> A word on what makes this security-sensitive: `vaddr` and `memsz` come out of a file, and if that
> file came from anywhere untrusted then a program header claiming vaddr = 0xC0100000 is asking us to
> map user-writable pages over the kernel. The bounds check below is the only thing standing between
> a malformed binary and a full compromise, and it is three lines that are very easy to leave out
> because every legitimate binary passes.

```c
        if (ph->vaddr >= KERNEL_VIRTUAL_BASE ||
            ph->vaddr + ph->memsz > KERNEL_VIRTUAL_BASE ||
            ph->vaddr + ph->memsz < ph->vaddr) {
            LOG_ERR("elf: segment %u wants [%08x,%08x) -- that is kernel space",
                    i, ph->vaddr, ph->vaddr + ph->memsz);
            return 0;
        }

        if ((size_t)ph->offset + ph->filesz > length) {
            LOG_ERR("elf: segment %u reads past the end of the file", i);
            return 0;
        }
        if (ph->filesz > ph->memsz) {
            LOG_ERR("elf: segment %u has filesz > memsz, which is nonsense", i);
            return 0;
        }
```

Three separate attacks:

**The kernel-space map.** A header with `vaddr = 0xC0100000` would map user-writable pages over the
kernel's code. Every subsequent instruction the kernel executes would be attacker-chosen.

Note the third clause: `ph->vaddr + ph->memsz < ph->vaddr` catches the overflow, exactly as
`user_range_ok` does (Chapter 33, §5.2). Without it, `vaddr = 0xBFFFF000, memsz = 0x80000000` wraps
and passes.

**The read past the end of the file.** `offset + filesz > length` — the copy would read kernel heap
memory past the image and write it into the user's address space. An information leak of whatever
`kmalloc` last had there.

**`filesz > memsz`** is nonsense and would make the copy write past the mapped region. There is no
legitimate binary where the file part is larger than the memory part.

These three checks are the entire security boundary of the loader, and all three pass trivially for
every binary you will ever test with.

---

## 4. `memsz` larger than `filesz` is `.bss`

```
  LOAD   0x004000 0x0804b000 0x0804b000 0x000e0 0x008e0 RW  0x1000
                                        ^file    ^mem
```

`0xe0` bytes in the file, `0x8e0` bytes in memory. The extra 2048 bytes are `.bss`.

> `memsz > filesz` is how `.bss` is expressed: the extra bytes are not in the file and must be zeroed
> by the loader. Forgetting that is a bug whose symptom is a program that works until it reads an
> uninitialised global, which may be months later.

Same idea as `entry.asm` zeroing the kernel's `.bss` (Chapter 9, §2.2), one level up: nobody stores
zeros in a file when the reader can produce them.

### 4.1 We zero first, then copy

```c
        for (vaddr_t v = seg_start; v < seg_end; v += PAGE_SIZE) {
            uint32_t *pte = paging_get_entry(dir, v, true);
            if (*pte & PTE_PRESENT) continue;

            paddr_t frame = pmm_alloc_frame();
            if (frame == PMM_NO_FRAME) { LOG_ERR("elf: out of memory"); return 0; }

            memset(P2V(frame), 0, PAGE_SIZE);
            *pte = frame | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
        }
```

then copy the file bytes on top.

> Doing it that way round — zero everything, then overwrite — is both simpler and safer than zeroing
> the tail afterwards: it guarantees no stale data from a previous process is ever visible in the
> padding at the end of the last page of a segment.

Consider the alternative. A segment ending at `0x0804b8e0` has 1824 bytes of unused space in its last
page. If frames are not zeroed, those bytes are whatever the previous owner left — which could be
another process's data.

Zeroing on allocate, exactly as FAT16 does for clusters (Chapter 42, §2.2), for the same reason.

---

## 5. Permissions, and the one we cannot express

```c
        uint32_t flags = PTE_USER;
        if (ph->flags & PF_W) flags |= PTE_WRITABLE;
```

`PTE_USER` always — it is a user program. `PTE_WRITABLE` only if asked, so `.text` and `.rodata` land
read-only.

### 5.1 Map writable, copy, then tighten

```c
            *pte = frame | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
        }

        /* ... copy ... */

        if (!(ph->flags & PF_W)) {
            for (vaddr_t v = seg_start; v < seg_end; v += PAGE_SIZE) {
                uint32_t *pte = paging_get_entry(dir, v, false);
                if (pte) *pte = (*pte & ~PTE_WRITABLE) | flags;
                paging_invalidate(v);
            }
        }
```

> Map writable first, unconditionally: we are about to copy into these pages, and `CR0.WP` means even
> the kernel cannot write to a read-only page.

Chapter 24, §5.5. With `WP` set, the kernel obeys page permissions — which is what we wanted, and
which means the loader has to widen before the copy and narrow after.

The `paging_invalidate` on each page is mandatory (Chapter 23, §5.2) because the writable mapping may
already be in the TLB.

### 5.2 No NX

> There is no "no-execute" bit to clear: plain 32-bit paging cannot mark a page non-executable. That
> needs PAE and the NX bit, which means 64-bit page table entries and a third level of tables.
> Chapter 48 explains what it would take; for now, every readable page in a Nimbus process is
> executable, as it was on every x86 before 2004.

That is the honest statement. `.rodata` is readable, therefore executable. A buffer on the stack is
writable and readable, therefore executable — which is what made classic stack-smashing attacks
possible for fifteen years.

NX arrived with AMD64 in 2003 and was back-ported to 32-bit via PAE. It is the single most
significant security feature added to the x86 memory system, and we cannot have it.

---

## 6. Copying through the direct map

```c
        for (uint32_t done = 0; done < ph->filesz; ) {
            vaddr_t  v      = ph->vaddr + done;
            uint32_t within = v & PAGE_MASK;
            uint32_t chunk  = MIN(PAGE_SIZE - within, ph->filesz - done);

            paddr_t phys = paging_virt_to_phys(dir, v);
            ASSERT(phys != PMM_NO_FRAME);

            memcpy((uint8_t *)P2V(phys & PTE_FRAME_MASK) + within,
                   image + ph->offset + done, chunk);
            done += chunk;
        }
```

> We are not necessarily running in `dir`, so we cannot just memcpy to `ph->vaddr`. Instead we
> translate each page to its physical frame and write through the kernel's direct map. This is the
> same trick the page table code uses and the reason the direct map exists.

Chapter 24, §2.1 again — the third place it earns its keep, after page tables and
`paging_clone_directory`.

`task_spawn_user` (Chapter 32, §5.3) uses the same technique for the initial stack;
`task_exec_regs` (Chapter 34, §3.3) switches instead, because it needs the *addresses*.

The `within` handling covers a segment that does not start on a page boundary, which happens when two
segments share a page — common for small binaries where `.text` and `.rodata` are adjacent.

---

## 7. The program break

```c
    if (out_brk) *out_brk = ALIGN_UP(highest, PAGE_SIZE);
```

> The heap starts just past the last segment, page aligned. That is what "the program break" means,
> and it is why a program's first malloc returns an address a little above its `.bss`.

```c
    current_task->brk = ALIGN_UP(brk, PAGE_SIZE);
```

and `sbrk` grows from there (Chapter 45, §4).

```
nimbus> forktest
parent pid is 4
```
with a debug print of `brk`:
```
[   18.402] inf  elf: segment 0 -> [08048000,0804a3c0) r-x
[   18.402] inf  elf: segment 1 -> [0804b000,0804b8e0) rw-
[   18.403] dbg  brk = 0804c000
```

`0x0804b8e0` rounded up to `0x0804c000`. Two pages past the end of `.data`, because the segment's
last page is partly used.

---

## 8. Running it

```
[    0.412] inf  elf: segment 0 -> [08048000,0804a3c0) r-x
[    0.412] inf  elf: segment 1 -> [0804b000,0804b8e0) rw-
[    0.413] inf  task: pid 1 (/bin/sh) entry 08048080, user esp bffffff8
```

Two segments, permissions matching `readelf`, an entry point at `0x08048080`.

### 8.1 Confirming the entry point

```bash
$ i686-elf-readelf -h bin/user/sh | grep Entry
  Entry point address:               0x8048080

$ i686-elf-objdump -d bin/user/sh | head -8

08048080 <_start>:
 8048080:  8b 04 24       mov    eax,DWORD PTR [esp]
 8048083:  8b 5c 24 04    mov    ebx,DWORD PTR [esp+0x4]
 8048087:  31 ed          xor    ebp,ebp
 8048089:  53             push   ebx
```

`0x08048080` is `_start` in `crt0.asm` (Chapter 45, §2). The kernel's log, the ELF header and the
disassembly all agree.

### 8.2 Testing the checks

```bash
$ cp /bin/ls bin/user/ls64        # a 64-bit host binary
```

```
nimbus> /bin/ls64
[   31.882] wrn  elf: class 2 -- this is a 64-bit binary, we are a 32-bit kernel
/bin/ls64: command not found
```

```bash
$ i686-elf-gcc -c foo.c -o foo.o  # an object file, not linked
```

```
[   33.101] wrn  elf: no program headers -- this is an object file, not an
                 executable. Did you forget to link it?
```

Each message names the fix. That is what §2.1 was for.

### 8.3 Testing the bounds checks

Craft a malicious header — flip the `vaddr` of segment 0 to `0xC0100000` with a hex editor:

```
[   35.220] err  elf: segment 0 wants [c0100000,c01023c0) -- that is kernel space
/bin/evil: command not found
```

Rejected, with the address printed. The shell survives.

That is a three-line check doing the work of the entire security model, and it is worth constructing
the attack once to see it stopped.

---

## 9. What demand paging would change

Right now `task_exec_regs` reads the whole file:

```c
    uint8_t *image = (uint8_t *)kmalloc(node->length);
    ssize_t got = vfs_read(node, 0, node->length, image);
```

For a 6 KB shell that is fine. For a 200 MB binary it is 200 MB of `kmalloc` and a long wait before
the first instruction runs.

Demand paging instead:

1. `elf_load` maps the segments **not present**, storing the file offset in the other 31 bits of the
   PTE — which the CPU ignores when `P` is clear (Chapter 23, §3.1).
2. First touch faults.
3. The handler reads the offset out of the entry, reads one page from the file, maps it, returns.

What it buys: instant start, and pages never touched are never read. Most large programs touch a
small fraction of themselves.

What it costs, and this is the structural part:

> the handler must be able to do disk I/O, which means it must be able to *block*, which means page
> faults can no longer be handled with the simple synchronous code above.

Chapter 26, §6. A blocking page fault handler means the fault path becomes a scheduling point, which
means everything it touches must be re-entrant.

That is the real reason we do not, and it is worth being clear that the obstacle is structure rather
than effort.

---

## 10. What is missing

| Missing | What it would need |
|---|---|
| Dynamic linking | A userland loader, `PT_INTERP`, `.dynamic`, PLT/GOT, symbol resolution |
| Relocation (PIE) | `.rel.dyn` parsing, `R_386_RELATIVE` (~40 lines) — Exercise 44.6 |
| ASLR | PIE, plus randomising the load base and the stack |
| NX | PAE, 64-bit page table entries, a third level |
| `PT_GNU_STACK` | Reading the flag and honouring it — which needs NX to mean anything |
| Core dumps | Writing an `ET_CORE` file with `PT_LOAD`s of the address space |

Dynamic linking is the big one and it is genuinely large. The kernel's job becomes: notice
`PT_INTERP`, load *that* (`/lib/ld.so`) instead, and jump to it with the original program's headers
on the stack. Everything after is a userland program that maps libraries, resolves symbols, and
finally jumps to the real entry point.

That is why `ldd` shows a "dynamic linker" as a dependency, and why a statically linked binary starts
measurably faster.

---

## 11. Exercises

🟢 **44.1** Run `readelf -l` on each userland binary and confirm the loader's log lines match.

🟢 **44.2** Remove the `.bss` zeroing by not zeroing frames in `elf_load`, then have a program print
an uninitialised global.

🟢 **44.3** Build a binary with `-pie` and read the rejection message.

🟡 **44.4** Craft the kernel-space attack from §8.3 with a hex editor and confirm the rejection.

🟡 **44.5** Print every program header type, including `PT_GNU_STACK` and `PT_NOTE`, and look up what
each means.

🟡 **44.6** Implement relocation for static PIE: accept `ET_DYN`, pick a load bias, and apply
`R_386_RELATIVE` entries from `.rel.dyn`. Then randomise the bias and you have ASLR.

🔴 **44.7** Implement demand paging: store the file offset in not-present PTEs, and read a page in the
fault handler. You will need the fault handler to run in a context that can block — work out what
that requires before writing any code.

🔴 **44.8** Implement core dumps: on a fatal signal, write an `ET_CORE` file containing a `PT_NOTE`
with the registers and a `PT_LOAD` per mapped region. Then load it in GDB.

---

## What we covered

- Two header tables describing the same bytes for two different readers, and why `strip` works.
- Six validation checks with six distinct messages, each naming the likely cause.
- `phentsize` read from the file rather than assumed, for forward compatibility.
- `ET_EXEC` versus `ET_DYN`, and why a modern `gcc` produces something our loader rejects.
- Three bounds checks that are the entire security boundary: kernel space, the arithmetic overflow,
  reading past the end of the file, and `filesz > memsz`.
- `memsz > filesz` as `.bss`, and zeroing before copying so no stale data survives in segment
  padding.
- Mapping writable, copying, then tightening — because `CR0.WP` binds the kernel too.
- No NX, and what it would cost to have it.
- Copying through the direct map because we are not in the target address space.
- The program break as the page after the last segment.
- Three ways the loader's output is independently confirmed.
- Why demand paging is a structural change rather than an incremental one.

[Chapter 45](45-user-libc.md) builds what runs before `main`, and the library it needs.

---

[← Descriptors and pipes](43-fds-and-pipes.md) · [Contents](README.md) · [Next: A C library →](45-user-libc.md)
