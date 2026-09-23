# Chapter 36 — Additive Synthesis

> Fourier says any sound is a sum of sines. Additive synthesis takes that literally: build the
> sound by specifying every partial. It is the most direct method, the most expensive, and the
> only one that gives you complete control.

---

## 36.1 The method

```
   partial 1 (f, a1, φ1) ──┐
   partial 2 (2f, a2, φ2) ─┼──► SUM ──► out
   partial 3 (3f, a3, φ3) ─┤
   ...                     ┘
```

```cpp
float nextSample()
{
    double sum = 0.0;
    for (size_t k = 0; k < partials_.size(); ++k)
    {
        sum += partials_[k].amplitude * std::sin(partials_[k].phase);
        partials_[k].phase += partials_[k].increment;
        if (partials_[k].phase >= kTwoPi) partials_[k].phase -= kTwoPi;
    }
    return static_cast<float>(sum);
}
```

You did this in Chapter 10 to build a square wave and in Chapter 12 to build band-limited
references. Now it becomes an instrument.

### What it gives you that nothing else does

**Every partial is independently controllable in frequency, amplitude and phase, over time.**

That is a genuinely different kind of control:

- **Inharmonic partials** at arbitrary frequencies — bells, gongs, struck metal — like FM, but
  *exactly specified* rather than emerging from Bessel functions.
- **Independent decay per partial**, which is what real instruments do: high partials die first
  because air and materials absorb high frequencies faster (Chapter 2). A piano note's brightness
  falls continuously as it decays, and additive is the only method where you simply *state* that.
- **Perfect band-limiting by construction.** Stop adding partials at Nyquist and aliasing is
  impossible. This is why we used it as the reference in Chapters 12 and 31.
- **Morphing between analysed sounds** by interpolating partial data.

### What it costs

One sine per partial per sample. A bright note might need 100 partials; a 16-voice polyphonic
patch is 1,600 oscillators. At ~30 operations each that is **2.1 billion operations per second** —
most of a modern core, for one instrument.

This is why additive never became mainstream despite being the most powerful method. Chapter 25's
FFT offers a way out (§36.5).

---

## 36.2 Partial envelopes: the whole point

A static additive spectrum sounds like a pipe organ — which is literally what a pipe organ is, a
bank of sine-ish pipes at fixed amplitudes.

The interest is in making each partial move.

```cpp
struct Partial
{
    double frequency = 440.0;
    double phase     = 0.0;
    double increment = 0.0;

    // A multi-segment amplitude envelope: the breakpoints that define
    // this partial's life.
    std::vector<std::pair<double, double>> breakpoints;   // (time, amplitude)

    // Optional frequency drift over time -- real partials are not static.
    double detuneCents    = 0.0;
    double frequencyDrift = 0.0;
};
```

**The physical rule to encode:** higher partials decay faster.

```cpp
// A plausible decay law: partial k decays k^p times faster than the fundamental.
const double decayTime = baseDecay / std::pow(static_cast<double>(k), damping);
```

| `damping` | Behaviour |
|---|---|
| 0.0 | All partials decay together — synthetic, organ-like |
| 0.5 | Gentle brightening loss — wood, guitar |
| 1.0 | Natural — piano, most struck strings |
| 2.0 | Very fast brightness loss — muted, damped, felt |

**Rendering the same partial set with damping 0 and damping 1 is the difference between "a chord
of sine waves" and "a struck instrument".** It is one line.

---

## 36.3 Inharmonicity

Real instruments are not perfectly harmonic, and encoding that is where additive earns its keep.

**Piano strings** are stiff, which makes higher partials sharp:

```cpp
// The standard piano inharmonicity model.
// B is typically 0.0001 (bass) to 0.001 (treble).
double partialFrequency(int k, double f0, double B)
{
    return f0 * k * std::sqrt(1.0 + B * k * k);
}
```

At `B = 0.0005`, the 16th partial is about 10 cents sharp of `16·f0`. That is small, and it is
*essential* — a piano synthesised with perfectly harmonic partials sounds unmistakably fake, and
this is why. It is also why piano tuners stretch the octaves.

**Bells and struck plates** are wildly inharmonic. A classic bell's partial ratios:

```
   0.5  (the hum tone, an octave below the strike note)
   1.0  (the prime)
   1.2  (the minor third -- what makes bells sound sad)
   1.5  (the fifth)
   2.0  (the nominal -- the pitch you actually perceive)
   2.5, 2.6, 3.0, 4.2, 5.4, ...
```

Notice the **1.2 ratio** — a minor third above the prime. That interval is built into the
physics of a bell's shape, and it is why bells sound melancholy regardless of their pitch. You
can move it by changing the bell's profile, which is exactly what bell founders do.

**Tubular bells, gongs, cymbals and struck metal** are all in this family, and additive (or
modal synthesis, Chapter 39) is how you build them.

---

## 36.4 Analysis and resynthesis

The powerful workflow: **analyse a real sound into partials, modify them, resynthesise.**

1. **STFT the source** (Chapter 27).
2. **Find the peaks** in each frame's magnitude spectrum.
3. **Refine each peak's frequency** by parabolic interpolation across the three bins around it —
   this gets you far better than bin resolution:

```cpp
// Parabolic interpolation on three log-magnitude values.
// Returns the offset from the centre bin, in bins.
double peakOffset(double mLeft, double mCentre, double mRight)
{
    const double denom = mLeft - 2.0 * mCentre + mRight;
    if (std::fabs(denom) < 1e-12) return 0.0;
    return 0.5 * (mLeft - mRight) / denom;
}
```

With a 4096-point FFT at 44.1 kHz, bins are 10.8 Hz apart — hopeless for pitch. With parabolic
interpolation the accuracy improves to a fraction of a hertz. **This one function is what makes
spectral analysis usable**, and it appears again in Chapters 54 and 67.

4. **Track partials across frames** — match each peak to the nearest peak in the previous frame,
   forming continuous tracks. Births and deaths must be handled: a new partial appearing, an old
   one fading.
5. **Store** each track's frequency and amplitude trajectory.
6. **Resynthesise** with an oscillator bank, interpolating between frames.

Once you have that representation you can do things no other method allows:

| Modification | Effect |
|---|---|
| Multiply all frequencies by `r` | Pitch shift with **no formant change** |
| Stretch the time axis only | Time stretch with **no pitch change** |
| Scale partial amplitudes by a curve | Arbitrary EQ, applied to partials rather than bands |
| Randomise frequencies slightly | Chorus, or "de-tune toward inharmonicity" |
| Interpolate two sounds' partial data | **Morph** between a voice and a violin |
| Keep only partials above a threshold | Remove the noise component, leaving pure tone |

That last pair is the basis of **spectral morphing**, one of the most distinctive cinematic
techniques — a sound that begins as a human voice and becomes a machine, continuously, with no
crossfade.

**The limitation:** additive analysis models only the *sinusoidal* part. Breath, bow scrape, pick
attack and cymbal wash are noise (Chapter 15) and do not decompose into stable partials. The
standard solution is **sines + noise modelling**: subtract the resynthesised sinusoids from the
original, treat the residual as filtered noise, and synthesise both. This is what SMS (Spectral
Modelling Synthesis) does, and it is what makes convincing additive instruments possible.

---

## 36.5 Making it affordable: the inverse FFT

Direct additive synthesis is one sine per partial. The FFT gives a dramatically cheaper route.

**Instead of summing sines in the time domain, write the partials directly into a spectrum and
inverse-FFT it.**

1. Allocate an empty complex spectrum of size `N`.
2. For each partial, write its magnitude and phase into the appropriate bin — plus a few
   neighbouring bins shaped by the analysis window, so that partials between bin centres come out
   correctly (Chapter 26's leakage, used deliberately).
3. Inverse FFT.
4. Overlap-add the frames (Chapter 27).

**Cost:** `O(N log N)` per frame regardless of how many partials there are. Synthesising 500
partials costs the same as synthesising 5.

```
   partials    direct (ops/sample)    IFFT method (ops/sample)
   ------------------------------------------------------------
      10              300                    ~25
      50            1,500                    ~25
     200            6,000                    ~28
     500           15,000                    ~32
```

The crossover is around 20–30 partials. Above that, the FFT method wins decisively, and it is how
every practical additive synthesiser works.

**The catch** is latency (one frame, typically 512–2048 samples) and the difficulty of handling
fast frequency changes cleanly, since a partial that moves quickly smears across bins. For
sustained, slowly-evolving sounds — which is what additive is best at — neither is a problem.

---

## 36.6 Where additive belongs

| Good at | Poor at |
|---|---|
| Bells, gongs, chimes, struck metal | Anything noisy (needs a noise model) |
| Organs, pipes, sustained tones | Fast transients |
| Evolving pads and drones | CPU-constrained polyphony |
| Analysis/resynthesis and morphing | Quick programming — too many parameters |
| Exact band-limited references | Real-time parameter tweaking |
| Complete control over every partial | — |

**For cinematic work specifically**, additive's value is in three places:

- **Bells and metallic textures** with exactly specified inharmonicity.
- **Morphing** — the voice-becomes-machine transformation.
- **Drones** built from slowly drifting partials, which sound organic in a way that a filtered
  sawtooth never does. Chapter 84 uses this.

---

## 36.7 Exercises

**36.1** Build an additive oscillator with 64 partials and render: a sawtooth (`1/k`), a square
(odd only, `1/k`), a triangle (odd, `1/k²`). Verify against Chapter 12's shapes.

**36.2** Add per-partial decay with a `damping` exponent. Render the same partial set at damping
0, 0.5, 1 and 2. Which sounds like an instrument?

**36.3** Implement the piano inharmonicity formula. Render a note at `B = 0`, `0.0005` and
`0.002`. Which sounds most like a piano?

**36.4** Build a bell using the ratios in §36.3. Which partial do you perceive as the pitch?
(It should be the 2.0 nominal, not the 1.0 prime — a genuinely strange perceptual fact.)

**36.5** Implement parabolic peak interpolation and test it: synthesise a sine at 1000.3 Hz,
analyse with a 4096-point FFT, and see how close your estimate is with and without interpolation.

**36.6** Build a partial tracker: STFT a recorded note, find the top 20 peaks per frame, match
them across frames, and print the tracks. Handle births and deaths.

**36.7** Resynthesise from tracked partials. Then multiply every frequency by 1.5 and resynthesise
again — a formant-preserving pitch shift.

**36.8** Implement the IFFT synthesis method and benchmark it against direct summation at 10, 50,
200 and 500 partials. Find your crossover point.

**36.9** Build a spectral morph: analyse two sounds, interpolate their partial frequencies and
amplitudes over 10 seconds, resynthesise. Choose two very different sources.

---

### Chapter summary

- Additive synthesis sums sines directly, giving **independent control of every partial's
  frequency, amplitude and phase over time** — control no other method offers.
- **Per-partial decay is the point.** Higher partials must decay faster (`decay / k^damping`) or
  it sounds like an organ rather than an instrument.
- **Inharmonicity** is where additive earns its keep: piano stiffness
  (`f0·k·√(1 + B·k²)`), and bells with their characteristic **1.2 minor-third partial** that makes
  them sound melancholy.
- **Analysis/resynthesis**: STFT, peak-pick, refine with **parabolic interpolation** (essential —
  it turns 10.8 Hz bins into sub-hertz accuracy), track across frames, then modify and
  resynthesise.
- That representation enables **formant-preserving pitch shift, pitch-preserving time stretch,
  and morphing** between unrelated sounds.
- Additive models only the sinusoidal part; noise needs a separate model (**sines + noise**, as
  in SMS).
- Direct synthesis costs one sine per partial. The **inverse-FFT method** costs `O(N log N)`
  regardless of partial count, and wins above about 25 partials. This is how real additive
  synths work.
- Best at bells, organs, evolving drones and morphing; poor at transients, noise and CPU-limited
  polyphony.

**Next:** [Chapter 37 — Granular Synthesis](37-granular-synthesis.md)
