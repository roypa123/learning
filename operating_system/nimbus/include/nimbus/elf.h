/* ===========================================================================
 *  nimbus/include/nimbus/elf.h  --  Executable and Linkable Format
 * ===========================================================================
 *
 *  ELF is what our cross-compiler emits and what our loader reads, so the
 *  kernel needs just enough of it to answer: is this a program, where does it
 *  start, and which bytes of the file go at which addresses?
 *
 *  An ELF file has two tables of headers describing the same bytes from two
 *  points of view. *Section* headers describe the file for the linker: .text,
 *  .rodata, .symtab, relocation info. *Program* headers describe it for the
 *  loader: "map 0x1a40 bytes of file offset 0x1000 at virtual address
 *  0x08049000, readable and executable". A loader reads only the program
 *  headers, and a stripped binary has no section headers at all.
 *
 *  Explained in: docs/44-elf-loader.md
 * =========================================================================== */
#ifndef NIMBUS_ELF_H
#define NIMBUS_ELF_H

#include <nimbus/types.h>
#include <nimbus/paging.h>

#define ELF_MAGIC 0x464C457Fu    /* "\x7FELF", little-endian                  */

#define ELFCLASS32   1
#define ELFDATA2LSB  1
#define EV_CURRENT   1
#define ET_EXEC      2           /* a fixed-address executable; what we load  */
#define ET_DYN       3           /* position independent; needs a dynamic loader */
#define EM_386       3

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
    uint32_t shoff;          /* file offset of the section header table       */
    uint32_t flags;
    uint16_t ehsize;
    uint16_t phentsize;      /* size of one program header                    */
    uint16_t phnum;          /* how many program headers                      */
    uint16_t shentsize;
    uint16_t shnum;
    uint16_t shstrndx;
} PACKED elf_header_t;

#define PT_NULL     0
#define PT_LOAD     1        /* the only type a loader must understand        */
#define PT_DYNAMIC  2
#define PT_INTERP   3        /* "run this dynamic linker instead of me"       */
#define PT_NOTE     4
#define PT_PHDR     6
#define PT_GNU_STACK 0x6474e551

#define PF_X        0x1
#define PF_W        0x2
#define PF_R        0x4

typedef struct elf_program_header {
    uint32_t type;
    uint32_t offset;         /* where in the file                             */
    uint32_t vaddr;          /* where in memory                               */
    uint32_t paddr;          /* ignored on systems with an MMU                */
    uint32_t filesz;         /* how many bytes to copy                        */
    uint32_t memsz;          /* how many bytes to reserve                     */
    uint32_t flags;
    uint32_t align;
} PACKED elf_program_header_t;

/*  memsz > filesz is how .bss is expressed: the extra bytes are not in the
 *  file and must be zeroed by the loader. Forgetting that is a bug whose
 *  symptom is a program that works until it reads an uninitialised global,
 *  which may be months later.                                                 */

/*  Load an ELF image into `dir` and return its entry point, or 0 on failure.
 *  The image must already be in kernel memory; the loader does not read files,
 *  because separating "parse" from "fetch" is what lets us load from the
 *  initrd, from FAT16, or from a buffer in a test.                            */
uint32_t elf_load(page_directory_t *dir, const uint8_t *image, size_t length,
                  vaddr_t *out_brk);
bool     elf_validate(const uint8_t *image, size_t length);

#endif /* NIMBUS_ELF_H */
