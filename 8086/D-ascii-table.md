# Appendix D — ASCII and scan codes

[← Appendix C](C-interrupt-reference.md) · [Contents](README.md) · [Appendix E →](E-pin-reference.md)

---

## 1. The four facts worth memorising

```
   '0' = 0x30 ... '9' = 0x39      digit -> value:  sub al, '0'   or  and al, 0x0F
   'A' = 0x41 ... 'Z' = 0x5A      value -> digit:  add al, '0'
   'a' = 0x61 ... 'z' = 0x7A
   space = 0x20                   case is BIT 5:   and al,0xDF upper
                                                   or  al,0x20 lower
                                                   xor al,0x20 toggle
```

The case trick works because `'a' − 'A'` is exactly `0x20`. That was deliberate when ASCII was
designed, and it is the reason Chapter 25 §7.1's idioms are one instruction each.

For hexadecimal digits there is a gap: `'9'` is `0x39` and `'A'` is `0x41`, so converting a value
10–15 needs `add al, '0'` followed by `add al, 7`. Chapter 43 §4 uses `XLAT` to avoid the branch.

---

## 2. The standard ASCII table, 0–127

| Dec | Hex | Char | Dec | Hex | Char | Dec | Hex | Char | Dec | Hex | Char |
|-----|-----|------|-----|-----|------|-----|-----|------|-----|-----|------|
| 0 | 00 | NUL | 32 | 20 | space | 64 | 40 | `@` | 96 | 60 | `` ` `` |
| 1 | 01 | SOH | 33 | 21 | `!` | 65 | 41 | `A` | 97 | 61 | `a` |
| 2 | 02 | STX | 34 | 22 | `"` | 66 | 42 | `B` | 98 | 62 | `b` |
| 3 | 03 | ETX | 35 | 23 | `#` | 67 | 43 | `C` | 99 | 63 | `c` |
| 4 | 04 | EOT | 36 | 24 | `$` | 68 | 44 | `D` | 100 | 64 | `d` |
| 5 | 05 | ENQ | 37 | 25 | `%` | 69 | 45 | `E` | 101 | 65 | `e` |
| 6 | 06 | ACK | 38 | 26 | `&` | 70 | 46 | `F` | 102 | 66 | `f` |
| 7 | 07 | BEL | 39 | 27 | `'` | 71 | 47 | `G` | 103 | 67 | `g` |
| 8 | 08 | BS | 40 | 28 | `(` | 72 | 48 | `H` | 104 | 68 | `h` |
| 9 | 09 | HT | 41 | 29 | `)` | 73 | 49 | `I` | 105 | 69 | `i` |
| 10 | 0A | LF | 42 | 2A | `*` | 74 | 4A | `J` | 106 | 6A | `j` |
| 11 | 0B | VT | 43 | 2B | `+` | 75 | 4B | `K` | 107 | 6B | `k` |
| 12 | 0C | FF | 44 | 2C | `,` | 76 | 4C | `L` | 108 | 6C | `l` |
| 13 | 0D | CR | 45 | 2D | `-` | 77 | 4D | `M` | 109 | 6D | `m` |
| 14 | 0E | SO | 46 | 2E | `.` | 78 | 4E | `N` | 110 | 6E | `n` |
| 15 | 0F | SI | 47 | 2F | `/` | 79 | 4F | `O` | 111 | 6F | `o` |
| 16 | 10 | DLE | 48 | 30 | `0` | 80 | 50 | `P` | 112 | 70 | `p` |
| 17 | 11 | DC1 | 49 | 31 | `1` | 81 | 51 | `Q` | 113 | 71 | `q` |
| 18 | 12 | DC2 | 50 | 32 | `2` | 82 | 52 | `R` | 114 | 72 | `r` |
| 19 | 13 | DC3 | 51 | 33 | `3` | 83 | 53 | `S` | 115 | 73 | `s` |
| 20 | 14 | DC4 | 52 | 34 | `4` | 84 | 54 | `T` | 116 | 74 | `t` |
| 21 | 15 | NAK | 53 | 35 | `5` | 85 | 55 | `U` | 117 | 75 | `u` |
| 22 | 16 | SYN | 54 | 36 | `6` | 86 | 56 | `V` | 118 | 76 | `v` |
| 23 | 17 | ETB | 55 | 37 | `7` | 87 | 57 | `W` | 119 | 77 | `w` |
| 24 | 18 | CAN | 56 | 38 | `8` | 88 | 58 | `X` | 120 | 78 | `x` |
| 25 | 19 | EM | 57 | 39 | `9` | 89 | 59 | `Y` | 121 | 79 | `y` |
| 26 | 1A | SUB | 58 | 3A | `:` | 90 | 5A | `Z` | 122 | 7A | `z` |
| 27 | 1B | ESC | 59 | 3B | `;` | 91 | 5B | `[` | 123 | 7B | `{` |
| 28 | 1C | FS | 60 | 3C | `<` | 92 | 5C | `\` | 124 | 7C | `\|` |
| 29 | 1D | GS | 61 | 3D | `=` | 93 | 5D | `]` | 125 | 7D | `}` |
| 30 | 1E | RS | 62 | 3E | `>` | 94 | 5E | `^` | 126 | 7E | `~` |
| 31 | 1F | US | 63 | 3F | `?` | 95 | 5F | `_` | 127 | 7F | DEL |

### 2.1 The control characters that matter

| Code | Name | Effect |
|------|------|--------|
| `07h` | BEL | beep |
| `08h` | BS | backspace |
| `09h` | HT | tab |
| `0Ah` | **LF** | **line feed — move down one line** |
| `0Ch` | FF | form feed / clear screen |
| `0Dh` | **CR** | **carriage return — move to column 0** |
| `1Ah` | SUB | **end of file** in a DOS text file (Ctrl-Z) |
| `1Bh` | ESC | escape — introduces ANSI sequences |
| `24h` | `$` | not a control character, but **DOS function 09h's terminator** |

**DOS needs both `0Dh` and `0Ah` for a new line.** Printing only `0Ah` moves down without returning
to the left margin. This is the `\r\n` versus `\n` difference.

---

## 3. Code page 437 — the extended characters, 128–255

The IBM PC's upper half. These are what `db 0xC9` produces on screen in text mode, and they are how
the box in Chapter 37 §7 is drawn.

### 3.1 The box-drawing characters

| Code | Char | Description |
|------|------|-------------|
| `B3h` | │ | single vertical |
| `B4h` | ┤ | single left tee |
| `BAh` | ║ | **double vertical** |
| `BBh` | ╗ | **double top-right** |
| `BCh` | ╝ | **double bottom-right** |
| `C0h` | └ | single bottom-left |
| `C4h` | ─ | single horizontal |
| `C8h` | ╚ | **double bottom-left** |
| `C9h` | ╔ | **double top-left** |
| `CDh` | ═ | **double horizontal** |
| `CEh` | ╬ | double cross |
| `D9h` | ┘ | single bottom-right |
| `DAh` | ┌ | single top-left |

**The five you need for a double-line box:** `C9h` `CDh` `BBh` `BAh` `C8h` `BCh`.

### 3.2 The block and shading characters

| Code | Char | Description |
|------|------|-------------|
| `B0h` | ░ | light shade |
| `B1h` | ▒ | medium shade |
| `B2h` | ▓ | dark shade |
| `DBh` | █ | **full block** |
| `DCh` | ▄ | lower half block |
| `DFh` | ▀ | upper half block |
| `FEh` | ■ | small square |

`B0h`–`B2h` and `DBh` give five levels of grey, which is how text-mode bar charts and progress bars
were drawn.

### 3.3 Other useful codes

| Code | Char | Description |
|------|------|-------------|
| `E3h` | π | pi |
| `EDh` | φ | phi |
| `F0h` | ≡ | identical to |
| `F1h` | ± | plus-minus |
| `F8h` | ° | degree |
| `FAh` | · | middle dot |
| `FBh` | √ | square root |

---

## 4. Keyboard scan codes

`INT 16h` function `00h` returns the ASCII code in `AL` and the **scan code** in `AH`
(Chapter 37 §3).

**`AL = 0` means the key has no ASCII equivalent** — look at `AH`.

### 4.1 The keys with no ASCII code

| Scan code | Key | Scan code | Key |
|-----------|-----|-----------|-----|
| `3Bh` | **F1** | `47h` | **Home** |
| `3Ch` | F2 | `48h` | **Up arrow** |
| `3Dh` | F3 | `49h` | **Page Up** |
| `3Eh` | F4 | `4Bh` | **Left arrow** |
| `3Fh` | F5 | `4Ch` | Keypad 5 |
| `40h` | F6 | `4Dh` | **Right arrow** |
| `41h` | F7 | `4Fh` | **End** |
| `42h` | F8 | `50h` | **Down arrow** |
| `43h` | F9 | `51h` | **Page Down** |
| `44h` | F10 | `52h` | **Insert** |
| `85h` | F11 (AT) | `53h` | **Delete** |
| `86h` | F12 (AT) | | |

### 4.2 Keys that do have an ASCII code

| Key | `AH` scan | `AL` ASCII |
|-----|-----------|-----------|
| Esc | `01h` | `1Bh` |
| Backspace | `0Eh` | `08h` |
| Tab | `0Fh` | `09h` |
| Enter | `1Ch` | `0Dh` |
| Space | `39h` | `20h` |
| `A` | `1Eh` | `61h` / `41h` shifted |
| `1` | `02h` | `31h` / `21h` shifted |

### 4.3 Shifted and control combinations

| Combination | `AL` | `AH` |
|-------------|------|------|
| Shift+F1 … Shift+F10 | 0 | `54h`–`5Dh` |
| Ctrl+F1 … Ctrl+F10 | 0 | `5Eh`–`67h` |
| Alt+F1 … Alt+F10 | 0 | `68h`–`71h` |
| Ctrl+Left / Ctrl+Right | 0 | `73h` / `74h` |
| Ctrl+Home / Ctrl+End | 0 | `77h` / `75h` |
| Alt+1 … Alt+0 | 0 | `78h`–`81h` |
| Ctrl+A … Ctrl+Z | `01h`–`1Ah` | the letter's scan code |

**`Ctrl+letter` gives the ASCII code 1–26.** Ctrl+C is `03h`, Ctrl+Z is `1Ah`, Ctrl+M is `0Dh` —
which is why Ctrl+M behaves as Enter.

### 4.4 Reading them

```asm
        xor  ah, ah
        int  0x16               ; AL = ASCII, AH = scan code
        or   al, al
        jnz  .normal_character

        ; AL = 0 -> an extended key; AH has the scan code
        cmp  ah, 0x48
        je   .up
        cmp  ah, 0x50
        je   .down
        cmp  ah, 0x4B
        je   .left
        cmp  ah, 0x4D
        je   .right
.normal_character:
```

### 4.5 The raw port `60h` codes

Reading port `60h` directly (Chapter 17 §6.1) gives the **make and break** codes, not what `INT 16h`
returns:

- **Make code:** the scan code, when the key goes down.
- **Break code:** the scan code **plus `80h`** — bit 7 set — when it comes up.

So pressing and releasing `A` produces `1Eh` then `9Eh`. Testing bit 7 distinguishes them:

```asm
        in   al, 0x60
        test al, 0x80
        jnz  .key_released
```

That is how a game reads several keys held at once, which `INT 16h` cannot report.

---

## 5. A conversion quick reference

```asm
; ASCII digit -> value
        sub  al, '0'            ; or:  and al, 0x0F

; value 0-9 -> ASCII digit
        add  al, '0'

; value 0-15 -> hex digit, with a branch
        add  al, '0'
        cmp  al, '9'
        jbe  .done
        add  al, 7
.done:

; value 0-15 -> hex digit, branchless
        mov  bx, hextab
        xlat
hextab: db  '0123456789ABCDEF'

; hex digit -> value
        cmp  al, '9'
        jbe  .digit
        or   al, 0x20           ; fold to lower case
        sub  al, 'a' - 10       ; NASM computes 0x57
        jmp  .done
.digit: sub  al, '0'
.done:

; upper case (letters only)
        and  al, 0xDF

; lower case (letters only)
        or   al, 0x20

; toggle case (letters only)
        xor  al, 0x20

; is it a digit?
        cmp  al, '0'
        jb   .no
        cmp  al, '9'
        ja   .no

; is it a letter?
        or   al, 0x20           ; fold, then one range test does both cases
        cmp  al, 'a'
        jb   .no
        cmp  al, 'z'
        ja   .no
```

---

[← Appendix C](C-interrupt-reference.md) · [Contents](README.md) · [Appendix E →](E-pin-reference.md)
