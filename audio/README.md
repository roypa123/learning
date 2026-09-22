# Audio Programming in C++
### From First Principles to Cinematic Sound

A complete, self-contained course. It assumes **you have never written a line of code**
and that you know **nothing about sound**. It ends with you building a cinematic audio
engine — the kind of thing that sits underneath a film trailer or a game.

---

## How this book works

**Everything is in order.** Chapter *n* only uses ideas from chapters *1 .. n-1*.
If you read straight through and type every example, nothing will ever appear out of nowhere.

**Every line of code is explained.** Not "this initialises the buffer" — but *what a buffer is,
why it has that size, what happens in memory, what breaks if you change it, and what it sounds
like when it breaks.* Code blocks are followed by line-by-line commentary.

**You will hear everything you build.** For the first four parts we do not use any library at
all. We write `.wav` files with plain C++ and open them in any media player. This means:

- No installation problems.
- No "it works on my machine".
- You can inspect every byte you produce.

Real-time audio (sound that comes out of the speakers *as* your program runs) arrives in
Part V, once you understand what a sample actually is. That order is deliberate — real-time
audio bugs are brutal to debug while you are still unsure what a sample is.

### Conventions

| Marker | Meaning |
|---|---|
| **Theory** | Concept explanation. No code. Do not skip these; the code is meaningless without them. |
| **Code** | A complete, compilable program or file — never a fragment you have to guess at. |
| **Walkthrough** | Line-by-line explanation of the code just above. |
| **Listen** | What you should hear, and what it means if you hear something else. |
| **Experiment** | Change a number, predict the result, then check. This is where learning happens. |
| **Pitfall** | A mistake almost everyone makes, and how to recognise it. |
| **Exercise** | Do it before moving on. Solutions in `code/solutions/`. |

### A note on "pages"

This book is plain Markdown, so it has no fixed page count. We use the publishing
convention of **400 words per page**. `tools/count.sh` reports the running total.
The target is **700+ pages**; the outline below is sized for roughly 800.

### Building the library

From Chapter 17 onward everything lives in one CMake project:

```bash
cmake -S . -B build          # configure (once)
cmake --build build          # build library, tests and examples
./build/bin/audio_tests      # run the test suite
./build/bin/ch17_tour        # run an example
```

Chapters 5-16 use standalone programs in `code/chNN/`, compiled directly:

```bash
g++ -std=c++17 -Wall -Wextra -O2 sine.cpp ../ch09/wavwriter.cpp -o sine.exe
```

### Folder layout

```
audio/
  README.md          <- you are here (master table of contents)
  PROGRESS.md        <- which chapters are written, running page count
  CMakeLists.txt     <- top-level build (Chapter 17 onward)
  book/              <- the chapters, in reading order
  code/              <- standalone programs for Chapters 5-16
    ch05/ ch06/ ...  <- one folder per chapter
  lib/               <- libaudio: the library we build in Chapter 17
    include/audio/   <- public headers
    src/             <- implementation
  tests/             <- the test suite
  examples/          <- programs that link libaudio
  assets/            <- .wav files your programs render
  tools/             <- helper scripts (word count)
```

---

# Table of Contents

## Part 0 — Before the first sound

Sound, hearing, numbers, and enough C++ to be dangerous.

1. [What You Are About to Learn](book/01-what-you-are-about-to-learn.md)
2. [What Sound Actually Is](book/02-what-sound-actually-is.md)
3. [How Hearing Works](book/03-how-hearing-works.md)
4. [From Air to Numbers: Sampling and Quantization](book/04-from-air-to-numbers.md)
5. [Setting Up Your C++ Toolchain](book/05-setting-up-your-toolchain.md)
6. [C++ Crash Course, Part 1: The Language](book/06-cpp-crash-course-1.md)
7. [C++ Crash Course, Part 2: Memory, Vectors, and Structs](book/07-cpp-crash-course-2.md)

## Part I — Making your first sounds

By the end of this part you have written a synthesiser that renders audio files.

8. [Bytes, Binary Files, and Endianness](book/08-bytes-and-binary-files.md)
9. [The WAV Format, Byte by Byte](book/09-the-wav-format.md)
10. [Your First Sine Wave](book/10-your-first-sine-wave.md)
11. [Amplitude, Decibels, and Loudness](book/11-amplitude-and-decibels.md)
12. [Square, Saw, Triangle: The Classic Waveforms](book/12-classic-waveforms.md)
13. [Envelopes: Making a Note Begin and End](book/13-envelopes.md)
14. [Mixing, Gain Staging, and Clipping](book/14-mixing-and-clipping.md)
15. [Noise: White, Pink, and Brown](book/15-noise.md)
16. [Reading WAV Files Back In](book/16-reading-wav-files.md)
17. [Building `libaudio`: Your Own Audio Library](book/17-building-libaudio.md)

## Part II — Digital signal processing core

The mathematics of audio, derived from scratch and implemented by hand.

18. [Signals and Samples: The Notation You Need](book/18-signals-and-samples.md)
19. [Complex Numbers and Phasors](book/19-complex-numbers-and-phasors.md)
20. [Linearity, Time-Invariance, and the Impulse Response](book/20-lti-and-impulse-response.md)
21. [Convolution From Scratch](book/21-convolution-from-scratch.md)
22. [FIR Filters and Windowed-Sinc Design](book/22-fir-filters.md)
23. [IIR Filters and the Biquad](book/23-iir-filters-and-biquads.md)
24. [Poles, Zeros, and Filter Stability](book/24-poles-zeros-stability.md)
25. [The DFT, and Then the FFT](book/25-dft-and-fft.md)
26. [Windowing and Spectral Leakage](book/26-windowing.md)
27. [The STFT and Spectrograms](book/27-stft-and-spectrograms.md)
28. [Resampling and Interpolation](book/28-resampling.md)
29. [Aliasing In Depth, and Oversampling](book/29-aliasing-and-oversampling.md)

## Part III — Synthesis

Building instruments.

30. [Oscillator Architecture](book/30-oscillator-architecture.md)
31. [Band-Limited Oscillators: PolyBLEP and BLIT](book/31-band-limited-oscillators.md)
32. [Wavetable Synthesis](book/32-wavetable-synthesis.md)
33. [Subtractive Synthesis and the Voice](book/33-subtractive-synthesis.md)
34. [LFOs and Modulation Routing](book/34-lfos-and-modulation.md)
35. [FM and Phase-Modulation Synthesis](book/35-fm-synthesis.md)
36. [Additive Synthesis](book/36-additive-synthesis.md)
37. [Granular Synthesis](book/37-granular-synthesis.md)
38. [Karplus-Strong and Digital Waveguides](book/38-karplus-strong-waveguides.md)
39. [Modal Synthesis and Resonators](book/39-modal-synthesis.md)
40. [Procedural Sound Design: Wind, Rain, Fire, Engines](book/40-procedural-sound-design.md)
41. [Project: A Polyphonic Synthesizer](book/41-project-polyphonic-synth.md)

## Part IV — Effects

Processing sound.

42. [Delay Lines and Circular Buffers](book/42-delay-lines.md)
43. [Echo, Feedback, and Stability](book/43-echo-and-feedback.md)
44. [Chorus, Flanger, and Fractional Delay](book/44-chorus-and-flanger.md)
45. [Phasers and Allpass Filters](book/45-phaser-and-allpass.md)
46. [Reverb I: Schroeder, Comb Filters, and Freeverb](book/46-reverb-schroeder.md)
47. [Reverb II: Feedback Delay Networks](book/47-reverb-fdn.md)
48. [Reverb III: Convolution](book/48-reverb-convolution.md)
49. [Reverb IV: Partitioned Low-Latency Convolution](book/49-reverb-partitioned.md)
50. [Dynamics I: Envelope Followers and Compressors](book/50-dynamics-compressor.md)
51. [Dynamics II: Limiters, Gates, Sidechains, Multiband](book/51-dynamics-advanced.md)
52. [Distortion, Saturation, and Waveshaping](book/52-distortion-and-saturation.md)
53. [Equalisation: Shelves, Bells, and Linear Phase](book/53-equalisation.md)
54. [Pitch Shifting and Time Stretching](book/54-pitch-and-time.md)
55. [Vocoders, Formants, and Voice Processing](book/55-vocoder-and-formants.md)

## Part V — Real-time audio

Sound out of the speakers, right now, without glitching.

56. [What "Real-Time" Actually Means](book/56-what-realtime-means.md)
57. [Audio APIs on Windows: WASAPI, ASIO, and Friends](book/57-audio-apis.md)
58. [Your First Real-Time Program with miniaudio](book/58-first-realtime-program.md)
59. [The Real-Time Rules: No Malloc, No Locks, No Excuses](book/59-realtime-rules.md)
60. [Lock-Free Ring Buffers and Thread Communication](book/60-lock-free-communication.md)
61. [Parameter Smoothing and Click-Free Automation](book/61-parameter-smoothing.md)
62. [MIDI Input](book/62-midi-input.md)
63. [Optimisation: Profiling, SIMD, and Cache](book/63-optimisation-and-simd.md)
64. [Graph-Based Audio Engines](book/64-audio-graphs.md)
65. [Audio Plugins: VST3, AU, and CLAP](book/65-audio-plugins.md)

## Part VI — Music, time, and structure

66. [Pitch, Tuning Systems, and Cents](book/66-pitch-and-tuning.md)
67. [Tempo and Sample-Accurate Timing](book/67-tempo-and-timing.md)
68. [Building a Sequencer](book/68-building-a-sequencer.md)
69. [Sample Playback and Disk Streaming](book/69-sample-playback.md)
70. [Algorithmic and Generative Music](book/70-generative-music.md)
71. [Adaptive Music: Layers, Transitions, Stingers](book/71-adaptive-music.md)

## Part VII — Psychoacoustics and spatial audio

Where sound stops being physics and becomes perception.

72. [Psychoacoustics: Critical Bands, Masking, Loudness](book/72-psychoacoustics.md)
73. [How We Localise Sound: ITD, ILD, and the HRTF](book/73-localisation.md)
74. [Panning Laws](book/74-panning-laws.md)
75. [Stereo Imaging, Mid/Side, and Width](book/75-stereo-imaging.md)
76. [Binaural Rendering with HRTFs](book/76-binaural-rendering.md)
77. [Distance, Air Absorption, and Doppler](book/77-distance-and-doppler.md)
78. [Occlusion, Diffraction, and Reflections](book/78-occlusion-and-reflections.md)
79. [Ambisonics](book/79-ambisonics.md)

## Part VIII — Cinematic sound

The craft, the theory, and the code behind sound that moves people.

80. [What Makes Sound Cinematic](book/80-what-makes-sound-cinematic.md)
81. [Anatomy of a Soundtrack: Dialogue, Foley, FX, Ambience, Music](book/81-anatomy-of-a-soundtrack.md)
82. [Layering: One Impact From Twelve Sounds](book/82-layering.md)
83. [The Low End: Subs, Drops, and Physical Fear](book/83-the-low-end.md)
84. [Risers, Whooshes, Braams, and Drones — In Code](book/84-risers-and-braams.md)
85. [Impacts and Transient Design](book/85-impacts-and-transients.md)
86. [Time Design: Tension, Release, and Silence](book/86-time-design.md)
87. [Immersive Formats: 5.1, 7.1, and Atmos](book/87-immersive-formats.md)
88. [Building an Object-Based Renderer](book/88-object-based-renderer.md)
89. [Mixing for Picture: LUFS, Dynamic Range, Stems](book/89-mixing-for-picture.md)
90. [Sound to Picture: Timecode and Tooling](book/90-sound-to-picture.md)
91. [Project: A Cinematic Audio Engine](book/91-project-cinematic-engine.md)

## Part IX — Capstones

92. [Capstone: Command-Line Synth and Effects Rack](book/92-capstone-synth-rack.md)
93. [Capstone: A 3D Game Audio Engine](book/93-capstone-3d-engine.md)
94. [Capstone: A Procedural Trailer Cue Generator](book/94-capstone-trailer-cue.md)
95. [Where to Go Next](book/95-where-to-go-next.md)

## Appendices

- A. [Mathematics Refresher](book/A-maths-refresher.md)
- B. [C++ Quick Reference for Audio](book/B-cpp-reference.md)
- C. [Decibel and Frequency Tables](book/C-tables.md)
- D. [The Filter Cookbook](book/D-filter-cookbook.md)
- E. [Audio File Formats](book/E-file-formats.md)
- F. [Troubleshooting: "It Sounds Wrong"](book/F-troubleshooting.md)
- G. [Glossary](book/G-glossary.md)
- H. [Further Reading](book/H-further-reading.md)

---

*Start here: [Chapter 1 — What You Are About to Learn](book/01-what-you-are-about-to-learn.md)*
