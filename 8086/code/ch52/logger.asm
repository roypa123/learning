; logger.asm — sample ADC channel 0 once a second and write to a file
; nasm -f bin logger.asm -o logger.com
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

PORTA   equ  0x00
PORTB   equ  0x02
PORTC   equ  0x04
CTRL    equ  0x06

SAMPLES equ  60                 ; one minute at one per second

start:
        mov  al, 0x99           ; A input, B output, C input
        out  CTRL, al

        ; --- create the output file ---
        mov  dx, filename
        xor  cx, cx
        mov  ah, 0x3C
        int  0x21
        jc   .file_error
        mov  [handle], ax

        print startmsg

        mov  cx, SAMPLES
        xor  si, si             ; SI = the sample number
.next:
        push cx

        xor  al, al             ; channel 0
        call adc_read
        jc   .adc_error
        mov  [sample], al

        ; --- format: "nn,mmmm\r\n" ---
        mov  di, line
        mov  ax, si
        call put_dec
        mov  al, ','
        stosb
        mov  al, [sample]
        xor  ah, ah
        call reading_to_mv
        call put_dec
        mov  al, 0x0D
        stosb
        mov  al, 0x0A
        stosb

        ; --- write it ---
        mov  cx, di
        sub  cx, line           ; CX = the line length
        mov  dx, line
        mov  bx, [handle]
        mov  ah, 0x40
        int  0x21
        jc   .write_error

        ; --- progress ---
        putc '.'

        inc  si
        mov  ax, 1
        call delay_seconds

        pop  cx
        loop .next

        ; --- close ---
        mov  bx, [handle]
        mov  ah, 0x3E
        int  0x21

        newline
        print donemsg
        exit 0

.file_error:  print ferrmsg
              exit 1
.adc_error:   pop cx
              print aerrmsg
              exit 2
.write_error: pop cx
              print werrmsg
              exit 3

; ---------------------------------------------------------------
; put_dec — write AX as decimal ASCII at ES:DI, advancing DI.
; ---------------------------------------------------------------
put_dec:
        push ax
        push bx
        push cx
        push dx
        mov  bx, 10
        xor  cx, cx
.divide:
        xor  dx, dx
        div  bx
        push dx
        inc  cx
        or   ax, ax
        jnz  .divide
.emit:
        pop  ax
        add  al, '0'
        stosb
        loop .emit
        pop  dx
        pop  cx
        pop  bx
        pop  ax
        ret

; ... adc_read, reading_to_mv, delay_seconds ...

filename: db  'ADCLOG.CSV', 0
handle:   dw  0
sample:   db  0
line:     times 32 db 0
startmsg: db  'Logging 60 samples to ADCLOG.CSV', 0x0D, 0x0A, '$'
donemsg:  db  'Finished.', 0x0D, 0x0A, '$'
ferrmsg:  db  'Cannot create the file.', 0x0D, 0x0A, '$'
aerrmsg:  db  'ADC failure.', 0x0D, 0x0A, '$'
werrmsg:  db  'Write failure — disk full?', 0x0D, 0x0A, '$'
