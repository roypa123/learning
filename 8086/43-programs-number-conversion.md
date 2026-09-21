# Chapter 43 — Programs: number conversion

[← Programs: strings](42-programs-strings.md) · [Contents](README.md) · [Next: Programs: matrices →](44-programs-matrices.md)

---

## Goal

The routines every other program needs: converting between binary, decimal, hexadecimal, octal,
BCD and ASCII, in both directions. Each is short, each is used constantly, and getting them right
once means never thinking about them again.

---

## 1. The conversions, mapped

```
                    ASCII text
                   ↗     ↑     ↖
        decimal   hex   binary  octal
                   ↘     ↓     ↙
                  16-bit value
                       ↕
                      BCD
```

Eight routines cover it:

| Routine | Direction |
|---------|-----------|
| `bin_to_dec` | value → decimal ASCII |
| `dec_to_bin` | decimal ASCII → value |
| `bin_to_hex` | value → hex ASCII |
| `hex_to_bin` | hex ASCII → value |
| `bin_to_bits` | value → binary ASCII |
| `bin_to_oct` | value → octal ASCII |
| `bin_to_bcd` | value → packed BCD |
| `bcd_to_bin` | packed BCD → value |

---

## 2. Binary to decimal

### 2.1 The algorithm

Divide by 10 repeatedly. Each remainder is a digit, produced **least significant first**. Push them,
then pop them back to reverse the order.

```
   1234 ÷ 10 = 123  r 4      <- push '4'
    123 ÷ 10 =  12  r 3      <- push '3'
     12 ÷ 10 =   1  r 2      <- push '2'
      1 ÷ 10 =   0  r 1      <- push '1'
   quotient is 0 — stop

   pop: '1' '2' '3' '4'
```

### 2.2 The routine

```asm
; ---------------------------------------------------------------
; bin_to_dec — convert AX to decimal ASCII at ES:DI, zero-terminated.
;
;   In:        AX = the unsigned value, ES:DI = the buffer (6 bytes)
;   Out:       DI advanced past the digits
;   Destroys:  AX, BX, CX, DX
; ---------------------------------------------------------------
bin_to_dec:
        mov  bx, 10
        xor  cx, cx             ; CX counts the digits
.divide:
        xor  dx, dx             ; DX:AX = the remaining value.
                                ;   CLEARING DX EACH TIME IS ESSENTIAL —
                                ;   a stale DX makes the dividend enormous
                                ;   and usually causes a divide overflow.
        div  bx
        push dx                 ; save the digit
        inc  cx
        or   ax, ax
        jnz  .divide
.emit:
        pop  ax
        add  al, '0'            ; value -> ASCII
        stosb
        loop .emit
        mov  byte [es:di], 0    ; terminate
        ret
```

**The stack reverses the digits for free.** That is the whole reason this idiom is universal.

### 2.3 Signed version

```asm
; ---------------------------------------------------------------
; sbin_to_dec — the same, for a signed value.
; ---------------------------------------------------------------
sbin_to_dec:
        or   ax, ax
        jns  bin_to_dec         ; positive — nothing extra to do
        push ax
        mov  al, '-'
        stosb
        pop  ax
        neg  ax                 ; NOTE: NEG of 0x8000 gives 0x8000 and sets OF.
                                ;   -32768 prints as "-32768" anyway, because
                                ;   the unsigned value 32768 is what we want.
        jmp  bin_to_dec
```

**The `0x8000` case works by accident and correctly.** `NEG 0x8000` leaves `0x8000` = 32,768
unsigned, which is exactly the magnitude we want to print after the minus sign.

---

## 3. Decimal to binary

### 3.1 The algorithm — Horner's method

```
   result = 0
   for each digit d:
       result = result × 10 + d
```

For `'1' '2' '3' '4'`:

```
   0 × 10 + 1 =    1
   1 × 10 + 2 =   12
  12 × 10 + 3 =  123
 123 × 10 + 4 = 1234
```

### 3.2 The routine

```asm
; ---------------------------------------------------------------
; dec_to_bin — convert the ASCIIZ decimal string at DS:SI to a value.
;
;   Out:       AX = the value, CF = 1 if the string was invalid
;   Destroys:  BX, CX, DX, SI
;
;   Accepts an optional leading '-'. Rejects an empty string, any
;   non-digit, and anything over 65535 (or over 32767 if negative).
; ---------------------------------------------------------------
dec_to_bin:
        xor  bx, bx             ; BX accumulates the result
        xor  cx, cx             ; CX counts valid digits
        xor  di, di             ; DI = the negative flag

        cmp  byte [si], '-'
        jne  .digits
        mov  di, 1
        inc  si

.digits:
        mov  al, [si]
        or   al, al
        jz   .done
        cmp  al, '0'
        jb   .bad
        cmp  al, '9'
        ja   .bad

        sub  al, '0'
        xor  ah, ah
        push ax                 ; save the digit
        mov  ax, bx
        mov  dx, 10
        mul  dx                 ; DX:AX = accumulator × 10
        or   dx, dx
        jnz  .overflow_pop      ; the product exceeded 16 bits
        mov  bx, ax
        pop  ax
        add  bx, ax
        jc   .overflow          ; the addition carried out

        inc  cx
        inc  si
        jmp  .digits

.done:
        or   cx, cx
        jz   .bad               ; no digits at all
        mov  ax, bx
        or   di, di
        jz   .ok
        neg  ax                 ; apply the sign
.ok:
        clc
        ret

.overflow_pop:
        pop  ax
.overflow:
.bad:
        stc
        ret
```

**Overflow is checked twice**, because there are two ways to exceed 16 bits: the multiplication can
overflow (`DX` non-zero), and the addition can carry. Both must be caught, or `dec_to_bin` on
`"99999"` silently returns 34,463.

---

## 4. Binary to hexadecimal

### 4.1 The simple version

Four nibbles, each converted to a character.

```asm
; ---------------------------------------------------------------
; bin_to_hex — convert AX to four hex digits at ES:DI.
;
;   Destroys:  AX, BX, CX, DX
; ---------------------------------------------------------------
bin_to_hex:
        mov  bx, ax
        mov  cx, 4
.digit:
        rol  bx, 1              ; four ROLs move the top nibble to the bottom
        rol  bx, 1              ;   (8086 has no ROL bx, 4)
        rol  bx, 1
        rol  bx, 1
        mov  al, bl
        and  al, 0x0F           ; isolate the nibble
        add  al, '0'
        cmp  al, '9'
        jbe  .emit
        add  al, 7              ; 'A'..'F' sit 7 past '9' + 1
.emit:
        stosb
        loop .digit
        mov  byte [es:di], 0
        ret
```

**Why `add al, 7`.** `'9'` is `0x39` and `'A'` is `0x41`. After `add al, '0'`, the value 10 becomes
`0x3A`; we need `0x41`, which is 7 more.

**Why `ROL` and not `SHR`.** Four `ROL`s leave `BX` rotated a full 16 bits after four iterations —
i.e. back to its original value. That costs nothing and means the routine could print the same value
twice.

### 4.2 The `XLAT` version

Faster and branchless:

```asm
hextab: db  '0123456789ABCDEF'

; ---------------------------------------------------------------
; bin_to_hex_fast — the same, using XLAT.
; ---------------------------------------------------------------
bin_to_hex_fast:
        push bx
        mov  dx, ax             ; keep the value in DX
        mov  bx, hextab         ; XLAT needs the table base in BX
        mov  cx, 4
.digit:
        rol  dx, 1
        rol  dx, 1
        rol  dx, 1
        rol  dx, 1
        mov  al, dl
        and  al, 0x0F
        xlat                    ; AL = [DS:BX + AL]
        stosb
        loop .digit
        mov  byte [es:di], 0
        pop  bx
        ret
```

| Version | Clocks per digit |
|---------|-----------------|
| Compare-and-add | 2×4 + 2 + 4 + 4 + 16 (jump) + 11 + 17 ≈ **62** |
| `XLAT` | 2×4 + 2 + 4 + 11 + 11 + 17 ≈ **53** |

Not dramatic, but the `XLAT` version has no branch, which also means no queue flush.

### 4.3 One nibble at a time

The two-instruction version, worth knowing:

```asm
; AL holds a value 0-15; convert it to a hex character.
        mov  bx, hextab
        xlat                    ; AL = the character
```

---

## 5. Hexadecimal to binary

```asm
; ---------------------------------------------------------------
; hex_to_bin — convert the ASCIIZ hex string at DS:SI to a value.
;
;   Out:       AX = the value, CF = 1 if invalid
;   Destroys:  BX, CX, SI
;
;   Accepts upper or lower case, and an optional '0x' or '$' prefix.
; ---------------------------------------------------------------
hex_to_bin:
        xor  bx, bx
        xor  cx, cx             ; digit count

        ; --- skip an optional prefix ---
        cmp  byte [si], '$'
        jne  .check0x
        inc  si
        jmp  .digits
.check0x:
        cmp  byte [si], '0'
        jne  .digits
        cmp  byte [si+1], 'x'
        je   .skip2
        cmp  byte [si+1], 'X'
        jne  .digits
.skip2:
        inc  si
        inc  si

.digits:
        mov  al, [si]
        or   al, al
        jz   .done

        ; --- convert one character to 0-15 ---
        cmp  al, '0'
        jb   .bad
        cmp  al, '9'
        jbe  .is_digit
        or   al, 0x20           ; fold to lower case
        cmp  al, 'a'
        jb   .bad
        cmp  al, 'f'
        ja   .bad
        sub  al, 'a' - 10       ; 'a' -> 10
        jmp  .accum
.is_digit:
        sub  al, '0'

.accum:
        cmp  cx, 4
        jae  .bad               ; more than four digits overflows 16 bits
        xor  ah, ah
        shl  bx, 1              ; result × 16
        shl  bx, 1
        shl  bx, 1
        shl  bx, 1
        add  bx, ax
        inc  cx
        inc  si
        jmp  .digits

.done:
        or   cx, cx
        jz   .bad
        mov  ax, bx
        clc
        ret
.bad:
        stc
        ret
```

**`shl bx, 1` four times rather than a multiply.** Multiplying by 16 is four left shifts — 8 clocks
against `MUL`'s 118.

**`sub al, 'a' - 10`** — the assembler computes `0x61 − 10 = 0x57`, so this is `sub al, 0x57`, a
single instruction that maps `'a'`→10, `'b'`→11 and so on.

---

## 6. Binary to binary text

```asm
; ---------------------------------------------------------------
; bin_to_bits — convert AX to sixteen '0'/'1' characters at ES:DI.
;
;   Destroys:  AX, BX, CX
; ---------------------------------------------------------------
bin_to_bits:
        mov  bx, ax
        mov  cx, 16
.next:
        rol  bx, 1              ; the top bit moves into CF
        mov  al, '0'
        jnc  .emit
        mov  al, '1'
.emit:
        stosb
        loop .next
        mov  byte [es:di], 0
        ret
```

**`ROL` then `JNC`.** The rotate puts the departing bit straight into `CF`, so the test needs no
masking. After 16 rotations `BX` holds its original value.

---

## 7. Binary to octal

Octal is awkward on a 16-bit machine because 16 is not a multiple of 3: a 16-bit value needs five
groups of three bits plus one left over, so the leading digit can only be 0 or 1. That makes the
bit-slicing approach fiddly.

**The divide-by-8 approach avoids the problem entirely**, and is the same shape as `bin_to_dec`:

```asm
; ---------------------------------------------------------------
; bin_to_oct — convert AX to octal ASCII at ES:DI, zero-terminated.
;
;   Identical to bin_to_dec with a divisor of 8 instead of 10.
;   Produces no leading zeros, so 0..7 give a single digit.
;
;   Destroys:  AX, BX, CX, DX
; ---------------------------------------------------------------
bin_to_oct:
        mov  bx, 8
        xor  cx, cx
.divide:
        xor  dx, dx
        div  bx                 ; DX = value mod 8, AX = value / 8
        push dx
        inc  cx
        or   ax, ax
        jnz  .divide
.emit:
        pop  ax
        add  al, '0'            ; octal digits are always '0'..'7'
        stosb
        loop .emit
        mov  byte [es:di], 0
        ret
```

**One routine, three bases.** Change `mov bx, 8` to 10 or 16 and you have decimal or hex — except
that hex needs the `'A'`–`'F'` correction, which is why §4 exists separately. Exercise 43.14 asks
for the general version.

The division costs 144–162 clocks per digit, against about 20 for the bit-slicing version. For six
digits that is roughly 900 clocks versus 120. If octal output is in a hot loop, slice the bits; for
anything else, this version is clearer and short enough.

---

## 8. Binary to packed BCD

A packed BCD word holds four decimal digits, one per nibble: 1234 becomes `0x1234`.

The obvious loop — divide by 10 four times, shifting each remainder in — does not fit comfortably,
because `DIV` needs `DX` for the remainder *and* the loop needs a counter. Building the digits from
the most significant end, with an explicit divisor each time, is clearer and uses one fewer register:

```asm
; ---------------------------------------------------------------
; bin_to_bcd — convert AX (0-9999) to packed BCD.
;
;   Out:       AX = packed BCD, CF = 1 on overflow
;   Destroys:  BX, CX, DX
; ---------------------------------------------------------------
bin_to_bcd:
        cmp  ax, 9999
        ja   .too_big

        xor  cx, cx             ; CX accumulates the BCD result
        mov  bx, 1000

        ; --- thousands ---
        xor  dx, dx
        div  bx                 ; AL = thousands digit, DX = remainder
        mov  cl, al
        shl  cx, 1
        shl  cx, 1
        shl  cx, 1
        shl  cx, 1              ; make room for the next digit

        ; --- hundreds ---
        mov  ax, dx
        mov  bx, 100
        xor  dx, dx
        div  bx
        or   cl, al
        shl  cx, 1
        shl  cx, 1
        shl  cx, 1
        shl  cx, 1

        ; --- tens ---
        mov  ax, dx
        mov  bx, 10
        xor  dx, dx
        div  bx
        or   cl, al
        shl  cx, 1
        shl  cx, 1
        shl  cx, 1
        shl  cx, 1

        ; --- units ---
        or   cl, dl

        mov  ax, cx
        clc
        ret
.too_big:
        stc
        ret
```

Trace for 1234: thousands = 1 → `CX` = `0x0010`; hundreds = 2 → `CX` = `0x0012` then shifted to
`0x0120`; tens = 3 → `0x0123` then `0x1230`; units = 4 → `0x1234`. ✔

---

## 9. Packed BCD to binary

```asm
; ---------------------------------------------------------------
; bcd_to_bin — convert packed BCD in AX to a binary value.
;
;   e.g. 0x1234 -> 1234
;
;   Out:       AX = the value
;   Destroys:  BX, CX, DX
; ---------------------------------------------------------------
bcd_to_bin:
        mov  bx, ax
        xor  ax, ax             ; AX accumulates the result
        mov  cx, 4              ; four nibbles
.next:
        mov  dx, 10
        push dx
        mul  word [ten]         ; AX = AX × 10
        pop  dx

        rol  bx, 1              ; bring the top nibble down
        rol  bx, 1
        rol  bx, 1
        rol  bx, 1
        mov  dl, bl
        and  dl, 0x0F
        xor  dh, dh
        add  ax, dx             ; += this digit
        loop .next
        ret

ten:    dw   10
```

Horner's method again, base 10, reading the nibbles from the top.

Trace for `0x1234`: 0×10+1 = 1; 1×10+2 = 12; 12×10+3 = 123; 123×10+4 = 1234. ✔

---

## 10. A complete converter program

```asm
; convert.asm — read a number and show it in every base
; nasm -f bin convert.asm -o convert.com
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
.again:
        print prompt
        mov  si, inbuf
        call read_line
        cmp  byte [inbuf], 0
        je   .quit

        mov  si, inbuf
        call dec_to_bin
        jc   .bad
        mov  [value], ax

        ; --- decimal ---
        print dmsg
        mov  ax, [value]
        call print_udec
        newline

        ; --- hexadecimal ---
        print hmsg
        mov  ax, [value]
        mov  di, outbuf
        push ds
        pop  es
        call bin_to_hex
        mov  si, outbuf
        call puts
        newline

        ; --- binary ---
        print bmsg
        mov  ax, [value]
        mov  di, outbuf
        call bin_to_bits
        mov  si, outbuf
        call puts
        newline

        ; --- packed BCD ---
        print cmsg
        mov  ax, [value]
        call bin_to_bcd
        jc   .bcd_too_big
        mov  di, outbuf
        call bin_to_hex
        mov  si, outbuf
        call puts
        newline
        jmp  .again
.bcd_too_big:
        print bigmsg
        jmp  .again

.bad:
        print badmsg
        jmp  .again
.quit:
        exit 0

; ---------------------------------------------------------------
; read_line — read a line into the ASCIIZ buffer at DS:SI
; ---------------------------------------------------------------
read_line:
        push si
.next:
        mov  ah, 0x01
        int  0x21
        cmp  al, 0x0D
        je   .done
        mov  [si], al
        inc  si
        jmp  .next
.done:
        mov  byte [si], 0
        newline
        pop  si
        ret

puts:
        cld
.next:  lodsb
        or   al, al
        jz   .done
        mov  dl, al
        mov  ah, 0x02
        int  0x21
        jmp  .next
.done:  ret

; ... bin_to_hex, bin_to_bits, bin_to_bcd, dec_to_bin from above ...

value:   dw   0
inbuf:   times 16 db 0
outbuf:  times 20 db 0
prompt:  db   0x0D, 0x0A, 'Number (Enter to quit): $'
dmsg:    db   'Decimal: $'
hmsg:    db   'Hex:     0x$'
bmsg:    db   'Binary:  $'
cmsg:    db   'BCD:     0x$'
badmsg:  db   'Not a valid number.', 0x0D, 0x0A, '$'
bigmsg:  db   '(exceeds 9999)', 0x0D, 0x0A, '$'
```

**Sample session:**

```
Number (Enter to quit): 1234
Decimal: 1234
Hex:     0x04D2
Binary:  0000010011010010
BCD:     0x1234

Number (Enter to quit): 255
Decimal: 255
Hex:     0x00FF
Binary:  0000000011111111
BCD:     0x0255
```

Check: 1234 = `0x4D2` ✔, and `0b0000010011010010` = 1024 + 128 + 64 + 16 + 2 = 1234 ✔.

---

## 11. The routines, collected

```
  value -> decimal   divide by 10 repeatedly, PUSH each remainder, POP to emit
                     ALWAYS xor dx,dx before each DIV
  decimal -> value   Horner: result = result × 10 + digit
                     check BOTH the multiply overflow (DX) and the add carry
  value -> hex       four ROLs to bring each nibble down, then XLAT or add '0'/+7
  hex -> value       result = result × 16 + digit; ×16 is four SHLs
  value -> binary    ROL into CF, emit '0' or '1', 16 times
  value -> BCD       divide by 1000, 100, 10; shift each digit into place
  BCD -> value       Horner again, base 10, reading nibbles from the top

  'A'-'F' are 7 past '9'+1  ->  add 7 after add al,'0'
  'a' -> 10 is  sub al, 'a'-10  =  sub al, 0x57
  digit -> value is  sub al,'0'  or  and al,0x0F
```

---

## Exercises

**43.1** Why must `xor dx, dx` appear *inside* the division loop of `bin_to_dec` rather than before
it? Predict the symptom if it is moved outside.

**43.2** Modify `bin_to_dec` to pad with leading zeros to a fixed width of 5 digits.

**43.3** Modify `bin_to_dec` to insert thousands separators: `1234` → `1,234`.

**43.4** `dec_to_bin` checks for overflow twice. Give an input that triggers each check.

**43.5** Write `dec_to_bin` for a 32-bit result in `DX:AX`.

**43.6** Explain the `add al, 7` in `bin_to_hex`. What would happen without it, for the value 10?

**43.7** Rewrite `bin_to_hex` to produce lower-case digits.

**43.8** Write `bin_to_hex8` — convert `AL` to two hex digits.

**43.9** Write a routine that converts a 32-bit value in `DX:AX` to eight hex digits.

**43.10** `hex_to_bin` rejects more than four digits. Rewrite it to accept leading zeros without
counting them against the limit.

**43.11** Write `oct_to_bin`.

**43.12** Trace `bin_to_bcd` for the input 9999 and give `CX` after each of the four stages.

**43.13** Write `bcd_to_dec_string` — convert packed BCD directly to ASCII without going through
binary.

**43.14** Write a routine that converts a string to a value in an arbitrary base 2–16, with the base
in `BL`.

**43.15** Compare the clock counts of the compare-and-add and `XLAT` versions of `bin_to_hex` for a
full 16-bit value. Which would you use, and does the answer change if the table costs 16 bytes?

Answers in [Appendix H](H-exercise-solutions.md#chapter-43).

---

[← Programs: strings](42-programs-strings.md) · [Contents](README.md) · [Next: Programs: matrices →](44-programs-matrices.md)
