# Chapter 9 — The WAV Format, Byte by Byte

> By the end of this chapter you will have a working `.wav` writer that you wrote yourself, from
> the specification, with no library. Everything you hear for the next thirty chapters comes out
> of this code.

---

## 9.1 Why write a WAV writer by hand?

There are libraries for this — libsndfile, dr_wav, miniaudio. In a real project you would use
one. So why spend a chapter writing your own?

**Because the WAV format is the perfect size for learning.** It is small enough to understand
completely in an hour, and structured enough to teach you the general shape of every binary
container format you will ever meet: chunks, sizes, headers, alignment, extensibility. Once you
have written a WAV writer, formats like AIFF, FLAC's metadata, and even MIDI files stop being
mysterious.

**Because you will debug WAV files for the rest of your career.** When a file will not play,
when a DAW says "invalid format", when the audio is noise or half-speed or mono-when-it-should-
be-stereo — you will open a hex editor and read the header. That skill comes only from having
written one.

**Because there is nowhere to hide.** A library that silently works teaches nothing. A header
you constructed yourself, where one wrong byte means silence, teaches precision.

---

## 9.2 RIFF: the container

WAV is one instance of **RIFF** (Resource Interchange File Format), a Microsoft/IBM container
design from 1991. RIFF's idea is simple and good: a file is a sequence of **chunks**, and every
chunk announces its own size.

Every chunk has the same shape:

```
   +--------+--------+--------------------------------+
   | ckID   | ckSize |           data                 |
   | 4 bytes| 4 bytes|        ckSize bytes            |
   +--------+--------+--------------------------------+
```

- **ckID** — four ASCII characters naming the chunk, e.g. `fmt ` (note the trailing space — it
  must be four characters) or `data`.
- **ckSize** — a little-endian `uint32` giving the size of the **data** that follows, *not*
  counting the 8 bytes of ckID and ckSize themselves.
- **data** — that many bytes.

This design has a valuable property: **a reader that does not recognise a chunk can skip it**.
Read the ID, read the size, jump forward that many bytes, continue. This is why WAV files can
carry metadata, loop points, broadcast timestamps and cue markers without breaking old players.

> **Padding rule.** If `ckSize` is odd, a single pad byte (usually zero) follows the data so
> that the next chunk starts at an even offset. The pad byte is **not** counted in `ckSize`.
> Forgetting this corrupts every chunk after an odd-sized one. For 16-bit audio, sizes are
> always even, so it rarely bites — until you write a `LIST` chunk with a name of odd length.

### The outermost chunk

A RIFF file is itself one big chunk with ID `RIFF`, whose data begins with a four-character
**form type** saying what kind of RIFF file it is. For audio, that is `WAVE`.

```
   RIFF chunk
   +--------+--------+--------+---------------------------------+
   | "RIFF" | size   | "WAVE" | ... sub-chunks ...              |
   +--------+--------+--------+---------------------------------+
                               fmt  chunk
                               data chunk
                               (optionally others)
```

So the overall structure of the simplest possible WAV file is:

```
   "RIFF"  <size>  "WAVE"
       "fmt "  16   <format description>
       "data"  <n>  <n bytes of samples>
```

That is it. Three nested pieces and you have a playable audio file.

---

## 9.3 The canonical 44-byte header, field by field

Here is the complete layout of the standard header for uncompressed PCM. Every serious audio
programmer knows this table; print it out.

| Offset | Size | Field | Type | Value for our files |
|---:|---:|---|---|---|
| 0 | 4 | ChunkID | ASCII | `"RIFF"` |
| 4 | 4 | ChunkSize | uint32 LE | `36 + dataSize` |
| 8 | 4 | Format | ASCII | `"WAVE"` |
| 12 | 4 | Subchunk1ID | ASCII | `"fmt "` (with trailing space) |
| 16 | 4 | Subchunk1Size | uint32 LE | `16` for PCM |
| 20 | 2 | AudioFormat | uint16 LE | `1` = PCM integer, `3` = IEEE float |
| 22 | 2 | NumChannels | uint16 LE | `1` mono, `2` stereo |
| 24 | 4 | SampleRate | uint32 LE | `44100` |
| 28 | 4 | ByteRate | uint32 LE | `SampleRate × NumChannels × BitsPerSample/8` |
| 32 | 2 | BlockAlign | uint16 LE | `NumChannels × BitsPerSample/8` |
| 34 | 2 | BitsPerSample | uint16 LE | `16` |
| 36 | 4 | Subchunk2ID | ASCII | `"data"` |
| 40 | 4 | Subchunk2Size | uint32 LE | `numFrames × NumChannels × BitsPerSample/8` |
| 44 | ... | Data | samples | the audio |

Now the explanations, because several of these fields are redundant and beginners get them
wrong.

### ChunkSize (offset 4)

The size of everything *after* this field. The file has 8 bytes before the data area starts
being counted (the `RIFF` tag and this size field itself), so:

```
   ChunkSize = fileSize - 8
             = 36 + dataSize
```

Where does 36 come from? `4 ("WAVE") + 8 (fmt header) + 16 (fmt data) + 8 (data header) = 36`.

**This is the field most often wrong**, because you do not know `dataSize` until you have
written all the samples. Two solutions:

1. Compute the sample count in advance (easy when rendering offline — we do this).
2. Write a placeholder, stream the audio, then **seek back** and patch it (necessary when
   recording). We implement that too, in §9.8.

A wrong ChunkSize produces one of two symptoms: the file plays but is truncated, or players
reject it outright. Some players ignore it entirely and read to end-of-file, which means your
file can be broken and still work in VLC while failing in a DAW — a frustrating class of bug.

### AudioFormat (offset 20)

| Value | Meaning |
|---|---|
| 1 | PCM integer (8/16/24/32-bit) — what we use |
| 3 | IEEE float (32/64-bit) |
| 6 | A-law |
| 7 | µ-law |
| 0xFFFE | WAVE_FORMAT_EXTENSIBLE — see §9.6 |

If you write float samples but leave AudioFormat at 1, players will interpret your float bit
patterns as integers. The result is very loud noise. This is worth remembering as a diagnostic:
**"my WAV is loud digital noise" almost always means a format/interpretation mismatch**, not a
DSP bug.

### ByteRate (offset 28)

```
   ByteRate = SampleRate × NumChannels × BitsPerSample / 8
```

For 44,100 Hz stereo 16-bit: `44100 × 2 × 2 = 176,400` bytes per second — the number from
Chapter 4.

This field is redundant (it is derivable from three other fields) and exists so that streaming
players can compute buffer sizes without arithmetic. Get it wrong and most players ignore it;
some report the wrong duration.

### BlockAlign (offset 32)

```
   BlockAlign = NumChannels × BitsPerSample / 8
```

The size of one **frame** — one sample for every channel. For 16-bit stereo, 4 bytes. Also
redundant, also expected to be correct.

### Subchunk2Size (offset 40)

The number of bytes of actual audio data:

```
   dataSize = numFrames × NumChannels × BitsPerSample / 8
```

For 3 seconds of 44,100 Hz stereo 16-bit: `132300 × 2 × 2 = 529,200` bytes. Add 44 for the
header and the file is 529,244 bytes.

**Get this one wrong and your file is truncated or padded with garbage.** It is the second most
common header bug.

### The sample data (offset 44 onward)

For multi-channel audio the samples are **interleaved** (Chapter 4):

```
   Stereo:  L0 R0 L1 R1 L2 R2 L3 R3 ...
```

Each sample is little-endian. For 16-bit, each is a signed `int16_t`.

Channel order for more than two channels is standardised: FL, FR, FC, LFE, BL, BR, ... We use
that order in Chapter 87 for surround.

---

## 9.4 Float to 16-bit integer, done correctly

Our DSP works in `float` in the range −1.0 … +1.0. The file needs `int16_t`. This conversion is
three lines and every one of them matters.

```cpp
int16_t floatToInt16(float sample)
{
    // 1. Clamp to the representable range.
    if (sample >  1.0f) sample =  1.0f;
    if (sample < -1.0f) sample = -1.0f;

    // 2. Scale to integer range, rounding to nearest.
    // 3. Convert.
    return static_cast<int16_t>(std::lround(sample * 32767.0f));
}
```

**Step 1 — clamping.** Without it, a sample of 1.5 scales to 49,150 which does not fit in an
`int16_t`. The conversion is undefined behaviour and in practice usually **wraps around** to a
large negative value. A signal that should have clipped mildly instead produces a full-scale
inverted spike — a violent click, far worse than the clipping you were trying to avoid. This is
the classic "my slightly-too-loud mix has horrible crackling" bug.

**Step 2 — scaling by 32,767, not 32,768.** As established in Chapter 8: the positive maximum is
32,767. Multiplying by 32,768 lets exactly +1.0 become 32,768, which overflows. The tiny
asymmetry this introduces (the negative peak reaches −32,767 instead of −32,768) is 0.00003 dB
and utterly inaudible.

**Step 3 — rounding, not truncating.** `static_cast<int16_t>(x)` truncates toward zero.
`std::lround` rounds to nearest. Truncation biases every sample toward zero, which is a form of
distortion — small, but free to avoid.

The inverse, for Chapter 16:

```cpp
float int16ToFloat(int16_t sample)
{
    return static_cast<float>(sample) / 32768.0f;
}
```

Dividing by 32,768 here guarantees the result is within −1.0 … +0.99997, never exceeding the
range. Using 32,767 would let −32,768 map to −1.00003, which then clips if passed straight back
out. Asymmetric constants in the two directions, and both are deliberate.

### Dither

Chapter 4 explained why: converting float to 16-bit is a bit-depth reduction, and without dither
the quantization error is correlated with the signal, producing distortion rather than noise. It
matters most on quiet passages and fade-outs.

TPDF (triangular probability density function) dither is the standard choice, and it is two
random numbers:

```cpp
float triangularDither(std::mt19937& rng)
{
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    return (dist(rng) - dist(rng)) / 32768.0f;   // range approx +/- 1 LSB
}
```

Subtracting two uniform random numbers gives a triangular distribution, which has better
properties than a single uniform one — the resulting noise is independent of the signal at both
first and second order. Add this to the float sample immediately before scaling and rounding.

Our writer makes dither optional and off by default, because for the synthetic test signals of
the next several chapters it is irrelevant and it would obscure the exact values you expect to
see in a hex dump. Turn it on for anything you intend to listen to seriously.

---

## 9.5 The complete WAV writer

**Code — `code/ch09/wavwriter.h`**

```cpp
#pragma once

#include <string>
#include <vector>
#include <cstdint>

// Writes uncompressed 16-bit PCM WAV files.
// Samples are float in the range -1.0 .. +1.0; anything outside is clamped.
class WavWriter
{
public:
    // Write interleaved float samples.
    //   samples    : interleaved, length = numFrames * numChannels
    //   sampleRate : e.g. 44100
    //   numChannels: 1 = mono, 2 = stereo
    //   dither     : add TPDF dither before quantizing (recommended for real audio)
    // Returns false if the file could not be written.
    static bool write(const std::string& path,
                      const std::vector<float>& samples,
                      int sampleRate  = 44100,
                      int numChannels = 1,
                      bool dither     = false);

    // Convenience: write planar channels (one vector per channel),
    // interleaving them on the way out.
    static bool writePlanar(const std::string& path,
                            const std::vector<std::vector<float>>& channels,
                            int sampleRate = 44100,
                            bool dither    = false);
};
```

**Walkthrough of the header**

`#pragma once` is an **include guard**: it tells the compiler to include this file only once per
translation unit, even if several files `#include` it. Without a guard you get "redefinition"
errors. The older portable form is `#ifndef WAVWRITER_H / #define WAVWRITER_H / ... / #endif`;
`#pragma once` is supported by every compiler you will use and is less error-prone.

`static` member functions belong to the class rather than to an object, so you call them as
`WavWriter::write(...)` without creating an instance. This suits a stateless utility.

Default arguments (`= 44100`, `= 1`, `= false`) let callers write
`WavWriter::write("out.wav", samples)` for the common case.

---

**Code — `code/ch09/wavwriter.cpp`**

```cpp
#include "wavwriter.h"

#include <fstream>
#include <cmath>
#include <random>
#include <algorithm>

namespace
{
    // --- little-endian primitives ------------------------------------

    void writeU16LE(std::ostream& out, uint16_t v)
    {
        out.put(static_cast<char>( v       & 0xFF));
        out.put(static_cast<char>((v >> 8) & 0xFF));
    }

    void writeU32LE(std::ostream& out, uint32_t v)
    {
        out.put(static_cast<char>( v        & 0xFF));
        out.put(static_cast<char>((v >>  8) & 0xFF));
        out.put(static_cast<char>((v >> 16) & 0xFF));
        out.put(static_cast<char>((v >> 24) & 0xFF));
    }

    void writeTag(std::ostream& out, const char* tag)
    {
        out.write(tag, 4);
    }

    // --- sample conversion -------------------------------------------

    int16_t floatToInt16(float sample)
    {
        sample = std::clamp(sample, -1.0f, 1.0f);
        return static_cast<int16_t>(std::lround(sample * 32767.0f));
    }
}

bool WavWriter::write(const std::string& path,
                      const std::vector<float>& samples,
                      int sampleRate,
                      int numChannels,
                      bool dither)
{
    if (numChannels < 1 || sampleRate < 1)
        return false;

    std::ofstream out(path, std::ios::binary);
    if (!out)
        return false;

    const uint16_t bitsPerSample = 16;
    const uint16_t blockAlign    = static_cast<uint16_t>(numChannels * bitsPerSample / 8);
    const uint32_t byteRate      = static_cast<uint32_t>(sampleRate) * blockAlign;
    const uint32_t dataSize      = static_cast<uint32_t>(samples.size() * sizeof(int16_t));

    // ---- RIFF header ----
    writeTag  (out, "RIFF");
    writeU32LE(out, 36 + dataSize);
    writeTag  (out, "WAVE");

    // ---- fmt chunk ----
    writeTag  (out, "fmt ");                                  // note the space
    writeU32LE(out, 16);                                      // PCM fmt chunk size
    writeU16LE(out, 1);                                       // AudioFormat = PCM
    writeU16LE(out, static_cast<uint16_t>(numChannels));
    writeU32LE(out, static_cast<uint32_t>(sampleRate));
    writeU32LE(out, byteRate);
    writeU16LE(out, blockAlign);
    writeU16LE(out, bitsPerSample);

    // ---- data chunk ----
    writeTag  (out, "data");
    writeU32LE(out, dataSize);

    // Convert to int16 in one pass, then write in one call.
    std::vector<int16_t> pcm(samples.size());

    if (dither)
    {
        std::mt19937 rng(12345);                              // fixed seed: reproducible
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);

        for (size_t i = 0; i < samples.size(); ++i)
        {
            const float noise = (dist(rng) - dist(rng)) / 32768.0f;   // TPDF, ~1 LSB
            pcm[i] = floatToInt16(samples[i] + noise);
        }
    }
    else
    {
        for (size_t i = 0; i < samples.size(); ++i)
            pcm[i] = floatToInt16(samples[i]);
    }

    out.write(reinterpret_cast<const char*>(pcm.data()),
              static_cast<std::streamsize>(pcm.size() * sizeof(int16_t)));

    return out.good();
}

bool WavWriter::writePlanar(const std::string& path,
                            const std::vector<std::vector<float>>& channels,
                            int sampleRate,
                            bool dither)
{
    if (channels.empty())
        return false;

    const size_t numChannels = channels.size();
    const size_t numFrames   = channels[0].size();

    for (const auto& ch : channels)
        if (ch.size() != numFrames)
            return false;                      // all channels must be the same length

    std::vector<float> interleaved(numFrames * numChannels);

    for (size_t frame = 0; frame < numFrames; ++frame)
        for (size_t c = 0; c < numChannels; ++c)
            interleaved[frame * numChannels + c] = channels[c][frame];

    return write(path, interleaved, sampleRate,
                 static_cast<int>(numChannels), dither);
}
```

### Walkthrough

**`namespace { ... }`** — an *anonymous namespace*. Everything inside is visible only within this
`.cpp` file. This is the modern replacement for `static` at file scope, and it prevents name
clashes when the project grows: another file can have its own `writeU16LE` without a link error.

**The order of writes is the specification.** Compare the sequence of calls against the table in
§9.3 — they match line for line. That correspondence is why the code is written as a flat list of
`writeXXX` calls rather than something cleverer: it should be *readable against the spec*.

**`writeTag(out, "fmt ")`** — the trailing space is not a typo. Chunk IDs are exactly four
characters. `"fmt"` would write only three and shift the entire rest of the file by one byte,
producing a file that no player will open. This is a real and common bug; it is worth
double-checking every time.

**`36 + dataSize`** — derived in §9.3. If you ever add another chunk, this number changes, which
is exactly the kind of coupling that makes hand-written format code fragile. §9.7 discusses a
more robust approach.

**Converting to a `pcm` vector first, then one `write` call.** Two reasons. Performance: one
system call instead of `samples.size()` of them, which for a three-minute stereo file is the
difference between milliseconds and seconds. And correctness: writing `int16_t` values one at a
time through `writeU16LE` would work, but writing the whole block relies on `int16_t` being
exactly two bytes little-endian — true on every platform we target, and the reason we keep the
explicit LE helpers for the *header* fields where correctness is subtle.

**`std::mt19937 rng(12345)`** — the Mersenne Twister generator from `<random>`, seeded with a
fixed value so that runs are reproducible. For dither, reproducibility is a virtue during
development: two runs of the same program produce byte-identical files, so you can diff them.
Chapter 15 covers random number generation properly.

**`return out.good()`** — checks that no error flag was set during writing. Disk full, permission
denied and path-not-found all show up here. Checking the return value of file operations is not
optional; silently producing no file is the most annoying failure mode there is.

**`writePlanar`'s interleaving loop** — `interleaved[frame * numChannels + c] = channels[c][frame]`
is the planar-to-interleaved index mapping, and it is worth staring at until it is obvious:
frame `f`, channel `c` lives at position `f * numChannels + c`. You will write this expression,
or its inverse, dozens of times.

---

## 9.6 A test program

**Code — `code/ch09/main.cpp`**

```cpp
#include "wavwriter.h"

#include <iostream>
#include <vector>
#include <cmath>

int main()
{
    const int    sampleRate = 44100;
    const double seconds    = 2.0;
    const int    numFrames  = static_cast<int>(sampleRate * seconds);

    // ---- 1. Silence -------------------------------------------------
    {
        std::vector<float> silence(static_cast<size_t>(numFrames), 0.0f);
        if (WavWriter::write("silence.wav", silence, sampleRate, 1))
            std::cout << "Wrote silence.wav  (" << numFrames << " frames)\n";
        else
            std::cerr << "FAILED to write silence.wav\n";
    }

    // ---- 2. A linear ramp: -1.0 up to +1.0 --------------------------
    // Not musical, but it makes the format obvious in a hex editor
    // and in a waveform view.
    {
        std::vector<float> ramp(static_cast<size_t>(numFrames));
        for (int i = 0; i < numFrames; ++i)
            ramp[static_cast<size_t>(i)] =
                -1.0f + 2.0f * static_cast<float>(i) / static_cast<float>(numFrames - 1);

        if (WavWriter::write("ramp.wav", ramp, sampleRate, 1))
            std::cout << "Wrote ramp.wav\n";
    }

    // ---- 3. Stereo: left ramps up, right ramps down -----------------
    {
        std::vector<std::vector<float>> stereo(2,
            std::vector<float>(static_cast<size_t>(numFrames)));

        for (int i = 0; i < numFrames; ++i)
        {
            const float t = static_cast<float>(i) / static_cast<float>(numFrames - 1);
            stereo[0][static_cast<size_t>(i)] =  2.0f * t - 1.0f;   // -1 -> +1
            stereo[1][static_cast<size_t>(i)] =  1.0f - 2.0f * t;   // +1 -> -1
        }

        if (WavWriter::writePlanar("ramp_stereo.wav", stereo, sampleRate))
            std::cout << "Wrote ramp_stereo.wav\n";
    }

    // ---- 4. Report the expected file sizes --------------------------
    const int monoBytes   = 44 + numFrames * 1 * 2;
    const int stereoBytes = 44 + numFrames * 2 * 2;
    std::cout << "\nExpected sizes:\n";
    std::cout << "  silence.wav / ramp.wav : " << monoBytes   << " bytes\n";
    std::cout << "  ramp_stereo.wav        : " << stereoBytes << " bytes\n";

    return 0;
}
```

**Build**

```bash
g++ -std=c++17 -Wall -Wextra -O2 main.cpp wavwriter.cpp -o wavtest.exe
./wavtest.exe
```

Note that **both** `.cpp` files go on the command line. Omitting `wavwriter.cpp` gives you the
link error from Chapter 5: `undefined reference to WavWriter::write(...)`.

**Expected output**

```
Wrote silence.wav  (88200 frames)
Wrote ramp.wav
Wrote ramp_stereo.wav

Expected sizes:
  silence.wav / ramp.wav : 176444 bytes
  ramp_stereo.wav        : 352844 bytes
```

### Verify the header

```bash
ls -l *.wav
xxd -l 64 ramp.wav
```

You should see:

```
00000000: 5249 4646 2404 0b00 5741 5645 666d 7420  RIFF$...WAVEfmt 
00000010: 1000 0000 0100 0100 44ac 0000 88580100  ........D....X..
00000020: 0200 1000 6461 7461 0004 0b00 0180 0480  ....data........
00000030: 0780 0a80 0d80 1080 1380 1680 1980 1c80  ................
```

**Read it against the table in §9.3:**

| Offset | Bytes | Decoded |
|---|---|---|
| 0 | `52 49 46 46` | `RIFF` |
| 4 | `24 04 0B 00` | `0x000B0424` = 721,956 = 176,444 − 8 ✓ |
| 8 | `57 41 56 45` | `WAVE` |
| 12 | `66 6D 74 20` | `fmt ` — note `0x20` is the space ✓ |
| 16 | `10 00 00 00` | 16 ✓ |
| 20 | `01 00` | PCM ✓ |
| 22 | `01 00` | 1 channel ✓ |
| 24 | `44 AC 00 00` | 44,100 ✓ |
| 28 | `88 58 01 00` | `0x00015888` = 88,200 = 44100 × 2 ✓ |
| 32 | `02 00` | BlockAlign 2 ✓ |
| 34 | `10 00` | 16 bits ✓ |
| 36 | `64 61 74 61` | `data` ✓ |
| 40 | `00 04 0B 00` | 721,920 = 88,200 × 2 ✓ |
| 44 | `01 80` | `0x8001` = −32,767 — the first ramp sample ✓ |

That last one is the payoff: `−1.0 × 32767 = −32767`, stored as `0x8001`, written low byte
first as `01 80`. Everything in Chapter 8 is visible in a single pair of bytes.

**Listen.** Open `ramp.wav` in any media player.

- `silence.wav` — nothing, as expected. If you hear anything at all, something is very wrong.
- `ramp.wav` — a **single click at the end**, and near-silence before it. This surprises people,
  so it is worth explaining: a slow linear ramp from −1 to +1 over two seconds is a change at
  0.5 Hz, far below hearing. It is essentially a DC offset sweeping slowly. You hear nothing
  until the waveform snaps from +1 back to 0 at the end of the file — a discontinuity, which is
  broadband, which is a click. **This is your first real lesson in how discontinuities become
  clicks**, and it is the exact mechanism behind envelope clicks in Chapter 13.
- `ramp_stereo.wav` — the same, but with the channels moving oppositely. If you sum it to mono
  it becomes silence, because the two channels are exact inverses. That is Chapter 2's
  destructive interference, made audible.

Open `ramp.wav` in Audacity and you will see a perfect diagonal line. That picture is worth more
than any amount of prose about what a waveform is.

---

## 9.7 WAVE_FORMAT_EXTENSIBLE and other chunks

A few things you will encounter in real files, for when you meet them.

### Extensible format

For more than two channels, or for bit depths that are not a multiple of 8, or when the channel
layout must be explicit, the spec defines `WAVE_FORMAT_EXTENSIBLE`:

- `AudioFormat` = `0xFFFE`
- `Subchunk1Size` = 40 instead of 16
- 22 extra bytes: valid bits per sample, a **channel mask** (a bitfield naming which speakers
  are present), and a 16-byte GUID identifying the real format

The channel mask is the interesting part for us. Bit 0 is front-left, bit 1 front-right, bit 2
front-centre, bit 3 LFE, and so on — this is how a 5.1 file states its speaker layout. Chapter 87
uses it.

Many players accept plain `fmt ` chunks with more than two channels anyway, so we defer this
until we actually need surround.

### Float WAV files

Set `AudioFormat` to 3, `BitsPerSample` to 32, and write raw `float` values with no scaling —
the range is still nominally −1.0 … +1.0 but values outside it are legal and preserved. Float
WAV is excellent for intermediate files between processing stages, because nothing is clipped and
nothing is quantized. Exercise 9.6 adds it to the writer, and Chapter 17 includes it in
`libaudio`.

### Chunks you will see in the wild

| Chunk | Purpose |
|---|---|
| `LIST` / `INFO` | Metadata: title, artist, software, comment |
| `fact` | Number of samples — required for non-PCM formats |
| `cue ` | Cue/marker points |
| `smpl` | Loop points, MIDI root note — used by samplers (Chapter 69) |
| `bext` | Broadcast Wave extension: timecode, originator, coding history (Chapter 90) |
| `iXML` | Production metadata used by film sound recorders |
| `JUNK` / `PAD ` | Padding, often reserving space for a later `bext` |

**A robust reader must skip unknown chunks**, not assume `data` is at offset 36. Chapter 16's
reader does this properly, and it is why so many naïve WAV loaders fail on files from
professional recorders — those files are full of `bext` and `iXML` before the audio.

### Streaming and patching the size

When recording, you do not know the length in advance. The standard technique:

```cpp
// 1. Write the header with placeholder sizes (0 or 0xFFFFFFFF).
// 2. Remember the file positions of the two size fields.
const auto riffSizePos = out.tellp();   // after writing "RIFF"
// ...
const auto dataSizePos = out.tellp();   // after writing "data"
// 3. Stream all the audio.
// 4. Seek back and patch:
const auto endPos = out.tellp();
const uint32_t dataSize = static_cast<uint32_t>(endPos - dataStartPos);

out.seekp(riffSizePos);
writeU32LE(out, 36 + dataSize);
out.seekp(dataSizePos);
writeU32LE(out, dataSize);
```

`tellp()` reports the current write position; `seekp()` moves it. This is why a recording that
crashes mid-take leaves a file with a zero size field — and why repair tools work by recomputing
the sizes from the actual file length.

---

## 9.8 Troubleshooting checklist

When a WAV file misbehaves, work down this list. It will resolve almost every case.

| Symptom | Likely cause |
|---|---|
| File will not open at all | `"fmt"` missing its trailing space; wrong ChunkSize; missing `std::ios::binary` |
| Loud digital noise | AudioFormat/bit-depth mismatch; wrote floats declared as PCM; wrote text mode |
| Plays at double speed, half pitch | SampleRate field wrong, or declared mono while writing stereo |
| Plays at half speed | Declared stereo while writing mono |
| Channels swapped or one silent | Interleaving index wrong (`frame*ch + c` inverted) |
| Silence | Buffer never filled (integer division!); or all samples clamped to 0 |
| Clicks at the very start or end | Discontinuity — needs a fade (Chapter 13) |
| Regular clicks throughout | Off-by-one at block boundaries; state not carried between blocks |
| Truncated | `dataSize` too small; or the stream was never flushed/closed |
| Extra garbage at the end | `dataSize` too large |
| File is exactly 44 bytes | Header written, samples not |
| File is half the expected size | Wrote `int8_t` where `int16_t` intended, or one channel of two |
| Crackling on loud passages | No clamping before conversion — wraparound |

> **The single fastest diagnostic is file size.** Compute `44 + frames × channels × bytesPerSample`
> and compare with `ls -l`. A mismatch immediately narrows the problem to a handful of causes.

---

## 9.9 Exercises

**9.1** Add a `bitsPerSample` parameter to `WavWriter::write` supporting 8, 16 and 24. Remember
that 8-bit WAV is **unsigned** (0–255, silence at 128) while 16- and 24-bit are signed. Verify
each with a hex dump.

**9.2** Add 32-bit float output (AudioFormat = 3). Write the same ramp as float and as 16-bit,
and compare the file sizes and the hex dumps.

**9.3** Write a WAV containing a single sample at full scale surrounded by silence — a perfect
impulse, 44,100 frames with `buffer[22050] = 1.0f`. Listen to it (quietly). You have just made
your first impulse, and Chapter 20 will explain why it is the most important test signal in DSP.

**9.4** *Deliberate breakage.* Change `writeTag(out, "fmt ")` to `writeTag(out, "fmt\0")` and see
which players still open the file. Then change ChunkSize to 0 and try again. Record which
players are tolerant and which are strict — this is genuinely useful knowledge.

**9.5** Write a stereo file where the left channel is a ramp and the right is silence. Confirm in
a hex dump that every other 16-bit value is zero. This verifies your interleaving.

**9.6** Implement the seek-back-and-patch streaming writer from §9.7 as a class
`StreamingWavWriter` with `open()`, `writeFrames()` and `close()`. Test it by writing 10 seconds
in 100 separate calls.

**9.7** Write a function `void printWavInfo(const std::string& path)` that reads only the header
and prints every field with its offset. You will use this constantly. (It is also the skeleton of
Chapter 16's reader.)

**9.8** How many bytes is a 5.1-channel 24-bit 48 kHz file lasting 90 minutes? Would it fit on a
DVD-R (4.7 GB)? Show your working.

---

### Chapter summary

- WAV is a **RIFF** container: a sequence of chunks, each `[4-byte ID][4-byte LE size][data]`,
  with a pad byte after odd-sized chunks.
- The canonical PCM header is **44 bytes**; §9.3's table is the specification and the code
  mirrors it call for call.
- `"fmt "` has a **trailing space**. ChunkSize is `36 + dataSize`. Subchunk2Size is
  `frames × channels × bytesPerSample`. These three fields cause most WAV bugs.
- Float→int16: **clamp**, then **scale by 32,767**, then **round**. Skipping the clamp causes
  wraparound, which is a violent click, not gentle clipping.
- int16→float: divide by **32,768**. The asymmetry between the two directions is deliberate.
- Multi-channel data is **interleaved**: `interleaved[frame * numChannels + channel]`.
- Dither (TPDF) before quantizing anything you will actually listen to.
- A robust reader skips unknown chunks rather than assuming `data` starts at offset 36.
- When something is wrong, **check the file size first**, then hex-dump the first 64 bytes
  against §9.3's table.

**Next:** [Chapter 10 — Your First Sine Wave](10-your-first-sine-wave.md)
