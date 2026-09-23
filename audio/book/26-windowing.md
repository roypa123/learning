# Chapter 26 — Windowing and Spectral Leakage

> Chapter 25 showed a 440 Hz sine smearing across dozens of FFT bins and promised a fix. This
> chapter delivers it, and in doing so explains the deepest trade-off in signal processing —
> the one you have now met four times under four different names.

---

## 26.1 The implicit assumption

The DFT does not analyse your block of `N` samples in isolation. It analyses a signal that is
**periodic with period `N`** — it assumes your block repeats forever in both directions.

```
   What you gave it:        [-----block-----]

   What it analyses:  ...][-----block-----][-----block-----][...
```

If the signal's frequency fits a whole number of cycles into the block, the repetition is
seamless:

```
   4 whole cycles in the block:
   \/\/\/\/|\/\/\/\/|\/\/\/\/        <- joins perfectly
```

If it does not, the repetition has a **discontinuity** at every join:

```
   4.5 cycles in the block:
   \/\/\/\/-|\/\/\/\/-|\/\/\/\/-     <- a jump at each join
            ^          ^
```

And Chapter 13 told you what a discontinuity is: **broadband energy**. That is spectral leakage.
It is not a flaw in the FFT; it is the FFT correctly reporting the spectrum of the signal it was
actually given.

### Why this always happens in practice

For the leakage to vanish, the frequency must be exactly `k·fs/N` for an integer `k`. Real
signals never oblige. A 440 Hz tone at 44,100 Hz with `N = 1024` needs `440 × 1024 / 44100 =
10.216` cycles. Not an integer. Leakage.

So **every practical spectrum has leakage**, and the question is only how much and what shape.

---

## 26.2 What leakage costs you

Leakage does three specific kinds of damage, and each one matters for a different application.

**1. It hides quiet components near loud ones.** A quiet 500 Hz tone next to a loud 440 Hz tone
disappears under the loud tone's skirts. In a spectrum analyser you simply do not see it.

**2. It corrupts magnitude readings.** A tone falling between bins splits its energy between
them, so the peak bin reads **low** — by up to 3.9 dB with no window. Measure a signal's level
from an unwindowed FFT and you will under-read it by an unpredictable amount.

**3. It ruins phase-based processing.** Pitch shifters, vocoders and spectral effects
(Chapters 54–55) reconstruct signals from FFT bins. Leakage puts energy in bins that do not
belong to any real partial, and reconstructing from those produces a smeared, metallic artefact
— the characteristic sound of a cheap pitch-shifter.

---

## 26.3 The fix: taper the edges

Multiply the block by a **window** that falls smoothly to zero at both ends:

```
   Signal:     ~~~~~~~~~~~~~~~~
   Window:     .-''''''''''-.
   Product:    ..-~~~~~~~~-..        <- no discontinuity at the joins
```

Now the assumed repetition is continuous, because both ends are zero. The discontinuity is gone,
and so is most of the leakage.

**But you have changed the signal.** You multiplied it by something, and Chapter 21's convolution
theorem says multiplication in time is convolution in frequency. So the true spectrum has been
convolved with the *window's* spectrum.

This means the window's spectrum is now the thing that matters, and every window is a compromise
between two features of that spectrum.

---

## 26.4 Main lobe and sidelobes

Every window's spectrum has the same general shape:

```
   dB
    0 |          /\                     <- MAIN LOBE
      |         /  \                       (width = frequency resolution)
  -20 |        /    \
      |       /      \   .-.
  -40 |      /        \./   \.-.        <- SIDELOBES
      |     /                   \.-._     (height = leakage floor)
  -60 |    /                         '--._
      +-------------------------------------> frequency
              |<-- width -->|
```

**Main lobe width** determines **frequency resolution** — how close two tones can be and still be
seen as separate peaks. Narrow is good.

**Sidelobe level** determines the **leakage floor** — how far down the spurious energy sits. Low
is good.

> **The trade-off, stated once and for all: you cannot have both.** A window with lower sidelobes
> always has a wider main lobe. This is not an engineering limitation; it is a mathematical
> theorem about functions and their transforms.

### The catalogue

| Window | Main lobe (bins) | Peak sidelobe | Sidelobe roll-off | Scalloping loss | Use |
|---|---|---|---|---|---|
| **Rectangular** | 2 | −13 dB | −6 dB/oct | **3.92 dB** | Transient analysis; never for spectra |
| **Hann** | 4 | −31 dB | −18 dB/oct | 1.42 dB | The default. STFT, resynthesis |
| **Hamming** | 4 | −43 dB | −6 dB/oct | 1.78 dB | Speech; better peak sidelobe than Hann |
| **Blackman** | 6 | −58 dB | −18 dB/oct | 1.10 dB | Wide dynamic range |
| **Blackman–Harris** | 8 | −92 dB | −6 dB/oct | 0.83 dB | Measurement, distortion analysis |
| **Kaiser (β=8.6)** | ~7 | −65 dB | adjustable | ~0.9 dB | Adjustable; good default for design |
| **Flat-top** | 10 | −93 dB | — | **0.01 dB** | Calibration: accurate amplitude, terrible resolution |

**Scalloping loss** is the last column and it is underappreciated: it is how much a peak reads
low when a tone falls exactly *between* two bins. With no window that is **3.92 dB** — a
substantial measurement error. Hann reduces it to 1.42 dB; the flat-top window reduces it to
0.01 dB, which is exactly why instrument calibration uses it despite its dreadful resolution.

**Hann vs Hamming** is a genuinely interesting pair. Hamming has a *lower first sidelobe*
(−43 dB vs −31 dB) but its sidelobes fall off slowly (−6 dB/octave), so far from the peak Hann
is much cleaner. If you care about a component right next to a loud one, use Hamming; if you care
about a component far away from it, use Hann.

---

## 26.5 Implementation and the scaling correction

Applying a window is one multiply per sample:

```cpp
for (size_t n = 0; n < N; ++n)
    windowed[n] = x[n] * window[n];
```

But the window has reduced the signal's energy, so magnitudes come out low. You must correct for
it, and **which correction depends on what you are measuring**:

```cpp
// Coherent gain: the mean of the window. Use for SINE amplitudes.
double coherentGain(const std::vector<double>& w)
{
    double s = 0.0;
    for (double v : w) s += v;
    return s / static_cast<double>(w.size());
}

// Power gain: the RMS of the window. Use for NOISE levels.
double powerGain(const std::vector<double>& w)
{
    double s = 0.0;
    for (double v : w) s += v * v;
    return std::sqrt(s / static_cast<double>(w.size()));
}
```

| Window | Coherent gain | Power gain |
|---|---|---|
| Rectangular | 1.000 | 1.000 |
| Hann | 0.500 | 0.612 |
| Hamming | 0.540 | 0.635 |
| Blackman | 0.420 | 0.509 |
| Blackman–Harris | 0.359 | 0.449 |

So for a Hann window, a sine's true amplitude is the measured bin amplitude divided by 0.5 —
i.e. **+6 dB of correction**. Forgetting this makes every measurement 6 dB low, which looks like
a gain bug and is one of the most common spectrum-analyser errors.

**Two different corrections for two different things.** Use coherent gain for discrete tones,
power gain for broadband noise. Using the wrong one introduces a ~1.7 dB error, which is enough
to matter in measurement work and not enough to be obvious.

### Periodic versus symmetric

A subtlety that catches people:

```cpp
// SYMMETRIC (for FIR filter design -- Chapter 22)
w[n] = 0.5 - 0.5 * cos(2π·n / (N-1));

// PERIODIC (for FFT analysis -- this chapter)
w[n] = 0.5 - 0.5 * cos(2π·n / N);
```

Note `N-1` versus `N`. The symmetric form has equal values at both ends and is right for filter
design, where symmetry gives linear phase. The periodic form is right for the FFT, where the
block is assumed to repeat — using the symmetric form there produces a tiny discontinuity at the
wrap point and slightly worse leakage.

The difference is small but real. Chapter 22 used symmetric; this chapter uses periodic. Both are
correct for their purpose, and a library that offers only one will subtly misbehave in the other
role.

---

## 26.6 Seeing it work

`examples/ch26_window.cpp` measures the leakage directly.

**A tone exactly at bin centre** (`k = 40`, so 1723.2 Hz at `N = 1024`):

```
  window            peak bin   amplitude    leakage floor   bins > -60 dB
  --------------------------------------------------------------------------
  Rectangular            40      0.5000        -313.2 dB            3
  Hann                   40      0.5000        -310.1 dB            5
  Blackman               40      0.5000        -306.8 dB            7
```

At an exact bin centre there is no leakage at all — even with no window. The repetition is
seamless, so nothing is smeared. Note the main lobe width showing up as the "bins > −60 dB"
count: rectangular occupies 3, Blackman 7. That is the resolution cost of the window.

**A tone halfway between bins** (`k = 40.5`):

```
  window            peak bin   amplitude   error (dB)   leakage floor   bins > -60 dB
  ----------------------------------------------------------------------------------
  Rectangular            40      0.3183      -3.92          -13.3 dB          412
  Hann                   40      0.4247      -1.42          -31.5 dB           31
  Hamming                40      0.4073      -1.78          -42.7 dB           27
  Blackman               40      0.4407      -1.10          -58.4 dB           19
  BlackmanHarris         40      0.4543      -0.83          -91.6 dB           13
  Flat-top               40      0.4999      -0.01          -93.1 dB           23
```

**Every number in that table is a claim from §26.4, measured.** The rectangular window reads
3.92 dB low and spreads energy across 412 of 513 bins. Blackman–Harris reads 0.83 dB low and
touches 13. The flat-top window reads the amplitude essentially perfectly (0.01 dB error) while
using 23 bins to do it.

**Two tones, one loud and one quiet.** A 1 kHz tone at 0 dB and a 1.2 kHz tone at −60 dB:

```
  Rectangular:    the -60 dB tone is INVISIBLE (buried at -13 dB sidelobes)
  Hann:           visible, but sitting on a -31 dB skirt
  Blackman:       cleanly visible
  BlackmanHarris: cleanly visible with 30 dB of margin
```

This is the practical reason distortion analysers use Blackman–Harris: to see a harmonic 80 dB
below the fundamental, you need sidelobes below −80 dB.

---

## 26.7 The trade-off, for the fourth time

You have now met the same principle in four guises. It is worth collecting them, because
recognising it saves a great deal of confusion later.

| Chapter | Appearance | Short version |
|---|---|---|
| 13 | Fade length vs click | A transition of `T` seconds splatters up to `1/T` Hz |
| 22 | FIR taps vs transition width | Halving the transition doubles the taps |
| 25 | FFT size vs time resolution | `Δf = fs/N`, window = `N/fs` seconds |
| 26 | Main lobe vs sidelobes | Narrow lobe ⟺ high sidelobes |

All four are the **time–frequency uncertainty principle**:

```
   Δt · Δf >= constant
```

A signal cannot be simultaneously narrow in time and narrow in frequency. This is the same
mathematics that gives Heisenberg's uncertainty principle in quantum mechanics — not an analogy,
the same theorem about functions and their Fourier transforms.

**Practical consequence:** whenever a tool offers you "better resolution", ask what it is costing
you. It is always costing you resolution in the other domain.

---

## 26.8 Choosing a window

A decision table, since this is the question people actually ask:

| Task | Window | Why |
|---|---|---|
| General spectrum display | **Hann** | Good all-round compromise; fast sidelobe roll-off |
| STFT for resynthesis (Ch 27, 54) | **Hann** | Overlaps to a constant sum — see below |
| Measuring a tone's exact amplitude | **Flat-top** | 0.01 dB scalloping loss |
| Finding a small harmonic near a big one | **Blackman–Harris** | −92 dB sidelobes |
| Separating two close tones | **Rectangular** or Hamming | Narrowest main lobe |
| Speech analysis | **Hamming** | Historical standard; low first sidelobe |
| Transient / onset detection | **Rectangular** or Hann, short `N` | Time resolution matters more |
| FIR filter design (Ch 22) | **Kaiser** | Adjustable; design from a specification |

**The overlap-add constraint** deserves its own note, because it governs Chapter 27. If you are
going to window, transform, modify and *reconstruct*, the windows must sum to a constant as they
overlap, or you get amplitude ripple in the output. Hann at 50% overlap sums to exactly 1.0; Hann
at 75% sums to exactly 2.0. This property — **COLA**, constant overlap-add — is why Hann dominates
in spectral processing, and Chapter 27 uses it.

---

## 26.9 Exercises

**26.1** Implement all seven windows in periodic form. Plot each (print 32 values). Verify that
each starts and ends near zero, except rectangular.

**26.2** Compute coherent gain and power gain for each window and reproduce §26.5's table.

**26.3** Reproduce §26.6's between-bins table. Does your rectangular scalloping loss come out at
3.92 dB?

**26.4** Take a 1 kHz sine at exactly 0 dBFS. Measure its peak bin amplitude with a Hann window,
**without** applying the coherent-gain correction. How many dB low is it? Now apply the
correction.

**26.5** Generate two tones 50 Hz apart at equal level, `N = 4096`. Which windows resolve them as
two peaks? Now move them 20 Hz apart. Which still do?

**26.6** Generate a 1 kHz tone at 0 dB plus a 3 kHz tone at −80 dB. Which windows let you see the
quiet one? Relate the answer to each window's sidelobe level.

**26.7** *Deliberate breakage.* Use the symmetric Hann formula (`N-1`) for FFT analysis instead of
the periodic one (`N`). Measure the leakage difference for a between-bins tone. How large is it?

**26.8** Verify the COLA property: sum overlapping Hann windows at 50% and 75% overlap and check
the total is constant. Now try Hamming at 50% — is it constant? What does that imply for
resynthesis?

**26.9** Implement a flat-top window from its published coefficients and verify its scalloping
loss is under 0.05 dB. Then measure its main lobe width in bins and confirm the resolution cost.

---

### Chapter summary

- The DFT assumes your block **repeats forever**. If the signal does not fit a whole number of
  cycles, the assumed repetition has a **discontinuity**, and discontinuities are broadband. That
  is **spectral leakage**, and it happens with essentially every real signal.
- Leakage hides quiet components, makes peak amplitudes read **up to 3.92 dB low**, and corrupts
  phase-based resynthesis.
- **Windowing** tapers the block to zero at both ends, removing the discontinuity — at the cost of
  convolving the true spectrum with the window's spectrum.
- Every window trades **main lobe width** (frequency resolution) against **sidelobe level**
  (leakage floor). You cannot improve both.
- **Correct the magnitude**: divide by **coherent gain** for tones, **power gain** for noise.
  Hann's coherent gain is 0.5, so forgetting it costs you 6 dB.
- Use the **periodic** window form (`/N`) for FFT analysis, **symmetric** (`/(N-1)`) for FIR
  design.
- Defaults: **Hann** for general use and resynthesis (it satisfies COLA), **Blackman–Harris** for
  dynamic range, **flat-top** for amplitude accuracy, **rectangular** only for transients.
- This is the **time–frequency uncertainty principle**, appearing for the fourth time. Whenever a
  tool offers better resolution, it is costing you resolution in the other domain.

**Next:** [Chapter 27 — The STFT and Spectrograms](27-stft-and-spectrograms.md)
