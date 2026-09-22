# Chapter 6 — C++ Crash Course, Part 1: The Language

> If you have never programmed, read every word of this chapter and type every example.
> If you already program in another language, read §6.3 (types), §6.4 (the division trap),
> §6.9 (floating-point reality) and §6.11 — those are where audio-specific pain lives —
> and skim the rest.
>
> This is not a complete C++ course. It is the subset of C++ that audio code is written in,
> which is a surprisingly small and stable subset. We add to it as the book needs more.

---

## 6.1 What a program is

A program is a list of instructions executed in order, top to bottom, with two ways to deviate:
**branching** (do this *or* that, depending on a condition) and **looping** (do this repeatedly).
That is genuinely all of it. Everything else — functions, classes, templates — is organisation,
invented so that humans can manage large collections of those three things.

For audio, the shape of almost every program in this book is:

```
   1. Decide the parameters (sample rate, duration, frequency)
   2. Make room for the samples
   3. LOOP over every sample:
          compute one number
          store it
   4. Write the numbers to a file (or hand them to the sound card)
```

Step 3 is the interesting part, and it runs 44,100 times per second of audio. Hold that shape in
your head; when a chapter gets complicated, it is still that shape underneath.

---

## 6.2 The anatomy of a C++ file

```cpp
// 1. Comments: ignored by the compiler, for humans.
/* This form spans
   multiple lines. */

// 2. Includes: pull in code from elsewhere.
#include <iostream>
#include <cmath>

// 3. Constants and declarations available to the whole file.
const double kSampleRate = 44100.0;

// 4. Functions: named, reusable blocks of instructions.
double midiToFrequency(int noteNumber)
{
    return 440.0 * std::pow(2.0, (noteNumber - 69) / 12.0);
}

// 5. main(): where execution starts.
int main()
{
    std::cout << midiToFrequency(69) << "\n";
    return 0;
}
```

**Careful — that `midiToFrequency` has a bug.** `(noteNumber - 69) / 12` is integer division,
so for note 60 it computes `-9 / 12`, which is `0`, not `-0.75`. Section 6.4 explains this
properly. It is the single most common numeric bug in beginner audio code and I have put it here
deliberately so you meet it early. The fix is `/ 12.0`.

A few conventions used throughout this book:

- `kSampleRate` — a leading `k` marks a compile-time constant. Common in audio codebases.
- `camelCase` for variables and functions, `PascalCase` for types. This matches JUCE and most
  audio code you will read.
- Braces on their own line for functions, same line for control flow. Purely cosmetic; be
  consistent and your bugs become more visible.

---

## 6.3 Variables and types

A **variable** is a named box in memory holding a value. C++ is **statically typed**: you must
say what kind of value goes in the box, and it cannot change.

```cpp
int   numSamples = 44100;      // whole number
float sample     = 0.5f;       // 32-bit decimal
double phase     = 0.0;        // 64-bit decimal
bool  isPlaying  = true;       // true or false
char  letter     = 'A';        // a single character
```

### The types you will actually use

| Type | Size | Range / precision | Used in this book for |
|---|---|---|---|
| `int` | 4 bytes | ±2.1 billion | Counters, sample indices, MIDI notes, channel counts |
| `size_t` | 8 bytes | 0 to 1.8×10¹⁹ | Sizes and indices of containers (never negative) |
| `float` | 4 bytes | ~7 significant digits | **Sample data.** Buffers, audio signals |
| `double` | 8 bytes | ~16 significant digits | **Phase, frequency, coefficients, time** |
| `bool` | 1 byte | `true` / `false` | Flags, gate states |
| `int16_t` | 2 bytes | −32,768 … 32,767 | 16-bit PCM sample values in files |
| `uint32_t` | 4 bytes | 0 … 4.29 billion | WAV header fields |
| `uint8_t` | 1 byte | 0 … 255 | Raw bytes |

The fixed-width types (`int16_t`, `uint32_t`, `uint8_t`) come from `#include <cstdint>`. Use
them whenever the *exact* number of bits matters — which for file formats and hardware is
always. `int` is "whatever is natural on this machine", which is fine for counting and fatal for
a file header.

### Why `float` for samples and `double` for phase?

This split is not arbitrary and it is worth understanding now, because you will see it in every
audio codebase.

**Samples are `float`** because:
- 24 bits of mantissa gives ~144 dB of dynamic range, equal to 24-bit integer audio and far
  beyond any listening situation.
- Half the memory of `double`. A stereo minute is 21 MB instead of 42 MB, and audio is
  memory-bandwidth-bound more often than compute-bound.
- Twice as many fit in a SIMD register, so vectorised code is twice as fast (Chapter 63).

**Phase and coefficients are `double`** because:
- Phase **accumulates**. Add a small increment 44,100 times a second for ten minutes and you
  have done 26 million additions. With `float`, rounding error compounds and the oscillator
  drifts audibly flat or sharp. With `double`, the error stays negligible for years of runtime.
- Filter coefficients involve differences of nearly-equal numbers, where precision loss is
  severe (Chapter 24 shows a biquad that is stable in `double` and explodes in `float` at low
  cutoff frequencies).

> **Rule of thumb:** anything that accumulates over time, or is computed from a formula with
> subtractions of similar magnitudes, should be `double`. Bulk sample storage should be `float`.

### `auto`

C++ can infer a type:

```cpp
auto x = 0.5;     // double (0.5 is a double literal)
auto y = 0.5f;    // float
auto n = 100;     // int
```

`auto` is useful for long type names (Chapter 7 has some) and dangerous for numeric code,
because `auto x = 1 / 2;` silently gives you `int` `0`. In this book I use `auto` only where the
type is obvious from the right-hand side or absurdly long. Being explicit about numeric types is
worth the extra characters.

### Literal suffixes

```cpp
0.5      // double
0.5f     // float
5        // int
5u       // unsigned int
5L       // long
```

Writing `float gain = 0.5;` creates a `double` and converts it, which with `-Wall -Wextra`
sometimes warns and always wastes a hair of time. Write `0.5f` when you mean `float`.

---

## 6.4 The integer division trap

**This is the most important section in the chapter.** More beginner audio bugs come from this
than from anything else, and they are silent — no crash, no warning, just wrong sound.

In C++, `/` between two integers performs **integer division**: the result is an integer and the
remainder is discarded.

```cpp
int a = 7 / 2;          // 3, not 3.5
double b = 7 / 2;       // 3.0 !!! the division happened FIRST, as integers
double c = 7.0 / 2;     // 3.5  (one operand is double, so double division)
double d = 7 / 2.0;     // 3.5
double e = 7.0 / 2.0;   // 3.5  (clearest)
```

Line 2 is the killer. Making the *destination* a `double` does not help — the division is
already over by the time the assignment happens.

### What this does to audio

```cpp
// BROKEN: intended to be a fraction of the sample rate
double increment = 440 / 44100;          // = 0, silence
double increment = 440.0 / 44100.0;      // = 0.00997732, correct

// BROKEN: intended to convert samples to seconds
double seconds = numSamples / 44100;      // truncated to whole seconds
double seconds = numSamples / 44100.0;    // correct

// BROKEN: MIDI note to frequency
double f = 440.0 * std::pow(2.0, (note - 69) / 12);     // exponent is an int: 0, or -1, or 1
double f = 440.0 * std::pow(2.0, (note - 69) / 12.0);   // correct

// BROKEN: pan position from an index
double pan = i / numVoices;               // 0 for every voice but the last
double pan = static_cast<double>(i) / numVoices;   // correct
```

The symptom of the first one is **silence**. The symptom of the third is **notes snapping to
octaves**. The symptom of the fourth is **everything panned hard left**. If you see any of
those, look for an integer division before you look anywhere else.

### The defences

1. **Always write a decimal point** on floating-point literals in numeric expressions. `12.0`,
   not `12`. `2.0`, not `2`.
2. **Cast explicitly** when a variable is an integer: `static_cast<double>(i)`.
3. When you get an unexpected `0`, a truncated value, or silence, **check every `/` in the
   expression**.

`static_cast<T>(value)` is C++'s conversion syntax. It is deliberately ugly so that conversions
are visible in the code — that ugliness is a feature. You will also see the C style `(double)i`;
it works, but `static_cast` is searchable and cannot accidentally do something more dangerous.

---

## 6.5 Operators

**Arithmetic:** `+  -  *  /  %`

`%` is the **modulo** (remainder) operator, integers only:

```cpp
7 % 3       // 1
10 % 5      // 0
```

Modulo is everywhere in audio: wrapping a circular buffer index, wrapping phase, finding beat
positions, alternating channels.

```cpp
writeIndex = (writeIndex + 1) % bufferSize;   // wrap around a delay line
bool isLeftChannel = (i % 2 == 0);            // in an interleaved stereo buffer
```

For floating-point remainder use `std::fmod(a, b)` from `<cmath>`.

**Comparison:** `==  !=  <  >  <=  >=` — each yields a `bool`.

> **`=` is assignment; `==` is comparison.** `if (x = 5)` assigns 5 to x and is always true.
> Modern compilers warn about this with `-Wall`. Heed the warning.

**Logical:** `&&` (and), `||` (or), `!` (not).

```cpp
if (frequency > 0.0 && frequency < nyquist) { /* valid */ }
if (!isPlaying) { /* stopped */ }
```

These **short-circuit**: `a && b` does not evaluate `b` if `a` is false. That is useful for
guarding: `if (index < size && buffer[index] > 0.0f)` is safe even when `index` is out of range,
because the second test never runs.

**Compound assignment:** `+=  -=  *=  /=  %=`

```cpp
phase += increment;      // same as phase = phase + increment
gain  *= 0.5;            // halve it
```

You will write `phase += increment;` roughly ten thousand times over this book.

**Increment:** `++i` and `i++` both add one. In a `for` loop they are equivalent; prefer `++i`
out of habit, since for complex types the post-increment form has to make a copy.

### Precedence

`*` and `/` bind tighter than `+` and `-`, as in ordinary arithmetic. Comparison binds looser
than arithmetic. `&&` binds tighter than `||`.

**Use parentheses when you are not certain.** They cost nothing, and audio expressions like
`0.5 * (a + b) * gain` are far easier to verify with them than without.

---

## 6.6 Control flow

### `if` / `else if` / `else`

```cpp
if (sample > 1.0f)
{
    sample = 1.0f;
}
else if (sample < -1.0f)
{
    sample = -1.0f;
}
else
{
    // in range: nothing to do
}
```

That is a **hard clipper**, and it is a real audio processor — Chapter 14 uses it. Braces are
optional for single statements but always use them; omitting them is the source of a famous
class of bugs where an added line silently falls outside the branch.

### `while`

```cpp
while (phase >= twoPi)
{
    phase -= twoPi;
}
```

That is **phase wrapping**, which every oscillator does. `while` rather than `if` because, if the
frequency is very high, the phase might have advanced past `2π` more than once in one sample.
(Though at that point you are above Nyquist and have other problems — Chapter 30 discusses it.)

### `for` — the sample loop

The workhorse. C++'s classic `for` has three parts separated by semicolons:

```cpp
for (int i = 0; i < numSamples; ++i)
{
    buffer[i] = computeSample();
}
```

- `int i = 0` — **initialisation**, runs once before the loop.
- `i < numSamples` — **condition**, checked before each iteration; the loop stops when it is
  false.
- `++i` — **update**, runs after each iteration.

So: start at 0, run while `i` is less than `numSamples`, adding one each time. For
`numSamples = 5`, `i` takes the values 0, 1, 2, 3, 4. **Five iterations, and the last index is
4, not 5.**

> **The off-by-one.** Arrays in C++ are indexed from **0**. An array of 5 elements has valid
> indices 0, 1, 2, 3, 4. Writing to index 5 does not crash reliably — it corrupts whatever
> happens to be next in memory. In audio this is precisely how you get random clicks, mysterious
> crashes half a second later, and bugs that vanish when you add a print statement.
>
> `i < numSamples` is correct. `i <= numSamples` is a bug. Chapter 7 shows you `.at()`, which
> checks the bound and tells you, and how to use `-fsanitize=address` to catch the rest.

### Range-based `for`

When you just want every element and do not need the index:

```cpp
for (float& s : buffer)
{
    s *= 0.5f;          // halve every sample
}
```

The `&` means "a reference to the actual element", so modifying `s` modifies the buffer. Without
it you would be modifying a copy and the buffer would be unchanged — a silent no-op bug, and a
common one. Chapter 7 covers references properly.

### `switch`

For selecting among several fixed values:

```cpp
switch (waveform)
{
    case 0:  sample = sine(phase);     break;
    case 1:  sample = square(phase);   break;
    case 2:  sample = saw(phase);      break;
    default: sample = 0.0f;            break;
}
```

**`break` is mandatory.** Without it, execution "falls through" into the next case. This is
occasionally useful and usually a bug; compilers warn about it with `-Wextra`.

---

## 6.7 Functions

A function is a named, reusable block that optionally takes inputs and optionally returns an
output.

```cpp
double frequencyFromMidiNote(int noteNumber)
{
    return 440.0 * std::pow(2.0, (noteNumber - 69) / 12.0);
}
```

- `double` — the **return type**.
- `frequencyFromMidiNote` — the **name**. Name functions after what they produce, not how.
- `(int noteNumber)` — the **parameter list**: the inputs, with their types.
- `return ...;` — sends a value back and exits the function immediately.

A function that returns nothing has return type `void`:

```cpp
void printBufferStats(const std::vector<float>& buffer)
{
    // ... prints; returns nothing
}
```

### Declaration vs definition

C++ processes a file top to bottom, so a function must be **known** before it is called. Two
options:

**Define it above the caller** — simplest, works for small programs.

**Declare it above, define it below** — a *declaration* (also called a prototype) is the
signature with a semicolon instead of a body:

```cpp
double frequencyFromMidiNote(int noteNumber);      // declaration: "this exists"

int main()
{
    std::cout << frequencyFromMidiNote(69) << "\n";
    return 0;
}

double frequencyFromMidiNote(int noteNumber)       // definition: here is the body
{
    return 440.0 * std::pow(2.0, (noteNumber - 69) / 12.0);
}
```

This is exactly what **header files** do, at file scope: a `.h` holds declarations, a `.cpp`
holds definitions. Chapter 17 builds `libaudio` that way. The link errors from Chapter 5 happen
when a declaration exists but no definition does.

### Passing by value vs by reference

```cpp
void byValue(float x)        { x = 0.0f; }   // modifies a COPY; caller unaffected
void byReference(float& x)   { x = 0.0f; }   // modifies the CALLER'S variable
void byConstRef(const std::vector<float>& v) { /* read-only, no copy made */ }
```

For small types (`int`, `float`, `double`, `bool`) pass by value — it is as cheap as copying a
register.

For large types (vectors, strings, structs with arrays) pass by **const reference**. Passing a
`std::vector<float>` of a million samples by value copies four megabytes on every call. This is
a classic, invisible performance disaster: the program is correct and mysteriously slow.

> **The default rule for this book:** small built-in types by value; everything else by
> `const T&` if read-only, or `T&` if the function must modify it.

### Default arguments and overloading

```cpp
double dbToGain(double db);                      // one version
float  dbToGain(float db);                       // another, same name: OVERLOADING
void   writeWav(const std::string& path,
                const std::vector<float>& data,
                int sampleRate = 44100,          // DEFAULT ARGUMENT
                int channels   = 1);
```

Overloading lets the same name work for different types — the compiler picks by argument type.
Default arguments let callers omit trailing parameters. Both are used heavily by the audio
library we build.

---

## 6.8 A complete audio-flavoured program

Everything so far, applied. This program prints a table of note frequencies and their sample
periods — genuinely useful output, and a template for the loop structure of everything to come.

**Code — `code/ch06/notetable.cpp`**

```cpp
#include <iostream>
#include <iomanip>
#include <cmath>
#include <string>

const double kSampleRate = 44100.0;

// Convert a MIDI note number to frequency in hertz.
// MIDI note 69 is A4 = 440 Hz; each semitone is a factor of 2^(1/12).
double frequencyFromMidiNote(int noteNumber)
{
    return 440.0 * std::pow(2.0, (noteNumber - 69) / 12.0);
}

// Return the note name for a MIDI note number, e.g. 60 -> "C4".
std::string nameFromMidiNote(int noteNumber)
{
    const std::string names[12] = { "C",  "C#", "D",  "D#", "E",  "F",
                                    "F#", "G",  "G#", "A",  "A#", "B" };

    const int pitchClass = noteNumber % 12;         // 0..11, which letter
    const int octave     = (noteNumber / 12) - 1;   // MIDI 60 = C4

    return names[pitchClass] + std::to_string(octave);
}

// How many samples does one cycle of this frequency occupy?
double samplesPerCycle(double frequency)
{
    return kSampleRate / frequency;
}

int main()
{
    std::cout << std::fixed << std::setprecision(3);
    std::cout << " MIDI  Note    Frequency (Hz)   Samples/cycle   Period (ms)\n";
    std::cout << "-----------------------------------------------------------\n";

    for (int note = 21; note <= 108; note += 12)   // A0 to C8, one per octave
    {
        const double freq    = frequencyFromMidiNote(note);
        const double perCyc  = samplesPerCycle(freq);
        const double periodMs = 1000.0 / freq;

        std::cout << std::setw(5)  << note
                  << std::setw(7)  << nameFromMidiNote(note)
                  << std::setw(17) << freq
                  << std::setw(16) << perCyc
                  << std::setw(14) << periodMs
                  << "\n";
    }

    std::cout << "\nNyquist at " << kSampleRate << " Hz is "
              << kSampleRate / 2.0 << " Hz.\n";

    const int highest = 108;
    const double f    = frequencyFromMidiNote(highest);
    const int harmonicsBelowNyquist = static_cast<int>((kSampleRate / 2.0) / f);

    std::cout << "The top note (" << nameFromMidiNote(highest) << ", " << f
              << " Hz) has room for only " << harmonicsBelowNyquist
              << " harmonics below Nyquist.\n";

    return 0;
}
```

**Build and run**

```bash
g++ -std=c++17 -Wall -Wextra -O2 notetable.cpp -o notetable.exe
./notetable.exe
```

**Output**

```
 MIDI  Note    Frequency (Hz)   Samples/cycle   Period (ms)
-----------------------------------------------------------
   21     A0           27.500         1603.636        36.364
   33     A1           55.000          801.818        18.182
   45     A2          110.000          400.909         9.091
   57     A3          220.000          200.455         4.545
   69     A4          440.000          100.227         2.273
   81     A5          880.000           50.114         1.136
   93     A6         1760.000           25.057         0.568
  105     A7         3520.000           12.523         0.284

Nyquist at 44100.000 Hz is 22050.000 Hz.
The top note (C8, 4186.009 Hz) has room for only 5 harmonics below Nyquist.
```

**Walkthrough**

`frequencyFromMidiNote` implements equal temperament. Each semitone multiplies frequency by
`2^(1/12) ≈ 1.05946`; twelve of them multiply by 2, an octave. Note 69 is the anchor at 440 Hz.
Note the `12.0` — with `12` this function returns octave-quantised nonsense, as warned in §6.4.

`nameFromMidiNote` uses two integer operations you should internalise:

- `noteNumber % 12` gives the pitch class 0–11, because note names repeat every 12 semitones.
  This is modulo used as "wrap into a range", exactly as we will use it for circular buffers.
- `(noteNumber / 12) - 1` gives the octave. Here integer division is *deliberate* and correct —
  we want truncation. The `- 1` is because MIDI note 60 is called C4 by convention, and
  `60 / 12 = 5`.

`std::string names[12] = {...}` is a **fixed-size array**, covered properly in Chapter 7. Index
it with `names[pitchClass]`.

`std::setw(n)` sets the field width for the *next* item only — unlike `setprecision` and
`fixed`, which are sticky. That asymmetry catches everyone once.

The last block is the payoff and is worth dwelling on. At 4,186 Hz (the top note of a piano),
only 5 harmonics fit below Nyquist. A real piano note has dozens of audible harmonics. So a
digital system simply cannot represent a bright high note the way a bright low note is
represented — there is nowhere to put the harmonics. Any synthesis method that generates them
anyway will alias (Chapter 4). This is why band-limited oscillators (Chapter 31) matter most at
the top of the keyboard, and why naïve synths sound progressively worse as you play higher.

**Experiment 6.1.** Change the loop to `note += 1` and look at the whole chromatic range. Find
the note at which fewer than 10 harmonics fit below Nyquist. Predict first.

**Experiment 6.2.** Change `kSampleRate` to `96000.0` and re-run. How many harmonics does C8
have room for now? This is one honest argument for high sample rates.

**Experiment 6.3.** Break it on purpose: change `12.0` to `12` in `frequencyFromMidiNote`. What
do the frequencies become? Explain the pattern. Then put it back.

---

## 6.9 Floating-point reality

Some facts about `float` and `double` that will otherwise bite you.

### Numbers are not exact

```cpp
double x = 0.1 + 0.2;
std::cout << (x == 0.3);      // prints 0 -- FALSE
std::cout << std::setprecision(20) << x;   // 0.30000000000000004441
```

Binary floating point cannot represent 0.1 exactly, any more than decimal can represent 1/3.
Small errors are inevitable.

**Therefore: never compare floats with `==`.** Compare with a tolerance:

```cpp
if (std::fabs(a - b) < 1e-9) { /* close enough */ }
```

The exception is comparing against exactly `0.0` when you assigned exactly `0.0` — that is
reliable. But `if (phase == twoPi)` will essentially never be true, which is why we wrap with
`>=`.

### Special values

```cpp
double inf  = 1.0 / 0.0;        // infinity
double nan  = 0.0 / 0.0;        // NaN: "not a number"
```

**NaN is contagious.** Any arithmetic involving NaN produces NaN. One NaN entering a reverb's
feedback loop turns the entire tail to NaN, and NaN played through a DAC is usually **full-scale
noise or a loud click**. This is a genuine hearing-safety issue, which is why Chapter 3 told you
to keep a limiter on.

NaN also compares false against everything, including itself:

```cpp
if (x != x) { /* x is NaN -- the classic test */ }
```

Or use `std::isnan(x)` and `std::isfinite(x)` from `<cmath>`. Chapter 59 adds NaN guards to the
real-time path.

The usual sources of NaN in audio: dividing by zero, `std::sqrt` of a negative, `std::log` of
zero or negative, `std::asin` of something slightly above 1.0 from rounding, and unstable
filters that run away to infinity and then subtract infinities.

### Denormals

When numbers get extremely small (below about 10⁻³⁸ for `float`), CPUs switch to a slower
"denormal" representation to preserve precision near zero. The arithmetic can become **10 to 100
times slower**.

This matters specifically in audio, because reverb tails and filter states decay exponentially
toward zero. A reverb left running with no input drifts into denormal range and your CPU load
mysteriously *rises* during silence. This is a real, famous problem, and Chapter 59 gives you
the two standard fixes (flush-to-zero mode, or adding an inaudible tiny DC offset).

### Precision loss on accumulation

```cpp
float phase = 0.0f;
for (long i = 0; i < 100000000; ++i)
    phase += 0.001f;             // by the end, phase is badly wrong
```

Once `phase` is large, adding a small number loses the small number's low bits entirely. This is
precisely why phase accumulators are `double` **and** wrapped back into `[0, 2π)` every sample —
keeping the magnitude small keeps the relative precision high. Chapter 30 covers this properly.

---

## 6.10 The maths functions you will use

From `#include <cmath>`, all in namespace `std`:

| Function | Purpose | Chapter it first appears |
|---|---|---|
| `std::sin(x)`, `std::cos(x)` | Oscillators. **x is in radians**, not degrees | 10 |
| `std::tan(x)` | Filter coefficient design | 23 |
| `std::atan2(y, x)` | Angle of a point; phase of a complex number | 19 |
| `std::pow(base, exp)` | Exponentials, note-to-frequency, curves | 6 |
| `std::exp(x)` | `e^x` — envelopes, decay curves, filter maths | 13 |
| `std::log(x)` | Natural log | 13 |
| `std::log10(x)` | Base-10 log — **decibels** | 11 |
| `std::log2(x)` | Base-2 log — octaves, frequency-to-note | 66 |
| `std::sqrt(x)` | RMS, distance, constant-power panning | 11 |
| `std::fabs(x)` | Absolute value for floats (`std::abs` for ints) | 11 |
| `std::fmod(a, b)` | Floating-point remainder — phase wrapping | 30 |
| `std::floor(x)`, `std::ceil(x)` | Round down / up | 28 |
| `std::round(x)` | Round to nearest | 9 |
| `std::isnan(x)`, `std::isfinite(x)` | Safety checks | 59 |
| `std::min(a,b)`, `std::max(a,b)` | From `<algorithm>`; clamping | 14 |
| `std::clamp(v, lo, hi)` | From `<algorithm>`, C++17. Clamp in one call | 14 |

**Radians, not degrees.** A full circle is `2π ≈ 6.283185` radians, not 360. `std::sin(90)` does
*not* give 1.0 — it gives 0.894, because 90 radians is about 14 full turns plus a bit. To convert:
`radians = degrees * π / 180.0`.

There is no `π` in the C++ standard library before C++20's `std::numbers::pi`, so define your own:

```cpp
constexpr double kPi    = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;
```

`constexpr` means "computed at compile time" — a stronger promise than `const`, and free.

---

## 6.11 Ten audio-specific C++ pitfalls

A checklist. Come back to it when something sounds wrong.

1. **Integer division.** `440 / 44100` is 0. §6.4.
2. **Off-by-one in the sample loop.** `i <= numSamples` writes one past the end.
3. **Comparing floats with `==`.** Use a tolerance.
4. **Uninitialised variables.** `float x;` contains garbage, which as audio is a full-scale
   click or noise burst. Always initialise: `float x = 0.0f;`.
5. **Forgetting `&` in a range-for.** `for (float s : buf) s *= 0.5f;` modifies copies and does
   nothing.
6. **Passing big vectors by value.** Silent 4 MB copies per call.
7. **Degrees instead of radians.**
8. **Not wrapping phase.** It grows without bound, loses precision, and eventually produces
   garbage.
9. **Signed/unsigned comparison.** `for (int i = 0; i < v.size(); ++i)` warns because `size()`
   returns `size_t` (unsigned). Mixing them can make a loop run 4 billion times. Use
   `size_t i` or `static_cast<int>(v.size())`.
10. **Assuming `char` is signed.** It is implementation-defined. For raw bytes use `uint8_t`.

---

## 6.12 Exercises

**6.1** Write a function `double secondsToSamples(double seconds, double sampleRate)` and its
inverse. Test with 0.5 s at 44,100 Hz (expect 22,050) and 1,000 samples at 48,000 Hz.

**6.2** Write `double semitonesToRatio(double semitones)` returning the frequency multiplier for
a given number of semitones. Check: 12 semitones → 2.0, −12 → 0.5, 7 → ~1.4983.

**6.3** Write a function that, given a frequency and a sample rate, returns the number of
harmonics that fit below Nyquist. Use it to print a table for every A from A0 to A7.

**6.4** Write a hard clipper `float clip(float x, float threshold)` using `if`/`else`. Then
rewrite it as a one-liner with `std::clamp`. Confirm both give identical results for inputs
−2.0, −0.5, 0.0, 0.5, 2.0.

**6.5** Write a loop that prints the first 20 values of `phase` for a 1,000 Hz oscillator at
44,100 Hz, where `phase += twoPi * freq / sampleRate` each step, wrapping at `twoPi`. How many
samples pass before the phase wraps for the first time? Does that match `44100/1000`?

**6.6** *Deliberate breakage.* Take `notetable.cpp` and change `kSampleRate` to `8000.0`. Which
notes now have *zero* harmonics below Nyquist? What does that tell you about telephone audio?

**6.7** Write a function `bool isPowerOfTwo(int n)` using `%`. You will need it in Chapter 25,
where the FFT requires power-of-two sizes.

**6.8** Predict the output of each line, then check:
```cpp
std::cout << 7 / 2 << " " << 7 % 2 << " " << 7.0 / 2 << " "
          << -7 / 2 << " " << -7 % 2 << " " << std::fmod(7.5, 2.0);
```
The negative cases surprise people; write down why.

---

### Chapter summary

- A program is sequence, branching, and looping. Audio programs are a loop that computes one
  number per sample.
- Use `float` for sample data, `double` for phase, frequency, time and coefficients. Use
  fixed-width types (`int16_t`, `uint32_t`) whenever the exact bit count matters.
- **Integer division is the number one silent audio bug.** Always write `12.0`, not `12`.
- `for (int i = 0; i < n; ++i)` — less-than, not less-than-or-equal. Indices start at 0.
- Functions: declaration vs definition; pass small types by value, large ones by `const&`.
- Floating point is inexact: never compare with `==`; guard against NaN; keep accumulators
  small and `double`; be aware of denormals in decaying signals.
- Trig takes radians. Define your own `kPi`.
- Ten pitfalls in §6.11 — that list is worth re-reading whenever something sounds wrong.

**Next:** [Chapter 7 — C++ Crash Course, Part 2: Memory, Vectors, and Structs](07-cpp-crash-course-2.md)
