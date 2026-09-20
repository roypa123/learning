; =============================================================================
;  spark/kernel/entry.asm  --  the first instruction of the kernel
; =============================================================================
;
;  Stage 2 does `jmp 0x100000`. Whatever byte sits at 0x100000 is the kernel's
;  first instruction, so this file has to be linked first -- see link.ld, which
;  places the section `.text.entry` at the very start of the image.
;
;  A C compiler cannot produce this code. Before C runs at all, three things
;  must already be true, and none of them are true when we arrive:
;
;      * ESP must point at a valid stack, because the first thing any C
;        function does is push things onto it.
;      * The .bss section must be zeroed, because the C standard promises that
;        uninitialised globals start at zero and the compiler emits code that
;        relies on that promise.
;      * EBP must be zero, so that a stack walker (Chapter 47) knows where the
;        chain of stack frames ends.
;
;  Explained in: docs/09-freestanding-c.md
; =============================================================================

[BITS 32]

global _start
extern kmain
extern __bss_start
extern __bss_end

; A dedicated section name, so that link.ld can name it and put it first.
section .text.entry

_start:
    cli                         ; stage 2 already cleared IF. Do it again: this
                                ; file must be correct on its own terms.

    ; ---- Zero the .bss -------------------------------------------------------
    ; .bss is "block started by symbol": globals that are zero at startup. The
    ; linker reserves address space for them but stores no bytes in the file,
    ; which is why a kernel with a 4 MiB page-aligned buffer is not a 4 MiB
    ; binary. Nobody zeroes it for us -- on a hosted system the loader does it,
    ; and we are the loader now.
    mov edi, __bss_start
    mov ecx, __bss_end
    sub ecx, edi                ; ECX = size of .bss in bytes
    xor eax, eax
    cld
    rep stosb                   ; while (ecx--) *edi++ = al;

    ; ---- Stack ---------------------------------------------------------------
    ; We are still running on the stack stage 2 set up at 0x90000. Switch to one
    ; that belongs to us and that the linker placed somewhere it knows about.
    ; x86 stacks grow downwards, so ESP starts at the *high* end.
    mov esp, stack_top
    xor ebp, ebp                ; end of the frame-pointer chain

    ; ---- Into C --------------------------------------------------------------
    call kmain

    ; kmain is not supposed to return. If it does, stop the machine in a way
    ; that costs no power and cannot be mistaken for a crash: hlt parks the CPU
    ; until an interrupt arrives, and IF is clear, so none ever will.
.halt:
    cli
    hlt
    jmp .halt

section .bss
align 16                        ; the SysV ABI wants 16-byte stack alignment at
                                ; call boundaries; some SSE instructions fault
                                ; without it, and GCC may emit them for struct
                                ; copies even with -mno-sse elsewhere.
stack_bottom:
    resb 16384                  ; 16 KiB of kernel stack
stack_top:
