# Chapter 57 — Audio APIs on Windows: WASAPI, ASIO, and Friends

> Getting samples to the hardware is platform-specific, tedious, and full of history. This
> chapter surveys the landscape so you know what you are choosing between, then recommends
> using a wrapper — because writing five backends is not a good use of your time.

---

## 57.1 The stack

```
   YOUR CODE
       │
   ┌───▼────────────────────────────────┐
   │  Cross-platform wrapper            │   miniaudio, RtAudio, PortAudio, JUCE
   └───┬────────────────────────────────┘
       │
   ┌───▼────────────────────────────────┐
   │  Platform API                       │   WASAPI, ASIO, CoreAudio, ALSA, JACK
   └───┬────────────────────────────────┘
       │
   ┌───▼────────────────────────────────┐
   │  Driver                             │
   └───┬────────────────────────────────┘
       │
      HARDWARE
```

Each layer adds latency and removes complexity. The decision is how far down to go.

---

## 57.2 The Windows APIs

**WASAPI** (Windows Audio Session API) — the modern native API, Vista onward.

Two modes, and the difference matters enormously:

| | Shared mode | Exclusive mode |
|---|---|---|
| Other apps can play | **Yes** | No |
| Latency | 10–30 ms | **3–10 ms** |
| Sample rate | Fixed by the system mixer | **Your choice** |
| Format | Converted by the mixer | Direct to hardware |
| Suitable for | Media players, games | **Production** |

**Shared mode goes through the Windows mixer**, which resamples everything to a common rate and
adds buffering. Exclusive mode bypasses it and talks to the driver directly.

**WASAPI event-driven mode** (as opposed to polling) is what you want: the driver signals an
event when it needs data, so your thread sleeps rather than spinning.

**ASIO** (Audio Stream Input/Output) — Steinberg's standard, and the professional default on
Windows.

- **Lowest latency** available on Windows: 1–5 ms is routine with good hardware.
- Bypasses everything Windows provides.
- **Requires a device-specific driver.** Cheap interfaces often have poor or no ASIO drivers.
- Exclusive by nature — one application at a time.
- **Licensing**: the SDK requires a Steinberg agreement. This is a real friction point, and it is
  why open-source projects often ship ASIO support as an optional build rather than bundling the
  SDK. **ASIO4ALL** is a widely-used wrapper that provides an ASIO interface over WDM drivers for
  hardware without native ASIO — useful, but it does not create low latency where the hardware
  cannot provide it.

**DirectSound** — legacy, high latency, emulated on modern Windows. Avoid.

**WinMM / waveOut** — very legacy, very high latency (50 ms+). Avoid, except as an absolute
fallback.

**WaveRT / WDM-KS** (Kernel Streaming) — direct kernel access, low latency, painful to use
directly.

### Recommendation

**Offer ASIO and WASAPI exclusive, defaulting to whichever is available.** Fall back to WASAPI
shared for casual use. A wrapper library gives you all of them.

---

## 57.3 The other platforms, briefly

**macOS: CoreAudio.** One API, well-designed, low latency out of the box, no exclusive mode
needed because the system mixer is already low-latency. Nothing else exists and nothing else is
needed. macOS is genuinely better than Windows in this specific respect.

**iOS: CoreAudio / AVAudioEngine.** Same foundation, with power-management complications.

**Linux: ALSA** is the kernel layer, **PulseAudio** the desktop sound server (high latency),
**PipeWire** the modern replacement (low latency, and now the default on most distributions), and
**JACK** the professional audio server (very low latency, application interconnection). For
production audio on Linux, target JACK or PipeWire.

**Android: AAudio** (modern, low latency on supported devices) or **OpenSL ES** (older). Android
audio latency is notoriously variable between devices.

---

## 57.4 Wrapper libraries

| Library | Size | License | Notes |
|---|---|---|---|
| **miniaudio** | Single header, ~90k lines | Public domain / MIT-0 | **No dependencies. Recommended for this book.** |
| RtAudio | Small, a few files | MIT | Mature, simple, well-tested |
| PortAudio | Medium | MIT-like | The long-standing standard; more backends |
| **JUCE** | Large framework | GPL / commercial | Audio + GUI + plugins. What most commercial plugins use |
| SDL2 audio | Medium | zlib | Fine for games, limited for production |

**We use miniaudio** for three reasons: it is a single header you drop into your project, it has
no build-system requirements at all, and it is public domain so there are no licensing questions.

```cpp
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
```

That is the entire installation.

**For a serious plugin or application, JUCE** is the pragmatic choice — it handles devices, MIDI,
GUI, plugin formats and parameter automation, which is most of the non-DSP work.

---

## 57.5 Device enumeration and negotiation

Whatever API you use, the shape of the setup is the same.

```cpp
// 1. Enumerate devices
ma_context context;
ma_context_init(nullptr, 0, nullptr, &context);

ma_device_info* playbackDevices;
ma_uint32 playbackCount;
ma_context_get_devices(&context, &playbackDevices, &playbackCount,
                       nullptr, nullptr);

for (ma_uint32 i = 0; i < playbackCount; ++i)
    std::cout << i << ": " << playbackDevices[i].name << "\n";

// 2. Configure
ma_device_config config = ma_device_config_init(ma_device_type_playback);
config.playback.format   = ma_format_f32;      // ask for float
config.playback.channels = 2;
config.sampleRate        = 48000;
config.periodSizeInFrames = 256;
config.dataCallback      = audioCallback;
config.pUserData         = &myEngine;

// 3. Initialise -- the device may NEGOTIATE different values
ma_device device;
if (ma_device_init(&context, &config, &device) != MA_SUCCESS)
    return false;

// 4. CHECK WHAT YOU ACTUALLY GOT
const double actualRate     = device.sampleRate;
const int    actualChannels = device.playback.channels;
const int    actualPeriod   = device.playback.internalPeriodSizeInFrames;

// 5. Start
ma_device_start(&device);
```

**Step 4 is not optional and is frequently skipped.** You *request* 48 kHz stereo at 256 frames;
you may *get* 44.1 kHz, 8 channels, at 480 frames. If your DSP was prepared for the requested
values, everything is subtly wrong: the pitch is off by 8.8%, filters are mistuned, and delay
times are incorrect.

**Always prepare your DSP using the values the device reports**, not the values you asked for.

### Sample rate negotiation

Devices support a limited set of rates. Common: 44100, 48000, 88200, 96000, 176400, 192000.

Some devices support only one. Some support a rate only in exclusive mode. Some report support
for a rate and then fail to open it.

**Defensive approach:**

```cpp
const int preferredRates[] = { 48000, 44100, 96000, 88200 };

for (int rate : preferredRates)
{
    config.sampleRate = rate;
    if (ma_device_init(&context, &config, &device) == MA_SUCCESS)
        break;
}
```

---

## 57.6 Buffer sizes in practice

The requested buffer size is also a negotiation, and the driver may:

- **Round** to its preferred granularity (often a multiple of 16, 32 or 64)
- **Clamp** to its minimum or maximum
- **Split** your requested period into multiple smaller callbacks
- **Vary** the size between callbacks — WASAPI shared mode genuinely does this

**Therefore: never assume the callback size.** Write your processing to handle any `numFrames`:

```cpp
void audioCallback(ma_device*, void* output, const void*, ma_uint32 frameCount)
{
    // frameCount can differ between calls. Handle it.
    engine->process(static_cast<float*>(output), frameCount);
}
```

**And pre-allocate for the maximum**, not the typical:

```cpp
void prepare(double sampleRate, int maxBlockSize)
{
    // Allocate for maxBlockSize, process whatever actually arrives.
    scratch_.resize(static_cast<size_t>(maxBlockSize) * 2);
}
```

A callback that arrives with more frames than you allocated for is a buffer overrun — Chapter 7's
worst kind of bug, in the worst possible place.

---

## 57.7 Interleaving

Chapter 4 introduced this. At the API boundary it becomes concrete.

**Almost every audio API delivers interleaved data:**

```
   [L0][R0][L1][R1][L2][R2]...
```

**Almost all DSP wants planar:**

```
   L: [L0][L1][L2]...    R: [R0][R1][R2]...
```

So there is a conversion at each end:

```cpp
void deinterleave(const float* in, AudioBuffer& out, int numFrames, int numChannels)
{
    for (int c = 0; c < numChannels; ++c)
    {
        auto& dst = out.channel(c);
        for (int i = 0; i < numFrames; ++i)
            dst[static_cast<size_t>(i)] = in[i * numChannels + c];
    }
}

void interleave(const AudioBuffer& in, float* out, int numFrames, int numChannels)
{
    for (int c = 0; c < numChannels; ++c)
    {
        const auto& src = in.channel(c);
        for (int i = 0; i < numFrames; ++i)
            out[i * numChannels + c] = src[static_cast<size_t>(i)];
    }
}
```

**The loop order matters for cache behaviour.** Iterating channel-outer/frame-inner writes
contiguously to the planar buffer and strides through the interleaved one. The reverse writes
contiguously to the interleaved buffer. Which is faster depends on sizes and cache, and at typical
audio buffer sizes the difference is small — but it is measurable at high channel counts.

**Some APIs offer non-interleaved mode** (CoreAudio, ASIO, and JACK natively). If yours does,
use it and skip the conversion entirely.

---

## 57.8 Sample formats

Devices may want `float32`, `int16`, `int24` or `int32`.

**Always request `float32` if available.** Chapter 4's reasoning: no clipping internally, and no
conversion needed from your float pipeline. Most modern devices support it natively.

If the device demands integers, the wrapper usually converts for you. If you do it yourself, use
Chapter 9's rules: clamp, scale by `2^(bits-1) - 1`, round.

**And dither** when reducing bit depth (Chapter 4), though for real-time monitoring output the
benefit is marginal.

---

## 57.9 Full-duplex and input

Input and output can be separate devices, and then they have **separate clocks**.

Two crystal oscillators are never exactly the same frequency. A 100 ppm difference — well within
normal tolerance — means one device produces 4.41 extra samples per second relative to the other.
Over a minute that is 265 samples of drift.

**The consequences:** eventually your input buffer either overflows or underruns, and you get a
glitch.

**Three solutions:**

1. **Use one device for both.** Then there is one clock. This is why professional interfaces do
   both input and output.
2. **Word clock / digital sync.** Slave one device's clock to the other's. Professional hardware
   supports this.
3. **Adaptive resampling.** Continuously measure the drift and resample by a tiny, varying ratio
   to compensate. This is what audio servers (JACK, PipeWire) and DAWs do when you combine
   devices. Chapter 28's variable-rate reader is the mechanism.

**For most purposes: use one device.** Combining devices is a source of subtle, intermittent
problems that are very hard to diagnose.

---

## 57.10 Exercises

**57.1** Download `miniaudio.h`. Write a program that enumerates and prints every playback and
capture device on your system, with their supported formats.

**57.2** Open a device requesting 48 kHz, 256 frames, stereo float. Print what you actually got.
Try several devices.

**57.3** Request a sample rate your device does not support. What happens? Implement the fallback
list from §57.5.

**57.4** Instrument the callback to record `frameCount` on every call. Run for a minute in both
WASAPI shared and exclusive mode. Does it vary?

**57.5** Implement `interleave` and `deinterleave` and verify they round-trip exactly.

**57.6** Benchmark both loop orders of the conversion at 2, 8 and 64 channels. Which wins, and
where?

**57.7** Measure round-trip latency for WASAPI shared, WASAPI exclusive, and ASIO (if you have it)
on the same hardware. Build the comparison table.

**57.8** Open separate input and output devices and measure the drift over five minutes by
counting samples on each side.

---

### Chapter summary

- The stack is: your code → a **wrapper** → the **platform API** → the driver → hardware. Each
  layer trades latency for convenience.
- On Windows: **WASAPI exclusive** (3–10 ms, bypasses the system mixer) or **ASIO** (1–5 ms,
  needs a device-specific driver, licensing friction). WASAPI **shared** goes through the mixer
  and is 10–30 ms. DirectSound and WinMM are legacy.
- macOS has **CoreAudio** and needs nothing else. Linux: target **JACK** or **PipeWire**.
- Use a **wrapper**. `miniaudio` is a single public-domain header with no dependencies; **JUCE**
  is the pragmatic choice for a real product.
- **Always check what the device actually gave you** — sample rate, channel count and buffer size
  are all negotiations, and preparing your DSP with the values you *requested* silently mistunes
  everything.
- **Never assume the callback size.** It can vary between calls. Pre-allocate for the maximum.
- Audio APIs deliver **interleaved**; DSP wants **planar**. Convert at the boundary, or use a
  non-interleaved mode if the API offers one.
- **Request `float32`.** No internal clipping, no conversion.
- **Separate input and output devices have separate clocks** and will drift. Use one device, sync
  them digitally, or resample adaptively.

**Next:** [Chapter 58 — Your First Real-Time Program with miniaudio](58-first-realtime-program.md)
