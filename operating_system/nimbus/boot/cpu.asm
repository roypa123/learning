; =============================================================================
;  nimbus/boot/cpu.asm  --  the instructions C cannot express
; =============================================================================
;
;  Five routines. Every one of them exists because there is no C for it: you
;  cannot write `lgdt` as an expression, you cannot reload CS with an
;  assignment, and you cannot return from a function into ring 3.
;
;  Explained in: docs/15-gdt.md, docs/30-context-switch.md, docs/32-usermode.md
; =============================================================================

[BITS 32]

; =============================================================================
;  gdt_flush(gdt_ptr_t *ptr)  --  install a GDT and start using it
; =============================================================================
global gdt_flush
gdt_flush:
    mov eax, [esp + 4]             ; the argument: address of the 6-byte pointer
    lgdt [eax]

    ; `lgdt` loads the register. It does not change a single segment register,
    ; and the CPU keeps using the descriptors it cached when those registers
    ; were last loaded -- from the *old* table. Until we reload them we are
    ; running on a GDT that may no longer exist.
    mov ax, 0x10                   ; SEL_KDATA
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; CS is the one that cannot be assigned. There is no `mov cs, ax`; the only
    ; instructions that load CS are far jumps, far calls, far returns and iret.
    ; A far jump to the very next instruction is the idiom, and it costs about
    ; twenty cycles because it also flushes the pipeline.
    jmp 0x08:.reload_cs            ; SEL_KCODE
.reload_cs:
    ret

; =============================================================================
;  idt_load(idt_ptr_t *ptr)
; =============================================================================
global idt_load
idt_load:
    mov eax, [esp + 4]
    lidt [eax]
    ret
    ; No reload dance here: there is no "current interrupt descriptor" cached
    ; in a register. The next interrupt simply uses the new table.

; =============================================================================
;  tss_flush(void)  --  tell the CPU which descriptor is the TSS
; =============================================================================
global tss_flush
tss_flush:
    mov ax, 0x2B                   ; SEL_TSS, with RPL 3 in the low bits
    ltr ax                         ; "load task register"
    ret
    ; RPL 3 looks wrong and is not. The CPU checks the *descriptor's* DPL when
    ; deciding whether ring 3 may use a gate; the RPL in the selector we load
    ; here is not checked against anything, and the value 0x2B is what every
    ; kernel uses because the TSS descriptor has DPL 3 so that `iret` to ring 3
    ; is legal. Setting it to 0x28 also works on real hardware and on QEMU.

; =============================================================================
;  switch_context(context_t **old, context_t *new)
; =============================================================================
;
;  The whole of multitasking is these fourteen instructions.
;
;  A "context" is not the full register set. The System V calling convention
;  divides registers into caller-saved (EAX, ECX, EDX -- a function may destroy
;  them) and callee-saved (EBX, ESI, EDI, EBP -- a function must preserve
;  them). Whoever called switch_context() is a C function, so it has already
;  spilled anything it cared about in the caller-saved set. We only have to
;  preserve the other four, plus the stack pointer, plus the return address --
;  and the return address is already on the stack, because we were called.
;
;  So: push four registers, save ESP, load the other ESP, pop four registers,
;  ret. The `ret` pops a return address that belongs to a *different task*, and
;  execution continues wherever that task last called switch_context. Nothing
;  about the instruction knows it just changed universes.
;
;  Explained in: docs/30-context-switch.md
;  Line by line: docs/line-by-line/nimbus-switch.md
; =============================================================================
global switch_context
switch_context:
    mov eax, [esp + 4]             ; eax = old, a context_t**
    mov edx, [esp + 8]             ; edx = new, a context_t*

    ; Push in this order so that the resulting memory layout, read from the
    ; lowest address upwards, is edi, esi, ebx, ebp, eip -- which is exactly
    ; the field order of `struct context`. If you reorder these four pushes,
    ; reorder the struct with them or the scheduler will restore EBP into EDI.
    push ebp
    push ebx
    push esi
    push edi

    mov [eax], esp                 ; *old = esp
                                   ; The saved ESP *is* the saved context: the
                                   ; five values live on the task's own kernel
                                   ; stack, and one pointer finds them all.

    mov esp, edx                   ; switch stacks. From this instruction on we
                                   ; are executing on the new task's stack --
                                   ; still in the old task's code, for one more
                                   ; nanosecond.

    pop edi
    pop esi
    pop ebx
    pop ebp

    ret                            ; pops the *new* task's saved EIP.
                                   ; A freshly created task has never run, so
                                   ; task.c hand-builds a stack whose `eip`
                                   ; slot holds the address of its entry point
                                   ; -- and this `ret` starts it.

; =============================================================================
;  enter_usermode(uint32_t entry, uint32_t user_stack)
; =============================================================================
;
;  There is no instruction that lowers your privilege level. The only way down
;  from ring 0 to ring 3 is to return from an interrupt that never happened --
;  so we build the stack frame the CPU would have pushed on the way *in*, and
;  execute `iret` to consume it.
;
;  Explained in: docs/32-usermode.md
; =============================================================================
global enter_usermode
enter_usermode:
    mov ebx, [esp + 4]             ; entry point in userland
    mov ecx, [esp + 8]             ; top of the user stack

    ; Data segments can be loaded with ring 3 selectors right now: the CPU only
    ; checks that our *current* privilege (0) is numerically <= the descriptor's
    ; DPL (3), and 0 <= 3. Going the other way is what is forbidden.
    mov ax, 0x23                   ; SEL_UDATA (descriptor 4, RPL 3)
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Now the fake interrupt frame, pushed in the order `iret` will pop it --
    ; which means pushing the last-popped item first.
    push 0x23                      ; SS   -- user stack segment
    push ecx                       ; ESP  -- user stack pointer

    pushf                          ; take a copy of the current EFLAGS...
    pop  eax
    or   eax, 0x200                ; ...and set IF, bit 9.
    push eax                       ; EFLAGS
    ; This one line is the difference between a working system and a machine
    ; that locks up the moment the first user program starts. Interrupts are
    ; off right now. If we hand ring 3 an EFLAGS with IF clear, the timer never
    ; fires again, the scheduler never runs again, and userland owns the CPU
    ; forever -- and ring 3 cannot execute `sti` to fix it, because `sti` is a
    ; privileged instruction. The only place IF can be set for a user process
    ; is here, in the frame we hand to `iret`.

    push 0x1B                      ; CS -- SEL_UCODE, RPL 3
    push ebx                       ; EIP

    iret
    ; The CPU pops EIP and CS, sees that the new CS has privilege 3 while the
    ; current one has 0, and therefore also pops ESP and SS and switches
    ; stacks. Then it is running user code. There is no ring-3 version of this
    ; instruction: the trip back is a system call or an interrupt.
