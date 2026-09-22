# Chapter 16 — Reading WAV Files Back In

> Writing a WAV file is easy: you control every byte. Reading one is harder, because you do not
> control the file — someone else's recorder, DAW or converter made it, and it may contain things
> you did not anticipate. This chapter is about writing code that survives the real world.

---

## 16.1 Why reading is the harder half

When we wrote a WAV in Chapter 9, we knew exactly what we had produced: 16-bit PCM, `fmt ` chunk
at offset 12, `data` chunk at offset 36, no surprises.

A file from a field recorder, a DAW export, or a sample library might have:

- A `bext` chunk (Broadcast Wave: timecode, originator, coding history) before the audio.
- `iXML`, `LIST`/`INFO`, `cue `, `smpl`, `JUNK`, `PAD ` chunks, in any order.
- 8-bit **unsigned**, 16-bit signed, 24-bit signed, 32-bit signed, 32-bit float, or 64-bit float
  samples.
- `WAVE_FORMAT_EXTENSIBLE` with a 40-byte `fmt ` chunk instead of 16.
- An odd-sized chunk with a pad byte.
- A wrong `ChunkSize` (very common — many recorders write a placeholder and crash before
  patching it).
- A `data` size of `0xFFFFFFFF`, used by some recorders to mean "read to end of file".
- More than 4 GB of audio, which plain WAV cannot express (that is what RF64 is for).

A reader that assumes "data starts at offset 44" will fail on a large fraction of professional
files. That assumption is the single most common bug in amateur WAV loaders, and it is why so
much software cannot open files straight off a Sound Devices or Zoom recorder.

> **The rule: never assume offsets. Always walk the chunks.**

---

## 16.2 Walking the chunk list

The algorithm, which works for any RIFF file:

```
1. Read 4 bytes. Must be "RIFF".
2. Read 4 bytes: the RIFF size. Note it but do not trust it.
3. Read 4 bytes. Must be "WAVE".
4. Loop until end of file:
     a. Read 4 bytes: the chunk ID.
     b. Read 4 bytes: the chunk size.
     c. Note the current position -- this is where the chunk data begins.
     d. If the ID is "fmt ", parse the format.
        If the ID is "data", note the position and size; we will read it later.
        Otherwise, skip it.
     e. Seek to (data begin + size), plus one pad byte if size is odd.
5. Check that we found both "fmt " and "data".
6. Seek back to the data position and read the samples.
```

Two details make this robust:

**Do not trust the RIFF size.** Use end-of-file as the terminating condition, and treat the
declared size as advisory. Files with a wrong RIFF size are common and usually perfectly
playable.

**Handle the pad byte.** If a chunk's size is odd, one pad byte follows and is not counted in the
size. Miss it and every subsequent chunk ID is misaligned by one byte, and your reader reports
garbage.

---

## 16.3 Sample format conversion

Once you know the format, each sample type converts to our `float` range differently. Getting
these right matters; each has a trap.

### 16-bit signed

```cpp
float s = static_cast<float>(value) / 32768.0f;
```

Chapter 9 explained the 32,768: it guarantees the result never exceeds ±1.0, because the most
negative value is −32,768.

### 8-bit **unsigned**

```cpp
float s = (static_cast<float>(value) - 128.0f) / 128.0f;
```

**8-bit WAV is unsigned**, unlike every other bit depth. 0 is the most negative, 128 is silence,
255 is the most positive. This inconsistency is historical and it catches everyone. If an 8-bit
file plays as loud distorted noise with a big DC offset, this is why.

### 24-bit signed

There is no 24-bit integer type, so you assemble three bytes and **sign-extend** by hand:

```cpp
int32_t assemble24(uint8_t b0, uint8_t b1, uint8_t b2)
{
    int32_t v = static_cast<int32_t>(b0)
              | (static_cast<int32_t>(b1) << 8)
              | (static_cast<int32_t>(b2) << 16);

    // Sign-extend from 24 bits to 32: if bit 23 is set, the value is
    // negative, so fill the top 8 bits with ones.
    if (v & 0x800000)
        v |= static_cast<int32_t>(0xFF000000);

    return v;
}

float s = static_cast<float>(assemble24(b0, b1, b2)) / 8388608.0f;   // 2^23
```

**The sign extension is the trap.** Without it, a sample of −1 (stored as `0xFFFFFF`) reads as
+16,777,215 — a full-scale positive value where there should be a nearly-silent negative one. The
symptom is distinctive: 24-bit files that play as harsh noise with roughly the right rhythm.

An alternative trick that avoids the branch: shift left by 8 into a full `int32_t`, then
arithmetic-shift right by 8, letting the hardware sign-extend for you.

```cpp
int32_t v = (static_cast<int32_t>(b0) <<  8)
          | (static_cast<int32_t>(b1) << 16)
          | (static_cast<int32_t>(b2) << 24);
float s = static_cast<float>(v >> 8) / 8388608.0f;
```

### 32-bit float

```cpp
float s = value;      // already in -1..+1 (nominally)
```

No conversion at all — the bytes are an IEEE 754 `float`, which you read directly. But note
**"nominally"**: float WAV files are allowed to exceed ±1.0, and files exported from a DAW with
an over will. Decide whether to clamp on load (safe) or preserve (faithful). This reader
preserves, and documents that it does.

### 32-bit integer

```cpp
float s = static_cast<float>(value) / 2147483648.0;    // 2^31
```

Note the intermediate must be `double` or you lose precision in the division — `float` cannot
represent 2^31 exactly enough for the low bits to survive.

---

## 16.4 The reader

**Code — `code/ch16/wavreader.h`**

```cpp
#pragma once

#include <string>
#include <vector>
#include <cstdint>

struct WavFile
{
    std::vector<std::vector<float>> channels;   // planar: channels[c][frame]
    int    sampleRate    = 44100;
    int    numChannels   = 0;
    int    bitsPerSample = 0;
    int    audioFormat   = 0;                   // 1 = PCM, 3 = float, 0xFFFE = extensible
    size_t numFrames     = 0;

    double durationSeconds() const
    {
        return sampleRate > 0
             ? static_cast<double>(numFrames) / static_cast<double>(sampleRate)
             : 0.0;
    }

    bool isValid() const { return numChannels > 0 && numFrames > 0; }
};

class WavReader
{
public:
    // Reads a WAV file into planar float channels.
    // On failure, returns a WavFile with isValid() == false and sets `error`.
    static WavFile read(const std::string& path, std::string& error);

    // Convenience overload that ignores the error message.
    static WavFile read(const std::string& path)
    {
        std::string ignored;
        return read(path, ignored);
    }

    // Reads only the header and prints every field with its offset.
    // Invaluable for diagnosing files that will not load.
    static bool printInfo(const std::string& path);
};
```

**Code — `code/ch16/wavreader.cpp`** (core of it; full listing in `code/`)

```cpp
#include "wavreader.h"

#include <fstream>
#include <iostream>
#include <iomanip>
#include <cstring>

namespace
{
    uint16_t readU16LE(std::istream& in)
    {
        uint8_t b[2];
        in.read(reinterpret_cast<char*>(b), 2);
        return static_cast<uint16_t>(b[0] | (b[1] << 8));
    }

    uint32_t readU32LE(std::istream& in)
    {
        uint8_t b[4];
        in.read(reinterpret_cast<char*>(b), 4);
        return static_cast<uint32_t>(b[0])
             | (static_cast<uint32_t>(b[1]) <<  8)
             | (static_cast<uint32_t>(b[2]) << 16)
             | (static_cast<uint32_t>(b[3]) << 24);
    }

    bool readTag(std::istream& in, char out[5])
    {
        in.read(out, 4);
        out[4] = '\0';
        return in.good();
    }
}

WavFile WavReader::read(const std::string& path, std::string& error)
{
    WavFile result;
    error.clear();

    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        error = "Cannot open file: " + path;
        return result;
    }

    // ---- RIFF / WAVE ----
    char tag[5];
    if (!readTag(in, tag) || std::strcmp(tag, "RIFF") != 0)
    {
        error = "Not a RIFF file (found '" + std::string(tag) + "')";
        return result;
    }

    readU32LE(in);                          // RIFF size -- read and ignore

    if (!readTag(in, tag) || std::strcmp(tag, "WAVE") != 0)
    {
        error = "Not a WAVE file (found '" + std::string(tag) + "')";
        return result;
    }

    // ---- walk the chunks ----
    bool haveFmt = false;
    std::streampos dataPos = 0;
    uint32_t       dataSize = 0;
    uint16_t       blockAlign = 0;

    while (in.good())
    {
        if (!readTag(in, tag))
            break;                          // clean end of file

        const uint32_t chunkSize = readU32LE(in);
        const std::streampos chunkStart = in.tellg();

        if (std::strcmp(tag, "fmt ") == 0)
        {
            result.audioFormat   = readU16LE(in);
            result.numChannels   = readU16LE(in);
            result.sampleRate    = static_cast<int>(readU32LE(in));
            readU32LE(in);                  // byte rate (derivable; ignore)
            blockAlign           = readU16LE(in);
            result.bitsPerSample = readU16LE(in);

            // WAVE_FORMAT_EXTENSIBLE: the real format is in the GUID.
            if (result.audioFormat == 0xFFFE && chunkSize >= 40)
            {
                readU16LE(in);              // cbSize
                readU16LE(in);              // valid bits per sample
                readU32LE(in);              // channel mask
                // First two bytes of the GUID hold the actual format tag.
                result.audioFormat = readU16LE(in);
            }

            haveFmt = true;
        }
        else if (std::strcmp(tag, "data") == 0)
        {
            dataPos  = chunkStart;
            dataSize = chunkSize;
        }
        // Every other chunk is skipped.

        // Seek past this chunk, honouring the pad byte for odd sizes.
        std::streamoff advance = static_cast<std::streamoff>(chunkSize);
        if (chunkSize % 2 != 0)
            advance += 1;

        in.clear();                          // clear any eof from a short chunk
        in.seekg(chunkStart + advance);

        if (in.peek() == EOF)
            break;
    }

    if (!haveFmt)  { error = "No 'fmt ' chunk found";  return result; }
    if (dataSize == 0 && dataPos == std::streampos(0))
    {
        error = "No 'data' chunk found";
        return result;
    }

    // Some recorders write 0xFFFFFFFF meaning "to end of file".
    if (dataSize == 0xFFFFFFFFu)
    {
        in.clear();
        in.seekg(0, std::ios::end);
        dataSize = static_cast<uint32_t>(in.tellg() - dataPos);
    }

    // ---- read the samples ----
    const int bytesPerSample = result.bitsPerSample / 8;
    if (bytesPerSample <= 0 || result.numChannels <= 0)
    {
        error = "Invalid format: " + std::to_string(result.bitsPerSample)
              + " bits, " + std::to_string(result.numChannels) + " channels";
        return result;
    }

    if (blockAlign == 0)
        blockAlign = static_cast<uint16_t>(bytesPerSample * result.numChannels);

    result.numFrames = dataSize / blockAlign;

    result.channels.assign(static_cast<size_t>(result.numChannels),
                           std::vector<float>(result.numFrames, 0.0f));

    in.clear();
    in.seekg(dataPos);

    std::vector<uint8_t> raw(dataSize);
    in.read(reinterpret_cast<char*>(raw.data()), dataSize);

    const size_t actuallyRead = static_cast<size_t>(in.gcount());
    if (actuallyRead < dataSize)
    {
        // Truncated file: keep what we got rather than failing outright.
        result.numFrames = actuallyRead / blockAlign;
        for (auto& ch : result.channels)
            ch.resize(result.numFrames);
    }

    for (size_t frame = 0; frame < result.numFrames; ++frame)
    {
        for (int c = 0; c < result.numChannels; ++c)
        {
            const size_t offset = frame * blockAlign
                                + static_cast<size_t>(c * bytesPerSample);

            float s = 0.0f;

            if (result.audioFormat == 3)                     // IEEE float
            {
                if (result.bitsPerSample == 32)
                {
                    float f;
                    std::memcpy(&f, &raw[offset], 4);
                    s = f;
                }
                else if (result.bitsPerSample == 64)
                {
                    double d;
                    std::memcpy(&d, &raw[offset], 8);
                    s = static_cast<float>(d);
                }
            }
            else                                             // integer PCM
            {
                switch (result.bitsPerSample)
                {
                    case 8:
                        s = (static_cast<float>(raw[offset]) - 128.0f) / 128.0f;
                        break;

                    case 16:
                    {
                        const int16_t v = static_cast<int16_t>(
                            raw[offset] | (raw[offset + 1] << 8));
                        s = static_cast<float>(v) / 32768.0f;
                        break;
                    }

                    case 24:
                    {
                        // Shift into the top 24 bits, then arithmetic-shift
                        // back down so the hardware sign-extends for us.
                        const int32_t v =
                              (static_cast<int32_t>(raw[offset    ]) <<  8)
                            | (static_cast<int32_t>(raw[offset + 1]) << 16)
                            | (static_cast<int32_t>(raw[offset + 2]) << 24);
                        s = static_cast<float>(v >> 8) / 8388608.0f;
                        break;
                    }

                    case 32:
                    {
                        int32_t v;
                        std::memcpy(&v, &raw[offset], 4);
                        s = static_cast<float>(
                                static_cast<double>(v) / 2147483648.0);
                        break;
                    }

                    default:
                        error = "Unsupported bit depth: "
                              + std::to_string(result.bitsPerSample);
                        return WavFile{};
                }
            }

            result.channels[static_cast<size_t>(c)][frame] = s;
        }
    }

    return result;
}
```

### Walkthrough of the tricky parts

**`std::memcpy` for float reads.** You might expect
`float f = *reinterpret_cast<float*>(&raw[offset]);`. Do not do that. It violates C++'s **strict
aliasing** rules — the compiler is allowed to assume a `uint8_t*` and a `float*` never point at
the same memory, and with `-O2` it may reorder or eliminate the read. It also breaks on platforms
that require aligned access, since `offset` may not be a multiple of 4.

`std::memcpy` is the portable, standards-compliant way to reinterpret bytes, and every compiler
optimises it to a single load. This is one of the few places where the "obvious" code is
genuinely wrong.

**`in.clear()` before every `seekg`.** Stream operations set error flags, and once `eofbit` is
set, `seekg` on some implementations silently does nothing. Clearing first is defensive and
costs nothing. Forgetting it produces a reader that works on small files and hangs or misreads on
large ones.

**`in.peek() == EOF`** is the loop terminator rather than the declared RIFF size, for the reasons
in §16.2. `peek` looks at the next byte without consuming it.

**Truncated files are salvaged, not rejected.** If a recording was interrupted, the `data` chunk
declares more bytes than exist. Reporting "corrupt file" and returning nothing is technically
correct and practically useless — the audio up to the truncation point is perfectly good. This is
a judgement call worth making explicitly, and worth documenting.

**Error reporting by `std::string&` rather than exceptions.** Both are valid. We use an out
parameter because it keeps the calling code simple and because, later in Part V, we will want the
same style of code inside an audio callback where exceptions are forbidden. Being consistent
avoids two styles in one codebase.

---

## 16.5 A `wavinfo` tool

`printInfo` walks the chunks and reports everything, without loading any audio. This is the tool
you reach for when a file will not open.

```bash
./wavinfo.exe somefile.wav
```

```
File: somefile.wav   (5,292,044 bytes)

  offset       id      size   notes
  ------------------------------------------------------------------
       0    RIFF   5292036   form type: WAVE
      12    fmt         16   PCM, 2 ch, 44100 Hz, 16 bit
      36    LIST       102   INFO
     146    data   5292000   132300 frames, 3.000 s
  ------------------------------------------------------------------

  Format         : PCM integer
  Channels       : 2
  Sample rate    : 44100 Hz
  Bit depth      : 16
  Frames         : 132300
  Duration       : 3.000 s
  Data starts at : offset 146       <-- NOT 44
  Byte rate      : 176400 (matches computed)
  Block align    : 4 (matches computed)

  Declared RIFF size : 5292036
  Actual file size   : 5292044  (= declared + 8)  OK
```

Note the `data` chunk starting at offset 146 because of the `LIST` chunk in between. A reader
that hard-codes 44 would read 102 bytes of metadata text as audio — which sounds like a burst of
noise at the start of the file. That is a real and recognisable symptom.

---

## 16.6 The round-trip test

The most valuable test you can write for file I/O: write a file, read it back, and check you got
what you put in.

```cpp
bool roundTripTest(double sampleRate, int channels)
{
    // 1. Generate known data.
    const size_t frames = 10000;
    std::vector<std::vector<float>> original(
        static_cast<size_t>(channels), std::vector<float>(frames));

    for (int c = 0; c < channels; ++c)
        for (size_t i = 0; i < frames; ++i)
            original[static_cast<size_t>(c)][i] =
                static_cast<float>(0.8 * std::sin(2.0 * kPi * (220.0 * (c + 1))
                                   * static_cast<double>(i) / sampleRate));

    // 2. Write it.
    WavWriter::writePlanar("roundtrip.wav", original,
                           static_cast<int>(sampleRate), false);

    // 3. Read it back.
    std::string err;
    WavFile loaded = WavReader::read("roundtrip.wav", err);

    if (!loaded.isValid())
    {
        std::cout << "  FAIL: " << err << "\n";
        return false;
    }

    // 4. Compare.
    if (loaded.numChannels != channels)  { std::cout << "  FAIL: channels\n";    return false; }
    if (loaded.sampleRate != static_cast<int>(sampleRate))
                                        { std::cout << "  FAIL: sample rate\n"; return false; }
    if (loaded.numFrames != frames)     { std::cout << "  FAIL: frame count\n"; return false; }

    double maxError = 0.0;
    for (int c = 0; c < channels; ++c)
        for (size_t i = 0; i < frames; ++i)
            maxError = std::max(maxError,
                std::fabs(static_cast<double>(original[static_cast<size_t>(c)][i])
                        - loaded.channels[static_cast<size_t>(c)][i]));

    std::cout << "  max error: " << maxError
              << "  (" << db::gainToDb(maxError) << " dBFS)\n";

    // 16-bit quantization means we expect an error up to about 1/32768.
    return maxError < 1.0 / 32000.0;
}
```

**Expected output**

```
  round trip mono   44100 Hz: max error: 3.05176e-05  (-90.31 dBFS)  PASS
  round trip stereo 44100 Hz: max error: 3.05176e-05  (-90.31 dBFS)  PASS
  round trip stereo 48000 Hz: max error: 3.05176e-05  (-90.31 dBFS)  PASS
```

**The error of 3.05e-05 is exactly `1/32768`** — one quantization step of 16-bit audio. This is
not a bug; it is the quantization error Chapter 4 predicted, and seeing exactly the predicted
value is strong evidence that both writer and reader are correct.

Its dB value, −90.3 dBFS, is close to the 16-bit noise floor figure of −96 dB. (The difference
is that −96 dB describes the *RMS* of the quantization noise, while we measured the *peak*
error of a single sample; peak is about 6 dB above RMS for a uniformly distributed error.)

> **If a round trip test shows an error of exactly 1.0, or 2.0**, you have a sign or polarity
> bug. **If it shows errors only on some samples**, you have an interleaving bug. **If every
> other sample is wrong**, you have a channel-count mismatch. The *pattern* of the error tells
> you which bug you have, which is why measuring the maximum alone is not enough — print the
> first ten mismatches too.

---

## 16.7 Sample rate: the thing the reader cannot fix

A reader tells you the file's sample rate. It does not make the file *match* your project.

If you load a 48 kHz file into a 44.1 kHz render and play it sample-for-sample, it plays **8.8%
too fast and too high** — about 1.5 semitones sharp. This is one of the most common bugs in
sample-playback code, and the symptom is distinctive: everything is slightly sharp and slightly
short.

Two correct responses:

1. **Resample** to the project rate on load. Chapter 28 builds this.
2. **Play back at a rate ratio**, computing `increment = fileRate / projectRate` and reading with
   interpolation — which is what a sampler does, and which Chapter 69 covers.

For now, the right thing is simply to **check and refuse**:

```cpp
if (loaded.sampleRate != projectSampleRate)
{
    std::cerr << "Warning: file is " << loaded.sampleRate
              << " Hz but project is " << projectSampleRate
              << " Hz. Playback will be "
              << (100.0 * loaded.sampleRate / projectSampleRate - 100.0)
              << "% off-speed.\n";
}
```

Silently ignoring a rate mismatch is the kind of bug that ships.

---

## 16.8 Other formats, briefly

You will eventually meet these. Knowing what they are saves confusion.

| Format | Notes |
|---|---|
| **AIFF** | Apple's equivalent. Same chunk idea, but **big-endian**, and the sample rate is stored as an 80-bit extended float. Chapter 8's byte-order helpers are what you need. |
| **RF64 / BW64** | WAV for files over 4 GB. Uses a `ds64` chunk with 64-bit sizes; the `RIFF` tag becomes `RF64`. Standard for long film recordings. |
| **CAF** | Apple's Core Audio Format. No 4 GB limit, flexible. |
| **FLAC** | Lossless compression, roughly 50–60% of WAV size. Bit-identical on decode. |
| **MP3 / AAC / Opus** | Lossy, perceptual (Chapter 3's masking). Decoding needs a library; do not write your own. |
| **Ogg Vorbis** | Lossy, open. |

For real projects, use **libsndfile** or **dr_libs** (single-header, public domain) rather than
writing readers for each. What you have built here is for understanding — and for the situations,
which do arise, where you need to handle a format quirk no library covers.

---

## 16.9 Exercises

**16.1** Run `wavinfo` on every `.wav` file you have produced so far. Confirm the data offsets are
all 44. Now find a file from elsewhere — a sample library, a recorder, a DAW export — and compare.

**16.2** Extend `printInfo` to decode and print the contents of a `LIST`/`INFO` chunk (title,
artist, software, comment) and a `smpl` chunk (loop points).

**16.3** Write a program that loads a WAV, prints its peak, RMS, crest factor and DC offset using
Chapter 11's tools, and reports whether it clips.

**16.4** Write `wavcat`: a tool that concatenates several WAV files into one, with a configurable
crossfade. It must reject files with mismatched sample rates or channel counts, with a clear
error.

**16.5** *Deliberate breakage.* Take a working WAV and use a hex editor to change the `data` chunk
size to half its true value. What does your reader do? What does a media player do? Now change it
to twice the true value.

**16.6** Remove the pad-byte handling (`if (chunkSize % 2 != 0) advance += 1;`) and construct a
file with an odd-sized `LIST` chunk. Confirm the reader then fails, and explain exactly where it
goes wrong.

**16.7** Add 24-bit **writing** to `WavWriter`, then round-trip test it. Verify the maximum error
is about `1/8388608`.

**16.8** Write a reader for AIFF. You will need big-endian helpers from Chapter 8 and a routine to
decode the 80-bit extended-precision sample rate. (This is a genuinely fiddly exercise and a good
test of Chapter 8's material.)

**16.9** Build a `wavdiff` tool that loads two files and reports the maximum and RMS difference in
dB, plus the sample index of the largest discrepancy. You will use this constantly to verify that
a DSP change did what you expected.

---

### Chapter summary

- **Never assume offsets.** Walk the chunk list: read ID, read size, handle or skip, seek
  forward, repeat. Professional files routinely have `bext`, `iXML` or `LIST` chunks before the
  audio.
- **Handle the pad byte** after odd-sized chunks, or everything after them misaligns.
- **Do not trust the declared RIFF size**; use end-of-file. Handle `0xFFFFFFFF` data sizes and
  truncated files gracefully.
- Conversion traps: **8-bit WAV is unsigned**; **24-bit needs sign extension**; use
  `std::memcpy` (not `reinterpret_cast`) to read floats from a byte buffer, because of strict
  aliasing and alignment.
- Always `in.clear()` before `seekg`.
- **Round-trip test everything.** A maximum error of exactly `1/32768` proves the 16-bit path is
  correct; the *pattern* of the error identifies which bug you have when it is not.
- A reader reports the sample rate; it cannot fix a mismatch. Check it, warn loudly, and
  resample (Chapter 28) or play at a ratio (Chapter 69).

**Next:** [Chapter 17 — Building `libaudio`: Your Own Audio Library](17-building-libaudio.md)
