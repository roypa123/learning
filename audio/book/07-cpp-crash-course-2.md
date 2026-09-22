# Chapter 7 — C++ Crash Course, Part 2: Memory, Vectors, and Structs

> Chapter 6 gave you numbers. This chapter gives you **collections of numbers**, which is what
> audio actually is. By the end you will have the `AudioBuffer` type that the rest of the book
> is built on, and you will know how to debug it when it misbehaves.

---

## 7.1 Where variables live: the stack and the heap

Your program has two regions of memory with very different properties. Audio programming forces
you to care about the difference, so learn it now rather than in Chapter 59 when it becomes a
real-time safety issue.

### The stack

```cpp
void myFunction()
{
    int   counter = 0;       // lives on the stack
    double phase  = 0.0;     // lives on the stack
    float  temp[64];         // lives on the stack (256 bytes)
}                            // <- all three vanish here, automatically
```

The stack is a region that grows and shrinks as functions are called and return. Every local
variable lives there.

**Properties:**
- **Extremely fast.** Allocating is one instruction — move a pointer.
- **Automatic cleanup.** When the function returns, everything is gone. No leaks possible.
- **Small.** Typically 1 MB on Windows, 8 MB on Linux, per thread.
- **Size must be known at compile time.**

That last pair is the catch. One second of stereo audio at 44.1 kHz is 352,800 bytes as
`float` — a third of your entire Windows stack. Ten seconds would not fit at all:

```cpp
float buffer[44100 * 10];    // 1.7 MB on the stack -> STACK OVERFLOW, instant crash
```

And note that a stack overflow does not politely report an error; the program dies, often with
no useful message.

### The heap

The heap is a large, general-purpose pool of memory — effectively all of your RAM. You request a
block, use it, and return it.

**Properties:**
- **Large.** Gigabytes.
- **Size can be decided at runtime.**
- **Slower.** An allocation may take hundreds of nanoseconds to microseconds, and it can
  occasionally take *much* longer if the allocator has to ask the operating system for more.
- **Must be managed.** Forget to release and you leak; release twice and you corrupt.

In older C++ you managed the heap by hand with `new` and `delete`. Modern C++ does it for you
with containers — above all, `std::vector`.

> **Look ahead to Chapter 59.** In real-time audio, heap allocation is *forbidden* inside the
> audio callback, because its timing is unpredictable and it may block on a lock inside the
> allocator. Miss your deadline and the user hears a click. The discipline is: allocate all your
> buffers up front, on the heap, before audio starts — then never allocate again. Everything in
> this chapter is compatible with that discipline as long as you resize buffers *outside* the
> callback.

---

## 7.2 `std::vector`: the audio buffer

`std::vector<T>` is a resizable array that manages heap memory for you. **It is the default
container for audio data in this book and in most real audio software outside the callback.**

```cpp
#include <vector>

std::vector<float> buffer;                    // empty
std::vector<float> buffer(44100);             // 44100 samples, all zero
std::vector<float> buffer(44100, 0.5f);       // 44100 samples, all 0.5
std::vector<float> small = {0.0f, 0.5f, 1.0f, 0.5f};   // four specific values
```

> **Careful with the parentheses.** `std::vector<float> v(3, 0.5f)` makes three elements of
> value 0.5. But `std::vector<int> v{3, 5}` (braces) makes *two* elements, 3 and 5, not three
> elements of value 5. This inconsistency is a known wart of the language. For audio buffers,
> always use parentheses: `std::vector<float> v(numSamples)`.

### The operations you need

```cpp
std::vector<float> buf(1000);

buf.size();              // 1000 -- returns size_t
buf[0] = 0.5f;           // write (no bounds check -- fast)
float x = buf[999];      // read
buf.at(1000);            // THROWS an exception -- bounds-checked
buf.resize(2000);        // grow; new elements are zero; may reallocate and copy
buf.reserve(5000);       // pre-allocate capacity without changing size
buf.push_back(0.1f);     // append one element, growing if needed
buf.clear();             // size becomes 0 (capacity is kept)
buf.empty();             // true if size() == 0
buf.data();              // raw pointer to the first element -- for C APIs and file I/O
buf.front(); buf.back(); // first and last elements
```

**`[]` versus `.at()`.** `[]` does no bounds checking and is as fast as a C array. `.at()`
checks and throws `std::out_of_range` if you are past the end.

The practical policy, and the one this book follows: **use `.at()` while developing a new piece
of DSP, then switch to `[]` once it is correct.** An out-of-bounds write with `[]` silently
corrupts memory and produces a bug that appears somewhere else entirely, often minutes later;
`.at()` tells you the exact line. Better still, use `[]` and build with
`-fsanitize=address` during development (see §7.10), which catches the same errors at nearly
full speed.

### The cost of growth

`push_back` on a full vector allocates a bigger block (typically double), copies everything
over, and frees the old one. Amortised over many pushes it is cheap, but any individual push may
be expensive, and the copy invalidates pointers into the vector.

For audio this means:

```cpp
// SLOW: repeatedly reallocates while filling a 10-second buffer
std::vector<float> buf;
for (int i = 0; i < 441000; ++i)
    buf.push_back(computeSample());

// FAST: one allocation, then plain writes
std::vector<float> buf(441000);
for (int i = 0; i < 441000; ++i)
    buf[i] = computeSample();

// ALSO FINE: reserve, then push_back
std::vector<float> buf;
buf.reserve(441000);
for (int i = 0; i < 441000; ++i)
    buf.push_back(computeSample());
```

The second form is what you will write almost everywhere in this book, because you nearly always
know the sample count in advance.

### The signed/unsigned loop wart

```cpp
for (int i = 0; i < buf.size(); ++i)      // warning: comparison of int with size_t
```

`size()` returns `size_t`, which is unsigned. Comparing signed with unsigned triggers a warning,
and in the worst case a subtraction that should be −1 becomes 18 quintillion and your loop runs
forever. Three clean fixes:

```cpp
for (size_t i = 0; i < buf.size(); ++i)                    // use size_t
for (int i = 0; i < static_cast<int>(buf.size()); ++i)     // cast once
for (float& s : buf)                                       // range-for, when you don't need i
```

This book uses `size_t` for container indices and `int` for sample counts that come from audio
APIs (which use `int`), converting explicitly at the boundary.

---

## 7.3 `std::array` and C-style arrays

`std::vector` allocates on the heap. Sometimes you want a fixed-size block on the stack — small,
fast, no allocation, and therefore **safe inside a real-time callback**.

```cpp
#include <array>

std::array<float, 4> coeffs = {1.0f, 0.5f, 0.25f, 0.125f};
std::array<double, 5> filterState{};   // {} zero-initialises -- do not omit it
```

`std::array` knows its own size, works with range-for, has `.at()`, and does not decay to a
pointer when passed around. Use it for filter coefficients, small lookup tables, and per-voice
state.

C-style arrays still appear in older code and in the WAV header work of Chapter 9:

```cpp
float buffer[256];             // 256 floats on the stack, UNINITIALISED (garbage!)
float buffer[256] = {};        // zero-initialised
char  chunkId[4] = {'R','I','F','F'};
```

Two dangers with C arrays: they contain garbage unless you initialise them, and they "decay" to
a bare pointer when passed to a function, losing their size. Prefer `std::array` and
`std::vector`.

> **Uninitialised memory in audio is not a subtle bug.** Garbage bytes interpreted as `float`
> samples are typically enormous values or NaN. Played back, that is a full-scale noise burst.
> Always initialise. This is another reason for `std::vector`, which zero-fills by default.

---

## 7.4 References and pointers

### References

A **reference** is another name for an existing variable. Declared with `&`:

```cpp
float sample = 0.5f;
float& ref   = sample;    // ref IS sample, not a copy
ref = 0.8f;               // sample is now 0.8
```

References must be bound when created and can never be re-bound. Their main use is parameter
passing:

```cpp
void applyGain(std::vector<float>& buffer, float gain)   // can modify the caller's buffer
{
    for (float& s : buffer)
        s *= gain;
}

float computeRms(const std::vector<float>& buffer)       // read-only, no copy
{
    // ...
}
```

The second form — `const T&` — is the single most common parameter type in C++ audio code. It
says two things at once: *I will not copy this* and *I will not modify it*. Both are guarantees
the compiler enforces.

### Pointers

A **pointer** holds a memory address. Declared with `*`, dereferenced with `*`, address taken
with `&`:

```cpp
float  sample = 0.5f;
float* ptr    = &sample;   // ptr holds the ADDRESS of sample
*ptr          = 0.8f;      // write through the pointer; sample is now 0.8
std::cout << *ptr;         // read through it: 0.8
```

Pointers can be null (`nullptr`), can be reassigned, and can be advanced through memory
(`ptr + 1` moves forward one `float`). That flexibility makes them essential for interfacing
with C APIs — and every audio hardware API is a C API:

```cpp
// A typical audio callback signature you will meet in Chapter 58
void audioCallback(float* output, const float* input, int numFrames);
```

Here `output` points at the first sample of a block the sound card owns. You write
`output[0] .. output[numFrames*channels - 1]`. There is no `size()`; the count comes as a
separate argument, and getting it wrong corrupts memory.

**Guidance for this book:** use references and `std::vector` for your own code; use pointers only
at API boundaries, and always alongside an explicit count. `buffer.data()` gives you the pointer
when a C API needs one:

```cpp
file.write(reinterpret_cast<const char*>(samples.data()),
           samples.size() * sizeof(int16_t));
```

That line appears in Chapter 9, and it is why pointers are in this chapter.

---

## 7.5 `const` correctness

`const` means "this will not change". Apply it aggressively:

```cpp
const double sampleRate = 44100.0;                  // the value never changes
void process(const std::vector<float>& in);         // the function won't modify `in`
float getGain() const;                              // this method doesn't modify the object
```

Three concrete benefits, all of which matter in audio:

1. **Documentation the compiler enforces.** `const std::vector<float>& input` tells a reader,
   correctly and permanently, that this is the input.
2. **Bugs caught at compile time.** Accidentally writing to the input buffer is one of the most
   confusing bugs in DSP — it corrupts data for every later stage. `const` makes it a compile
   error.
3. **Better optimisation.** The compiler can keep values in registers when it knows nothing can
   change them.

Adopt the habit now: make everything `const` unless you have a reason not to.

---

## 7.6 Structs: grouping data

A **struct** bundles related values into one type.

```cpp
struct AudioFormat
{
    int    sampleRate = 44100;
    int    numChannels = 1;
    int    bitsPerSample = 16;
};

AudioFormat fmt;                 // uses the defaults
fmt.sampleRate = 48000;          // access members with .
std::cout << fmt.numChannels;
```

The `= 44100` parts are **default member initialisers** (C++11 onward). They mean you can never
accidentally get an uninitialised field — worth using everywhere.

Structs can contain functions too:

```cpp
struct Oscillator
{
    double phase = 0.0;
    double phaseIncrement = 0.0;

    void setFrequency(double freqHz, double sampleRate)
    {
        phaseIncrement = 2.0 * 3.14159265358979323846 * freqHz / sampleRate;
    }

    float nextSample()
    {
        const float out = static_cast<float>(std::sin(phase));
        phase += phaseIncrement;
        if (phase >= 2.0 * 3.14159265358979323846)
            phase -= 2.0 * 3.14159265358979323846;
        return out;
    }
};
```

Used like this:

```cpp
Oscillator osc;
osc.setFrequency(440.0, 44100.0);
for (int i = 0; i < numSamples; ++i)
    buffer[i] = osc.nextSample();
```

**This is the shape of nearly every DSP object in this book.** State as members, a `setSomething`
to configure it, a `nextSample()` (or `process(buffer)`) to run it. Learn this pattern; Chapters
30 through 55 are variations on it.

Notice what the struct bought us: the phase is no longer a loose variable in `main` that could be
confused with another oscillator's phase. Two oscillators are simply two objects, each with its
own state. That is the whole argument for structs, and it becomes decisive when you have sixteen
voices with four oscillators each.

### `struct` vs `class`

In C++ they are the same thing with one difference: `struct` members are **public** by default,
`class` members are **private**. Convention:

- `struct` for plain data bundles and simple DSP components.
- `class` when you want to hide internals behind an interface.

```cpp
class Compressor
{
public:
    void  setThreshold(float db);
    float processSample(float input);

private:
    float threshold = 0.0f;
    float envelope  = 0.0f;   // internal state; callers must not touch it
};
```

`private` prevents outside code from corrupting internal state — here, setting `envelope`
directly would break the compressor's behaviour in a way that is very hard to diagnose.

---

## 7.7 Constructors and RAII

A **constructor** runs automatically when an object is created:

```cpp
class DelayLine
{
public:
    DelayLine(int maxDelaySamples)
        : buffer(maxDelaySamples, 0.0f), writeIndex(0)
    {
        // buffer is already allocated and zeroed by the initialiser list above
    }

    float process(float input, int delaySamples);

private:
    std::vector<float> buffer;
    int writeIndex;
};
```

The `: buffer(...), writeIndex(0)` part is the **member initialiser list**. It constructs members
directly with the right values, which is both faster and safer than assigning inside the body.
Members are initialised in **declaration order**, not the order you write them in the list — a
detail that has caused real bugs, and one that `-Wall` warns about.

**RAII** — Resource Acquisition Is Initialisation — is the idea that an object acquires its
resources in its constructor and releases them in its destructor. `std::vector` does exactly
this: it allocates when constructed and frees when destroyed, automatically, including if an
exception is thrown.

The result is that well-written modern C++ has no `new`, no `delete`, and no memory leaks. You
will not write a single `delete` in this book.

---

## 7.8 The `AudioBuffer` type

Let us build the thing everything else uses. This is a first version; Chapter 17 turns it into a
proper library component with more features.

**Code — `code/ch07/audiobuffer.cpp`**

```cpp
#include <iostream>
#include <iomanip>
#include <vector>
#include <cmath>
#include <algorithm>

// A block of audio samples: one or more channels, stored planar
// (each channel is its own contiguous vector).
struct AudioBuffer
{
    std::vector<std::vector<float>> channels;
    double sampleRate = 44100.0;

    AudioBuffer(int numChannels, int numFrames, double rate)
        : channels(static_cast<size_t>(numChannels),
                   std::vector<float>(static_cast<size_t>(numFrames), 0.0f)),
          sampleRate(rate)
    {
    }

    int numChannels() const { return static_cast<int>(channels.size()); }
    int numFrames()   const { return channels.empty()
                                     ? 0
                                     : static_cast<int>(channels[0].size()); }

    double durationSeconds() const { return numFrames() / sampleRate; }

    // Direct access to one channel.
    std::vector<float>&       channel(int c)       { return channels[static_cast<size_t>(c)]; }
    const std::vector<float>& channel(int c) const { return channels[static_cast<size_t>(c)]; }

    void clear()
    {
        for (auto& ch : channels)
            std::fill(ch.begin(), ch.end(), 0.0f);
    }

    void applyGain(float gain)
    {
        for (auto& ch : channels)
            for (float& s : ch)
                s *= gain;
    }

    // Largest absolute sample value across all channels.
    float peak() const
    {
        float p = 0.0f;
        for (const auto& ch : channels)
            for (float s : ch)
                p = std::max(p, std::fabs(s));
        return p;
    }

    // Root mean square across all channels: correlates with perceived loudness.
    float rms() const
    {
        double sumOfSquares = 0.0;
        size_t count = 0;

        for (const auto& ch : channels)
            for (float s : ch)
            {
                sumOfSquares += static_cast<double>(s) * static_cast<double>(s);
                ++count;
            }

        if (count == 0)
            return 0.0f;

        return static_cast<float>(std::sqrt(sumOfSquares / static_cast<double>(count)));
    }

    // Scale so that the loudest sample sits at `targetPeak`.
    void normalise(float targetPeak = 0.99f)
    {
        const float p = peak();
        if (p > 0.0f)
            applyGain(targetPeak / p);
    }
};

int main()
{
    AudioBuffer buf(2, 44100, 44100.0);     // stereo, one second

    std::cout << "Channels : " << buf.numChannels() << "\n";
    std::cout << "Frames   : " << buf.numFrames() << "\n";
    std::cout << "Duration : " << buf.durationSeconds() << " s\n";
    std::cout << "Peak     : " << buf.peak() << "  (silent buffer)\n\n";

    // Fill the left channel with a 440 Hz sine at amplitude 0.5,
    // and the right channel with the same sine at amplitude 0.25.
    const double twoPi = 2.0 * 3.14159265358979323846;
    const double freq  = 440.0;
    const double inc   = twoPi * freq / buf.sampleRate;

    double phase = 0.0;
    for (int i = 0; i < buf.numFrames(); ++i)
    {
        const float s = static_cast<float>(std::sin(phase));
        buf.channel(0)[static_cast<size_t>(i)] = s * 0.5f;
        buf.channel(1)[static_cast<size_t>(i)] = s * 0.25f;

        phase += inc;
        if (phase >= twoPi)
            phase -= twoPi;
    }

    std::cout << std::fixed << std::setprecision(4);
    std::cout << "After filling with a 440 Hz sine:\n";
    std::cout << "Peak     : " << buf.peak() << "\n";
    std::cout << "RMS      : " << buf.rms()  << "\n";
    std::cout << "Peak/RMS : " << buf.peak() / buf.rms() << "\n\n";

    buf.normalise();
    std::cout << "After normalise():\n";
    std::cout << "Peak     : " << buf.peak() << "\n";
    std::cout << "RMS      : " << buf.rms()  << "\n";

    return 0;
}
```

**Expected output**

```
Channels : 2
Frames   : 44100
Duration : 1 s
Peak     : 0  (silent buffer)

After filling with a 440 Hz sine:
Peak     : 0.5000
RMS      : 0.3953
Peak/RMS : 1.2649

After normalise():
Peak     : 0.9900
RMS      : 0.7827
```

**Walkthrough**

`std::vector<std::vector<float>> channels` — a vector of vectors, one per channel. This is the
**planar** layout from Chapter 4. It costs one extra pointer hop per channel, and in exchange
every DSP loop becomes a simple contiguous pass over one channel, which is both clearer and
faster to vectorise. Production engines often use a single flat vector with manual offsets to
avoid the double indirection; Chapter 17 discusses the trade-off.

The constructor's initialiser list is dense, so read it slowly:
`channels(numChannels, std::vector<float>(numFrames, 0.0f))` uses the "N copies of this value"
form of the vector constructor, where the value is itself a vector of `numFrames` zeros. One
line, fully allocated, fully zeroed.

`numChannels() const` — the trailing `const` promises the method does not modify the object,
which lets you call it on a `const AudioBuffer&`. Forgetting it is a very common cause of "passing
const ... discards qualifiers" errors.

`channel(int c)` exists in two versions, one `const` and one not. This pairing is a standard C++
idiom: the non-const one allows writing, the const one is what you get from a `const` buffer.

`peak()` uses `std::max` and `std::fabs`. Peak tells you how close you are to clipping.

`rms()` accumulates into a `double` even though the samples are `float`. This is deliberate and
important: summing 44,100 squared floats into a `float` accumulator loses precision badly once
the running total grows. **Accumulate in `double`.** The same rule applies to every sum over a
buffer you will write — convolution, FFT, loudness metering.

`normalise()` scales so the peak lands at 0.99 rather than 1.0. The margin exists because of
inter-sample peaks (Chapter 4): a reconstructed waveform can overshoot between samples, and
leaving a little room avoids clipping downstream.

**The Peak/RMS ratio of 1.2649 is worth noting.** For a pure sine, peak/RMS is exactly `√2 ≈
1.414`. We got 1.2649 because the buffer contains two channels at *different* amplitudes, and
RMS is computed across both. Change the right channel to also be 0.5 and you will get 1.4142.
This ratio is the **crest factor**, and Chapters 50 and 89 use it constantly: a high crest factor
means dynamic and punchy, a low one means dense and compressed.

**Experiment 7.1.** Change `buf.channel(1)` to use amplitude 0.5 as well. Predict the new
peak/RMS, then check.

**Experiment 7.2.** Change `rms()` to accumulate into a `float` instead of a `double`. With one
second of audio you may see no difference; try 60 seconds (`AudioBuffer buf(2, 44100*60, ...)`)
and compare the results. This is precision loss made audible — or at least visible.

**Experiment 7.3.** Call `buf.channel(5)` on a 2-channel buffer. What happens? Now change the
`channel()` methods to use `.at()` instead of `[]` and try again. Which failure would you rather
debug?

---

## 7.9 `std::string` and text

Briefly, since we need filenames:

```cpp
#include <string>

std::string path = "assets/sine440.wav";
std::string name = "sine";
std::string full = "assets/" + name + ".wav";       // concatenation with +
std::cout << full.size();                            // number of characters
std::string num = std::to_string(440);               // "440"
```

Pass strings as `const std::string&` for the usual reasons. In C++17 you may also see
`std::string_view`, a non-owning view that avoids copies; we use plain `std::string` for clarity.

---

## 7.10 Debugging: how to find out what is actually happening

Four techniques, in increasing order of power. You will use all of them.

### 1. Print

Crude, effective, and often fastest:

```cpp
std::cout << "i=" << i << " phase=" << phase << " sample=" << sample << "\n";
```

**Never print inside the full sample loop** — 44,100 lines per second will bury you and slow the
program by orders of magnitude. Print every N samples:

```cpp
if (i % 4410 == 0)
    std::cout << "t=" << i / 44100.0 << "s  value=" << buffer[i] << "\n";
```

### 2. Dump a buffer summary

A small function you will reuse constantly:

```cpp
void describe(const std::vector<float>& buf, const std::string& label)
{
    float mn = buf.empty() ? 0.0f : buf[0];
    float mx = mn;
    double sum = 0.0;
    int nanCount = 0;

    for (float s : buf)
    {
        if (std::isnan(s)) { ++nanCount; continue; }
        mn = std::min(mn, s);
        mx = std::max(mx, s);
        sum += s;
    }

    std::cout << label
              << ": n=" << buf.size()
              << " min=" << mn
              << " max=" << mx
              << " mean=" << (buf.empty() ? 0.0 : sum / buf.size())
              << " NaNs=" << nanCount << "\n";
}
```

Call it after every processing stage. Three things to look for:

- **`max` and `min` both 0** → your stage produced silence. Check for integer division.
- **`max` huge or `NaNs` nonzero** → your stage blew up. Check for division by zero or an
  unstable filter.
- **`mean` far from 0** → DC offset (Chapter 2). Usually a missing subtraction somewhere.

### 3. The debugger

With `launch.json` from Chapter 5, press <kbd>F5</kbd>. Click in the margin to set a breakpoint.
When execution stops:

- **Variables** pane shows every local, including vector contents (thanks to pretty-printing).
- **Watch** pane: type an expression like `buffer[i]` or `phase * 180 / 3.14159` to evaluate it
  live.
- <kbd>F10</kbd> steps over a line, <kbd>F11</kbd> steps into a function, <kbd>F5</kbd>
  continues.
- **Conditional breakpoints**: right-click a breakpoint and add a condition like `i == 22050`, so
  it fires exactly at the half-second mark. This is the single most useful debugger feature for
  audio, since you rarely care about the first sample.

Build with `-g -O0` when debugging. With `-O2`, the optimiser reorders and eliminates code, and
stepping becomes bewildering.

### 4. Sanitizers

The most powerful tool in this list, and the least known among beginners:

```bash
g++ -std=c++17 -Wall -Wextra -g -O1 -fsanitize=address,undefined prog.cpp -o prog.exe
```

**AddressSanitizer** detects out-of-bounds reads and writes, use-after-free, and leaks, and it
reports the exact line — for both the access and the original allocation.
**UndefinedBehaviorSanitizer** catches integer overflow, bad shifts, and misaligned access.

Programs run maybe 2× slower, which is irrelevant when you are rendering to a file. Turn it on
whenever a bug is mysterious.

> **Note for MSYS2 users:** ASan support on MinGW is patchy depending on your GCC build. If the
> flags are rejected, rely on `.at()` and the debugger instead — or build the same code under
> WSL, where sanitizers work flawlessly. It is worth the setup once you hit your first
> memory-corruption bug.

### 5. Render and listen

The audio-specific technique, and often the fastest of all. Write the buffer to a WAV file
(Chapter 9) and open it in a free editor like Audacity. You can then **see** the waveform and the
spectrum.

Bugs have visual signatures:

| What you see | What it means |
|---|---|
| Flat line | Silence — integer division, or never wrote to the buffer |
| Square-ish, flat tops | Clipping — gain too high |
| Waveform drifting off centre | DC offset |
| A vertical spike | A click — discontinuity, usually a buffer boundary or an envelope with no ramp |
| Regular ticks at exactly the buffer period | Off-by-one at block boundaries, or uninitialised state between blocks |
| Amplitude growing over time | Unstable feedback |
| Correct shape but wrong speed | Sample-rate mismatch or a frames/samples confusion |

Learning to read those signatures is worth a great deal, and it is faster than any debugger for
a whole class of problems.

---

## 7.11 Exercises

**7.1** Add a `void fadeIn(int numFrames)` method to `AudioBuffer` that ramps gain linearly from
0 to 1 over the first `numFrames` frames. Apply it to the sine buffer and check that the peak is
unchanged but the first sample is 0.

**7.2** Add `float peakChannel(int c) const` and `float rmsChannel(int c) const`. Use them to
print per-channel statistics.

**7.3** Add `void mixInto(AudioBuffer& dest, float gain) const` that adds this buffer's samples
into `dest`, scaled by `gain`. What should happen if the buffers have different channel counts or
lengths? Decide, document your decision in a comment, and implement it.

**7.4** *Deliberate breakage.* In `rms()`, change `sumOfSquares` to `float`. Render 5 minutes of
audio and compare the RMS against the `double` version. How large is the error? Now explain why
convolution (Chapter 21) would be affected far more severely.

**7.5** Write a function `std::vector<float> generateSine(double freq, double amplitude, double
seconds, double sampleRate)` that returns a new buffer. Should it return by value? (It should —
C++ moves the vector rather than copying it. Look up "return value optimisation" if you want the
details; the short version is that returning big vectors by value is free.)

**7.6** Write `bool hasNaN(const std::vector<float>& buf)` and `bool hasClipping(const
std::vector<float>& buf)`. These two functions belong in every audio project you ever write.

**7.7** Create a `std::vector<float>` of size 10, then deliberately write to index 10 using `[]`.
Does the program crash? Run it five times. Now use `.at()`. Now build with `-fsanitize=address`.
Write down the three different experiences — this is the most valuable exercise in the chapter.

---

### Chapter summary

- **Stack**: fast, automatic, small (~1 MB), compile-time size. **Heap**: large, runtime size,
  slower, managed by containers. Audio buffers go on the heap.
- `std::vector<float>` is the audio buffer. Size it up front (`std::vector<float> buf(n)`)
  rather than growing with `push_back` in a loop.
- Use `.at()` or sanitizers while developing; `[]` in released code. Out-of-bounds writes are
  the source of the most confusing audio bugs.
- Pass small types by value, everything else by `const T&`. Use `const` everywhere you can.
- `std::array` for small fixed-size state; it is real-time safe because it does not allocate.
- References are aliases; pointers are addresses. Use references in your code, pointers only at
  C API boundaries, always with an explicit count.
- Structs bundle state with behaviour. **Every DSP component in this book follows the pattern:
  state as members, `setX()` to configure, `process()` or `nextSample()` to run.**
- Accumulate sums in `double`, even when the samples are `float`.
- Debug with: periodic prints, a `describe()` buffer summary, the debugger with conditional
  breakpoints, sanitizers, and — most powerfully — rendering to a file and looking at the
  waveform.

**Part 0 is complete.** You know what sound is, how hearing works, how sound becomes numbers,
and enough C++ to manipulate those numbers. In the next chapter we start writing bytes to disk,
and two chapters after that you will hear your first sound.

**Next:** [Chapter 8 — Bytes, Binary Files, and Endianness](08-bytes-and-binary-files.md)
