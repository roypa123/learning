# Progress

Target: **700+ pages** (400 words = 1 page). Run `bash tools/count.sh` for the live count.

**Current: 341 pages / 700.**

## Status by part

| Part | Chapters | Status |
|---|---|---|
| 0 — Before the first sound | 1–7 | **complete (84 pp)** |
| I — Making your first sounds | 8–17 | **complete (106 pp)** |
| II — DSP core | 18–29 | **complete (95 pp)** |
| III — Synthesis | 30–41 | in progress (9 of 12, 45 pp) |
| IV — Effects | 42–55 | not started |
| V — Real-time audio | 56–65 | not started |
| VI — Music, time, structure | 66–71 | not started |
| VII — Psychoacoustics & spatial | 72–79 | not started |
| VIII — Cinematic sound | 80–91 | not started |
| IX — Capstones | 92–95 | not started |
| Appendices | A–H | not started |

## Chapters written

**Part 0**
- [x] 01 What You Are About to Learn — 8 pp
- [x] 02 What Sound Actually Is — 16 pp
- [x] 03 How Hearing Works — 13 pp
- [x] 04 From Air to Numbers: Sampling and Quantization — 12 pp
- [x] 05 Setting Up Your C++ Toolchain — 9 pp
- [x] 06 C++ Crash Course, Part 1: The Language — 12 pp
- [x] 07 C++ Crash Course, Part 2: Memory, Vectors, Structs — 11 pp

**Part I**
- [x] 08 Bytes, Binary Files, and Endianness — 10 pp
- [x] 09 The WAV Format, Byte by Byte — 11 pp
- [x] 10 Your First Sine Wave — 11 pp
- [x] 11 Amplitude, Decibels, and Loudness — 11 pp
- [x] 12 Square, Saw, Triangle: The Classic Waveforms — 10 pp
- [x] 13 Envelopes: Making a Note Begin and End — 11 pp
- [x] 14 Mixing, Gain Staging, and Clipping — 9 pp
- [x] 15 Noise: White, Pink, and Brown — 10 pp
- [x] 16 Reading WAV Files Back In — 8 pp
- [x] 17 Building `libaudio` — 10 pp

**Part II**
- [x] 18 Signals and Samples: The Notation You Need — 9 pp
- [x] 19 Complex Numbers and Phasors — 9 pp
- [x] 20 Linearity, Time-Invariance, and the Impulse Response — 9 pp
- [x] 21 Convolution From Scratch — 11 pp
- [x] 22 FIR Filters and Windowed-Sinc Design — 9 pp
- [x] 23 IIR Filters and the Biquad — 9 pp
- [x] 24 Poles, Zeros, and Filter Stability — 7 pp
- [x] 25 The DFT, and Then the FFT — 9 pp
- [x] 26 Windowing and Spectral Leakage — 6 pp
- [x] 27 The STFT and Spectrograms — 6 pp
- [x] 28 Resampling and Interpolation — 5 pp
- [x] 29 Aliasing In Depth, and Oversampling — 6 pp

**Part III**
- [x] 30 Oscillator Architecture — 6 pp
- [x] 31 Band-Limited Oscillators: PolyBLEP and BLIT — 5 pp
- [x] 32 Wavetable Synthesis — 4 pp
- [x] 33 Subtractive Synthesis and the Voice — 6 pp
- [x] 34 LFOs and Modulation Routing — 5 pp
- [x] 35 FM and Phase-Modulation Synthesis — 5 pp
- [x] 36 Additive Synthesis — 5 pp
- [x] 37 Granular Synthesis — 5 pp
- [x] 38 Karplus-Strong and Digital Waveguides — 6 pp
- [ ] 39 Modal Synthesis and Resonators
- [ ] 40 Procedural Sound Design
- [ ] 41 Project: A Polyphonic Synthesizer

## libaudio

Built in Chapter 17. `cmake -S . -B build && cmake --build build`

| Header | Contents | Chapter |
|---|---|---|
| `audio/types.h` | constants, `AudioBuffer`, `midiToFrequency` | 6, 7 |
| `audio/db.h` | `gainToDb`/`dbToGain`, peak, RMS, crest, DC, NaN/clip checks, fader law | 11 |
| `audio/wav.h` | `writeWav`, `readWav`, `printWavInfo`, `WavFile` | 9, 16 |
| `audio/osc.h` | `Oscillator` (5 waveforms), `additiveSaw`/`additiveSquare` | 10, 12 |
| `audio/envelope.h` | `ADSR` | 13 |
| `audio/noise.h` | `FastRandom`, `PinkVoss`, `PinkKellett`, `Brown`, `Blue` | 15 |
| `audio/filter.h` | `DCBlocker` | 14 |
| `audio/signal.h` | impulse/step/exponential/sine generators, shift/scale/add/multiply, energy | 18 |
| `audio/convolve.h` | `convolve`, `correlate`, `autocorrelate`, `OverlapAddConvolver` | 21 |
| `audio/fir.h` | windowed-sinc design (6 windows, Kaiser from spec), `FIRFilter` | 22 |
| `audio/biquad.h` | `BiquadCoeffs` (RBJ cookbook, 8 types), `Biquad`, `BiquadCascade` | 23 |
| `audio/fft.h` | `fft`, `fftReal`, `dft`, `fastConvolve`, Parseval check | 25 |
| `audio/window.h` | 7 analysis windows, coherent/power gain | 26 |
| `audio/stft.h` | `STFT` analyse/synthesise, spectrograms, PGM/PPM output | 27 |
| `audio/resample.h` | 4 interpolators, `resample`, `VariableRateReader` | 28 |
| `audio/oversample.h` | `Oversampler`, waveshapers, `aliasingEnergyDb` | 29 |
| `audio/oscillator.h` | `Osc` with PolyBLEP, PM/FM, cents helpers | 30, 31 |
| `audio/wavetable.h` | `WavetableMipmap`, `WavetableOsc` (morphing) | 32 |
| `audio/processor.h` | `Processor` interface, `Gain`, `Chain` | 17 |
| `audio/test.h` | `CHECK`, `CHECK_CLOSE`, `summary()` | 17 |

`tests/test_main.cpp` — ~90 checks across every component.
**Examples** (`examples/`, built by CMake):
- `ch17_tour.cpp` — the whole of Part I in 70 lines
- `ch19_phasors.cpp` — rotation by j, Euler, a phasor oscillator, negative frequency
- `ch20_lti.cpp` — the linearity / time-invariance / convolution-equivalence table
- `ch21_convreverb.cpp` — synthetic room IRs and real convolution reverb
- `ch22_fir.cpp` — window and tap-count comparison, Kaiser from spec, filter sweep
- `ch23_biquad.cpp` — eight filter types, Q/resonance, Butterworth cascades, the classic sweep
- `ch24_polezero.cpp` — ASCII z-plane plots, stability triangle, resonators by pole radius
- `ch25_fft.cpp` — four-way FFT verification, leakage, DFT-vs-FFT timing, fast convolution

> **Not yet compiled.** No C++ compiler is installed on this machine (see Chapter 5).
> The library and tests are written but unverified. Install MSYS2/GCC and run
> `cmake -S . -B build && cmake --build build && ./build/bin/audio_tests`.

## Standalone programs (Chapters 5–16)

- `code/ch05/hello.cpp`, `mathcheck.cpp` — toolchain and maths check
- `code/ch06/notetable.cpp` — MIDI note / frequency / samples-per-cycle table
- `code/ch07/audiobuffer.cpp` — first AudioBuffer type
- `code/ch08/bytes.cpp` — little-endian writing + hex dump
- `code/ch09/wavwriter.{h,cpp}`, `main.cpp` — the WAV writer
- `code/ch10/sine.cpp` — sines, beating, cancellation, sweep, additive square
- `code/ch11/decibels.h`, `levels.cpp` — measurement and −6 dB ladder
- `code/ch12/waveforms.cpp` — four shapes, naive vs band-limited, aliasing, PWM
- `code/ch13/adsr.h`, `envelopes.cpp` — click vs fade, ADSR gallery, kick drum
- `code/ch14/dcblocker.h`, `mixer.h`, `mix.cpp` — clipped vs saturated vs normalised
- `code/ch15/noisegen.h`, `noise.cpp` — all noise colours, tilt measurement, wind
- `code/ch16/wavreader.{h,cpp}`, `wavinfo.cpp` — chunk-walking reader, round-trip tests
