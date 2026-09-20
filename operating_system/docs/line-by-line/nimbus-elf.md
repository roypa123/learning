# Line by line: `nimbus/kernel/elf.c`

[Index](README.md) · [Chapter 44](../44-elf-loader.md)

A hundred and twenty lines, and the interesting parts are three bounds checks.

---

## `elf_validate`

```c
    if (length < sizeof(elf_header_t)) {
        LOG_WARN("elf: file is %u bytes, too short for a header", (uint32_t)length);
        return false;
    }
```
Before dereferencing anything. A 20-byte file would otherwise have its header read past the end of
the allocation.

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
```
⚠️ **Six checks, six distinct messages.**

> "not an executable" as the only diagnostic turns a five-second problem — you built for the wrong
> architecture — into an afternoon.

Each message names the likely cause. That is the difference between `-ENOEXEC` and a log line that
fixes the problem.

```c
    if (eh->type != ET_EXEC) {
        LOG_WARN("elf: type %u -- we load only ET_EXEC. A PIE (ET_DYN) needs "
                 "relocation, which this loader does not do.", eh->type);
        return false;
    }
```
Modern toolchains default to PIE, so `gcc -o prog prog.c` on a current Linux produces an `ET_DYN`
this loader rejects. `user.ld` fixes the base, so ours are `ET_EXEC`.

```c
    if (eh->phoff == 0 || eh->phnum == 0) {
        LOG_WARN("elf: no program headers -- this is an object file, not an "
                 "executable. Did you forget to link it?");
        return false;
    }
```
An `.o` file has section headers but no program headers. The message says what to do.

```c
    if (eh->phoff + (size_t)eh->phnum * eh->phentsize > length) {
        LOG_WARN("elf: program header table runs past the end of the file");
        return false;
    }
```
The table itself must be inside the file, before we start indexing into it.

---

## `elf_load`

```c
        const elf_program_header_t *ph =
            (const elf_program_header_t *)(image + eh->phoff + (size_t)i * eh->phentsize);
```
⚠️ **`phentsize`, not `sizeof`.** The format allows the header size to vary; reading it from the file
is what makes a loader forward-compatible.

The same pattern as the Multiboot memory map's variable-length entries.

```c
        if (ph->type != PT_LOAD) continue;
        if (ph->memsz == 0) continue;
```
A loader reads only `PT_LOAD`. `PT_DYNAMIC`, `PT_INTERP`, `PT_NOTE` and `PT_GNU_STACK` are for the
dynamic linker, the debugger and the kernel's stack policy.

---

## The three bounds checks

```c
        if (ph->vaddr >= KERNEL_VIRTUAL_BASE ||
            ph->vaddr + ph->memsz > KERNEL_VIRTUAL_BASE ||
            ph->vaddr + ph->memsz < ph->vaddr) {
            LOG_ERR("elf: segment %u wants [%08x,%08x) -- that is kernel space",
                    i, ph->vaddr, ph->vaddr + ph->memsz);
            return 0;
        }
```
⚠️ **The entire security boundary of the loader.**

> `vaddr` and `memsz` come out of a file, and if that file came from anywhere untrusted then a
> program header claiming vaddr = 0xC0100000 is asking us to map user-writable pages over the kernel.
> The bounds check below is the only thing standing between a malformed binary and a full compromise,
> and it is three lines that are very easy to leave out because every legitimate binary passes.

The third clause catches the overflow, exactly as `user_range_ok` does: `vaddr = 0xBFFFF000,
memsz = 0x80000000` wraps and would otherwise pass.

```c
        if ((size_t)ph->offset + ph->filesz > length) {
            LOG_ERR("elf: segment %u reads past the end of the file", i);
            return 0;
        }
```
⚠️ The copy would read kernel heap memory past the image and write it into the user's address space —
an information leak of whatever `kmalloc` last had there.

```c
        if (ph->filesz > ph->memsz) {
            LOG_ERR("elf: segment %u has filesz > memsz, which is nonsense", i);
            return 0;
        }
```
⚠️ Would make the copy write past the mapped region. There is no legitimate binary where the file
part exceeds the memory part.

---

## Permissions

```c
        uint32_t flags = PTE_USER;
        if (ph->flags & PF_W) flags |= PTE_WRITABLE;
```
`PTE_USER` always. `PTE_WRITABLE` only if asked, so `.text` and `.rodata` land read-only.

> There is no "no-execute" bit to clear: plain 32-bit paging cannot mark a page non-executable.
> [...] every readable page in a Nimbus process is executable, as it was on every x86 before 2004.

⚠️ `.rodata` is readable, therefore executable. A stack buffer is writable and readable, therefore
executable — which is what made classic stack-smashing attacks possible for fifteen years.

---

## Mapping and zeroing

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
⚠️ **Mapped writable unconditionally**, because we are about to copy into these pages and `CR0.WP`
means even the kernel cannot write to a read-only page.

⚠️ **Zeroed, then overwritten.**

> Doing it that way round — zero everything, then overwrite — is both simpler and safer than zeroing
> the tail afterwards: it guarantees no stale data from a previous process is ever visible in the
> padding at the end of the last page of a segment.

A segment ending at `0x0804b8e0` has 1824 unused bytes in its last page. Without zeroing they are
whatever the previous owner left.

This is also how `.bss` gets zeroed: `memsz > filesz`, and the extra bytes are already zero.

`if (*pte & PTE_PRESENT) continue;` handles two segments sharing a page — common for small binaries
where `.text` and `.rodata` are adjacent.

---

## The copy

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
⚠️ **We are not necessarily running in `dir`**, so we cannot `memcpy` to `ph->vaddr`.

Translate each page to its physical frame and write through the direct map. The third place the
direct map earns its keep, after page tables and `paging_clone_directory`.

`within` handles a segment not starting on a page boundary.

The `ASSERT` cannot fail — we mapped every page in the previous loop — and it is there because a
silent NULL would produce a copy into `0xC0000000`.

---

## Tightening

```c
        if (!(ph->flags & PF_W)) {
            for (vaddr_t v = seg_start; v < seg_end; v += PAGE_SIZE) {
                uint32_t *pte = paging_get_entry(dir, v, false);
                if (pte) *pte = (*pte & ~PTE_WRITABLE) | flags;
                paging_invalidate(v);
            }
        }
```
Map writable, copy, narrow.

⚠️ `paging_invalidate` on each page, because the writable mapping may already be in the TLB.

---

## The program break

```c
        if (ph->vaddr + ph->memsz > highest) highest = ph->vaddr + ph->memsz;
    }

    if (out_brk) *out_brk = ALIGN_UP(highest, PAGE_SIZE);
```
> The heap starts just past the last segment, page aligned. That is what "the program break" means,
> and it is why a program's first malloc returns an address a little above its `.bss`.

```c
    return eh->entry;
```
The virtual address of the first instruction, which `task_exec_regs` writes into `regs->eip`.

---

## The logging

```c
        LOG_DEBUG("elf: segment %u -> [%08x,%08x) %c%c%c",
                  i, ph->vaddr, ph->vaddr + ph->memsz,
                  (ph->flags & PF_R) ? 'r' : '-',
                  (ph->flags & PF_W) ? 'w' : '-',
                  (ph->flags & PF_X) ? 'x' : '-');
```
```
[    0.412] inf  elf: segment 0 -> [08048000,0804a3c0) r-x
[    0.412] inf  elf: segment 1 -> [0804b000,0804b8e0) rw-
```

Directly comparable with:

```bash
$ i686-elf-readelf -l bin/user/sh
  LOAD   0x001000 0x08048000 0x08048000 0x023c0 0x023c0 R E 0x1000
  LOAD   0x004000 0x0804b000 0x0804b000 0x000e0 0x008e0 RW  0x1000
```

Which is the check worth doing once.

---

## What demand paging would change

Right now `task_exec_regs` reads the whole file before calling this. For a 6 KB shell that is fine;
for a 200 MB binary it is 200 MB of `kmalloc`.

Demand paging: map the segments **not present**, storing the file offset in the other 31 bits — which
the CPU ignores when `P` is clear — and read one page in the fault handler.

⚠️ The obstacle is structural, not effort:

> the handler must be able to do disk I/O, which means it must be able to *block*, which means page
> faults can no longer be handled with the simple synchronous code above.

---

[Index](README.md) · [Chapter 44](../44-elf-loader.md)
