# Chapter 37 — BIOS services

[← DOS services](36-dos-int21.md) · [Contents](README.md) · [Next: Macros and modular programs →](38-macros-and-modules.md)

---

## Goal

The BIOS interrupt services: video (`INT 10h`), keyboard (`INT 16h`), disk (`INT 13h`), clock
(`INT 1Ah`) and the system-information calls. What they do, when to use them instead of DOS, and
when to bypass both and write to hardware directly.

---

## 1. BIOS versus DOS

The **BIOS** (Basic Input/Output System) is firmware in ROM at `0xF0000`–`0xFFFFF`. It initialises
the hardware at power-on and provides a set of interrupt services one level below DOS.

| | BIOS | DOS |
|---|------|-----|
| Lives in | ROM | loaded from disk |
| Level | hardware | files, devices, redirection |
| Video | direct control: cursor, colour, pixels, modes | a character stream |
| Keyboard | scan codes, shift state | a character stream |
| Disk | absolute sectors | files and directories |
| Redirectable | **no** | yes |
| Speed | faster (no DOS layer) | slower |
| Portability | tied to a PC-compatible | works anywhere DOS runs |

**The rule of thumb:** use DOS for anything that should be redirectable (normal program output,
file I/O); use the BIOS when you need control DOS does not offer (cursor position, colour, graphics,
raw keys); bypass both and write to `0xB8000` when you need speed.

### 1.1 The BIOS interrupts

| Interrupt | Service |
|-----------|---------|
| `INT 10h` | **video** |
| `INT 11h` | equipment list |
| `INT 12h` | conventional memory size |
| `INT 13h` | **disk** |
| `INT 14h` | serial port |
| `INT 15h` | miscellaneous system services |
| `INT 16h` | **keyboard** |
| `INT 17h` | printer |
| `INT 1Ah` | **real-time clock** |

---

## 2. `INT 10h` — video

The most useful BIOS interrupt. `AH` selects the function.

### 2.1 Function 00h — set video mode

```
   In:   AH = 00h,  AL = mode
```

| Mode | Type | Size | Colours |
|------|------|------|---------|
| `00h` | text | 40×25 | mono |
| `03h` | **text** | **80×25** | 16 — the normal DOS mode |
| `06h` | graphics | 640×200 | 2 |
| `0Dh` | graphics | 320×200 | 16 |
| `12h` | graphics | 640×480 | 16 (VGA) |
| `13h` | **graphics** | **320×200** | **256** — the one used in Chapter 45 |

```asm
        mov  ax, 0x0013         ; AH = 0, AL = 13h
        int  0x10               ; switch to 320×200, 256 colours
```

**Setting a mode clears the screen.** That is the cheapest way to clear it:

```asm
        mov  ax, 0x0003
        int  0x10               ; back to 80×25 text, screen cleared
```

### 2.2 Function 02h — set cursor position

```
   In:   AH = 02h,  BH = page (0),  DH = row (0-24),  DL = column (0-79)
```

```asm
        mov  ah, 0x02
        xor  bh, bh             ; page 0
        mov  dh, 10             ; row 10
        mov  dl, 30             ; column 30
        int  0x10
```

**This is the thing DOS cannot do.** DOS output is a stream; there is no "move the cursor to row 10".

### 2.3 Function 03h — get cursor position

```
   In:   AH = 03h,  BH = page
   Out:  DH = row, DL = column, CH/CL = cursor shape
```

### 2.4 Function 06h / 07h — scroll a window

```
   In:   AH = 06h (up) or 07h (down)
         AL = lines to scroll (0 = clear the whole window)
         BH = attribute for the blank lines
         CH, CL = top-left row, column
         DH, DL = bottom-right row, column
```

```asm
; Clear the whole screen to white-on-blue.
        mov  ax, 0x0600         ; scroll up, 0 lines = clear
        mov  bh, 0x1F           ; blue background, white foreground
        xor  cx, cx             ; from 0,0
        mov  dx, 0x184F         ; to row 24 (18h), column 79 (4Fh)
        int  0x10
```

This clears with a chosen colour, which mode-setting cannot do.

### 2.5 Function 09h / 0Ah — write a character with attribute

```
   In:   AH = 09h,  AL = character,  BH = page,  BL = attribute,  CX = repeat count
```

```asm
; Write 20 red asterisks at the cursor.
        mov  ah, 0x09
        mov  al, '*'
        xor  bh, bh
        mov  bl, 0x04           ; red on black
        mov  cx, 20
        int  0x10
```

**Function 09h does not move the cursor.** All `CX` characters are written starting at the current
position, and the cursor stays where it was. Function 0Ah is the same but keeps the existing
attribute.

### 2.6 Function 0Eh — teletype output

```
   In:   AH = 0Eh,  AL = character,  BH = page,  BL = colour (graphics modes)
```

```asm
        mov  ah, 0x0E
        mov  al, 'X'
        xor  bh, bh
        int  0x10
```

This one **does** advance the cursor and handle `0x0D`, `0x0A` and `0x08`. It is the BIOS equivalent
of DOS function 02h, and it works before DOS is loaded — which is why every boot sector uses it.

### 2.7 Function 0Ch / 0Dh — write / read a pixel

```
   0Ch:  AH = 0Ch,  AL = colour,  CX = x,  DX = y,  BH = page
   0Dh:  AH = 0Dh,  CX = x,  DX = y,  BH = page  ->  AL = colour
```

```asm
        mov  ax, 0x0C0F         ; write pixel, colour 15 (white)
        mov  cx, 160            ; x
        mov  dx, 100            ; y
        xor  bh, bh
        int  0x10
```

**Correct but slow** — about 500 clocks per pixel. Chapter 45 writes straight to `0xA0000` instead,
at about 20 clocks per pixel.

### 2.8 The attribute byte

In text mode each screen cell is two bytes: the character, then its attribute.

```
   bit  7   6 5 4   3   2 1 0
       ┌───┬───────┬───┬───────┐
       │BLK│  BG   │INT│  FG   │
       └───┴───────┴───┴───────┘
```

| Value | Colour |
|-------|--------|
| 0 | black |
| 1 | blue |
| 2 | green |
| 3 | cyan |
| 4 | red |
| 5 | magenta |
| 6 | brown |
| 7 | light grey |

Add 8 (the intensity bit) for the bright versions: 9 = bright blue, 15 = white.

```
   attribute = background × 16 + foreground

   0x07  light grey on black    — the default
   0x1F  white on blue
   0x4E  yellow on red
   0x8F  blinking white on black
```

---

## 3. `INT 16h` — keyboard

### 3.1 Function 00h — read a key, waiting

```
   In:   AH = 00h
   Out:  AL = ASCII code (0 for an extended key),  AH = scan code
```

```asm
        xor  ah, ah
        int  0x16               ; waits; AL = ASCII, AH = scan code
```

**You get both the character and the scan code**, which DOS does not give you. That matters for
arrow keys, function keys and for distinguishing the two Enter keys.

### 3.2 Function 01h — check for a key, not waiting

```
   In:   AH = 01h
   Out:  ZF = 1 if no key is waiting;  ZF = 0 and AX = the key if there is
```

**The key is not removed from the buffer.** Call function 00h afterwards to take it.

```asm
.poll:
        mov  ah, 0x01
        int  0x16
        jz   .no_key            ; ZF = 1 -> nothing waiting
        xor  ah, ah
        int  0x16               ; now actually read it
        ; AL = the character
.no_key:
```

### 3.3 Function 02h — get shift status

```
   In:   AH = 02h
   Out:  AL = the shift flags
```

| Bit | Meaning |
|-----|---------|
| 0 | right Shift held |
| 1 | left Shift held |
| 2 | Ctrl held |
| 3 | Alt held |
| 4 | Scroll Lock on |
| 5 | Num Lock on |
| 6 | Caps Lock on |
| 7 | Insert on |

```asm
        mov  ah, 0x02
        int  0x16
        test al, 0x04           ; Ctrl held?
        jnz  .ctrl_down
```

### 3.4 The scan codes you will want

| Key | Scan code (`AH`) | ASCII (`AL`) |
|-----|------------------|--------------|
| Esc | `0x01` | `0x1B` |
| Enter | `0x1C` | `0x0D` |
| Backspace | `0x0E` | `0x08` |
| Tab | `0x0F` | `0x09` |
| Space | `0x39` | `0x20` |
| F1–F10 | `0x3B`–`0x44` | **`0x00`** |
| Up | `0x48` | `0x00` |
| Left | `0x4B` | `0x00` |
| Right | `0x4D` | `0x00` |
| Down | `0x50` | `0x00` |
| Home | `0x47` | `0x00` |
| End | `0x4F` | `0x00` |
| PgUp | `0x49` | `0x00` |
| PgDn | `0x51` | `0x00` |

**`AL = 0` means "this is an extended key; look at `AH`".** Full table in
[Appendix D](D-ascii-table.md).

```asm
        xor  ah, ah
        int  0x16
        or   al, al
        jnz  .normal_char
        ; extended key — AH holds the scan code
        cmp  ah, 0x48
        je   .up_arrow
        cmp  ah, 0x50
        je   .down_arrow
.normal_char:
```

---

## 4. `INT 13h` — disk

Raw sector access. This is what a boot loader uses, and what a disk utility uses; ordinary programs
should use DOS file functions.

### 4.1 Function 02h — read sectors

```
   In:   AH = 02h
         AL = number of sectors
         CH = cylinder (track), low 8 bits
         CL = sector (1-based!)  plus the high 2 bits of the cylinder in bits 7-6
         DH = head
         DL = drive (0 = A:, 0x80 = first hard disk)
         ES:BX = buffer
   Out:  CF = 1 on error, AH = error code;  AL = sectors actually read
```

```asm
; Read one sector: cylinder 0, head 0, sector 1 of drive A.
        mov  ah, 0x02
        mov  al, 1              ; one sector
        mov  ch, 0              ; cylinder 0
        mov  cl, 1              ; sector 1 — SECTORS ARE NUMBERED FROM 1
        mov  dh, 0              ; head 0
        mov  dl, 0              ; drive A:
        mov  bx, buffer
        push ds
        pop  es                 ; ES:BX = the buffer
        int  0x13
        jc   .disk_error
```

**Sectors are numbered from 1, not 0.** Cylinders and heads are numbered from 0. This
inconsistency has caused decades of off-by-one errors.

### 4.2 Function 03h — write sectors

Identical parameters; `AH = 03h`.

### 4.3 Function 00h — reset the disk system

```asm
        xor  ah, ah
        mov  dl, 0
        int  0x13
```

**Retry failed operations three times, resetting between attempts.** Floppy drives genuinely need
this — the motor may not be at speed.

```asm
        mov  cx, 3              ; three attempts
.retry:
        push cx
        ; ... set up and issue the read ...
        int  0x13
        pop  cx
        jnc  .ok
        xor  ah, ah
        int  0x13               ; reset
        loop .retry
        jmp  .failed
.ok:
```

### 4.4 The CHS encoding

`CL` packs the sector number and the top two cylinder bits:

```
   CL:  bits 5-0 = sector number (1-63)
        bits 7-6 = cylinder bits 9-8
   CH:  cylinder bits 7-0
```

So cylinder 300 (`0x12C` = `01 0010 1100`), head 1, sector 5:

```asm
        mov  ch, 0x2C           ; cylinder bits 7-0
        mov  cl, 0x45           ; bits 7-6 = 01 (cylinder bits 9-8), bits 5-0 = 5
        mov  dh, 1              ; head
```

---

## 5. `INT 1Ah` — clock

### 5.1 Function 00h — read the tick counter

```
   In:   AH = 00h
   Out:  CX:DX = ticks since midnight,  AL = 1 if midnight has passed
```

The counter increments **18.2065 times per second** (1,193,182 Hz ÷ 65,536).

```asm
; A delay of about one second.
        mov  ah, 0x00
        int  0x1A
        add  dx, 18             ; 18 ticks ≈ 1 second
        mov  bx, dx
.wait:
        mov  ah, 0x00
        int  0x1A
        cmp  dx, bx
        jb   .wait
```

Crude — it ignores the high word and midnight rollover — but adequate for a pause.

### 5.2 Reading the counter directly

The BIOS keeps it at `0040:006C`, and reading it there avoids the 51-clock `INT`:

```asm
        push ds
        mov  ax, 0x0040
        mov  ds, ax
        mov  ax, [0x6C]         ; low word
        mov  dx, [0x6E]         ; high word
        pop  ds
```

Wrap it in `CLI`/`STI` if you read both words (Chapter 30 §11).

---

## 6. System information

### 6.1 `INT 11h` — equipment list

```
   Out:  AX = the equipment word
```

| Bits | Meaning |
|------|---------|
| 0 | a floppy drive is present |
| 1 | an 8087 is present |
| 3–2 | system board RAM (obsolete) |
| 5–4 | initial video mode: 01 = 40×25 colour, 10 = 80×25 colour, 11 = 80×25 mono |
| 7–6 | number of floppy drives, minus one |
| 11–9 | number of serial ports |
| 14–13 | number of printer ports |

```asm
        int  0x11
        test ax, 0x0002
        jnz  .has_8087
```

### 6.2 `INT 12h` — memory size

```
   Out:  AX = conventional memory in KiB (typically 640)
```

```asm
        int  0x12               ; AX = 640 on a fully populated PC
```

---

## 7. Worked program — a coloured, positioned display

```asm
; screen.asm — draw a bordered box with BIOS video calls
; nasm -f bin screen.asm -o screen.com
        cpu  8086
        org  0x100

TOP     equ  5
LEFT    equ  20
WIDTH   equ  40
HEIGHT  equ  10
ATTR    equ  0x1F              ; white on blue

start:
        ; --- clear the screen to blue ---
        mov  ax, 0x0600         ; scroll up 0 lines = clear the window
        mov  bh, ATTR
        xor  cx, cx             ; top-left 0,0
        mov  dx, 0x184F         ; bottom-right 24,79
        int  0x10

        ; --- draw the top edge ---
        mov  dh, TOP
        mov  dl, LEFT
        call gotoxy
        mov  al, 0xC9           ; ╔
        call putchar_attr
        mov  cx, WIDTH - 2
        mov  al, 0xCD           ; ═
        call putrep
        mov  dl, LEFT + WIDTH - 1
        call gotoxy
        mov  al, 0xBB           ; ╗
        call putchar_attr

        ; --- the sides ---
        mov  cx, HEIGHT - 2
        mov  dh, TOP + 1
.side:
        push cx
        mov  dl, LEFT
        call gotoxy
        mov  al, 0xBA           ; ║
        call putchar_attr
        mov  dl, LEFT + WIDTH - 1
        call gotoxy
        mov  al, 0xBA
        call putchar_attr
        inc  dh
        pop  cx
        loop .side

        ; --- the bottom edge ---
        mov  dh, TOP + HEIGHT - 1
        mov  dl, LEFT
        call gotoxy
        mov  al, 0xC8           ; ╚
        call putchar_attr
        mov  cx, WIDTH - 2
        mov  al, 0xCD
        call putrep
        mov  dl, LEFT + WIDTH - 1
        call gotoxy
        mov  al, 0xBC           ; ╝
        call putchar_attr

        ; --- the title ---
        mov  dh, TOP + 2
        mov  dl, LEFT + 10
        call gotoxy
        mov  si, title
        call puts

        ; --- wait for a key ---
        xor  ah, ah
        int  0x16

        ; --- restore the screen ---
        mov  ax, 0x0003         ; set mode 3 — clears to the default colours
        int  0x10

        mov  ax, 0x4C00
        int  0x21

; ---------------------------------------------------------------
; gotoxy — move the cursor.  In: DH = row, DL = column.
;          Preserves everything.
; ---------------------------------------------------------------
gotoxy:
        push ax
        push bx
        mov  ah, 0x02
        xor  bh, bh
        int  0x10
        pop  bx
        pop  ax
        ret

; ---------------------------------------------------------------
; putchar_attr — write AL at the cursor with ATTR, without moving it.
; ---------------------------------------------------------------
putchar_attr:
        push ax
        push bx
        push cx
        mov  ah, 0x09
        xor  bh, bh
        mov  bl, ATTR
        mov  cx, 1
        int  0x10
        pop  cx
        pop  bx
        pop  ax
        ret

; ---------------------------------------------------------------
; putrep — write CX copies of AL at the cursor.
; ---------------------------------------------------------------
putrep:
        push ax
        push bx
        mov  ah, 0x09
        xor  bh, bh
        mov  bl, ATTR
        int  0x10
        pop  bx
        pop  ax
        ret

; ---------------------------------------------------------------
; puts — write the zero-terminated string at DS:SI, advancing the
;        cursor.  Uses teletype output so newlines work.
; ---------------------------------------------------------------
puts:
        push ax
        push bx
.next:
        lodsb
        or   al, al
        jz   .done
        mov  ah, 0x0E
        xor  bh, bh
        int  0x10
        jmp  .next
.done:
        pop  bx
        pop  ax
        ret

; ---------------------------------------------------------------
title:  db   'BIOS video demo', 0
```

The box-drawing characters (`0xC9`, `0xCD`, `0xBB`, `0xBA`, `0xC8`, `0xBC`) are from IBM code page
437 — the PC's extended character set. [Appendix D](D-ascii-table.md) has the full set.

### 7.1 Notes

**Function 09h writes without moving the cursor**, which is why `gotoxy` is called before each
character. Function 0Eh *does* move it, which is why `puts` uses that one.

**Every helper preserves its registers**, so the main routine's `CX` and `DH` survive. That
discipline is what lets the `.side` loop work.

**Restoring mode 3 at the end** returns the screen to normal DOS colours. A program that leaves the
screen blue is a nuisance.

---

## 8. When to use what

| Task | Use |
|------|-----|
| Normal program output that might be redirected | DOS 02h / 09h / 40h |
| Positioning the cursor | **BIOS 10h/02h** |
| Colour | **BIOS 10h/09h**, or direct to `0xB8000` |
| Clearing the screen | BIOS 10h/06h (with colour) or 10h/00h (mode reset) |
| Reading arrow and function keys | **BIOS 16h/00h** |
| Checking Shift/Ctrl/Alt | **BIOS 16h/02h** |
| Full-screen text at speed | **direct to `0xB8000`** |
| Graphics pixels | **direct to `0xA0000`** (Chapter 45) |
| Files | **DOS** |
| Raw sectors, or a boot loader | **BIOS 13h** |
| Timing | BIOS 1Ah, or the counter at `0040:006C` |

---

## 9. Summary

```
  INT 10h video
     00h set mode        AL = mode (03h text 80x25, 13h graphics 320x200x256)
     02h set cursor      BH = page, DH = row, DL = column
     03h get cursor      -> DH, DL
     06h scroll/clear    AL = lines (0 = clear), BH = attr, CX = TL, DX = BR
     09h write char+attr AL = char, BL = attr, CX = count — DOES NOT MOVE THE CURSOR
     0Eh teletype        AL = char — DOES move it, handles CR/LF/BS
     0Ch write pixel     AL = colour, CX = x, DX = y  (slow)

  attribute = background × 16 + foreground;  +8 = bright;  bit 7 = blink
     0x07 grey on black · 0x1F white on blue · 0x4E yellow on red

  INT 16h keyboard
     00h read key, wait  -> AL = ASCII (0 = extended), AH = scan code
     01h peek            -> ZF = 1 if nothing waiting; key stays in the buffer
     02h shift status    -> AL bits: 0 RShift 1 LShift 2 Ctrl 3 Alt 6 Caps

  INT 13h disk
     02h/03h read/write  AL = count, CH = cyl, CL = SECTOR (FROM 1), DH = head,
                         DL = drive, ES:BX = buffer.  CF = 1 on error — retry 3x.

  INT 1Ah clock  00h -> CX:DX = ticks since midnight, 18.2 per second
  INT 11h equipment word · INT 12h conventional memory in KiB

  BIOS is not redirectable and is PC-specific; DOS is neither.
```

---

## Exercises

**37.1** Write the two instructions that clear the screen and switch to 80×25 text mode.

**37.2** Write the code that clears the screen to yellow-on-blue using function 06h.

**37.3** Compute the attribute byte for: bright white on red; green on black; blinking yellow on
blue.

**37.4** Write the code that positions the cursor at row 12, column 40 and prints `Centre`.

**37.5** What is the difference between BIOS function 09h and 0Eh? Which advances the cursor?

**37.6** Write a loop that waits for a keypress and reports whether it was a normal character or an
extended key, printing the scan code for extended keys.

**37.7** Write the code that checks whether Ctrl is held down.

**37.8** Read one sector from cylinder 0, head 0, sector 1 of drive A into a buffer, with three
retries.

**37.9** Why are sectors numbered from 1 while cylinders and heads are numbered from 0?

**37.10** Encode cylinder 450, head 3, sector 17 into `CH`, `CL` and `DH`.

**37.11** Write a delay of exactly 3 seconds using `INT 1Ah`.

**37.12** Rewrite that delay to read `0040:006C` directly. Why is it faster, and what must you be
careful about?

**37.13** Why should a program restore video mode 3 before exiting?

**37.14** For each of these, say whether you would use DOS, BIOS or direct hardware access, and why:
printing a status line at the bottom of the screen; writing a log file; reading the arrow keys;
filling the screen with a colour; drawing 10,000 pixels.

Answers in [Appendix H](H-exercise-solutions.md#chapter-37).

---

[← DOS services](36-dos-int21.md) · [Contents](README.md) · [Next: Macros and modular programs →](38-macros-and-modules.md)
