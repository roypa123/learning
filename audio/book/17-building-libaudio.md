# Chapter 17 — Building `libaudio`: Your Own Audio Library

> Sixteen chapters of code are scattered across sixteen folders, and Chapter 14's build line
> already reads `g++ mix.cpp ../ch09/wavwriter.cpp ...`. That does not scale. This chapter
> consolidates everything into one library, introduces CMake, and adds the two C++ features —
> virtual functions and a test harness — that the rest of the book depends on.
>
> This is the last chapter of Part I. When it is done, every subsequent chapter is
> `#include <audio/...>` and one build command.

---

## 17.1 What a library is, and why

A **library** is compiled code plus headers, packaged so other programs can use it without
recompiling it or knowing how it works.

Three kinds:

| Kind | Extension | How it works |
|---|---|---|
| **Header-only** | `.h` only | All code in headers. Simple; recompiled into every user. |
| **Static** | `.a` / `.lib` | Compiled once; copied into your executable at link time. |
| **Dynamic** | `.dll` / `.so` / `.dylib` | Loaded at run time; shared between programs. |

We build a **static library**. It is the right default for this book: one compile per change, no
DLL-deployment problems, and the linker discards anything you do not use.

The benefits are concrete and immediate:

- One place to fix a bug, instead of sixteen copies.
- Compile the library once; subsequent builds of example programs take a fraction of a second.
- A stable, documented API, which forces you to think about what each component's interface
  should be — a genuinely useful discipline.
- Tests can run against the library, so a change that breaks something is caught immediately.

---

## 17.2 The structure

```
audio/
  CMakeLists.txt              <- top-level build description
  lib/
    include/audio/            <- PUBLIC headers (what users include)
      audio.h                 <- convenience: includes everything
      types.h                 <- Sample, constants, AudioBuffer
      db.h                    <- decibels
      wav.h                   <- reader + writer
      osc.h                   <- oscillators
      envelope.h              <- ADSR
      noise.h                 <- noise generators
      filter.h                <- DC blocker (biquads arrive in Ch 23)
      processor.h             <- the Processor interface
    src/                      <- IMPLEMENTATION (.cpp)
      wav_writer.cpp
      wav_reader.cpp
      osc.cpp
      ...
    CMakeLists.txt
  tests/
    test_main.cpp
    test_wav.cpp
    test_db.cpp
    ...
    CMakeLists.txt
  examples/
    ch10_sine.cpp
    ch12_waveforms.cpp
    ...
    CMakeLists.txt
```

Two decisions worth explaining.

**Why `include/audio/` rather than just `include/`?** So that users write
`#include <audio/osc.h>`. The `audio/` prefix prevents collisions — if you later use a library
that also has a `filter.h`, there is no ambiguity. Every serious C++ library does this.

**Why separate `include/` from `src/`?** Because the distinction between *public interface* and
*private implementation* is the most important structural decision in a library. Headers in
`include/` are promises to your users. Files in `src/` can change freely.

---

## 17.3 Namespaces

Everything goes in a namespace so it cannot collide with other code:

```cpp
namespace audio {

class AudioBuffer { /* ... */ };
float dbToGain(float db);

}   // namespace audio
```

Used as `audio::AudioBuffer buf;`.

Inside the library's own `.cpp` files, add `using namespace audio;` at the top for brevity. In
*headers*, never do that — you would force the `using` on everyone who includes you, which defeats
the purpose.

For internal helpers that should not be visible at all, nest an anonymous namespace inside:

```cpp
namespace audio {
namespace {           // visible only within this .cpp
    uint16_t readU16LE(std::istream& in) { /* ... */ }
}
}
```

---

## 17.4 What goes in a header, and what does not

This is a genuine source of confusion, so here is the rule set.

**In the header:**
- Class and struct *declarations* (the member list and function signatures).
- Function declarations.
- `inline` functions — small ones where the call overhead would dominate.
- `constexpr` constants.
- Templates (they must be visible to every user).

**In the `.cpp`:**
- Function bodies of anything non-trivial.
- Anything with heavy `#include` requirements.
- Anything private.

**Why it matters:** every `#include` is a textual paste (Chapter 5). A header that includes
`<iostream>`, `<fstream>`, `<random>` and `<algorithm>` forces all of that onto every file that
includes it, and compile times balloon. Keep headers thin.

```cpp
// GOOD header: minimal includes, declarations only
#pragma once
#include <vector>
#include <string>

namespace audio {
    bool writeWav(const std::string& path, const std::vector<float>& samples,
                  int sampleRate = 44100, int numChannels = 1);
}
```

```cpp
// GOOD .cpp: heavy includes live here
#include <audio/wav.h>
#include <fstream>
#include <random>
#include <algorithm>
#include <cmath>
```

### Forward declarations

If a header only needs to know that a type *exists* (because it uses a pointer or reference to
it), declare it rather than including its header:

```cpp
namespace audio {
    class AudioBuffer;                 // forward declaration -- no include needed

    class Processor
    {
    public:
        virtual void process(AudioBuffer& buffer) = 0;   // reference is fine
    };
}
```

You need the full definition only when you use the type *by value*, access its members, or
inherit from it.

---

## 17.5 The `Processor` interface — and virtual functions

Here is the one new C++ concept in this chapter, and it is the one that makes Parts III–V
possible.

Every effect in Part IV — reverb, delay, compressor, filter — does the same thing from the
outside: take a buffer, change it. If they all share an interface, you can store them in a list
and run them in sequence without knowing what any of them are.

```cpp
namespace audio {

class Processor
{
public:
    virtual ~Processor() = default;

    // Called before processing starts. Allocate here, not in process().
    virtual void prepare(double sampleRate, int maxBlockSize) = 0;

    // Process a block in place.
    virtual void process(AudioBuffer& buffer) = 0;

    // Clear internal state (filter memory, delay lines, envelopes).
    virtual void reset() = 0;

    // For display and debugging.
    virtual const char* name() const = 0;
};

}   // namespace audio
```

### What each piece means

**`virtual`** means "the *actual* type of the object decides which function runs, not the type of
the pointer". This is **polymorphism**, and it is the whole point.

```cpp
std::vector<std::unique_ptr<Processor>> chain;
chain.push_back(std::make_unique<Gain>(0.5f));
chain.push_back(std::make_unique<DCBlocker>());
chain.push_back(std::make_unique<Reverb>());

for (auto& p : chain)
    p->process(buffer);            // each runs ITS OWN process()
```

The loop does not know or care what the processors are. That is an effects rack, and it is
Chapter 64's audio graph in embryo.

**`= 0`** makes the function **pure virtual**: there is no default implementation and every
derived class *must* provide one. A class with pure virtual functions is **abstract** — you
cannot create one directly, only classes derived from it. This is exactly what an interface
should be.

**`virtual ~Processor() = default;`** — the virtual destructor. **This one is not optional and
omitting it is a real bug.**

```cpp
Processor* p = new Reverb();
delete p;              // without a virtual destructor: only ~Processor() runs.
                       // Reverb's buffers are never freed. Memory leak.
```

When you delete through a base pointer, a non-virtual destructor calls only the base's
destructor. The derived class's members are never cleaned up. The rule: **any class meant to be
inherited from gets a virtual destructor.** Compilers warn about this with `-Wall` in some cases;
do not rely on it.

**`std::unique_ptr<Processor>`** — a smart pointer that owns its object and deletes it
automatically when it goes out of scope. This is RAII (Chapter 7) applied to polymorphic objects,
and it is why you still will not write `delete`.

```cpp
#include <memory>
auto p = std::make_unique<Gain>(0.5f);   // allocates; deletes itself when p dies
```

### Implementing one

```cpp
namespace audio {

class Gain : public Processor
{
public:
    explicit Gain(float linearGain = 1.0f) : gain(linearGain) {}

    void prepare(double, int) override {}      // nothing to allocate

    void process(AudioBuffer& buffer) override
    {
        for (int c = 0; c < buffer.numChannels(); ++c)
            for (auto& s : buffer.channel(c))
                s *= gain;
    }

    void reset() override {}                   // no state

    const char* name() const override { return "Gain"; }

    void setGain(float g)     { gain = g; }
    void setGainDb(float dB)  { gain = static_cast<float>(dbToGain(dB)); }

private:
    float gain = 1.0f;
};

}   // namespace audio
```

**`: public Processor`** — inheritance. `Gain` *is a* `Processor` and can be used wherever one is
expected.

**`override`** — tells the compiler "this is meant to replace a virtual function from the base".
If the signature does not match one (a typo, a missing `const`, a different parameter type), you
get a compile error instead of a function that silently never gets called. **Always write
`override`.** The bug it prevents — a `process` that is never called because you wrote
`process(AudioBuffer buffer)` instead of `process(AudioBuffer&)` — is genuinely hard to find.

**`explicit`** on the constructor prevents accidental implicit conversion. Without it,
`someFunctionTakingGain(0.5f)` would silently construct a `Gain`. Mark single-argument
constructors `explicit` by default.

**`void prepare(double, int) override {}`** — the parameters are unnamed because we do not use
them. This silences the "unused parameter" warning from `-Wextra` while keeping the signature.

---

## 17.6 CMake

`g++ a.cpp b.cpp c.cpp -o prog` does not scale past about three files. **CMake** generates build
files (Makefiles, Ninja files, Visual Studio projects) from a description of your project.

### The top-level `CMakeLists.txt`

```cmake
cmake_minimum_required(VERSION 3.16)

project(audiobook
        VERSION 1.0.0
        DESCRIPTION "Audio Programming in C++ - companion library"
        LANGUAGES CXX)

# --- language settings -------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- default to a Release build ---------------------------------------
# Unoptimised DSP can be 10x too slow. See Chapter 5.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# --- warnings ----------------------------------------------------------
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# --- put all binaries in one place -------------------------------------
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# --- subdirectories ----------------------------------------------------
add_subdirectory(lib)
add_subdirectory(tests)
add_subdirectory(examples)
```

**Line by line:**

`cmake_minimum_required` — refuses to run on older CMake, and also selects which behaviours CMake
uses. 3.16 is available everywhere current.

`project(...)` — names the project and declares that it uses C++ (`LANGUAGES CXX`). Without
`LANGUAGES`, CMake also looks for a C compiler, which is a slow no-op here.

`CMAKE_CXX_STANDARD 17` with `STANDARD_REQUIRED ON` — use C++17, and fail rather than silently
falling back. `EXTENSIONS OFF` means `-std=c++17` rather than `-std=gnu++17`, which keeps the
code portable.

**The Release default matters.** CMake's default build type on single-config generators is
*empty*, which means **no optimisation flags at all** — worse than `-O0`. Chapter 5 explained why
that is unacceptable for audio.

`CMAKE_RUNTIME_OUTPUT_DIRECTORY` — all executables land in `build/bin/` instead of scattered
through the build tree.

### `lib/CMakeLists.txt`

```cmake
add_library(audio STATIC
    src/wav_writer.cpp
    src/wav_reader.cpp
    src/audio_buffer.cpp
    src/osc.cpp
    src/noise.cpp
)

target_include_directories(audio
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_compile_features(audio PUBLIC cxx_std_17)
```

`add_library(audio STATIC ...)` creates a static library from those sources. It will be called
`libaudio.a` (or `audio.lib` on MSVC).

`target_include_directories(... PUBLIC ...)` is the important line. It says: to build this
library, and **for anything that links against it**, add `lib/include` to the include path. That
is why an example only needs `target_link_libraries(example PRIVATE audio)` — the include path
comes along automatically.

The three visibility keywords:

| Keyword | Meaning |
|---|---|
| `PRIVATE` | Needed to build this target only. |
| `PUBLIC` | Needed to build this target *and* anything that links it. |
| `INTERFACE` | Needed only by things that link it, not by the target itself. |

Getting these right is most of what "modern CMake" means. A header that appears in your public
headers is `PUBLIC`; an implementation detail is `PRIVATE`.

### `examples/CMakeLists.txt`

```cmake
# One executable per example, all linking the library.
set(EXAMPLES
    ch10_sine
    ch12_waveforms
    ch13_envelopes
    ch14_mix
    ch15_noise
    ch16_wavinfo
)

foreach(name ${EXAMPLES})
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE audio)
endforeach()
```

Adding a new example is one line in the list.

### Building

```bash
cd /c/roy/learning/audio
cmake -S . -B build -G "Ninja"          # configure (once, or after CMakeLists changes)
cmake --build build                      # build
./build/bin/ch10_sine.exe                # run
```

| Command | What it does |
|---|---|
| `cmake -S . -B build` | Configure: read CMakeLists, generate build files in `build/` |
| `-G "Ninja"` | Use Ninja (fast). Omit for the default; `-G "MinGW Makefiles"` also works |
| `cmake --build build` | Build |
| `cmake --build build --target clean` | Clean |
| `cmake --build build -j 8` | Build with 8 parallel jobs |
| `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug` | Configure a debug build |

**The `build/` directory is disposable.** Delete it and reconfigure whenever anything seems
strange; it holds only generated files. Add it to `.gitignore`.

VS Code's CMake Tools extension (installed in Chapter 5) picks all this up automatically: it
shows a build button in the status bar and lets you pick the build type and the target to debug.

---

## 17.7 Tests

Untested library code decays. You do not need a testing framework — sixty lines of C++ gives you
enough.

**Code — `lib/include/audio/test.h`**

```cpp
#pragma once

#include <iostream>
#include <iomanip>
#include <cmath>
#include <string>

namespace audio { namespace test {

inline int  g_passed = 0;
inline int  g_failed = 0;

inline void check(bool condition, const std::string& what, const char* file, int line)
{
    if (condition)
    {
        ++g_passed;
    }
    else
    {
        ++g_failed;
        std::cout << "  FAIL  " << what << "\n        at " << file << ":" << line << "\n";
    }
}

inline void checkClose(double a, double b, double tolerance,
                       const std::string& what, const char* file, int line)
{
    const bool ok = std::fabs(a - b) <= tolerance;
    if (!ok)
        std::cout << "  FAIL  " << what << "\n"
                  << "        expected " << b << ", got " << a
                  << " (difference " << std::fabs(a - b)
                  << ", tolerance " << tolerance << ")\n"
                  << "        at " << file << ":" << line << "\n";
    ok ? ++g_passed : ++g_failed;
}

inline int summary()
{
    std::cout << "\n" << g_passed << " passed, " << g_failed << " failed.\n";
    return g_failed == 0 ? 0 : 1;
}

}}   // namespace audio::test

#define CHECK(cond)                  audio::test::check((cond), #cond, __FILE__, __LINE__)
#define CHECK_CLOSE(a, b, tol)       audio::test::checkClose((a), (b), (tol), \
                                         #a " ~= " #b, __FILE__, __LINE__)
```

**Walkthrough**

`inline int g_passed = 0;` — an **inline variable** (C++17). Before C++17 you needed a `.cpp` file
to hold the definition; now a header can define a variable that is shared across all translation
units. Very convenient for small header-only utilities.

`#define CHECK(cond) ...` — a macro. Macros are usually to be avoided, but here they earn their
place: `__FILE__` and `__LINE__` expand at the *call site*, so a failure reports the line in the
test, not the line in `test.h`. And `#cond` is the **stringification operator** — it turns the
literal text of the condition into a string, so `CHECK(peak < 1.0)` prints `peak < 1.0` in the
failure message. A function cannot do either of those.

### Writing tests

```cpp
#include <audio/audio.h>
#include <audio/test.h>

using namespace audio;

void testDecibels()
{
    std::cout << "decibels\n";

    CHECK_CLOSE(gainToDb(1.0),   0.0,   0.001);
    CHECK_CLOSE(gainToDb(0.5),  -6.0206, 0.001);
    CHECK_CLOSE(gainToDb(2.0),   6.0206, 0.001);
    CHECK_CLOSE(dbToGain(0.0),   1.0,   0.0001);
    CHECK_CLOSE(dbToGain(-6.0),  0.50119, 0.0001);

    // Round trip
    for (double d : { 0.0, -3.0, -12.0, -40.0, 6.0 })
        CHECK_CLOSE(gainToDb(dbToGain(d)), d, 0.0001);

    // The zero guard: must not return -inf or NaN
    CHECK(std::isfinite(gainToDb(0.0)));
    CHECK(dbToGain(-300.0) == 0.0);
}

void testOscillator()
{
    std::cout << "oscillator\n";

    SineOsc osc;
    osc.setSampleRate(44100.0);
    osc.setFrequency(441.0);            // exactly 100 samples per cycle

    std::vector<float> buf(400);
    for (auto& s : buf) s = osc.nextSample();

    // Should be periodic with period 100
    for (int i = 0; i < 300; ++i)
        CHECK_CLOSE(buf[i], buf[i + 100], 1e-5);

    // Peak should be 1.0, RMS should be 1/sqrt(2)
    CHECK_CLOSE(peak(buf), 1.0, 0.001);
    CHECK_CLOSE(rms(buf),  0.70710678, 0.001);

    // A sine has zero DC offset
    CHECK_CLOSE(dcOffset(buf), 0.0, 1e-6);
}

void testWavRoundTrip()
{
    std::cout << "wav round trip\n";

    std::vector<float> original(1000);
    for (size_t i = 0; i < original.size(); ++i)
        original[i] = static_cast<float>(std::sin(i * 0.1) * 0.8);

    CHECK(writeWav("test_tmp.wav", original, 44100, 1));

    std::string err;
    WavFile loaded = readWav("test_tmp.wav", err);

    CHECK(loaded.isValid());
    CHECK(loaded.numChannels == 1);
    CHECK(loaded.sampleRate == 44100);
    CHECK(loaded.numFrames == 1000);

    double maxError = 0.0;
    for (size_t i = 0; i < original.size(); ++i)
        maxError = std::max(maxError,
            std::fabs(static_cast<double>(original[i]) - loaded.channels[0][i]));

    // One 16-bit quantization step
    CHECK(maxError < 1.0 / 32000.0);
}

int main()
{
    testDecibels();
    testOscillator();
    testWavRoundTrip();
    // ... more
    return audio::test::summary();
}
```

**Output**

```
decibels
oscillator
wav round trip
envelope
noise
dc blocker

47 passed, 0 failed.
```

### What to test in audio code

Audio is harder to test than most software, because "does it sound right" is not a boolean. But a
great deal *is* testable, and these categories catch most real bugs:

| Category | Example |
|---|---|
| **Known values** | A sine's RMS is `1/√2`. A square's crest factor is 0 dB. |
| **Round trips** | Write→read, dB→gain→dB, encode→decode. |
| **Invariants** | A filter must not produce NaN. Output must stay in range. RMS after unity gain must equal RMS before. |
| **Periodicity** | An oscillator at 441 Hz repeats every 100 samples at 44.1 kHz. |
| **Boundaries** | Zero-length buffers. One-sample buffers. Frequencies of 0 and Nyquist. Gain of 0. |
| **Stability** | Run a filter for a million samples; assert the output is still finite. |
| **Regression** | Save a known-good output hash; assert it does not change unexpectedly. |

That last one is worth setting up early: render a fixed test signal through your whole chain,
store its checksum, and fail the test if it changes. When it does change, you look at the diff
and decide whether the change was intended. This catches the class of bug where a refactor
silently alters the sound.

---

## 17.8 The public API

The convenience header ties it together:

**`lib/include/audio/audio.h`**

```cpp
#pragma once

// Audio Programming in C++ -- companion library
//
//   #include <audio/audio.h>
//   using namespace audio;

#include <audio/types.h>       // Sample, constants, AudioBuffer
#include <audio/db.h>          // decibels and measurement
#include <audio/wav.h>         // read/write WAV
#include <audio/osc.h>         // oscillators
#include <audio/envelope.h>    // ADSR
#include <audio/noise.h>       // white / pink / brown
#include <audio/filter.h>      // DC blocker (biquads from Chapter 23)
#include <audio/processor.h>   // the Processor interface and Chain
```

A tour of what exists after Part I:

```cpp
namespace audio {

// --- constants -------------------------------------------------------
constexpr double kPi           = 3.14159265358979323846;
constexpr double kTwoPi        = 2.0 * kPi;
constexpr double kDefaultRate  = 44100.0;

// --- buffers ---------------------------------------------------------
class AudioBuffer {
    AudioBuffer(int numChannels, int numFrames, double sampleRate);
    int    numChannels() const;
    int    numFrames()   const;
    double sampleRate()  const;
    double durationSeconds() const;
    std::vector<float>&       channel(int c);
    const std::vector<float>& channel(int c) const;
    void clear();
    void applyGain(float g);
    void applyGainDb(float dB);
    void normalise(float targetPeak = 0.99f);
    void fadeIn(int frames);
    void fadeOut(int frames);
    void mixFrom(const AudioBuffer& src, float gain = 1.0f);
    void appendFrom(const AudioBuffer& src);
    float peak() const;
    float rms()  const;
};

// --- decibels and measurement ---------------------------------------
double gainToDb(double gain);
double dbToGain(double dB);
double peak(const std::vector<float>& buf);
double rms(const std::vector<float>& buf);
double peakDb(const std::vector<float>& buf);
double rmsDb(const std::vector<float>& buf);
double crestFactorDb(const std::vector<float>& buf);
double dcOffset(const std::vector<float>& buf);

// --- files ------------------------------------------------------------
bool    writeWav(const std::string& path, const std::vector<float>& samples,
                 int sampleRate = 44100, int numChannels = 1, bool dither = false);
bool    writeWav(const std::string& path, const AudioBuffer& buffer,
                 bool dither = false);
WavFile readWav(const std::string& path, std::string& error);
bool    printWavInfo(const std::string& path);

// --- oscillators -------------------------------------------------------
enum class Waveform { Sine, Triangle, Square, Saw, Pulse };

class Oscillator {
    void  setSampleRate(double sr);
    void  setFrequency(double hz);
    void  setWaveform(Waveform w);
    void  setPulseWidth(double w);         // 0..1, for Pulse
    void  setPhase(double normalised);     // 0..1
    float nextSample();
    void  reset();
};

// --- envelope ----------------------------------------------------------
class ADSR { /* Chapter 13 */ };

// --- noise -------------------------------------------------------------
class FastRandom  { /* Chapter 15 */ };
class PinkNoise   { /* Chapter 15 */ };
class BrownNoise  { /* Chapter 15 */ };

// --- filters -----------------------------------------------------------
class DCBlocker { /* Chapter 14 */ };

// --- processing --------------------------------------------------------
class Processor { /* abstract interface */ };
class Gain      : public Processor { };
class Chain     : public Processor { void add(std::unique_ptr<Processor>); };

}   // namespace audio
```

And an example program becomes:

```cpp
#include <audio/audio.h>

using namespace audio;

int main()
{
    AudioBuffer buf(1, 44100 * 2, 44100.0);

    Oscillator osc;
    osc.setSampleRate(buf.sampleRate());
    osc.setFrequency(220.0);
    osc.setWaveform(Waveform::Saw);

    ADSR env;
    env.setSampleRate(buf.sampleRate());
    env.setParameters(0.01, 0.3, 0.6, 0.5);
    env.noteOn();

    auto& ch = buf.channel(0);
    for (size_t i = 0; i < ch.size(); ++i)
    {
        if (i == ch.size() * 3 / 4)
            env.noteOff();
        ch[i] = osc.nextSample() * env.nextSample() * 0.5f;
    }

    buf.normalise(0.9f);
    writeWav("out.wav", buf, true);

    std::cout << "peak " << peakDb(ch) << " dBFS, "
              << "crest " << crestFactorDb(ch) << " dB\n";
    return 0;
}
```

Compare that with Chapter 14's build line and argument juggling. That improvement is what the
chapter bought.

---

## 17.9 Version control hygiene

A `.gitignore` for this project:

```gitignore
# Build output
build/
*.o
*.a
*.exe
*.lib
*.dll

# Rendered audio -- regenerable, and large
assets/*.wav
*.wav
!assets/reference/*.wav      # except any reference files we deliberately keep

# Editor
.vscode/ipch/
.cache/
compile_commands.json
```

**Do not commit rendered audio.** It is large, it is binary (so diffs are useless), and it is
reproducible by running the program. Commit the *code* that generates it. The exception is a
small set of reference files used by regression tests — keep those deliberately, in their own
folder, and note in a README why each exists.

---

## 17.10 Exercises

**17.1** Set up the full directory structure and get `cmake -S . -B build && cmake --build build`
working. Port Chapter 10's sine program to use the library.

**17.2** Port every example from Chapters 9–16 into `examples/`, removing all the duplicated
helper code. Count how many lines disappear.

**17.3** Write tests for every component built so far. Aim for at least 50 checks. Include at
least one boundary test per component (zero-length buffer, zero frequency, gain of 0).

**17.4** Implement `Chain`, a `Processor` that holds a list of other processors and runs them in
order. It should itself be a `Processor`, so chains can nest. Test that a chain of
`Gain(0.5)` and `Gain(0.5)` produces the same result as `Gain(0.25)`.

**17.5** Add `AudioBuffer::appendFrom` and `AudioBuffer::mixFrom`, with sensible behaviour when
channel counts or lengths differ. Document your decisions in the header, and test them.

**17.6** Add a `CMakeLists.txt` option for sanitizers:
```cmake
option(ENABLE_SANITIZERS "Build with address and UB sanitizers" OFF)
```
and wire it to the compile and link flags. Run the test suite with it on.

**17.7** Add a `Benchmark` helper using `<chrono>` that times a processor over a million samples
and reports both nanoseconds per sample and the "real-time factor" (how many times faster than
real time). You will use this constantly from Part IV onward.

**17.8** *Deliberate breakage.* Remove `virtual` from `~Processor()`, create a derived class that
allocates a large vector, delete a thousand of them through base pointers, and watch memory usage.
Then put it back.

**17.9** Write a `Regression` test: render a fixed 2-second signal through a fixed chain, compute
a checksum of the output samples, and compare it against a stored value. Make it fail, then make
it pass.

---

### Chapter summary

- A **static library** consolidates your code: one place to fix bugs, fast rebuilds, a designed
  API.
- Structure: `lib/include/audio/` for public headers, `lib/src/` for implementation, plus
  `tests/` and `examples/`. The `audio/` include prefix prevents collisions.
- Put everything in a `namespace`. Never `using namespace` in a header.
- Keep headers thin: declarations, `inline`, `constexpr`, templates. Heavy includes go in `.cpp`.
- **`Processor`** is an abstract interface with pure virtual functions. `virtual` enables
  polymorphism; **`virtual ~Processor()` is mandatory** or derived destructors never run;
  **always write `override`**.
- Manage polymorphic objects with `std::unique_ptr`, so you still never write `delete`.
- **CMake**: `add_library`, `target_include_directories(... PUBLIC ...)`,
  `target_link_libraries(... PRIVATE audio)`. Default the build type to **Release** — CMake's
  own default has no optimisation at all.
- A 60-line test header with `CHECK` and `CHECK_CLOSE` macros is enough. Test known values, round
  trips, invariants, periodicity, boundaries, and stability — and set up a regression checksum.
- Commit code, not rendered audio.

---

## Part I is complete

You can now: write and read WAV files byte by byte; generate sines, saws, squares, triangles and
every colour of noise; shape them with envelopes; mix, pan, saturate and measure them; and you
have a tested library to build on.

You have also met, without it being labelled as such, a surprising amount of DSP: superposition,
harmonic series, aliasing, the one-pole filter, convolution's shadow in the click problem, and
the time-frequency trade-off.

**Part II makes all of that explicit.** It is the hardest part of the book. It is also the part
after which you can read any DSP paper and build any effect you can find described. Take it
slowly.

**Next:** [Chapter 18 — Signals and Samples: The Notation You Need](18-signals-and-samples.md)
