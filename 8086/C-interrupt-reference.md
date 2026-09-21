# Appendix C — Interrupt reference

[← Appendix B](B-opcode-map.md) · [Contents](README.md) · [Appendix D →](D-ascii-table.md)

---

The DOS and BIOS services used in this book, with their register conventions. Chapter 36 covers DOS,
Chapter 37 the BIOS.

---

## 1. The interrupt map

| Type | Purpose |
|------|---------|
| `00h` | **divide error** — processor exception |
| `01h` | **single step** — processor exception, `TF = 1` |
| `02h` | **NMI** — the `NMI` pin |
| `03h` | **breakpoint** — the one-byte `INT 3` |
| `04h` | **overflow** — `INTO` with `OF = 1` |
| `05h` | BIOS: print screen |
| `06h`–`07h` | reserved (80186+ exceptions) |
| `08h`–`0Fh` | **hardware IRQ0–IRQ7** via the 8259A |
| `10h` | **BIOS video** |
| `11h` | BIOS equipment list |
| `12h` | BIOS memory size |
| `13h` | **BIOS disk** |
| `14h` | BIOS serial |
| `15h` | BIOS miscellaneous |
| `16h` | **BIOS keyboard** |
| `17h` | BIOS printer |
| `18h` | ROM BASIC |
| `19h` | bootstrap loader |
| `1Ah` | **BIOS clock** |
| `1Bh` | Ctrl-Break handler |
| `1Ch` | **timer tick user hook** — called 18.2 times a second |
| `1Dh`–`1Fh` | pointers to BIOS tables |
| `20h` | DOS: terminate (the CP/M way) |
| `21h` | **DOS function dispatcher** |
| `22h`–`24h` | DOS terminate / Ctrl-C / critical error addresses |
| `25h`, `26h` | DOS absolute disk read / write |
| `27h` | DOS terminate and stay resident (old form) |
| `28h`–`2Eh` | DOS internal |
| `2Fh` | DOS multiplex |
| `33h` | mouse driver |
| `70h`–`77h` | hardware IRQ8–IRQ15 (AT) |

---

## 2. `INT 21h` — DOS functions

`AH` selects the function. Errors return `CF = 1` with a code in `AX`.

### 2.1 Console and character I/O

| `AH` | Function | Input | Output |
|------|----------|-------|--------|
| `01h` | read character, **echoed** | — | `AL` = character; waits |
| `02h` | **write character** | `DL` = character | `AL` destroyed |
| `03h` | read from AUX | — | `AL` |
| `04h` | write to AUX | `DL` | — |
| `05h` | write to printer | `DL` | — |
| `06h` | direct console I/O | `DL = FFh` to read, else write | `AL`, `ZF` |
| `07h` | read character, no echo, no Ctrl-C check | — | `AL` |
| `08h` | **read character, no echo** | — | `AL` |
| `09h` | **write `$`-terminated string** | `DS:DX` | — |
| `0Ah` | **read buffered line** | `DS:DX` = buffer | buffer filled |
| `0Bh` | **check keyboard status** | — | `AL = FFh` if a key waits |
| `0Ch` | flush the buffer, then function `AL` | `AL` = 01/06/07/08/0Ah | as that function |

**Function 0Ah's buffer:**

```
   [0]  maximum characters to accept    (you set this)
   [1]  characters actually read        (DOS sets this)
   [2+] the text, terminated by 0Dh
```

### 2.2 File handling

| `AH` | Function | Input | Output |
|------|----------|-------|--------|
| `3Ch` | **create a file** | `DS:DX` = ASCIIZ path, `CX` = attributes | `AX` = handle |
| `3Dh` | **open a file** | `DS:DX` = path, `AL` = 0 read / 1 write / 2 both | `AX` = handle |
| `3Eh` | **close** | `BX` = handle | — |
| `3Fh` | **read** | `BX`, `CX` = bytes, `DS:DX` = buffer | `AX` = bytes read (**0 = EOF**) |
| `40h` | **write** | `BX`, `CX`, `DS:DX` | `AX` = bytes written |
| `41h` | **delete** | `DS:DX` = path | — |
| `42h` | **seek** | `AL` = 0/1/2, `BX`, `CX:DX` = offset | `DX:AX` = new position |
| `43h` | get/set attributes | `AL` = 0/1, `DS:DX`, `CX` | `CX` |
| `45h` | duplicate a handle | `BX` | `AX` |
| `46h` | force a handle duplicate | `BX`, `CX` | — |
| `56h` | **rename** | `DS:DX` = old, `ES:DI` = new | — |
| `57h` | get/set file date and time | `AL` = 0/1, `BX`, `CX`, `DX` | `CX`, `DX` |

**Standard handles:** 0 stdin · 1 stdout · 2 **stderr** · 3 aux · 4 printer.

**File paths are ASCIIZ (zero-terminated), not `$`-terminated.**

### 2.3 Directories and drives

| `AH` | Function | Input | Output |
|------|----------|-------|--------|
| `0Eh` | select drive | `DL` = drive (0 = A:) | `AL` = number of drives |
| `19h` | get current drive | — | `AL` (0 = A:) |
| `39h` | create directory | `DS:DX` = ASCIIZ path | — |
| `3Ah` | remove directory | `DS:DX` | — |
| `3Bh` | change directory | `DS:DX` | — |
| `47h` | get current directory | `DL` = drive, `DS:SI` = 64-byte buffer | buffer filled |
| `1Ah` | set the Disk Transfer Area | `DS:DX` | — |
| `4Eh` | **find first** | `DS:DX` = wildcard, `CX` = attributes | DTA filled |
| `4Fh` | **find next** | — | DTA filled; `CF = 1` when done |
| `36h` | get free disk space | `DL` = drive | `AX`, `BX`, `CX`, `DX` |

**The find-first DTA layout:**

```
   [00h] 21 bytes  DOS internal search state
   [15h]  1 byte   attribute
   [16h]  2 bytes  time
   [18h]  2 bytes  date
   [1Ah]  4 bytes  file size
   [1Eh] 13 bytes  ASCIIZ filename
```

### 2.4 Memory

| `AH` | Function | Input | Output |
|------|----------|-------|--------|
| `48h` | **allocate** | `BX` = paragraphs | `AX` = segment; on failure `BX` = largest available |
| `49h` | **free** | `ES` = segment | — |
| `4Ah` | **resize** | `ES` = segment, `BX` = new size | — |

A `.COM` program owns all of memory, so `4Ah` must shrink it before `48h` can succeed.

### 2.5 Interrupt vectors, date and time, process control

| `AH` | Function | Input | Output |
|------|----------|-------|--------|
| `25h` | **set interrupt vector** | `AL` = number, `DS:DX` = handler | — |
| `35h` | **get interrupt vector** | `AL` = number | `ES:BX` = handler |
| `2Ah` | **get date** | — | `CX` = year, `DH` = month, `DL` = day, `AL` = weekday |
| `2Bh` | set date | `CX`, `DH`, `DL` | `AL = FFh` if invalid |
| `2Ch` | **get time** | — | `CH` = hour, `CL` = min, `DH` = sec, `DL` = 1/100 s |
| `2Dh` | set time | `CH`, `CL`, `DH`, `DL` | |
| `30h` | get DOS version | — | `AL` = major, `AH` = minor |
| `4Bh` | load and execute | `AL` = 0/3, `DS:DX` = path, `ES:BX` = block | |
| `4Ch` | **terminate** | `AL` = exit code | does not return |
| `4Dh` | get the child's return code | — | `AL` = code, `AH` = how it ended |
| `31h` | **terminate and stay resident** | `AL` = code, `DX` = paragraphs to keep | does not return |
| `62h` | get the PSP segment | — | `BX` |

### 2.6 DOS error codes (returned in `AX` when `CF = 1`)

| Code | Meaning |
|------|---------|
| 1 | invalid function |
| **2** | **file not found** |
| **3** | **path not found** |
| 4 | too many open files |
| **5** | **access denied** |
| 6 | invalid handle |
| 7 | memory control block destroyed |
| **8** | **insufficient memory** |
| 9 | invalid memory block address |
| 10 | invalid environment |
| 11 | invalid format |
| 12 | invalid access code |
| 13 | invalid data |
| 15 | invalid drive |
| 16 | cannot remove the current directory |
| 17 | not the same device |
| 18 | no more files |

---

## 3. `INT 10h` — BIOS video

| `AH` | Function | Input |
|------|----------|-------|
| `00h` | **set video mode** | `AL` = mode |
| `01h` | set cursor shape | `CH` = start line, `CL` = end line |
| `02h` | **set cursor position** | `BH` = page, `DH` = row, `DL` = column |
| `03h` | **get cursor position** | `BH` = page → `DH`, `DL`, `CH`, `CL` |
| `05h` | select the active page | `AL` = page |
| `06h` | **scroll up** | `AL` = lines (0 = clear), `BH` = attribute, `CX` = top-left, `DX` = bottom-right |
| `07h` | scroll down | as `06h` |
| `08h` | read character and attribute | `BH` = page → `AL` = char, `AH` = attribute |
| `09h` | **write character and attribute** | `AL` = char, `BH` = page, `BL` = attribute, `CX` = count |
| `0Ah` | write character only | `AL`, `BH`, `CX` |
| `0Ch` | **write pixel** | `AL` = colour, `CX` = x, `DX` = y, `BH` = page |
| `0Dh` | read pixel | `CX`, `DX`, `BH` → `AL` |
| `0Eh` | **teletype output** | `AL` = char, `BH` = page, `BL` = colour |
| `0Fh` | get video mode | → `AL` = mode, `AH` = columns, `BH` = page |
| `13h` | write a string (AT and later) | `ES:BP` = string, `CX` = length, `DX` = position |

### 3.1 Video modes

| Mode | Type | Size | Colours | Buffer |
|------|------|------|---------|--------|
| `00h` | text | 40×25 | 16 grey | `B8000` |
| `01h` | text | 40×25 | 16 | `B8000` |
| `02h` | text | 80×25 | 16 grey | `B8000` |
| **`03h`** | **text** | **80×25** | **16** | **`B8000`** |
| `04h` | graphics | 320×200 | 4 | `B8000` |
| `05h` | graphics | 320×200 | 4 grey | `B8000` |
| `06h` | graphics | 640×200 | 2 | `B8000` |
| `07h` | text | 80×25 | mono | `B0000` |
| `0Dh` | graphics | 320×200 | 16 | `A0000` |
| `0Eh` | graphics | 640×200 | 16 | `A0000` |
| `10h` | graphics | 640×350 | 16 | `A0000` |
| `12h` | graphics | 640×480 | 16 | `A0000` |
| **`13h`** | **graphics** | **320×200** | **256** | **`A0000`** |

### 3.2 The text attribute byte

```
   bit  7    6 5 4    3    2 1 0
       blink  background  intensity  foreground
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
| 8–15 | the same, bright |

`attribute = background × 16 + foreground`. `07h` = grey on black, `1Fh` = white on blue,
`4Eh` = yellow on red, `8Fh` = blinking white on black.

---

## 4. `INT 16h` — BIOS keyboard

| `AH` | Function | Output |
|------|----------|--------|
| `00h` | **read a key, waiting** | `AL` = ASCII (0 = extended), `AH` = scan code |
| `01h` | **check for a key** | `ZF = 1` if none; `AX` = the key, **not removed** |
| `02h` | **get shift status** | `AL` = flags |
| `03h` | set the typematic rate (AT) | |
| `05h` | push a key into the buffer (AT) | |
| `10h` | read an extended key (AT) | as `00h`, including the extra AT keys |
| `11h` | check for an extended key (AT) | as `01h` |
| `12h` | get extended shift status (AT) | `AX` |

### 4.1 Shift status bits (`AH = 02h` → `AL`)

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

---

## 5. `INT 13h` — BIOS disk

| `AH` | Function | Input |
|------|----------|-------|
| `00h` | **reset the disk system** | `DL` = drive |
| `01h` | get the last status | `DL` → `AH` |
| `02h` | **read sectors** | `AL` = count, `CH` = cylinder, `CL` = sector, `DH` = head, `DL` = drive, `ES:BX` = buffer |
| `03h` | **write sectors** | as `02h` |
| `04h` | verify sectors | as `02h` without a buffer |
| `05h` | format a track | |
| `08h` | get drive parameters | `DL` → `CH`, `CL`, `DH`, `DL` |
| `15h` | get the disk type | `DL` → `AH` |

**Sectors are numbered from 1. Cylinders and heads from 0.**

`CL` packs the sector number in bits 5–0 and cylinder bits 9–8 in bits 7–6.

**Drive numbers:** `00h` = A:, `01h` = B:, `80h` = first hard disk, `81h` = second.

### 5.1 Error codes (in `AH` when `CF = 1`)

| Code | Meaning |
|------|---------|
| `01h` | invalid command |
| `02h` | address mark not found |
| `03h` | write protected |
| `04h` | **sector not found** |
| `08h` | DMA overrun |
| `09h` | **DMA crossed a 64 KiB boundary** |
| `10h` | **CRC error** — the data is corrupt |
| `20h` | controller failure |
| `40h` | seek failure |
| `80h` | **timeout — no disk, or the drive is not ready** |

**Retry three times with a reset between attempts** before reporting failure.

---

## 6. `INT 1Ah` — BIOS clock

| `AH` | Function | Output |
|------|----------|--------|
| `00h` | **read the tick counter** | `CX:DX` = ticks since midnight, `AL = 1` if midnight passed |
| `01h` | set the tick counter | `CX:DX` |
| `02h` | read the real-time clock (AT) | `CH` = hour, `CL` = min, `DH` = sec, all BCD |
| `04h` | read the RTC date (AT) | `CH` = century, `CL` = year, `DH` = month, `DL` = day, BCD |

The counter increments **18.2065 times per second** and is also readable directly at
`0040:006C` (low word) and `0040:006E` (high word).

---

## 7. Other BIOS interrupts

### `INT 11h` — equipment list → `AX`

| Bits | Meaning |
|------|---------|
| 0 | a floppy drive is present |
| 1 | an 8087 is present |
| 5–4 | initial video mode |
| 7–6 | floppy drives, minus one |
| 11–9 | serial ports |
| 14–13 | printer ports |

### `INT 12h` — conventional memory → `AX` in KiB

### `INT 14h` — serial

| `AH` | Function |
|------|----------|
| `00h` | initialise the port; `AL` = parameters, `DX` = port |
| `01h` | send `AL` |
| `02h` | receive into `AL` |
| `03h` | get the status |

### `INT 17h` — printer

| `AH` | Function |
|------|----------|
| `00h` | print `AL` |
| `01h` | initialise |
| `02h` | get the status |

---

## 8. The BIOS data area at segment `0040h`

| Offset | Size | Contents |
|--------|------|----------|
| `00h` | 8 | serial port base addresses |
| `08h` | 8 | parallel port base addresses |
| `10h` | 2 | the equipment word (as `INT 11h`) |
| `13h` | 2 | conventional memory in KiB |
| `17h` | 1 | **keyboard shift flags** |
| `1Eh` | 32 | the keyboard buffer |
| `49h` | 1 | **current video mode** |
| `4Ah` | 2 | screen columns |
| `4Eh` | 2 | video buffer offset |
| `50h` | 16 | cursor positions for 8 pages |
| `60h` | 2 | cursor shape |
| `62h` | 1 | active display page |
| `63h` | 2 | video port base (`3D4h` or `3B4h`) |
| **`6Ch`** | **4** | **the timer tick counter** |
| `70h` | 1 | midnight rollover flag |
| `72h` | 2 | `1234h` means a warm boot is in progress |

Reading these directly is faster than the BIOS call and is entirely conventional — the BIOS itself
is just reading the same bytes.

---

[← Appendix B](B-opcode-map.md) · [Contents](README.md) · [Appendix D →](D-ascii-table.md)
