/* ===========================================================================
 *  nimbus/kernel/elf.c  --  turning a file into a running program
 * ===========================================================================
 *
 *  The loader is the smallest interesting part of an executable format. All it
 *  has to do is walk the program headers and, for each one that says PT_LOAD,
 *  put `filesz` bytes of the file at `vaddr` and zero the next
 *  `memsz - filesz`.
 *
 *  Everything else in ELF -- sections, symbols, relocations, the dynamic
 *  table, the hash table -- is for the linker, the debugger and the dynamic
 *  loader. A static executable needs none of it, which is why this file is
 *  120 lines and `ld` is 200,000.
 *
 *  Explained in: docs/44-elf-loader.md
 *  Line by line: docs/line-by-line/nimbus-elf.md
 * =========================================================================== */

#include <nimbus/elf.h>
#include <nimbus/paging.h>
#include <nimbus/pmm.h>
#include <nimbus/task.h>
#include <nimbus/kernel.h>
#include <nimbus/string.h>

bool elf_validate(const uint8_t *image, size_t length)
{
    if (length < sizeof(elf_header_t)) {
        LOG_WARN("elf: file is %u bytes, too short for a header", (uint32_t)length);
        return false;
    }

    const elf_header_t *eh = (const elf_header_t *)image;

    /*  Check everything, and say which check failed. "not an executable" as
     *  the only diagnostic turns a five-second problem -- you built for the
     *  wrong architecture -- into an afternoon.                               */
    if (eh->magic != ELF_MAGIC) {
        LOG_WARN("elf: bad magic %08x (expected %08x)", eh->magic, ELF_MAGIC);
        return false;
    }
    if (eh->class != ELFCLASS32) {
        LOG_WARN("elf: class %u -- this is a 64-bit binary, we are a 32-bit kernel",
                 eh->class);
        return false;
    }
    if (eh->endian != ELFDATA2LSB) {
        LOG_WARN("elf: big-endian binary on a little-endian machine");
        return false;
    }
    if (eh->machine != EM_386) {
        LOG_WARN("elf: machine %u, expected %u (i386)", eh->machine, EM_386);
        return false;
    }
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
    if (eh->phoff + (size_t)eh->phnum * eh->phentsize > length) {
        LOG_WARN("elf: program header table runs past the end of the file");
        return false;
    }
    return true;
}

/* ---------------------------------------------------------------------------
 *  elf_load
 *
 *  `dir` must be a fresh address space -- we map into it and do not check for
 *  collisions with anything already there.
 *
 *  A word on what makes this security-sensitive: `vaddr` and `memsz` come out
 *  of a file, and if that file came from anywhere untrusted then a program
 *  header claiming vaddr = 0xC0100000 is asking us to map user-writable pages
 *  over the kernel. The bounds check below is the only thing standing between
 *  a malformed binary and a full compromise, and it is three lines that are
 *  very easy to leave out because every legitimate binary passes.
 * ------------------------------------------------------------------------- */
uint32_t elf_load(page_directory_t *dir, const uint8_t *image, size_t length,
                  vaddr_t *out_brk)
{
    if (!elf_validate(image, length)) return 0;

    const elf_header_t *eh = (const elf_header_t *)image;
    vaddr_t highest = 0;

    for (uint16_t i = 0; i < eh->phnum; i++) {
        const elf_program_header_t *ph =
            (const elf_program_header_t *)(image + eh->phoff + (size_t)i * eh->phentsize);

        if (ph->type != PT_LOAD) continue;
        if (ph->memsz == 0) continue;

        /* ---- Refuse anything outside the user half --------------------------- */
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

        /* ---- Permissions -------------------------------------------------------
         *
         * PTE_USER always -- it is a user program. PTE_WRITABLE only if the
         * segment asked for it, so .text and .rodata land read-only.
         *
         * There is no "no-execute" bit to clear: plain 32-bit paging cannot
         * mark a page non-executable. That needs PAE and the NX bit, which
         * means 64-bit page table entries and a third level of tables.
         * Chapter 48 explains what it would take; for now, every readable page
         * in a Nimbus process is executable, as it was on every x86 before
         * 2004.
         */
        uint32_t flags = PTE_USER;
        if (ph->flags & PF_W) flags |= PTE_WRITABLE;

        vaddr_t seg_start = ALIGN_DOWN(ph->vaddr, PAGE_SIZE);
        vaddr_t seg_end   = ALIGN_UP(ph->vaddr + ph->memsz, PAGE_SIZE);

        /*  Map writable first, unconditionally: we are about to copy into
         *  these pages, and CR0.WP means even the kernel cannot write to a
         *  read-only page. The permissions are tightened after the copy.      */
        for (vaddr_t v = seg_start; v < seg_end; v += PAGE_SIZE) {
            uint32_t *pte = paging_get_entry(dir, v, true);
            if (*pte & PTE_PRESENT) continue;

            paddr_t frame = pmm_alloc_frame();
            if (frame == PMM_NO_FRAME) { LOG_ERR("elf: out of memory"); return 0; }

            memset(P2V(frame), 0, PAGE_SIZE);
            *pte = frame | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
        }

        /* ---- Copy the file bytes -------------------------------------------------
         *
         * We are not necessarily running in `dir`, so we cannot just memcpy to
         * ph->vaddr. Instead we translate each page to its physical frame and
         * write through the kernel's direct map. This is the same trick the
         * page table code uses and the reason the direct map exists.
         */
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

        /*  Bytes between filesz and memsz are .bss and are already zero,
         *  because every frame was zeroed before the copy. Doing it that way
         *  round -- zero everything, then overwrite -- is both simpler and
         *  safer than zeroing the tail afterwards: it guarantees no stale data
         *  from a previous process is ever visible in the padding at the end
         *  of the last page of a segment.                                     */

        if (!(ph->flags & PF_W)) {
            for (vaddr_t v = seg_start; v < seg_end; v += PAGE_SIZE) {
                uint32_t *pte = paging_get_entry(dir, v, false);
                if (pte) *pte = (*pte & ~PTE_WRITABLE) | flags;
                paging_invalidate(v);
            }
        }

        if (ph->vaddr + ph->memsz > highest) highest = ph->vaddr + ph->memsz;

        LOG_DEBUG("elf: segment %u -> [%08x,%08x) %c%c%c",
                  i, ph->vaddr, ph->vaddr + ph->memsz,
                  (ph->flags & PF_R) ? 'r' : '-',
                  (ph->flags & PF_W) ? 'w' : '-',
                  (ph->flags & PF_X) ? 'x' : '-');
    }

    /*  The heap starts just past the last segment, page aligned. That is what
     *  "the program break" means, and it is why a program's first malloc
     *  returns an address a little above its .bss.                            */
    if (out_brk) *out_brk = ALIGN_UP(highest, PAGE_SIZE);

    return eh->entry;
}
