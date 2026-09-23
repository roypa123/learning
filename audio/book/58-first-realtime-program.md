# Chapter 58 — Your First Real-Time Program with miniaudio

> Sound out of the speakers, right now, from code you wrote. This chapter is short and entirely
> practical: get a tone playing, then a synth, then handle the failure modes.

---

## 58.1 Setup

Download `miniaudio.h` from `github.com/mackron/miniaudio` into `lib/external/`.

**One translation unit must define the implementation:**

```cpp
// lib/src/miniaudio_impl.cpp -- this file contains nothing else.
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
```

Everywhere else, just `#include "miniaudio.h"`.

> **Why a separate file?** The header contains ~90,000 lines of implementation guarded by that
> macro. Compiling it once keeps your other files' build times short. Defining the macro in a
> header that several files include gives you duplicate-symbol link errors.

CMake:

```cmake
target_include_directories(audio PUBLIC lib/external)
target_sources(audio PRIVATE lib/src/miniaudio_impl.cpp)

if(WIN32)
    target_link_libraries(audio PRIVATE winmm ole32)
elseif(APPLE)
    target_link_libraries(audio PRIVATE "-framework CoreAudio"
                                        "-framework CoreFoundation"
                                        "-framework AudioToolbox")
else()
    target_link_libraries(audio PRIVATE pthread m dl)
endif()
```

---

## 58.2 A tone

```cpp
#include "miniaudio.h"
#include <audio/audio.h>
#include <iostream>
#include <atomic>

// ---- state shared between the main thread and the audio thread ----
struct ToneState
{
    double phase      = 0.0;
    double sampleRate = 48000.0;

    // atomic: the GUI/main thread writes, the audio thread reads.
    std::atomic<double> frequency{ 440.0 };
    std::atomic<float>  amplitude{ 0.2f };
};

void dataCallback(ma_device* device, void* output, const void*, ma_uint32 frameCount)
{
    auto* state = static_cast<ToneState*>(device->pUserData);
    auto* out   = static_cast<float*>(output);

    const int channels = static_cast<int>(device->playback.channels);

    // Read the atomics ONCE per block, not per sample: cheaper, and it
    // guarantees a consistent value across the whole block.
    const double freq = state->frequency.load(std::memory_order_relaxed);
    const float  amp  = state->amplitude.load(std::memory_order_relaxed);

    const double increment = kTwoPi * freq / state->sampleRate;

    for (ma_uint32 i = 0; i < frameCount; ++i)
    {
        const float s = static_cast<float>(std::sin(state->phase)) * amp;

        // Interleaved: write the same sample to every channel.
        for (int c = 0; c < channels; ++c)
            out[i * channels + c] = s;

        state->phase += increment;
        if (state->phase >= kTwoPi) state->phase -= kTwoPi;
    }
}

int main()
{
    ToneState state;

    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format   = ma_format_f32;
    config.playback.channels = 2;
    config.sampleRate        = 48000;
    config.periodSizeInFrames = 256;
    config.dataCallback      = dataCallback;
    config.pUserData         = &state;

    ma_device device;
    if (ma_device_init(nullptr, &config, &device) != MA_SUCCESS)
    {
        std::cerr << "Failed to open audio device\n";
        return 1;
    }

    // CHECK what we actually got (Chapter 57).
    state.sampleRate = device.sampleRate;

    std::cout << "Device : " << device.playback.name << "\n";
    std::cout << "Rate   : " << device.sampleRate << " Hz\n";
    std::cout << "Channels: " << device.playback.channels << "\n";
    std::cout << "Period : " << device.playback.internalPeriodSizeInFrames
              << " frames ("
              << device.playback.internalPeriodSizeInFrames * 1000.0 / device.sampleRate
              << " ms)\n\n";

    std::cout << "*** TURN YOUR VOLUME DOWN ***\n";
    std::cout << "Press Enter to start...";
    std::cin.get();

    ma_device_start(&device);

    std::cout << "Playing. Enter a frequency (or 0 to quit):\n";
    double f = 440.0;
    while (std::cin >> f && f > 0.0)
        state.frequency.store(f);

    ma_device_uninit(&device);
    return 0;
}
```

**Run it.** A tone comes out of your speakers, and typing a number changes its pitch while it
plays.

---

## 58.3 What the code is doing right

**`std::atomic` for shared parameters.** The main thread writes, the audio thread reads, and
without `atomic` that is a **data race** — undefined behaviour, which in practice means the audio
thread can read a half-updated value. For a `double` on x86 it would probably work; "probably
works" is not a basis for real-time code, and on other architectures it genuinely does not.

**`memory_order_relaxed`** is correct here: we only need the value to be read atomically, not to
be ordered relative to other operations. It is the cheapest ordering and compiles to a plain load
on x86.

**Reading atomics once per block.** Cheaper than per sample, and it guarantees the whole block
uses one consistent value. Per-sample reads of a parameter that changes mid-block would produce
a step — Chapter 13's click. Chapter 61 handles that properly with smoothing.

**Phase stored in the state, not recreated.** The callback is called repeatedly; any state that
must persist between calls lives in the struct. A `static` local would also work but is a
thread-safety hazard and makes multiple instances impossible.

**Checking the actual sample rate** after `ma_device_init`. Chapter 57's rule.

**Volume warning before starting.** Chapter 3's rule, and it belongs in the program, not just in
the documentation.

---

## 58.4 A real synth

Now connect Chapter 41's polyphonic synth.

```cpp
struct EngineState
{
    audio::Synth       synth;
    audio::AudioBuffer scratch;          // pre-allocated
    LockFreeQueue<NoteEvent> noteQueue;  // main -> audio (Chapter 60)
    std::atomic<float> masterGain{ 0.5f };
};

void dataCallback(ma_device* device, void* output, const void*, ma_uint32 frameCount)
{
    auto* state = static_cast<EngineState*>(device->pUserData);
    auto* out   = static_cast<float*>(output);
    const int channels = static_cast<int>(device->playback.channels);

    // --- 1. drain the event queue ---------------------------------
    NoteEvent ev;
    while (state->noteQueue.pop(ev))
    {
        if (ev.isNoteOn) state->synth.noteOn(ev.note, ev.velocity);
        else             state->synth.noteOff(ev.note);
    }

    // --- 2. process into the PRE-ALLOCATED buffer ------------------
    // frameCount can vary, so use only the part we need.
    state->scratch.clear();
    state->synth.process(state->scratch, static_cast<int>(frameCount));

    // --- 3. interleave out ----------------------------------------
    const float gain = state->masterGain.load(std::memory_order_relaxed);

    for (ma_uint32 i = 0; i < frameCount; ++i)
        for (int c = 0; c < channels; ++c)
        {
            const int srcChannel = std::min(c, state->scratch.numChannels() - 1);
            out[i * channels + c] =
                state->scratch.channel(srcChannel)[i] * gain;
        }
}
```

**The three-step shape** — drain events, process, output — is what every real-time audio callback
looks like. Everything else is detail.

**`state->scratch` is allocated once, in `prepare()`,** sized for the maximum block. Chapter 59
explains why this is non-negotiable.

---

## 58.5 The failure modes

Things go wrong in characteristic ways. Learn to recognise them by ear.

| Symptom | Cause |
|---|---|
| **Silence, no error** | Wrong device selected; muted system; `ma_device_start` not called |
| **Regular clicking at the block rate** | Uninitialised state between callbacks; not handling varying `frameCount` |
| **Crackling under load** | Buffer too small, or the callback occasionally overruns |
| **Crackling during silence** | **Denormals** (Chapter 59) |
| **Plays at the wrong pitch** | Prepared DSP with the requested rate, not the actual one |
| **One channel only** | Channel count mismatch; writing to `out[i]` instead of `out[i*channels+c]` |
| **Very loud noise on start** | Uninitialised buffer, or NaN in the DSP. **This is a hearing hazard** |
| **Fine for a while, then glitches** | Allocation, denormals building up, or a growing container |
| **Glitches when the GUI is used** | A lock shared between threads, or allocation on a parameter change |

**The "very loud noise on start" case deserves a defence**, not just a diagnosis:

```cpp
// A safety net at the very end of the callback. Three lines.
for (ma_uint32 i = 0; i < frameCount * channels; ++i)
{
    if (!std::isfinite(out[i])) out[i] = 0.0f;
    out[i] = std::clamp(out[i], -1.0f, 1.0f);
}
```

This converts "a NaN got into a filter and now full-scale noise is going to the user's
headphones" into "brief silence". Given Chapter 3's warning about permanent hearing damage, this
is a safety feature and it belongs in every real-time program you write while learning.

---

## 58.6 Audio input

```cpp
ma_device_config config = ma_device_config_init(ma_device_type_duplex);
config.capture.format    = ma_format_f32;
config.capture.channels  = 1;
config.playback.format   = ma_format_f32;
config.playback.channels = 2;
config.sampleRate        = 48000;
config.dataCallback      = duplexCallback;

void duplexCallback(ma_device* device, void* output, const void* input,
                    ma_uint32 frameCount)
{
    const auto* in  = static_cast<const float*>(input);
    auto*       out = static_cast<float*>(output);

    for (ma_uint32 i = 0; i < frameCount; ++i)
    {
        const float processed = effect.process(in[i]);
        out[i * 2 + 0] = processed;
        out[i * 2 + 1] = processed;
    }
}
```

> **FEEDBACK WARNING.** A microphone and speakers in the same room form a loop, and Chapter 43
> explained what happens when a feedback loop's gain exceeds 1. This can reach painful levels in
> under a second.
>
> **Use headphones for any input experiment.** Start with the output gain very low. Put a limiter
> (Chapter 51) on the output before you begin.

---

## 58.7 Clean shutdown

```cpp
~AudioEngine()
{
    // 1. Stop the device FIRST. This waits for any in-flight callback
    //    to finish, so nothing is running when we destroy things.
    ma_device_stop(&device_);

    // 2. Now it is safe to tear down.
    ma_device_uninit(&device_);

    // 3. Only now can the engine's buffers be freed.
}
```

**The order is not optional.** Destroying an object while a callback is using it is a
use-after-free, and it will crash intermittently — the worst kind of bug, because it usually
works.

**`ma_device_stop` blocks** until any running callback returns. That is exactly what you want,
and it is why the callback must never block on anything itself: if it did, shutdown could
deadlock.

---

## 58.8 Exercises

**58.1** Get the tone program running. Confirm you can change the frequency while it plays.

**58.2** *Deliberate breakage.* Remove `std::atomic` and use a plain `double`. Does it still work?
(It probably does on x86 — which is exactly why this is dangerous.) Build with
`-fsanitize=thread` if available and see what it reports.

**58.3** Change the amplitude from the main thread in large steps while a tone plays. Listen for
the clicks at block boundaries. This motivates Chapter 61.

**58.4** Print `frameCount` from the callback (to a ring buffer, printed from `main`). Does it
vary?

**58.5** Add the NaN/clamp safety net. Then deliberately introduce a NaN and confirm you get
silence rather than noise.

**58.6** Connect Chapter 41's synth. Play notes by typing MIDI note numbers.

**58.7** Measure the callback duration and print the peak load once a second. What is your synth's
worst case?

**58.8** Reduce the buffer size until you hear glitches. What is the smallest size your system
handles reliably?

**58.9** *With headphones.* Build a duplex passthrough with a limiter. Then add a delay effect.

**58.10** *Deliberate breakage.* Call `ma_device_uninit` before `ma_device_stop`. Run it twenty
times and see how often it crashes.

---

### Chapter summary

- `miniaudio` is a single header. Define `MINIAUDIO_IMPLEMENTATION` in **exactly one** `.cpp`.
- Shared state between the main and audio threads must be **`std::atomic`** — anything else is a
  data race, and "probably works on x86" is not a basis for real-time code.
- **Read atomics once per block**, not per sample: cheaper, and it guarantees a consistent value
  across the block.
- **Check the device's actual sample rate, channel count and buffer size** and prepare your DSP
  with those.
- Every callback has the same three-step shape: **drain the event queue, process into a
  pre-allocated buffer, interleave out.**
- Learn the failure modes by ear: block-rate clicking is uninitialised state; crackling during
  *silence* is denormals; wrong pitch is a sample-rate mismatch; sudden loud noise is a NaN.
- **Add a NaN check and clamp at the end of the callback.** Three lines, and it is a hearing-
  safety feature.
- **Use headphones for input experiments** — a mic and speakers in one room is a feedback loop.
- **Stop the device before destroying anything.** `ma_device_stop` waits for the in-flight
  callback, which is why the callback must never block.

**Next:** [Chapter 59 — The Real-Time Rules: No Malloc, No Locks, No Excuses](59-realtime-rules.md)
