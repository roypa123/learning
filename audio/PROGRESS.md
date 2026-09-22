# Progress

Target: **700+ pages** (400 words = 1 page). Run `bash tools/count.sh` for the live count.

**Current: 130 pages / 700.**

## Status by part

| Part | Chapters | Status |
|---|---|---|
| 0 — Before the first sound | 1–7 | **complete (84 pp)** |
| I — Making your first sounds | 8–17 | in progress (4 of 10) |
| II — DSP core | 18–29 | not started |
| III — Synthesis | 30–41 | not started |
| IV — Effects | 42–55 | not started |
| V — Real-time audio | 56–65 | not started |
| VI — Music, time, structure | 66–71 | not started |
| VII — Psychoacoustics & spatial | 72–79 | not started |
| VIII — Cinematic sound | 80–91 | not started |
| IX — Capstones | 92–95 | not started |
| Appendices | A–H | not started |

## Chapters written

- [x] 01 What You Are About to Learn — 8 pp
- [x] 02 What Sound Actually Is — 16 pp
- [x] 03 How Hearing Works — 13 pp
- [x] 04 From Air to Numbers: Sampling and Quantization — 12 pp
- [x] 05 Setting Up Your C++ Toolchain — 9 pp
- [x] 06 C++ Crash Course, Part 1: The Language — 12 pp
- [x] 07 C++ Crash Course, Part 2: Memory, Vectors, Structs — 11 pp
- [x] 08 Bytes, Binary Files, and Endianness — 10 pp
- [x] 09 The WAV Format, Byte by Byte — 11 pp
- [x] 10 Your First Sine Wave — 11 pp
- [x] 11 Amplitude, Decibels, and Loudness — 11 pp
- [ ] 12 Square, Saw, Triangle: The Classic Waveforms
- [ ] 13 Envelopes
- [ ] 14 Mixing, Gain Staging, and Clipping
- [ ] 15 Noise
- [ ] 16 Reading WAV Files Back In
- [ ] 17 Building libaudio

## Code written

- `code/ch05/hello.cpp` — toolchain check
- `code/ch05/mathcheck.cpp` — maths library check
- `code/ch06/notetable.cpp` — MIDI note / frequency / samples-per-cycle table
- `code/ch07/audiobuffer.cpp` — first AudioBuffer type with peak/RMS/normalise
- `code/ch08/bytes.cpp` — little-endian writing + hex dump
- `code/ch09/wavwriter.{h,cpp}` — the WAV writer used by everything after
- `code/ch09/main.cpp` — silence, ramp, stereo ramp
- `code/ch10/sine.cpp` — sines, beating, cancellation, sweep, additive square
- `code/ch11/decibels.h` — dB conversions, peak/RMS/crest/DC measurement
- `code/ch11/levels.cpp` — measurement demo and −6 dB ladder
