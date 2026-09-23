# Chapter 32 — Wavetable Synthesis

> Precompute the waveform instead of calculating it. That one idea makes oscillators five times
> faster, removes aliasing almost entirely, and — as a side effect nobody anticipated in the
> 1970s — creates a whole synthesis method built on *morphing between shapes*.

---

## 32.1 The idea

A periodic waveform repeats. So compute one cycle once, store it in an array, and read it back
at whatever rate gives the pitch you want.

```cpp
const size_t tableSize = 2048;
std::vector<float> table(tableSize);

for (size_t i = 0; i < tableSize; ++i)
    table[i] = std::sin(kTwoPi * i / tableSize);

// Playback:
const double index = phase * tableSize;
out = table[static_cast<size_t>(index)];        // plus interpolation
```

That replaces a `std::sin` call (~20–40 operations) with an array read and an interpolation
(~8 operations). Chapter 30 counted 192 oscillators in a typical polyphonic synth; this is the
single biggest saving available.

**But the table read needs interpolation**, because `phase · tableSize` is almost never an
integer — which is Chapter 28's entire subject, arriving exactly where it is needed.

| Interpolation | Quality | Cost |
|---|---|---|
| Truncate | Awful (−25 dB noise) | 3 ops |
| **Linear** | Good with a large table | 8 ops |
| Hermite | Excellent | 14 ops |
| Sinc | Overkill here | 40+ ops |

**Linear interpolation plus a 2048-entry table is the standard combination.** Chapter 28 said
linear degrades near Nyquist — but here "Nyquist" means relative to the *table*, and with 2048
points per cycle the content is nowhere near it. Doubling the table size buys about 6 dB.

---

## 32.2 The mipmap: why wavetables do not alias

Here is the part that makes wavetables better than PolyBLEP.

A sawtooth at 100 Hz can have 220 harmonics below Nyquist. At 5 kHz it can have 4. **So they need
different tables.**

Store a set of tables — a **mipmap**, borrowing the term from graphics — each band-limited for a
different pitch range:

```
   table[0]:  fundamental 20 Hz .. 40 Hz     -> up to 551 harmonics
   table[1]:  40 .. 80 Hz                    -> up to 275 harmonics
   table[2]:  80 .. 160 Hz                   -> up to 137 harmonics
   ...
   table[9]:  5120 .. 10240 Hz               -> up to 2 harmonics
   table[10]: 10240 Hz and above             -> 1 harmonic (a sine)
```

Each is generated additively (Chapter 12), summing only harmonics that fit below Nyquist for the
**top** of that table's range. Playing anywhere within the range is then guaranteed alias-free.

```cpp
std::vector<std::vector<float>> buildMipmap(Waveform shape, double sampleRate,
                                            size_t tableSize = 2048,
                                            int numTables = 11)
{
    std::vector<std::vector<float>> tables;
    const double nyquist = sampleRate * 0.5;

    double topFreq = 20.0;

    for (int t = 0; t < numTables; ++t)
    {
        topFreq *= 2.0;                                  // one octave per table

        // How many harmonics fit below Nyquist at the TOP of this range?
        int maxHarmonic = static_cast<int>(nyquist / topFreq);
        maxHarmonic = std::max(1, maxHarmonic);

        std::vector<float> table(tableSize, 0.0f);

        for (int h = 1; h <= maxHarmonic; ++h)
        {
            double amp = 0.0;
            switch (shape)
            {
                case Waveform::Saw:
                    amp = (2.0 / kPi) * ((h % 2) ? 1.0 : -1.0) / h;
                    break;
                case Waveform::Square:
                    if (h % 2 == 0) continue;            // odd harmonics only
                    amp = (4.0 / kPi) / h;
                    break;
                case Waveform::Triangle:
                    if (h % 2 == 0) continue;
                    amp = (8.0 / (kPi * kPi)) * (((h - 1) / 2) % 2 ? -1.0 : 1.0)
                        / (h * h);
                    break;
                default:
                    amp = (h == 1) ? 1.0 : 0.0;          // sine
                    break;
            }

            for (size_t i = 0; i < tableSize; ++i)
                table[i] += static_cast<float>(
                    amp * std::sin(kTwoPi * h * i / tableSize));
        }

        tables.push_back(std::move(table));
    }

    return tables;
}
```

**This is expensive** — 11 tables × 2048 samples × up to 550 harmonics — but it runs **once, at
startup**. At play time it is a table lookup.

That is the trade: memory and startup cost, in exchange for runtime speed and near-perfect
quality. 11 tables × 2048 floats is 90 KB per waveform, which is nothing.

### Choosing the table

```cpp
size_t tableIndexFor(double frequency) const
{
    size_t idx = 0;
    double top = 40.0;
    while (idx < tables_.size() - 1 && frequency >= top)
    {
        top *= 2.0;
        ++idx;
    }
    return idx;
}
```

**The switch is audible if you do it abruptly.** Moving between tables changes the harmonic
content instantly — a discontinuity, which Chapter 13 told you is a click. For a glissando or a
pitch bend crossing a table boundary, that click is obvious.

**The fix: crossfade between adjacent tables** near the boundary:

```cpp
// Read from both neighbouring tables and blend.
const double blend = (logFreq - tableBottom) / (tableTop - tableBottom);
out = lerp(readTable(idx, phase), readTable(idx + 1, phase), blend);
```

Doubles the read cost in the crossfade region, and it is what good implementations do. Some go
further and crossfade across the entire range, at double cost throughout.

---

## 32.3 Morphing: the reason wavetables are a *synthesis method*

Everything so far treats a wavetable as a faster oscillator. The interesting part is different.

**If you can crossfade between a saw table and a square table, you can crossfade between
*anything*.**

Store a *series* of related waveforms and sweep a "position" control through them:

```
   position 0.0:  sine
   position 0.25: triangle
   position 0.5:  square
   position 0.75: saw
   position 1.0:  something noisy and harsh
```

Modulate that position with an LFO or an envelope and you get timbral movement that no
subtractive synth can produce — not a filter sweep, but the waveform itself changing shape.

This is the entire basis of the PPG Wave, the Waldorf Microwave, Native Instruments Massive and
Xfer Serum, and it is the dominant synthesis method in modern electronic and cinematic music.

```cpp
float readMorph(double phase, double position)
{
    const double scaled = position * (numFrames_ - 1);
    const size_t frameA = static_cast<size_t>(scaled);
    const size_t frameB = std::min(frameA + 1, numFrames_ - 1);
    const double blend  = scaled - frameA;

    const float a = readFrame(frameA, phase);
    const float b = readFrame(frameB, phase);

    return static_cast<float>(a * (1.0 - blend) + b * blend);
}
```

Two reads and a blend. Note that **each frame needs its own mipmap**, so a 64-frame wavetable
with 11 mip levels is 64 × 11 × 2048 floats — 5.8 MB per wavetable. Manageable, but it explains
why wavetable synths have noticeable load times.

### Why linear crossfading is not ideal

Crossfading two waveforms linearly in the time domain can cause **cancellation** if their phases
disagree — Chapter 2's destructive interference. Halfway between a saw and its inverse you get
silence.

Two mitigations, both used in practice:

- **Phase-align the frames** when building the wavetable, so corresponding features line up.
- **Morph in the frequency domain**: interpolate magnitudes and phases separately, then inverse
  FFT. More expensive, much smoother, and it is what high-end wavetable synths do.

This is the same phase-coherence problem as Chapter 27's STFT resynthesis, in a different costume.

---

## 32.4 Building tables from audio

You can make a wavetable from a recording, and it is a standard sound-design technique:

1. Take a sample — a voice, a violin, a piece of metal.
2. Chop it into `N` chunks of exactly one cycle each, at the detected pitch.
3. Resample each chunk to the table size (Chapter 28).
4. Optionally phase-align them and band-limit each into a mipmap.
5. Sweeping the position now plays *through the timbral evolution of the original sound*,
   at any pitch and any speed.

This is how "vocal" and "organic" wavetables are made, and it is why a wavetable synth can sound
like a choir that you can freeze mid-vowel.

**The pitch detection step is the hard one** — the cycle boundaries must be found accurately or
the frames do not align and the morph sounds like garbage. Autocorrelation (Chapter 21) is the
usual tool.

---

## 32.5 Performance notes

**Table size.** 2048 is standard. 256 is audibly gritty with linear interpolation; 4096 buys
about 6 dB for twice the memory. Powers of two let you index with a bit shift when using
fixed-point phase (Chapter 30).

**Cache behaviour matters more than operation count.** A 2048-float table is 8 KB, which fits in
L1 cache. Sixty-four voices reading *different* tables do not all fit, and the resulting cache
misses can cost more than the arithmetic. This is a real effect in large synths, and it is why
some engines deliberately share tables between voices.

**Fixed-point indexing** (Chapter 30) is a natural fit:

```cpp
phase += increment;                                   // uint32_t, wraps for free
const uint32_t idx  = phase >> (32 - 11);             // top 11 bits -> 0..2047
const uint32_t frac = (phase << 11) >> 8;             // next bits -> interpolation
```

No branches, no floating-point conversion, exact wrapping.

**The measured comparison**, 192 oscillators for 10 seconds:

```
  method                        time      relative    aliasing
  ------------------------------------------------------------
  std::sin                     2.84 s        1.00x     n/a (sine)
  naive saw                    0.31 s        9.2x      -14 dB
  PolyBLEP saw                 0.94 s        3.0x      -49 dB
  wavetable, linear interp     0.58 s        4.9x      -92 dB
  wavetable, no interp         0.24 s       11.8x      -25 dB
```

**The wavetable is both faster than PolyBLEP and far cleaner.** That is why it wins for
production synths, and the only costs are memory and startup time.

---

## 32.6 Exercises

**32.1** Build a 2048-entry sine table and compare it against `std::sin` for accuracy and speed.
How large is the error with linear interpolation? With truncation?

**32.2** Measure the aliasing of a single-table sawtooth (built for 20 Hz) played at 20 Hz,
200 Hz, 2 kHz and 5 kHz. Then build the mipmap and repeat.

**32.3** *Deliberate breakage.* Switch tables abruptly during a slow pitch glide from 100 Hz to
1 kHz. Can you hear the switches? Now add crossfading.

**32.4** Build a 5-frame morphing wavetable (sine → triangle → square → saw → pulse) and render a
slow sweep through the position. Then modulate the position with a 0.3 Hz LFO.

**32.5** Demonstrate the cancellation problem: morph between a saw and an inverted saw. What
happens at position 0.5? Fix it by phase-aligning.

**32.6** Build a wavetable from a recorded sample: detect the pitch with autocorrelation, extract
32 single-cycle frames, resample each to 2048, and morph through them.

**32.7** Compare table sizes of 256, 512, 2048 and 8192 with linear interpolation. Measure the
noise floor for each. How many dB per doubling?

**32.8** Implement fixed-point table indexing and verify it is bit-exact over 100 million
samples. Benchmark it against the floating-point version.

**32.9** Implement frequency-domain morphing: FFT two frames, interpolate magnitudes and phases
separately, inverse FFT. Compare with time-domain crossfading on a difficult pair.

---

### Chapter summary

- A wavetable trades **memory and startup cost for runtime speed**: ~8 operations instead of
  ~40 for a sine, and far better aliasing than PolyBLEP.
- Table reads need **interpolation** (Chapter 28). Linear plus a **2048-entry** table is the
  standard combination; each doubling of table size buys about 6 dB.
- The **mipmap** — one band-limited table per octave, each generated additively with only the
  harmonics that fit below Nyquist — is what makes wavetables essentially alias-free
  (**−92 dB**).
- **Crossfade between adjacent tables** near the boundaries, or pitch glides click.
- **Morphing between stored frames** turns a fast oscillator into a synthesis method: timbral
  change that no filter can produce. Each frame needs its own mipmap, so memory grows quickly.
- **Linear morphing can cancel** if frames are out of phase. Phase-align the frames, or morph in
  the frequency domain.
- Wavetables can be **built from recordings** by detecting the pitch, extracting single cycles,
  and resampling — the source of "organic" and vocal wavetables.
- Wavetables are **both faster and cleaner than PolyBLEP**. Watch cache behaviour with many
  voices; fixed-point indexing is a natural fit.

**Next:** [Chapter 33 — Subtractive Synthesis and the Voice](33-subtractive-synthesis.md)
