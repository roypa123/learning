# Chapter 54 — Pitch Shifting and Time Stretching

> Changing pitch without changing duration, or duration without changing pitch. Both are
> impossible in the naive sense — the two are physically coupled — so every method is a
> different negotiation with that fact, and each fails in a characteristic way.

---

## 54.1 Why it is hard

Play a recording faster and **both** the pitch and the duration change. That is resampling
(Chapter 28), and it is the only *lossless* operation in this chapter: nothing is invented, so
nothing can be wrong.

Everything else must **fabricate information**:

- Stretching time means producing audio that was never recorded, between the samples you have.
- Shifting pitch means producing frequencies that were never present.

So every method makes assumptions about what the missing content should be, and the artefacts you
hear are those assumptions failing.

| Method | Domain | Best for | Characteristic failure |
|---|---|---|---|
| **Resampling** | Time | Samplers, when both should change | N/A — it is exact |
| **Granular / SOLA** | Time | Speech, percussion, moderate ratios | Stuttering, doubling |
| **Phase vocoder** | Frequency | Sustained, musical, large ratios | "Phasiness", smeared transients |
| **PSOLA** | Time, pitch-synchronous | Monophonic voice | Needs accurate pitch detection |
| **Sines + noise** | Spectral model | Extreme ratios, morphing | Loses anything unmodelled |

---

## 54.2 Granular / SOLA

Chapter 37 covered this: grain playback rate and grain scheduling rate are independent, so you can
change one without the other.

**SOLA** (Synchronised Overlap-Add) is the refinement that makes it work on musical material:
instead of placing each grain at a fixed hop, **search for the position that best correlates with
what is already there**.

```cpp
size_t findBestOffset(const std::vector<float>& output, size_t writePos,
                      const std::vector<float>& input, size_t readPos,
                      size_t searchRange, size_t overlapLength)
{
    double bestCorrelation = -1e30;
    size_t bestOffset = 0;

    for (size_t offset = 0; offset < searchRange; ++offset)
    {
        double correlation = 0.0;
        for (size_t i = 0; i < overlapLength; ++i)
            correlation += static_cast<double>(output[writePos + i])
                         * static_cast<double>(input[readPos + offset + i]);

        if (correlation > bestCorrelation)
        {
            bestCorrelation = correlation;
            bestOffset = offset;
        }
    }

    return bestOffset;
}
```

**This is Chapter 21's cross-correlation**, used to align waveform periods so the overlap is
constructive rather than destructive.

**Without the search** you get **phase cancellation** at every grain boundary — the classic
granular "fluttering" artefact where the signal partially cancels itself at the crossfade.
**With it**, the grains line up cycle-for-cycle and the overlap is clean.

**WSOLA** is a further refinement that searches in the *input* for the best match to the output,
rather than the reverse. It handles varying pitch better.

**Strengths:** cheap, works on any material, excellent on speech and percussion.
**Weakness:** the search assumes the signal is quasi-periodic. Polyphonic music with several
independent pitches has no single best alignment, and SOLA produces a characteristic warbling.

---

## 54.3 The phase vocoder

The frequency-domain approach, and the one used for high-quality musical stretching.

**The idea:** STFT the signal (Chapter 27), then **synthesise with a different hop size than you
analysed with.** Analysing at hop 256 and synthesising at hop 512 stretches time 2×.

**The problem:** the phases no longer line up. Each frame's phase was correct for its original
position; placed at a different position, consecutive frames are no longer coherent, and the
result has a hollow, smeared, metallic quality — "phasiness".

### The fix: instantaneous frequency

For each bin, compute how much the phase *actually* advanced between frames, deduce the true
frequency, and then advance the synthesis phase by the correct amount for the new hop.

```cpp
void PhaseVocoder::processFrame(const std::vector<Complex>& frame,
                                std::vector<Complex>& output,
                                double stretchRatio)
{
    const size_t numBins = frame.size() / 2 + 1;

    for (size_t k = 0; k < numBins; ++k)
    {
        const double magnitude = std::abs(frame[k]);
        const double phase     = std::arg(frame[k]);

        // --- how much did the phase advance since the last frame? ---
        double deltaPhase = phase - lastPhase_[k];
        lastPhase_[k] = phase;

        // --- subtract the EXPECTED advance for this bin's centre ---
        const double expected = kTwoPi * static_cast<double>(analysisHop_)
                              * static_cast<double>(k)
                              / static_cast<double>(frame.size());

        deltaPhase -= expected;

        // --- wrap into -pi..pi -- ESSENTIAL ------------------------
        // Phase is only known modulo 2*pi, so the raw difference is
        // ambiguous. Wrapping picks the smallest plausible deviation.
        deltaPhase = wrapToPi(deltaPhase);

        // --- the bin's TRUE frequency, in radians per sample --------
        const double trueFreq = (expected + deltaPhase)
                              / static_cast<double>(analysisHop_);

        // --- advance the synthesis phase by the right amount --------
        synthesisPhase_[k] += trueFreq * static_cast<double>(synthesisHop_);

        output[k] = std::polar(magnitude, synthesisPhase_[k]);

        // Mirror for the real-signal conjugate symmetry.
        if (k > 0 && k < frame.size() / 2)
            output[frame.size() - k] = std::conj(output[k]);
    }
}

double wrapToPi(double x)
{
    x = std::fmod(x + kPi, kTwoPi);
    if (x < 0.0) x += kTwoPi;
    return x - kPi;
}
```

**The phase-wrapping step is the crux of the algorithm.** Phase is only known modulo 2π, so the
measured difference between frames is ambiguous by any multiple of 2π. Subtracting the expected
advance and wrapping to ±π resolves it by assuming the deviation is the smallest plausible one —
which is correct as long as the bin's true frequency is within half a bin of its centre. That
assumption is why the phase vocoder needs sufficient overlap: at 75% overlap it holds; at 50% it
starts to fail.

### Transient smearing

The phase vocoder's other failure is that transients smear. A drum hit occupies one instant, but
the algorithm spreads it across the analysis window — so a sharp hit becomes a soft thud.

**Transient handling** detects onsets (Chapter 27's energy-rise detector) and **resets the
phases** to the analysis phases at those frames, re-aligning everything:

```cpp
if (isTransient(frame))
{
    // Reset: use the analysis phase directly. The transient stays sharp,
    // at the cost of a small discontinuity in the sustained partials.
    for (size_t k = 0; k < numBins; ++k)
        synthesisPhase_[k] = std::arg(frame[k]);
}
```

This is the single most important improvement to a basic phase vocoder, and it is what separates
a usable implementation from an academic one.

### Phase locking

A further refinement. Real partials occupy several adjacent bins (Chapter 26's leakage), and
treating each bin independently lets those bins drift apart, which smears the partial.

**Identity phase locking**: find each spectral peak and lock its neighbouring bins' phases to it.

```cpp
// Bins around a peak get the peak's phase advance, keeping the
// partial coherent.
for (size_t k = peakStart; k <= peakEnd; ++k)
    synthesisPhase_[k] = synthesisPhase_[peakBin]
                       + (std::arg(frame[k]) - std::arg(frame[peakBin]));
```

Cheap, and it noticeably reduces phasiness. Laroche and Dolson's 1999 paper is the reference.

---

## 54.4 Pitch shifting from time stretching

**Pitch shift = time stretch + resample.**

```cpp
std::vector<float> pitchShift(const std::vector<float>& input,
                              double semitones, double sampleRate)
{
    const double ratio = std::pow(2.0, semitones / 12.0);

    // 1. Stretch by 1/ratio -- the duration changes, pitch does not.
    auto stretched = timeStretch(input, 1.0 / ratio);

    // 2. Resample by ratio -- BOTH change, and the duration comes back
    //    to the original while the pitch ends up shifted.
    return resample(stretched, sampleRate, sampleRate / ratio);
}
```

Shifting up a fifth (7 semitones, ratio 1.498): stretch to 1.498× the length, then resample to
0.668× — back to the original length, a fifth higher.

**This works but has a problem: the formants move too.**

---

## 54.5 Formants

Chapter 3 and Chapter 55: the **formants** are the resonances of the vocal tract, and they are
fixed by the size of the person's head and throat. They do **not** move when someone sings
higher.

Shift a voice up an octave by any method that scales the whole spectrum, and you scale the
formants too — the listener hears not a higher voice but a *smaller person*. That is the
chipmunk effect, and it is instantly recognisable.

**Formant-preserving pitch shift:**

1. Extract the **spectral envelope** (the formant structure) from the original.
2. Pitch-shift the signal.
3. Apply the *original* envelope to the shifted result.

```cpp
// Extract the spectral envelope by heavily smoothing the magnitude
// spectrum -- this captures the resonances, not the individual partials.
std::vector<double> spectralEnvelope(const std::vector<double>& magnitudes,
                                     size_t smoothingBins = 20)
{
    std::vector<double> env(magnitudes.size());

    for (size_t k = 0; k < magnitudes.size(); ++k)
    {
        double sum = 0.0;
        size_t count = 0;
        const size_t lo = (k > smoothingBins) ? k - smoothingBins : 0;
        const size_t hi = std::min(k + smoothingBins, magnitudes.size() - 1);

        for (size_t j = lo; j <= hi; ++j) { sum += magnitudes[j]; ++count; }
        env[k] = sum / count;
    }
    return env;
}

// Then: shifted[k] *= originalEnvelope[k] / shiftedEnvelope[k];
```

**Better methods** use cepstral smoothing or LPC (Chapter 55) to extract the envelope more
accurately. But even the crude box-smoothing version above dramatically improves a shifted voice.

**Deliberately moving the formants independently** is the basis of gender/age transformation and,
in cinematic work, of creature design: shifting pitch down while moving formants *up* produces
something that sounds large but wrong — a very effective unsettling effect.

---

## 54.6 PSOLA

For monophonic pitched material — a solo voice, a bass line — **PSOLA** (Pitch-Synchronous
Overlap-Add) gives the best quality of any time-domain method.

**The idea:** find every pitch period (using autocorrelation, Chapter 21), extract grains
centred on the period marks, and re-space them.

- **Wider spacing** → lower pitch, same duration (if you repeat grains)
- **Narrower spacing** → higher pitch
- **Repeating or skipping grains** → time stretch, same pitch

```
   Original:  |--T--|--T--|--T--|--T--|      (T = one pitch period)

   Pitch up:  |-T'-|-T'-|-T'-|-T'-|-T'-|     (grains closer, some repeated)

   Stretched: |--T--|--T--|--T--|--T--|--T--|--T--|   (grains repeated)
```

**Because grains are aligned to pitch periods, the overlap is always phase-coherent** — the
cancellation problem of naive granular processing cannot occur.

**Strengths:** very high quality on monophonic voice, cheap, no latency beyond a couple of
periods, and it preserves formants naturally (the grain content is unchanged; only the spacing
moves).

**Weaknesses:** requires accurate pitch detection, and fails completely on polyphonic material or
on unvoiced sounds where there is no period to synchronise to. Practical implementations detect
voiced/unvoiced and switch to SOLA for the unvoiced parts.

**PSOLA is what most speech-processing systems use**, and it is why pitch-corrected vocals can
sound natural while pitch-shifted full mixes rarely do.

---

## 54.7 Choosing a method

| Material | Ratio | Method |
|---|---|---|
| Drums, percussion | any | **SOLA/WSOLA** with transient preservation |
| Solo voice | up to ±5 semitones | **PSOLA** |
| Solo voice | extreme | **Phase vocoder** + formant correction |
| Sustained instruments, pads | any | **Phase vocoder** with phase locking |
| Full mix | up to ±2 semitones | Phase vocoder with transient handling |
| Full mix | extreme | Nothing works well; consider it an effect |
| Sound design, any material | extreme | **Granular** — the artefacts are the point |
| Sampler playback | any | **Resampling** — it is not a defect there |

**For cinematic sound design the ranking inverts.** Artefacts that are failures in a music
context are often exactly what you want:

- **Granular stretching at 20×** turns any sound into an evolving texture (Chapter 37).
- **Phase-vocoder smearing** produces the ghostly, unreal quality used for dreams and memory.
- **Extreme downward pitch shifting** with formants preserved makes things enormous; with
  formants shifted too, it makes them monstrous.
- **PSOLA with deliberately wrong period detection** produces robotic, glitching artefacts.

Chapter 84 uses all four.

---

## 54.8 Exercises

**54.1** Implement naive overlap-add time stretching with no correlation search. Stretch a bass
note 2× and listen for the cancellation flutter.

**54.2** Add the SOLA correlation search. Compare. How large does the search range need to be?

**54.3** Build the phase vocoder. Stretch a sustained pad 2× and compare with SOLA.

**54.4** *Deliberate breakage.* Remove the phase wrapping (`wrapToPi`). What happens?

**54.5** Implement transient detection and phase reset. Stretch a drum loop 1.5× with and without
it.

**54.6** Implement identity phase locking. Measure the improvement on a sustained chord — is the
phasiness reduced?

**54.7** Pitch-shift a voice up 12 semitones without formant correction. Then add it. Compare.

**54.8** Shift a voice **down** 8 semitones with the formants shifted **up** 4 semitones. Describe
the result. This is a standard creature-design technique.

**54.9** Implement PSOLA with autocorrelation pitch detection. Shift a solo voice ±5 semitones and
compare with the phase vocoder.

**54.10** Take one source and process it four ways at 8× stretch: SOLA, phase vocoder, granular
with heavy spray, and PSOLA. Which would you use for a cinematic texture, and why?

---

### Chapter summary

- Pitch and duration are physically coupled. Only **resampling** is lossless; every other method
  must **fabricate information**, and its artefacts are its assumptions failing.
- **SOLA** searches for the best-correlating overlap position (Chapter 21's cross-correlation),
  which prevents the phase cancellation that plagues naive overlap-add.
- The **phase vocoder** analyses and synthesises with different hop sizes. Its crux is computing
  each bin's **instantaneous frequency** by subtracting the expected phase advance and **wrapping
  to ±π** — phase is only known modulo 2π.
- Its two failures: **transient smearing** (fixed by detecting onsets and **resetting the
  phases**) and **phasiness** (reduced by **identity phase locking** around spectral peaks).
- **Pitch shift = time stretch + resample.**
- **Formants do not move when pitch does.** Scaling the whole spectrum gives the chipmunk effect.
  Extract the **spectral envelope**, shift, then reapply the original envelope. Moving formants
  *independently* is a creature-design technique.
- **PSOLA** aligns grains to detected pitch periods, so the overlap is always coherent. Best
  quality for monophonic voice; fails on polyphonic and unvoiced material.
- Method choice depends on material and ratio — **and for sound design the ranking inverts**,
  because the artefacts are often exactly what you want.

**Next:** [Chapter 55 — Vocoders, Formants, and Voice Processing](55-vocoder-and-formants.md)
