# Chapter 72 — Psychoacoustics: Critical Bands, Masking, Loudness

> Chapter 3 introduced hearing qualitatively. This chapter makes it quantitative — the numbers
> you can compute with, which is what turns "the ear does something interesting here" into a
> masking threshold you can implement.

---

## 72.1 Critical bands

The cochlea (Chapter 3) behaves as a bank of overlapping band-pass filters. A **critical band**
is the bandwidth within which sounds interact — masking each other, fusing, or producing
roughness.

**Two standard scales:**

**The Bark scale** (Zwicker) divides hearing into 24 bands:

```cpp
double hzToBark(double f)
{
    return 13.0 * std::atan(0.00076 * f)
         + 3.5 * std::atan((f / 7500.0) * (f / 7500.0));
}
```

| Bark | Centre (Hz) | Bandwidth (Hz) |
|---|---|---|
| 1 | 60 | 80 |
| 5 | 450 | 110 |
| 10 | 1175 | 190 |
| 15 | 2500 | 450 |
| 20 | 6400 | 1300 |
| 24 | 13500 | 3500 |

**The ERB scale** (Equivalent Rectangular Bandwidth, Moore & Glasberg) is more modern and better
fits the data at low frequencies:

```cpp
double erbBandwidth(double f)
{
    return 24.7 * (4.37 * f / 1000.0 + 1.0);
}

double hzToErbScale(double f)
{
    return 21.4 * std::log10(4.37 * f / 1000.0 + 1.0);
}
```

**The essential fact**: bandwidth is roughly constant (about 100 Hz) below 500 Hz and roughly
proportional to frequency (about 15%) above it.

**Why this matters practically:**

- **Two tones within one critical band interact.** Below about 15 Hz apart they beat; between 15
  and one bandwidth apart they sound rough; beyond that they separate into two tones. Chapter 2's
  observation, now with a number attached.
- **Masking operates within bands.** Chapter 3's masking curve is essentially "one critical band
  wide, with skirts".
- **Analysis should use ERB or Bark spacing**, not linear. A spectrum analyser with linear
  frequency bins wastes most of its resolution where the ear has none.
- **EQ bandwidth in Q is not perceptually uniform.** A Q of 2 at 100 Hz is much wider in ERB terms
  than a Q of 2 at 5 kHz.

---

## 72.2 Computing a masking threshold

The useful, implementable version of Chapter 3's masking.

```cpp
// The spreading function: how far a masker's influence extends.
// Asymmetric -- masking spreads UPWARD much more than downward.
double spreadingFunctionDb(double maskerBark, double maskeeBark, double maskerDb)
{
    const double dz = maskeeBark - maskerBark;

    if (dz >= 0.0)
    {
        // Upward spread: shallower, and it gets shallower as the masker
        // gets louder -- loud sounds mask a wider range above them.
        const double slope = -(22.0 - std::min(230.0 / (maskerBark * 100.0 + 1.0), 10.0)
                               - 0.2 * maskerDb);
        return maskerDb + slope * dz;
    }

    // Downward spread: steep, about -27 dB per Bark.
    return maskerDb - 27.0 * (-dz);
}

std::vector<double> maskingThreshold(const std::vector<double>& spectrumDb,
                                     double sampleRate, size_t fftSize)
{
    const size_t bins = spectrumDb.size();
    std::vector<double> threshold(bins, -120.0);

    for (size_t m = 0; m < bins; ++m)
    {
        if (spectrumDb[m] < -90.0) continue;              // too quiet to mask

        const double maskerBark = hzToBark(binFrequency(m, fftSize, sampleRate));

        for (size_t k = 0; k < bins; ++k)
        {
            const double bark = hzToBark(binFrequency(k, fftSize, sampleRate));
            const double contribution = spreadingFunctionDb(maskerBark, bark,
                                                            spectrumDb[m]);

            // Thresholds add in POWER, not dB.
            threshold[k] = 10.0 * std::log10(
                std::pow(10.0, threshold[k] / 10.0)
              + std::pow(10.0, contribution / 10.0));
        }
    }

    // Anything below the absolute threshold of hearing is inaudible
    // regardless of masking.
    for (size_t k = 0; k < bins; ++k)
        threshold[k] = std::max(threshold[k],
                                absoluteThresholdDb(binFrequency(k, fftSize, sampleRate)));

    return threshold;
}
```

**The asymmetry is the important part**: upward spreading is around −10 to −20 dB per Bark and
gets *shallower* as the masker gets louder; downward spreading is a steep −27 dB per Bark and is
largely level-independent.

**This is why bass-heavy content dominates a mix** (Chapter 53's §53.4) and why a cut at 300 Hz
reveals more than a boost at 3 kHz.

**Thresholds add in power**, not decibels — Chapter 11's rule, and forgetting it gives thresholds
that are wildly too high.

---

## 72.3 The absolute threshold of hearing

The quietest audible sound at each frequency, in the absence of any masker:

```cpp
// Terhardt's approximation, in dB SPL.
double absoluteThresholdDb(double f)
{
    const double kHz = f / 1000.0;
    return 3.64 * std::pow(kHz, -0.8)
         - 6.5 * std::exp(-0.6 * (kHz - 3.3) * (kHz - 3.3))
         + 0.001 * std::pow(kHz, 4.0);
}
```

| Frequency | Threshold (dB SPL) |
|---|---|
| 20 Hz | ~75 |
| 100 Hz | ~25 |
| 1 kHz | ~3 |
| **3.5 kHz** | **~−4 (the minimum)** |
| 10 kHz | ~15 |
| 15 kHz | ~35 |
| 20 kHz | ~70 |

**The minimum is negative** because the reference (20 µPa) was defined at 1 kHz, and the ear is
*more* sensitive at 3.5 kHz — Chapter 3's ear-canal resonance.

**Practical use:** anything below this curve is inaudible and need not be computed, transmitted,
or worried about. Perceptual codecs use exactly this, combined with §72.2's masking threshold.

---

## 72.4 Loudness models

**Phons** are loudness level: a tone's loudness in phons is the dB SPL of a 1 kHz tone that
sounds equally loud. Chapter 3's equal-loudness contours are curves of constant phons.

**Sones** are perceived loudness on a ratio scale:

```
   sones = 2^((phons - 40) / 10)
```

| Phons | Sones | Description |
|---|---|---|
| 20 | 0.25 | Very quiet |
| 40 | **1.0** | The reference |
| 50 | 2.0 | Twice as loud as 40 |
| 60 | 4.0 | |
| 70 | 8.0 | |
| 100 | 64.0 | |

**The formula encodes Chapter 3's "+10 dB is twice as loud"**, and it is the basis of every
serious loudness model.

**ITU-R BS.1770 (LUFS)** is the practical standard, and it is deliberately simple compared with
full psychoacoustic models:

```cpp
class LoudnessMeter
{
public:
    void prepare(double sampleRate)
    {
        // Stage 1: a high-shelf approximating head diffraction.
        preFilter_.setFilter(FilterType::HighShelf, 1681.97, 0.7071, 3.999);

        // Stage 2: a high-pass removing content below ~38 Hz.
        rlbFilter_.setFilter(FilterType::HighPass, 38.13, 0.5);
    }

    void process(const AudioBuffer& buffer)
    {
        for (int c = 0; c < buffer.numChannels(); ++c)
        {
            const double weight = channelWeight(c);    // surround channels get +1.5 dB

            for (float s : buffer.channel(c))
            {
                float f = preFilter_.processSample(s, c);
                f = rlbFilter_.processSample(f, c);

                sumOfSquares_ += weight * static_cast<double>(f) * f;
                ++sampleCount_;
            }
        }
    }

    double integratedLUFS() const
    {
        const double meanSquare = sumOfSquares_ / sampleCount_;
        return -0.691 + 10.0 * std::log10(meanSquare);
    }

private:
    Biquad preFilter_, rlbFilter_;
    double sumOfSquares_ = 0.0;
    size_t sampleCount_ = 0;
};
```

**K-weighting is two biquads**: a high shelf approximating the head's diffraction, and a
high-pass removing inaudible sub-bass. That is the entire frequency weighting — far simpler than
a full loudness model, and it was chosen deliberately for reproducibility across implementations.

**The `-0.691` offset** calibrates the result so that a full-scale 1 kHz sine reads 0 LUFS.

**Gating** is the other essential part of the standard: blocks more than 10 LU below the ungated
mean are excluded, so silence does not drag the measurement down. Chapter 89 implements it fully.

---

## 72.5 Roughness and sharpness

Two perceptual attributes with computable models, both directly useful for sound design.

**Roughness** peaks when two tones are separated by about 25% of a critical band — around 20–30 Hz
in the midrange. It is the sensation of harshness or grating.

```cpp
// Roughness contribution from two partials (Sethares' model).
double roughness(double f1, double f2, double a1, double a2)
{
    const double fmin = std::min(f1, f2);
    const double s = 0.24 / (0.0207 * fmin + 18.96);
    const double diff = std::fabs(f2 - f1);

    return a1 * a2 * (std::exp(-3.5 * s * diff) - std::exp(-5.75 * s * diff));
}
```

**Summing this over every pair of partials gives a roughness score** — and minimising it over
possible tunings *derives* consonance from first principles. Sethares showed that the consonant
intervals of a tuning system follow from the timbre's harmonic structure, which is why gamelan
tunings match gamelan timbres.

**For cinematic work this is directly actionable:** to make something feel harsh and threatening,
place partials 20–30 Hz apart in the midrange. To make it feel pure, avoid that spacing.

**Sharpness** (in **acum**) measures the weighting of high-frequency energy — the "piercing"
quality. High sharpness reads as urgent, aggressive, alarming. It is why a sound can be quiet and
still feel threatening.

---

## 72.6 Temporal effects

**Temporal integration.** Loudness is integrated over about 100–200 ms (Chapter 3). Below that,
halving the duration reduces loudness by about 3 dB — so a 10 ms burst needs to be about 10 dB
higher in level than a 200 ms one to sound equally loud.

**This is why transients can be much louder in peak terms without sounding loud**, and it is the
perceptual basis for the peak/RMS distinction of Chapter 11.

**Forward masking** decays over 100–200 ms:

```cpp
// Post-masking threshold after a masker ends.
double forwardMaskingDb(double maskerDb, double timeMs)
{
    if (timeMs <= 0.0) return maskerDb;
    if (timeMs > 200.0) return -120.0;
    return maskerDb - 0.4 * maskerDb * std::log10(timeMs / 0.5) / std::log10(400.0);
}
```

**Backward masking** extends only 5–20 ms before the masker, and it is much weaker.

**Practical use in cinematic work:** you can hide almost anything in the 100 ms after a loud
impact. Editing artefacts, joins, and the start of a new element are all inaudible there. Chapter
82 uses this deliberately — it is why layered impacts can have sloppy internal edits that nobody
hears.

---

## 72.7 Applying psychoacoustics

Where these models earn their place.

**Perceptual codecs.** Compute the masking threshold, allocate bits so that quantisation noise
stays below it, discard what is masked. That is MP3, AAC and Opus.

**Mixing decisions.** Chapter 53's advice — cut what masks rather than boosting what is masked —
is directly derived from §72.2's asymmetric spreading.

**Automatic mixing.** Compute each element's contribution to the masking of the dialogue, and
duck the worst offenders. Chapter 89 builds a frequency-selective version.

**Loudness compliance.** LUFS metering for broadcast and streaming delivery.

**Watermarking and steganography.** Hide data below the masking threshold where it is inaudible.

**Sound design.** The roughness model tells you how to make something harsh; the sharpness model
tells you how to make it urgent; the temporal masking model tells you where you can hide things.

---

## 72.8 The honest limits

Psychoacoustic models are statistical descriptions of average listeners under laboratory
conditions. They are useful and they are not the last word.

**They assume steady signals.** Real audio is transient, and the models extend imperfectly.

**They assume attentive, isolated listening.** A listener attending to a specific element hears
far more than the model predicts — trained listeners routinely detect artefacts that masking
models say are inaudible.

**Individual variation is large.** Thresholds vary by 10–20 dB between listeners, and hearing
damage shifts them further.

**They model detection, not annoyance.** Something can be technically above threshold and
unobjectionable, or below threshold and — through some other mechanism — still bothersome.

**The practical position:** use these models to *guide* decisions and to *automate* what would
otherwise be tedious. Do not use them to overrule listening. Chapter 3's principle stands —
measure everything you can, listen to everything you measure, and when they disagree, the
disagreement is the interesting part.

---

## 72.9 Exercises

**72.1** Implement `hzToBark` and `hzToErbScale`. Plot bandwidth against frequency for both.

**72.2** Generate two tones 10, 25, 50, 100 and 300 Hz apart at 1 kHz. At which separation does
beating become roughness, and roughness become two tones?

**72.3** Implement the masking threshold. Compute it for a 1 kHz tone at 80 dB and verify the
asymmetry.

**72.4** Generate a 1 kHz tone at 80 dB and a 1.2 kHz tone at a level just below the computed
threshold. Can you hear it? Now put the quiet tone at 800 Hz instead.

**72.5** Implement the absolute threshold curve and verify the minimum is near 3.5 kHz.

**72.6** Build the K-weighting filters and verify a full-scale 1 kHz sine reads 0 LUFS.

**72.7** Measure several commercial tracks in LUFS. How do they compare with streaming targets
(−14 LUFS)?

**72.8** Implement the roughness model. Compute roughness for a major third and a tritone using
harmonic timbres. Does it match your perception of consonance?

**72.9** Generate partials 25 Hz apart at 1 kHz and compare with 200 Hz apart at the same total
level. Which sounds harsher?

**72.10** Hide a quiet click 50 ms after a loud impact, then 50 ms before it. Which is audible?

---

### Chapter summary

- **Critical bands** are the widths within which sounds interact. Bandwidth is roughly constant
  (~100 Hz) below 500 Hz and roughly 15% of frequency above. Use **Bark** or **ERB** spacing for
  analysis, never linear.
- **Masking spreads asymmetrically**: upward at −10 to −20 dB/Bark (shallower as the masker gets
  louder), downward at a steep −27 dB/Bark. This is why bass dominates a mix and why cutting
  beats boosting.
- **Thresholds add in power, not dB.**
- The **absolute threshold of hearing** is minimum near 3.5 kHz — the ear-canal resonance.
  Anything below it is inaudible regardless of masking.
- **Sones** encode "+10 dB is twice as loud": `sones = 2^((phons−40)/10)`.
- **LUFS** uses **K-weighting — just two biquads** — plus gating, chosen for reproducibility over
  perceptual sophistication.
- **Roughness** peaks at ~25% of a critical band (20–30 Hz in the midrange): place partials there
  to make something harsh. **Sharpness** measures piercing quality, which reads as urgency.
- **Temporal integration** over 100–200 ms means short bursts need ~10 dB more level to sound
  equally loud. **Forward masking hides anything in the 100 ms after a loud impact** — which is
  why layered impacts can have sloppy internal edits.
- The models describe average listeners under laboratory conditions. **Use them to guide and
  automate, not to overrule listening.**

**Next:** [Chapter 73 — How We Localise Sound: ITD, ILD, and the HRTF](73-localisation.md)
