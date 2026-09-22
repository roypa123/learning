# Chapter 8 — Bytes, Binary Files, and Endianness

> A `.wav` file is nothing but a precisely arranged sequence of bytes. Before we can write one,
> we need to be completely clear about what a byte is, how numbers are stored in them, and how
> to get them onto disk exactly — with nothing helpfully "corrected" on the way.
>
> This chapter has no sound in it. It is the last purely preparatory chapter in the book.

---

## 8.1 Bits, bytes, and why we care

A **bit** is one binary digit: 0 or 1. A **byte** is eight bits, and it is the smallest unit of
memory you can address directly.

Eight bits give 2⁸ = **256** distinct patterns, `00000000` through `11111111`. What those
patterns *mean* is entirely a matter of interpretation:

| Byte pattern | As unsigned integer | As signed integer | As ASCII character |
|---|---|---|---|
| `01000001` | 65 | 65 | `A` |
| `11111111` | 255 | −1 | (non-printable) |
| `00000000` | 0 | 0 | (null) |
| `10000000` | 128 | −128 | (non-printable) |

**This is the single most important idea in the chapter: a byte has no intrinsic type.** The
same eight bits are 65, or `A`, or part of a floating-point number, depending only on how the
reading program chooses to interpret them.

A file is a sequence of bytes. A `.wav` file and a `.txt` file are both just bytes; what makes
one playable and the other readable is a shared agreement — a **format** — about what the bytes
at each position mean. Chapter 9 teaches you that agreement for WAV. This chapter teaches you how
to put exact bytes where you want them.

### Hexadecimal

Binary is unreadable at length, decimal does not align with byte boundaries, so everyone uses
**hexadecimal** (base 16). Digits are `0-9` then `A-F`:

| Hex | Decimal | Binary |
|---|---|---|
| `0` | 0 | `0000` |
| `7` | 7 | `0111` |
| `9` | 9 | `1001` |
| `A` | 10 | `1010` |
| `F` | 15 | `1111` |

The convenience is that **one hex digit is exactly four bits**, so one byte is exactly two hex
digits. `0xFF` is 255, `0x00` is 0, `0x41` is 65 (`A`).

The `0x` prefix marks a hex literal in C++:

```cpp
int a = 255;      // decimal
int b = 0xFF;     // hex -- same value
int c = 0b11111111;  // binary literal (C++14) -- same value again
```

You will read hex constantly when inspecting files, so it is worth being able to convert the
common ones on sight: `0x10` = 16, `0x20` = 32, `0x40` = 64, `0x80` = 128, `0xFF` = 255.

---

## 8.2 How integers are stored

### Unsigned

Straightforward place value, base 2:

```
   Byte:       0 0 1 0 1 1 0 1
   Place:    128 64 32 16  8 4 2 1
   Set bits:      32 +  8 + 4 + 1  =  45
```

An 8-bit unsigned integer covers 0–255; 16-bit covers 0–65,535; 32-bit covers 0–4,294,967,295.

### Signed: two's complement

Negative numbers use **two's complement**, which is worth understanding because WAV sample data
is signed and you will look at it in a hex editor.

The rule: the top bit is the **sign bit**. If it is 0, read the rest as a normal positive
number. If it is 1, the value is that number *minus* 2ⁿ.

For 8 bits:

| Binary | Unsigned reading | Signed reading |
|---|---|---|
| `00000000` | 0 | 0 |
| `01111111` | 127 | 127 |
| `10000000` | 128 | **−128** |
| `11111111` | 255 | **−1** |

A quicker mental method: to negate a number, flip every bit and add 1.

```
    5 = 00000101
  flip = 11111010
    +1 = 11111011 = -5
```

Two's complement is used universally because addition works without special cases: `5 + (−5)`
is `00000101 + 11111011 = 100000000`, and the ninth bit falls off the end, leaving `00000000`.
The hardware needs no subtraction circuit at all.

**The audio consequence you must remember:** for 16-bit samples the range is **−32,768 to
+32,767**. One more negative value than positive. This is why Chapter 4's conversion rule
multiplies by 32,767 on the way out but divides by 32,768 on the way in — using 32,768 on the
way out lets a sample at exactly +1.0 overflow to −32,768, which is a full-scale negative spike:
an extremely loud click.

### Fixed-width types

`int` is "whatever the machine likes" — usually 32 bits today, but not guaranteed. For file
formats you need exactness, so use the types from `<cstdint>`:

```cpp
#include <cstdint>

uint8_t  b;     // exactly 8 bits, unsigned  (0..255)
int8_t   sb;    // exactly 8 bits, signed    (-128..127)
uint16_t u16;   // exactly 16 bits, unsigned (0..65535)
int16_t  s16;   // exactly 16 bits, signed   (-32768..32767)  <- WAV sample
uint32_t u32;   // exactly 32 bits, unsigned (0..4294967295)  <- WAV header fields
int32_t  s32;   // exactly 32 bits, signed
```

`sizeof(T)` returns the size in bytes, and it is a compile-time constant:

```cpp
sizeof(uint8_t)   // 1
sizeof(int16_t)   // 2
sizeof(uint32_t)  // 4
sizeof(float)     // 4
sizeof(double)    // 8
```

Use `sizeof` rather than writing `2` or `4` literally. If a type ever changes, the code follows.

---

## 8.3 How floating-point numbers are stored

You do not need this in detail, but a rough picture explains several things you have already
met.

A 32-bit `float` (IEEE 754 single precision) divides its bits into three fields:

```
   [ S ][   Exponent (8)   ][        Mantissa (23)         ]
     1            8                       23                  = 32 bits
```

- **Sign (1 bit)**: 0 positive, 1 negative.
- **Exponent (8 bits)**: the power of two, stored with a bias of 127.
- **Mantissa (23 bits)**: the significant digits, with an implicit leading 1.

The value is roughly `(-1)^S × 1.mantissa × 2^(exponent-127)`.

Three consequences you have already encountered:

- **Precision is relative, not absolute.** You always get ~24 bits of significance (23 stored
  plus the implicit 1), whether the number is 0.001 or 1,000,000. That is ~144 dB of dynamic
  range at any magnitude, which is exactly why float suits audio.
- **Denormals** are the special case when the exponent field is all zeros: the implicit leading 1
  is dropped, allowing very small numbers at reduced precision. This is the slow path that
  Chapter 59 deals with.
- **NaN and infinity** are exponent-all-ones patterns. That is why NaN propagates: the hardware
  has a dedicated encoding for "invalid", and any operation touching it returns it.

For the record, `0.5f` is stored as `0x3F000000`, and `1.0f` as `0x3F800000`. You will see those
patterns if you ever hex-dump a 32-bit float WAV file.

---

## 8.4 Text files versus binary files

```cpp
std::ofstream file("out.txt");
file << 440;
```

This writes **three bytes**: `'4'`, `'4'`, `'0'` — the characters, values 0x34 0x34 0x30. The
number has been converted to human-readable text.

```cpp
std::ofstream file("out.bin", std::ios::binary);
uint32_t value = 440;
file.write(reinterpret_cast<const char*>(&value), sizeof(value));
```

This writes **four bytes**: the raw bit pattern of the integer, `0xB8 0x01 0x00 0x00`. No
conversion, no human readability, exact.

Audio files are binary. A million samples written as text would be megabytes of digits that no
media player could parse. Written as binary they are exactly two bytes each.

### `std::ios::binary` — and why Windows will bite you without it

On Windows, a stream opened in **text mode** performs a translation: every `\n` (byte 0x0A) you
write becomes `\r\n` (0x0D 0x0A). This is a hangover from teletype conventions and it is
invisible for text.

For binary data it is a disaster. Any sample whose byte value happens to be 0x0A gets a spurious
0x0D inserted before it. Your file grows by an unpredictable number of bytes, every offset after
that point shifts, the header's declared sizes become wrong, and the file is corrupt. The
symptom is characteristic: **a WAV file that plays as loud noise, or that plays correctly for a
fraction of a second and then degenerates.**

```cpp
std::ofstream f("audio.wav");                       // WRONG on Windows -- text mode
std::ofstream f("audio.wav", std::ios::binary);     // correct
```

On Linux and macOS there is no translation and both forms work, which is exactly why this bug
survives in cross-platform code until someone runs it on Windows.

> **Rule: every audio file stream, in or out, gets `std::ios::binary`.** No exceptions. Add it
> reflexively, the way you add `-O2`.

---

## 8.5 Writing raw bytes in C++

The workhorse is `std::ostream::write`:

```cpp
file.write(const char* data, std::streamsize count);
```

It takes a pointer to bytes and a count. Note the `char*` — a historical wart; it means "raw
bytes here", not "text". To pass anything else you must convert the pointer type, and C++ makes
you say so explicitly:

```cpp
uint32_t value = 44100;
file.write(reinterpret_cast<const char*>(&value), sizeof(value));
```

**Reading that line piece by piece:**

- `&value` — the *address* of the variable, of type `uint32_t*`.
- `reinterpret_cast<const char*>(...)` — "treat this address as pointing at bytes instead". This
  is the cast that says *I know what I am doing with raw memory*. It performs no conversion; it
  only changes how the compiler types the pointer.
- `sizeof(value)` — 4. Write four bytes starting at that address.

The same pattern writes an entire buffer in one call:

```cpp
std::vector<int16_t> samples(44100);
// ... fill samples ...
file.write(reinterpret_cast<const char*>(samples.data()),
           static_cast<std::streamsize>(samples.size() * sizeof(int16_t)));
```

`samples.data()` gives a pointer to the first element, and vector elements are guaranteed
contiguous, so one `write` call emits the lot. This is dramatically faster than writing samples
one at a time — a single system call instead of 44,100 of them.

> **`reinterpret_cast` is a sharp tool.** It is exactly right for "write these bytes to disk"
> and exactly wrong as a way to convert between number types. `reinterpret_cast<int>(someFloat)`
> does not round 3.7 to 4; it reads the float's bit pattern as an integer and gives you
> 1,080,452,710. To convert values, use `static_cast`. To reinterpret memory, use
> `reinterpret_cast`. Knowing which you want is the whole distinction.

---

## 8.6 Endianness

Here is the subtlety that trips up everyone writing a file format for the first time.

A `uint32_t` holding 44,100 is `0x0000AC44` in hex — four bytes: `00`, `00`, `AC`, `44`. In what
*order* do those bytes go into memory, and therefore into the file?

There are two answers, and both are in use.

**Little-endian** — least significant byte first:

```
   Address:  +0    +1    +2    +3
   Bytes:    44    AC    00    00
```

**Big-endian** — most significant byte first:

```
   Address:  +0    +1    +2    +3
   Bytes:    00    00    AC    44
```

Big-endian matches how we write numbers on paper. Little-endian looks backwards but has genuine
advantages in hardware (you can read the low byte of a multi-byte value without knowing its
size).

| Platform / format | Endianness |
|---|---|
| x86, x86-64 (all PCs) | **Little** |
| ARM (phones, Apple Silicon) | Little (configurable, always little in practice) |
| **WAV / RIFF files** | **Little** |
| AIFF files | **Big** |
| Network protocols (TCP/IP) | Big ("network byte order") |
| PowerPC, older Macs, some DSPs | Big |

**The convenient news:** WAV is little-endian and your PC is little-endian. So on Windows,
macOS, and Linux on Intel or ARM, writing a `uint32_t` straight to the file with
`reinterpret_cast` produces exactly the right bytes. No conversion needed.

**The inconvenient news:** relying on that silently means your code is not portable, and — more
practically — you cannot *read* a big-endian AIFF, or a WAV produced on an unusual platform,
without understanding this. So we write explicit helpers and use them everywhere. They cost
nothing and they make the code self-documenting: when you read `writeU32LE(file, sampleRate)`,
you know without checking that four little-endian bytes go out.

### Explicit little-endian helpers

```cpp
void writeU16LE(std::ostream& out, uint16_t value)
{
    out.put(static_cast<char>( value        & 0xFF));   // low byte first
    out.put(static_cast<char>((value >> 8)  & 0xFF));   // then high byte
}

void writeU32LE(std::ostream& out, uint32_t value)
{
    out.put(static_cast<char>( value        & 0xFF));
    out.put(static_cast<char>((value >>  8) & 0xFF));
    out.put(static_cast<char>((value >> 16) & 0xFF));
    out.put(static_cast<char>((value >> 24) & 0xFF));
}
```

**Walkthrough of the bit manipulation:**

- `value & 0xFF` — the **bitwise AND** with `11111111` keeps only the lowest eight bits and
  zeroes everything else. This *extracts* the low byte.
- `value >> 8` — the **right shift** moves every bit eight positions right, so what was the
  second byte is now the lowest. Then `& 0xFF` extracts it.
- `>> 16` and `>> 24` reach the third and fourth bytes the same way.
- `out.put(char)` writes exactly one byte.

Work through `value = 44100 = 0x0000AC44`:

| Expression | Value | Byte written |
|---|---|---|
| `value & 0xFF` | `0x44` | `44` |
| `(value >> 8) & 0xFF` | `0xAC` | `AC` |
| `(value >> 16) & 0xFF` | `0x00` | `00` |
| `(value >> 24) & 0xFF` | `0x00` | `00` |

Output order: `44 AC 00 00` — little-endian, correct, and correct **on every platform**
regardless of that platform's native byte order, because we did the byte extraction with
arithmetic rather than by copying memory.

The reading counterparts reassemble in the same order:

```cpp
uint16_t readU16LE(std::istream& in)
{
    const uint8_t b0 = static_cast<uint8_t>(in.get());
    const uint8_t b1 = static_cast<uint8_t>(in.get());
    return static_cast<uint16_t>(b0 | (b1 << 8));
}

uint32_t readU32LE(std::istream& in)
{
    const uint8_t b0 = static_cast<uint8_t>(in.get());
    const uint8_t b1 = static_cast<uint8_t>(in.get());
    const uint8_t b2 = static_cast<uint8_t>(in.get());
    const uint8_t b3 = static_cast<uint8_t>(in.get());
    return static_cast<uint32_t>(b0)
         | (static_cast<uint32_t>(b1) <<  8)
         | (static_cast<uint32_t>(b2) << 16)
         | (static_cast<uint32_t>(b3) << 24);
}
```

`|` is **bitwise OR**, which merges the bytes into their slots; `<<` is left shift, moving each
byte up to its position. Note the casts to `uint32_t` *before* shifting — shifting a `uint8_t`
left by 24 would overflow the 8-bit type on some compilers before the cast happened. This is
exactly the kind of bug `-Wextra` sometimes catches and sometimes does not, so be deliberate.

---

## 8.7 Bit operations reference

You now have all of them; here they are collected, since audio code uses them for file formats,
MIDI parsing, and circular buffer indexing.

| Operator | Name | Example | Result |
|---|---|---|---|
| `&` | AND | `0b1100 & 0b1010` | `0b1000` — bits set in *both* |
| `\|` | OR | `0b1100 \| 0b1010` | `0b1110` — bits set in *either* |
| `^` | XOR | `0b1100 ^ 0b1010` | `0b0110` — bits set in exactly one |
| `~` | NOT | `~0b1100` | flips every bit |
| `<<` | left shift | `1 << 4` | `16` — multiply by 2ⁿ |
| `>>` | right shift | `16 >> 2` | `4` — divide by 2ⁿ |

Idioms you will meet later in the book:

```cpp
value & 0xFF                 // extract the low byte
value & 0x7F                 // extract the low 7 bits (MIDI data bytes, Chapter 62)
(status >> 4) & 0x0F         // extract the MIDI message type nibble
index & (size - 1)           // wrap an index, if size is a power of two (Chapter 42)
1 << n                       // 2 to the power n
(n & (n - 1)) == 0           // true if n is a power of two (Chapter 25's FFT check)
```

That fourth one is worth noting now: `index & (size - 1)` is equivalent to `index % size` **only
when `size` is a power of two**, and it is considerably faster than a division. This is why
delay lines and FFT buffers are so often sized to powers of two.

---

## 8.8 A complete program: write and inspect a binary file

Time to make all of this concrete. This program writes a small binary file with a mixture of
types, then reads it back and prints a hex dump.

**Code — `code/ch08/bytes.cpp`**

```cpp
#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <string>
#include <cstdint>

// ---------------------------------------------------------------- writing

void writeU16LE(std::ostream& out, uint16_t value)
{
    out.put(static_cast<char>( value       & 0xFF));
    out.put(static_cast<char>((value >> 8) & 0xFF));
}

void writeU32LE(std::ostream& out, uint32_t value)
{
    out.put(static_cast<char>( value        & 0xFF));
    out.put(static_cast<char>((value >>  8) & 0xFF));
    out.put(static_cast<char>((value >> 16) & 0xFF));
    out.put(static_cast<char>((value >> 24) & 0xFF));
}

// Write four ASCII characters, as RIFF chunk identifiers require.
void writeTag(std::ostream& out, const char* tag)
{
    out.write(tag, 4);
}

// ---------------------------------------------------------------- dumping

void hexDump(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        std::cerr << "Could not open " << path << "\n";
        return;
    }

    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)),
                                std::istreambuf_iterator<char>());

    std::cout << "File: " << path << "  (" << bytes.size() << " bytes)\n\n";

    for (size_t offset = 0; offset < bytes.size(); offset += 16)
    {
        // Offset column.
        std::cout << std::setw(8) << std::setfill('0') << std::hex
                  << offset << "  " << std::setfill(' ');

        // Hex column.
        for (size_t i = 0; i < 16; ++i)
        {
            if (offset + i < bytes.size())
                std::cout << std::setw(2) << std::setfill('0') << std::hex
                          << static_cast<int>(bytes[offset + i]) << ' ';
            else
                std::cout << "   ";

            if (i == 7)
                std::cout << ' ';            // gap in the middle for readability
        }

        // ASCII column.
        std::cout << " |";
        for (size_t i = 0; i < 16 && offset + i < bytes.size(); ++i)
        {
            const uint8_t b = bytes[offset + i];
            std::cout << (b >= 32 && b < 127 ? static_cast<char>(b) : '.');
        }
        std::cout << "|\n";
    }

    std::cout << std::dec << std::setfill(' ') << "\n";
}

// ---------------------------------------------------------------- main

int main()
{
    const std::string path = "bytes_demo.bin";

    {
        std::ofstream out(path, std::ios::binary);
        if (!out)
        {
            std::cerr << "Could not create " << path << "\n";
            return 1;
        }

        writeTag(out, "DEMO");              // 4 bytes of ASCII
        writeU32LE(out, 44100);             // sample rate
        writeU16LE(out, 2);                 // channel count
        writeU16LE(out, 16);                // bits per sample

        // Six 16-bit signed samples, written as raw little-endian.
        const int16_t samples[6] = { 0, 16384, 32767, 0, -16384, -32768 };
        out.write(reinterpret_cast<const char*>(samples), sizeof(samples));
    }   // `out` is destroyed here, which flushes and closes the file

    hexDump(path);

    // Read the header fields back and check them.
    std::ifstream in(path, std::ios::binary);
    char tag[5] = {};
    in.read(tag, 4);

    auto readU16LE = [&in]() -> uint16_t {
        const uint8_t b0 = static_cast<uint8_t>(in.get());
        const uint8_t b1 = static_cast<uint8_t>(in.get());
        return static_cast<uint16_t>(b0 | (b1 << 8));
    };
    auto readU32LE = [&in]() -> uint32_t {
        const uint8_t b0 = static_cast<uint8_t>(in.get());
        const uint8_t b1 = static_cast<uint8_t>(in.get());
        const uint8_t b2 = static_cast<uint8_t>(in.get());
        const uint8_t b3 = static_cast<uint8_t>(in.get());
        return static_cast<uint32_t>(b0)
             | (static_cast<uint32_t>(b1) <<  8)
             | (static_cast<uint32_t>(b2) << 16)
             | (static_cast<uint32_t>(b3) << 24);
    };

    const uint32_t rate     = readU32LE();
    const uint16_t channels = readU16LE();
    const uint16_t bits     = readU16LE();

    std::cout << "Read back:\n";
    std::cout << "  tag      = " << tag << "\n";
    std::cout << "  rate     = " << rate << "\n";
    std::cout << "  channels = " << channels << "\n";
    std::cout << "  bits     = " << bits << "\n";

    return 0;
}
```

**Expected output**

```
File: bytes_demo.bin  (24 bytes)

00000000  44 45 4d 4f 44 ac 00 00  02 00 10 00 00 00 00 40  |DEMOD..........@|
00000010  ff 7f 00 00 00 c0 00 80                           |........|

Read back:
  tag      = DEMO
  rate     = 44100
  channels = 2
  bits     = 16
```

**Walkthrough — reading the hex dump**

This is the skill the chapter exists to teach. Go byte by byte:

| Offset | Bytes | Meaning |
|---|---|---|
| 0–3 | `44 45 4D 4F` | ASCII `D`, `E`, `M`, `O` — visible in the right-hand column |
| 4–7 | `44 AC 00 00` | 44,100 little-endian. Low byte `0x44` first, exactly as §8.6 predicted |
| 8–9 | `02 00` | 2 channels, little-endian 16-bit |
| 10–11 | `10 00` | `0x10` = 16 bits per sample |
| 12–13 | `00 00` | sample 0 |
| 14–15 | `00 40` | `0x4000` = 16,384 |
| 16–17 | `FF 7F` | `0x7FFF` = 32,767 — the maximum positive 16-bit value |
| 18–19 | `00 00` | sample 0 |
| 20–21 | `00 C0` | `0xC000` = −16,384 in two's complement |
| 22–23 | `00 80` | `0x8000` = −32,768 — the most negative value |

Every one of those confirms something from earlier in the chapter. `FF 7F` reversed is `7FFF`,
the largest positive signed 16-bit number. `00 80` reversed is `8000`, which as unsigned would
be 32,768 but as signed is −32,768. That asymmetry is the two's complement fact from §8.2, and
now you have seen it on disk.

**Walkthrough — the code**

`std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)),
std::istreambuf_iterator<char>());` reads the whole file into a vector in one expression. The
two iterators mean "from the start of the stream to the end". The extra parentheses around the
first argument are required to avoid C++'s "most vexing parse", a genuine language wart where
the compiler would otherwise read this as a function declaration.

`std::setw(2) << std::setfill('0') << std::hex` formats each byte as exactly two hex digits with
a leading zero. `std::hex` is sticky, which is why the code resets with `std::dec` at the end —
forgetting that leads to the delightful bug where all your later numbers print in hex.

`(b >= 32 && b < 127 ? static_cast<char>(b) : '.')` is the **ternary operator**: `condition ?
valueIfTrue : valueIfFalse`. Printable ASCII is 32 to 126; anything else prints as a dot. That
is what every hex editor in the world does, and it is why `DEMO` jumps out of the right-hand
column.

The braces around the writing section — `{ std::ofstream out(...); ... }` — create a scope. When
it ends, `out` is destroyed, which **flushes and closes the file**. This is RAII from Chapter 7
doing real work. Without it, the file might still be buffered and unwritten when `hexDump` tries
to read it.

> **Pitfall: the unflushed file.** If a program writes a WAV and it appears truncated or empty,
> the usual cause is that the stream was still open when something else read it, or the program
> crashed before the destructor ran. Either scope the stream as above, or call `out.close()`
> explicitly before reading.

The `auto readU16LE = [&in]() -> uint16_t { ... };` syntax is a **lambda** — an anonymous
function defined inline. The `[&in]` is the *capture list*: it lets the lambda use the `in`
variable from the surrounding scope by reference. Lambdas appear throughout the rest of the book
for callbacks and small local helpers; that is the whole syntax.

---

## 8.9 Inspecting files without writing code

You will often want to look at a file you did not produce. Options:

**Command line (Git Bash / MSYS2):**

```bash
xxd -l 64 audio.wav          # first 64 bytes as hex + ASCII
od -A x -t x1z -v audio.wav | head    # same idea, POSIX tool
ls -l audio.wav                       # file size -- a very useful sanity check
```

**PowerShell:**

```powershell
Format-Hex -Path audio.wav -Count 64
```

**GUI:** HxD is the standard free hex editor on Windows and is worth installing. Audacity can
also import "raw" data, which is a good way to check whether a mysterious file is really PCM.

> **Sanity check to internalise now.** A 16-bit mono WAV at 44,100 Hz has a 44-byte header
> followed by `2 × 44100 = 88,200` bytes per second. So a 3-second file should be **264,644
> bytes**. If your file is 44 bytes, you wrote a header and no samples. If it is half the
> expected size, you probably wrote one channel where you declared two, or `int8_t` where you
> meant `int16_t`. File size is the fastest diagnostic you have, and it costs one `ls -l`.

---

## 8.10 Exercises

**8.1** Convert by hand, then check with a program: `0x7FFF`, `0x8000`, `0xFFFF` as *signed*
16-bit integers. `0x0A`, `0x41`, `0x20` as ASCII characters.

**8.2** Write `writeU24LE` for 24-bit values (three bytes). You will need it in Chapter 16 for
24-bit WAV files. Test it with the values 0, 1, 8,388,607 and −8,388,608, and hex-dump the
result.

**8.3** Write `writeU16BE` and `writeU32BE` (big-endian). Modify `bytes.cpp` to write the header
big-endian and dump it. Compare the two dumps side by side and state the rule in one sentence.

**8.4** What does `0.5f` look like in memory? Write a program that stores `0.5f`, `1.0f`,
`-1.0f` and `0.0f` in a `float`, then hex-dumps four bytes for each using `reinterpret_cast`.
Can you see the sign bit change?

**8.5** *Deliberate breakage.* Remove `std::ios::binary` from the `ofstream` in `bytes.cpp` and
run it on Windows. Compare the dump with the correct one. Which byte got duplicated, and why
that one specifically? (Hint: look for `0x0A` in the correct output.)

**8.6** Write a program that reports the endianness of the machine it runs on. One approach:
store `uint32_t x = 1;` and inspect its first byte with a `reinterpret_cast<uint8_t*>`.

**8.7** Given a 16-bit stereo WAV at 48,000 Hz whose file size is 5,760,044 bytes, how long is
it in seconds? Show your working, and state what you had to assume about the header.

**8.8** Implement `bool isPowerOfTwo(uint32_t n)` using `(n & (n - 1)) == 0`. Why does that
work? What does it return for `n = 0`, and is that answer acceptable? Fix it if not.

---

### Chapter summary

- A byte is eight bits and has **no intrinsic type**; meaning comes from how it is interpreted.
- Hex is the working notation: one hex digit = four bits, two digits = one byte.
- Signed integers use two's complement, which is why 16-bit audio runs −32,768 … +32,767 and why
  the float→int16 conversion multiplies by 32,767 rather than 32,768.
- Use fixed-width types (`int16_t`, `uint32_t`) for anything that touches a file format.
- **Always open audio files with `std::ios::binary`.** On Windows, text mode silently corrupts
  binary data by expanding `0x0A` into `0x0D 0x0A`.
- `file.write(reinterpret_cast<const char*>(data), byteCount)` writes raw bytes; write whole
  buffers in one call rather than sample by sample.
- WAV is **little-endian**, as is every PC. Write explicit `writeU16LE`/`writeU32LE` helpers
  using shifts and masks, so the code is correct on any platform and says what it means.
- Learn to read a hex dump. `xxd -l 64 file.wav` and a quick file-size calculation will diagnose
  most file-format bugs in seconds.

**Next:** [Chapter 9 — The WAV Format, Byte by Byte](09-the-wav-format.md)
