; =============================================================================
;  nimbus/boot/boot.asm  --  multiboot header, early paging, higher-half jump
; =============================================================================
;
;  The hardest twenty lines in the kernel, and they run before anything else.
;
;  The problem: we want the kernel to *live* at virtual address 0xC0100000, so
;  that every process can have the bottom 3 GiB of its address space to itself.
;  But the bootloader loads us at physical 0x00100000 with paging off, which
;  means that for the first few instructions the kernel is at an address it was
;  not linked for. Any absolute reference -- a global variable, a call to
;  another function, a string constant -- points 3 GiB too high, into memory
;  that does not exist.
;
;  The trick, which link.ld makes possible, is to split the kernel into two
;  pieces. The sections named `.multiboot.*` are linked at low physical
;  addresses and hold only this file's early code and its page directory;
;  everything else is linked at 0xC0100000. So the early code can refer to its
;  own data normally, set up paging, and *then* jump into the high-addressed
;  world, after which the low addresses are removed and never used again.
;
;  Explained in: docs/11-multiboot.md and docs/25-higher-half.md
;  Line by line: docs/line-by-line/nimbus-boot.md
; =============================================================================

[BITS 32]

; ---- Multiboot 1 header constants -------------------------------------------
MB_ALIGN     equ 1 << 0                     ; align loaded modules on 4 KiB
MB_MEMINFO   equ 1 << 1                     ; give us the memory map
MB_FLAGS     equ MB_ALIGN | MB_MEMINFO
MB_MAGIC     equ 0x1BADB002
MB_CHECKSUM  equ -(MB_MAGIC + MB_FLAGS)     ; the three must sum to zero mod 2^32

KERNEL_VIRTUAL_BASE equ 0xC0000000
KERNEL_PAGE_NUMBER  equ (KERNEL_VIRTUAL_BASE >> 22)   ; = 768

; How much memory the boot page directory covers, in 4 MiB pages. Four is
; enough for the kernel image, the multiboot structures and the initrd module,
; all of which the bootloader packs into the first few megabytes. paging_init()
; replaces this directory with a proper 4 KiB-granular one almost immediately.
BOOT_PAGES   equ 4

; =============================================================================
;  .multiboot.data -- linked at physical 0x00100000
; =============================================================================
section .multiboot.data

;  The header must appear within the first 8 KiB of the file and be 4-byte
;  aligned. The bootloader scans for the magic number; if it is not found, you
;  get "Error: invalid multiboot header" or, from QEMU, a blunt refusal to boot
;  a file it otherwise loaded perfectly.
align 4
multiboot_header:
    dd MB_MAGIC
    dd MB_FLAGS
    dd MB_CHECKSUM

;  The boot page directory.
;
;  A page directory must be 4 KiB aligned because CR3 holds only the top 20
;  bits of its address -- the low 12 are used for flags, so a misaligned
;  directory is not merely slow, it is unrepresentable.
;
;  We fill it with 4 MiB pages (the PS bit), which skips the second level
;  entirely: one directory entry covers 4 MiB with no page table at all. That
;  requires CR4.PSE, which every CPU since the Pentium has. Using 4 KiB pages
;  here would mean assembling four 4 KiB page tables by hand in assembly, for a
;  mapping we are about to throw away.
;
;  Entry value 0x00000083 = frame 0 | PS | RW | PRESENT.
align 4096
global boot_page_directory
boot_page_directory:
    ; Entries 0..3: identity map the first 16 MiB. Needed for exactly three
    ; instructions -- the ones between enabling paging and jumping high -- and
    ; deleted immediately afterwards. If we left it in place, a null pointer
    ; dereference in the kernel would quietly read the IVT instead of faulting.
    %assign i 0
    %rep BOOT_PAGES
        dd (i << 22) | 0x83
    %assign i i+1
    %endrep

    ; Entries 4..767: not present.
    times (KERNEL_PAGE_NUMBER - BOOT_PAGES) dd 0

    ; Entries 768..771: the same 16 MiB, mapped again at 0xC0000000.
    %assign i 0
    %rep BOOT_PAGES
        dd (i << 22) | 0x83
    %assign i i+1
    %endrep

    ; The rest of the higher half: not present yet.
    times (1024 - KERNEL_PAGE_NUMBER - BOOT_PAGES) dd 0

; =============================================================================
;  .multiboot.text -- also linked low, because it runs before paging
; =============================================================================
section .multiboot.text

global _start
extern kmain
extern __bss_start
extern __bss_end

_start:
    ; State on entry, guaranteed by the Multiboot specification:
    ;   EAX = 0x2BADB002
    ;   EBX = physical address of the multiboot_info structure
    ;   CS  = a flat 32-bit ring 0 code segment; DS/ES/FS/GS/SS flat data
    ;   A20 is enabled, paging is off, interrupts are off, ESP is undefined.
    ;
    ; "ESP is undefined" is the one that bites: we cannot call anything, push
    ; anything, or use a single C function until we set up a stack. Until then
    ; EAX and EBX are the only places the boot information can live, so nothing
    ; below is allowed to clobber them.

    ; ---- Turn on 4 MiB pages -------------------------------------------------
    mov ecx, cr4
    or  ecx, 0x00000010             ; CR4.PSE, bit 4
    mov cr4, ecx

    ; ---- Point CR3 at the directory -----------------------------------------
    ; boot_page_directory is in .multiboot.data, which link.ld placed at a low
    ; address, so this symbol *is* the physical address. This is the entire
    ; payoff of the two-section split: no `- 0xC0000000` fudge factor, and
    ; therefore no chance of forgetting one.
    mov ecx, boot_page_directory
    mov cr3, ecx

    ; ---- Turn on paging ------------------------------------------------------
    mov ecx, cr0
    or  ecx, 0x80000000             ; CR0.PG, bit 31
    mov cr0, ecx
    ; From this instruction onwards, every address the CPU sees goes through
    ; the MMU. EIP is currently somewhere around 0x00101000, which is covered
    ; by directory entry 0 -- the identity mapping. That is the only reason the
    ; *next* instruction can be fetched at all.

    ; ---- Jump to the higher half ---------------------------------------------
    ; An absolute indirect jump, not a relative one. `jmp higher_half` would
    ; assemble to a relative displacement and keep EIP down here in the low
    ; mapping; loading the absolute address into a register first forces EIP to
    ; become 0xC01xxxxx, which is what lets us unmap the low addresses.
    lea ecx, [higher_half]
    jmp ecx

; =============================================================================
;  Everything from here is linked at 0xC0100000 and up
; =============================================================================
section .text

higher_half:
    ; ---- Remove the identity mapping ------------------------------------------
    ; We are executing out of the higher half now, so the low mapping has done
    ; its job. Zeroing these entries means a stray write to a small address --
    ; the classic `*(int*)0 = 1` -- becomes a page fault we can report, instead
    ; of silently overwriting the interrupt vector table.
    ;
    ; The write itself goes through the *high* mapping: both windows point at
    ; the same physical frame, so 0xC0100000 + offset reaches the same bytes.
    mov dword [boot_page_directory + KERNEL_VIRTUAL_BASE +  0], 0
    mov dword [boot_page_directory + KERNEL_VIRTUAL_BASE +  4], 0
    mov dword [boot_page_directory + KERNEL_VIRTUAL_BASE +  8], 0
    mov dword [boot_page_directory + KERNEL_VIRTUAL_BASE + 12], 0

    ; Changing a page table does not change the TLB. Reloading CR3 flushes
    ; every non-global entry, which is the blunt instrument; `invlpg` is the
    ; scalpel, and here the blunt instrument is correct because we changed four
    ; entries covering 16 MiB.
    mov ecx, cr3
    mov cr3, ecx

    ; ---- Zero the .bss ---------------------------------------------------------
    ; A multiboot loader is supposed to zero the difference between a segment's
    ; file size and its memory size, and both GRUB and QEMU do. Doing it
    ; ourselves anyway costs eight instructions and removes a dependency on
    ; someone else's correctness for a failure -- uninitialised globals -- that
    ; would present as an unreproducible bug weeks later.
    ;
    ; No stack is needed: `rep stosb` touches only EDI, ECX and EAX. Which is
    ; just as well, because the stack we are about to use is inside the region
    ; being cleared.
    mov edi, __bss_start
    mov ecx, __bss_end
    sub ecx, edi
    xor eax, eax
    cld
    rep stosb

    ; ---- Stack ---------------------------------------------------------------
    mov esp, stack_top
    xor ebp, ebp                    ; terminate the frame-pointer chain

    ; ---- Call kmain(magic, mbi_physical) -------------------------------------
    ; Pushed in reverse order, because the System V cdecl convention puts the
    ; first argument at the lowest address.
    push ebx                        ; arg 2: multiboot info, PHYSICAL address
    push eax                        ; arg 1: the magic number, to verify
    call kmain

    ; kmain must not return. If it does, say so on the screen rather than
    ; wandering into whatever bytes follow -- a returning kmain is a bug, and a
    ; silent one is a bug you will chase for an hour.
    cli
.hang:
    hlt
    jmp .hang

; =============================================================================
;  The initial kernel stack
; =============================================================================
;  16 KiB, in .bss so it costs nothing in the image. This is the stack the
;  kernel runs on until task_init() gives every task its own; after that it
;  becomes the idle task's stack.
;
;  There is no guard page below it. Chapter 26 adds one, and explains what a
;  kernel stack overflow looks like without one: silent corruption of whatever
;  .bss variable happens to be allocated just underneath.
section .bss
align 16
global stack_bottom
global stack_top
stack_bottom:
    resb 16384
stack_top:
