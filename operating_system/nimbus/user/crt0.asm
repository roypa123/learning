; =============================================================================
;  nimbus/user/crt0.asm  --  what runs before main()
; =============================================================================
;
;  Every C program on every system has one of these, and almost nobody has read
;  one. It is the answer to "how does main get its arguments, and what happens
;  when it returns?"
;
;  The kernel does not call main. It cannot: main's signature is a C
;  convention, the kernel has no idea whether this program even has a main, and
;  there is no way to "call" into a process that is not running yet. What the
;  kernel does is far blunter -- it points EIP at the ELF header's entry point
;  and irets. Whatever is at that address is the program.
;
;  So `_start` is the entry point, and its job is to turn the kernel's
;  convention (argc and argv on the stack) into C's convention (arguments
;  pushed for a cdecl call), and to turn main's return value into an exit()
;  call, because returning from _start would return to nowhere.
;
;  Explained in: docs/45-user-libc.md
; =============================================================================

[BITS 32]

global _start
extern main
extern exit

section .text

_start:
    ; task_spawn_user() and task_exec_regs() both leave the stack like this:
    ;
    ;       [esp]      argc
    ;       [esp+4]    argv          (a char ** into the strings above)
    ;
    ; That layout is a choice our kernel made, not a law. Linux puts argc,
    ; then argv, then a NULL, then envp, then a NULL, then the auxiliary
    ; vector -- and a real crt0 has to walk past all of it to find envp.

    mov eax, [esp]                  ; argc
    mov ebx, [esp + 4]              ; argv

    xor ebp, ebp                    ; end the frame-pointer chain here, so a
                                    ; backtrace stops at _start rather than
                                    ; walking into whatever the kernel left
                                    ; on the stack below us.

    ; cdecl: arguments pushed right to left, so argv first and argc second.
    push ebx
    push eax
    call main
    add esp, 8

    ; main returned. Its value is in EAX, which is exactly where exit() wants
    ; its argument to come from -- push it and call.
    ;
    ; This is why `int main()` without a return statement is defined to return
    ; 0 in C99 and undefined before it: without that rule, the exit status is
    ; whatever happened to be in EAX, which is usually the return value of the
    ; last function main called.
    push eax
    call exit

    ; exit() never returns. If it somehow did, do not fall off the end of the
    ; program into unmapped memory -- stop here, visibly.
.hang:
    jmp .hang
