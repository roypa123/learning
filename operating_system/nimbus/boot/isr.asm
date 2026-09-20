; =============================================================================
;  nimbus/boot/isr.asm  --  the 49 doors into the kernel
; =============================================================================
;
;  An IDT entry can only point at an address. It cannot pass an argument, so
;  the handler for vector 13 has no way of knowing it is vector 13 -- unless we
;  give every vector its own tiny stub that pushes the number and then jumps to
;  shared code. That is all this file is: 49 stubs and one common tail.
;
;  It has to be assembly for two reasons that are worth being precise about.
;
;  First, the return. A C function ends with `ret`, which pops one dword. An
;  interrupt handler must end with `iret`, which pops EIP, CS and EFLAGS -- and
;  SS:ESP too if the interrupt crossed a privilege boundary. GCC will not emit
;  `iret` for you. (It has an `__attribute__((interrupt))` that will, but it
;  produces subtly different stack layouts across versions and hides exactly
;  the mechanism this book exists to show.)
;
;  Second, the entry. The CPU does not save the general-purpose registers. If
;  the first thing that runs is compiled C, that C will clobber EAX before
;  anything has saved it, and the interrupted program will resume with a
;  corrupted register and no clue why.
;
;  Explained in: docs/16-idt-exceptions.md
;  Line by line: docs/line-by-line/nimbus-idt.md
; =============================================================================

[BITS 32]

extern interrupt_dispatch          ; the single C entry point, in kernel/isr.c

; -----------------------------------------------------------------------------
;  Two macros, because the CPU is inconsistent about error codes
;
;  For eight of the 32 exceptions the CPU pushes a 32-bit error code before the
;  EIP/CS/EFLAGS frame. For the other 24 it does not. If we did nothing, half
;  our handlers would see a stack one dword deeper than the other half, and
;  every access to regs->eip would be right half the time.
;
;  So the no-error-code stubs push a zero themselves. Every frame in the kernel
;  then has identical shape, and `registers_t` describes all of them.
; -----------------------------------------------------------------------------

%macro ISR_NOERR 1
global isr%1
isr%1:
    push dword 0                   ; fake error code, so all frames match
    push dword %1                  ; the vector number
    jmp  isr_common_stub
%endmacro

%macro ISR_ERR 1
global isr%1
isr%1:
    ; no push of zero: the CPU already pushed a real error code
    push dword %1
    jmp  isr_common_stub
%endmacro

; ---- The 32 CPU exceptions ---------------------------------------------------
;  The eight with error codes are 8, 10, 11, 12, 13, 14, 17 and 30. There is no
;  pattern to it; it is a list you check against the manual (Volume 3, Table
;  6-1) and then never think about again.
ISR_NOERR 0      ; #DE  divide error
ISR_NOERR 1      ; #DB  debug
ISR_NOERR 2      ;      non-maskable interrupt
ISR_NOERR 3      ; #BP  breakpoint (int3)
ISR_NOERR 4      ; #OF  overflow
ISR_NOERR 5      ; #BR  bound range exceeded
ISR_NOERR 6      ; #UD  invalid opcode
ISR_NOERR 7      ; #NM  device not available (no FPU / TS set)
ISR_ERR   8      ; #DF  double fault          <- always error code 0
ISR_NOERR 9      ;      coprocessor segment overrun (386 only)
ISR_ERR   10     ; #TS  invalid TSS
ISR_ERR   11     ; #NP  segment not present
ISR_ERR   12     ; #SS  stack-segment fault
ISR_ERR   13     ; #GP  general protection
ISR_ERR   14     ; #PF  page fault            <- CR2 holds the address
ISR_NOERR 15     ;      reserved
ISR_NOERR 16     ; #MF  x87 floating point
ISR_ERR   17     ; #AC  alignment check
ISR_NOERR 18     ; #MC  machine check
ISR_NOERR 19     ; #XM  SIMD floating point
ISR_NOERR 20     ; #VE  virtualisation
ISR_NOERR 21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_ERR   30     ; #SX  security exception
ISR_NOERR 31

; ---- The 16 hardware IRQs, remapped to vectors 32..47 -----------------------
;  They use the same macro and the same stub. The dispatcher tells them apart
;  by number and sends the end-of-interrupt signal for the ones that need it.
ISR_NOERR 32     ; IRQ0  timer
ISR_NOERR 33     ; IRQ1  keyboard
ISR_NOERR 34     ; IRQ2  cascade
ISR_NOERR 35     ; IRQ3  COM2
ISR_NOERR 36     ; IRQ4  COM1
ISR_NOERR 37     ; IRQ5
ISR_NOERR 38     ; IRQ6  floppy
ISR_NOERR 39     ; IRQ7  LPT1 / spurious
ISR_NOERR 40     ; IRQ8  RTC
ISR_NOERR 41     ; IRQ9
ISR_NOERR 42     ; IRQ10
ISR_NOERR 43     ; IRQ11
ISR_NOERR 44     ; IRQ12 mouse
ISR_NOERR 45     ; IRQ13 FPU
ISR_NOERR 46     ; IRQ14 ATA primary
ISR_NOERR 47     ; IRQ15 ATA secondary

; ---- The system call gate ----------------------------------------------------
ISR_NOERR 128    ; int 0x80

; =============================================================================
;  isr_common_stub -- shared by all 49
; =============================================================================
;
;  On entry the stack looks like this, from the top (lowest address) down:
;
;      esp+0    the vector number      <- our stub pushed this
;      esp+4    the error code         <- the CPU's, or our zero
;      esp+8    EIP                    <- +
;      esp+12   CS                       | the CPU pushed these three
;      esp+16   EFLAGS                 <- +
;      esp+20   ESP  (only if we came from ring 3)
;      esp+24   SS   (only if we came from ring 3)
;
;  and we are going to add to it until it matches `registers_t` exactly.
; =============================================================================
isr_common_stub:
    pusha                          ; push EAX ECX EDX EBX ESP EBP ESI EDI, in
                                   ; that order -- so they appear on the stack
                                   ; in the reverse order, EDI lowest, which is
                                   ; why registers_t lists them that way.

    ; Save DS. The interrupted code might have been ring 3 with user segments
    ; loaded; the handler must run with kernel segments or every memory access
    ; through DS goes to the wrong descriptor.
    ;
    ; Only DS is saved, not ES/FS/GS. That is a deliberate shortcut and it is
    ; safe *because* we always reload all four with the same kernel selector
    ; and `iret` restores the ring 3 ones from the stack frame along with CS
    ; and SS. It would not be safe in a kernel that used FS or GS for
    ; per-CPU data, which is exactly what Chapter 48 warns about for SMP.
    mov  eax, ds
    push eax

    mov  ax, 0x10                  ; SEL_KDATA
    mov  ds, ax
    mov  es, ax
    mov  fs, ax
    mov  gs, ax

    ; ESP now points at the bottom of a complete `registers_t`. Pass it as the
    ; single argument -- by pointer, not by value, so that a handler can
    ; *modify* the saved state. That is not a convenience: it is how a system
    ; call returns a value (write regs->eax), how the scheduler preempts (change
    ; nothing, but switch stacks around it), and how a debugger single-steps
    ; (set the TF bit in regs->eflags).
    push esp
    call interrupt_dispatch
    add  esp, 4                    ; discard the argument

    ; ---- Unwind, exactly in reverse ------------------------------------------
    ;
    ; This label is not just a comment marker. task.c builds a brand-new kernel
    ; stack for a forked child whose topmost contents are a copy of its
    ; parent's `registers_t`, and arranges for the child's very first scheduled
    ; instruction to be this one. The child then unwinds a trap frame it never
    ; pushed and `iret`s into userland as if it had just made a system call --
    ; which, from its point of view, it had. Chapter 34 walks through it.
global isr_return
isr_return:
    pop  eax
    mov  ds, ax
    mov  es, ax
    mov  fs, ax
    mov  gs, ax

    popa
    add  esp, 8                    ; discard int_no and err_code: the CPU knows
                                   ; nothing about them and `iret` would take
                                   ; them for EIP and CS.
    iret
    ; `iret` pops EIP, CS and EFLAGS -- and if the CS it pops has a lower
    ; privilege level than the current one, it also pops ESP and SS and
    ; switches stacks. One instruction, two entirely different behaviours,
    ; chosen by two bits of a value on the stack. It is the single densest
    ; instruction on the machine and Chapter 32 spends a section on it.

; =============================================================================
;  The stub table, so that idt.c can install all 49 in a loop
; =============================================================================
;  Without this, idt.c needs 49 `extern void isrN(void);` declarations and 49
;  calls to idt_set_gate. With it, a four-line loop. The cost is that the table
;  and the stubs can drift apart -- so the assembler builds the table from the
;  same numbers, with a %rep, and drift is impossible.
section .data
global isr_stub_table
isr_stub_table:
%assign vector 0
%rep 48
    dd isr %+ vector
%assign vector vector + 1
%endrep

global isr_syscall_stub
isr_syscall_stub:
    dd isr128
