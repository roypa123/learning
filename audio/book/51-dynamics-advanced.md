# Chapter 51 — Dynamics II: Limiters, Gates, Sidechains, Multiband

> Four variations on Chapter 50's mechanism, each solving a different problem. The limiter is the
> one you must get right, because it is the last thing between your signal and the listener's
> ears.

---

## 51.1 The limiter

A limiter is a compressor with an infinite ratio and a very fast attack: **nothing gets past the
ceiling.**

The problem is that "very fast attack" is not fast enough. Even a 0.1 ms attack lets 4 samples
through at 44.1 kHz, and those 4 samples can be at full scale.

### Lookahead

The solution: **delay the audio while the detector looks at the un-delayed signal.** The gain
reduction is then already in place when the peak arrives.

```cpp
class Limiter : public Processor
{
public:
    void prepare(double sr, int) override
    {
        sr_ = sr;
        lookaheadSamples_ = static_cast<size_t>(lookaheadMs_ * 0.001 * sr);

        for (auto& d : delays_) d.prepare(sr, 0.05);

        // The gain smoothing filter's length must match the lookahead,
        // so the reduction ramps in exactly as the peak arrives.
        smoothCoeff_ = std::exp(-1.0 / (lookaheadSamples_ * 0.5));
    }

    void process(AudioBuffer& buffer) override
    {
        const int channels = buffer.numChannels();

        for (int i = 0; i < buffer.numFrames(); ++i)
        {
            // --- detect on the UNDELAYED signal ---------------------
            float peak = 0.0f;
            for (int c = 0; c < channels; ++c)
                peak = std::max(peak, std::fabs(buffer.channel(c)[i]));

            // Required gain to bring this peak to the ceiling.
            const float required = (peak > ceiling_) ? (ceiling_ / peak) : 1.0f;

            // Take the MINIMUM: gain reduction engages instantly,
            // and releases slowly.
            if (required < gain_)
                gain_ = required;                        // instant attack
            else
                gain_ = required + releaseCoeff_ * (gain_ - required);

            // Smooth the gain so the reduction is a ramp, not a step.
            // A stepped gain is a discontinuity, which is distortion.
            smoothedGain_ = gain_ + smoothCoeff_ * (smoothedGain_ - gain_);

            // --- apply to the DELAYED signal -------------------------
            for (int c = 0; c < channels; ++c)
            {
                delays_[c].write(buffer.channel(c)[i]);
                buffer.channel(c)[i] =
                    delays_[c].read(static_cast<double>(lookaheadSamples_))
                    * smoothedGain_;
            }
        }
    }

    int latencySamples() const { return static_cast<int>(lookaheadSamples_); }

private:
    std::array<DelayLine, 8> delays_;
    double sr_ = kDefaultRate, lookaheadMs_ = 2.0;
    size_t lookaheadSamples_ = 88;
    float  ceiling_ = 0.891f;                 // -1 dBFS
    float  gain_ = 1.0f, smoothedGain_ = 1.0f;
    float  releaseCoeff_ = 0.9999f, smoothCoeff_ = 0.99f;
};
```

**Three things that must be right:**

**The gain must be smoothed, not stepped.** An instantaneous gain change is a discontinuity —
Chapter 13 — and produces a click. Smoothing over roughly the lookahead period gives a ramp that
arrives exactly as the peak does.

**Lookahead introduces latency**, and it must be reported. `latencySamples()` exists so the host
can delay-compensate. An uncompensated limiter in a parallel path comb-filters (Chapter 50).

**The release must be slow enough** not to pump, but fast enough to recover. 50–500 ms is typical;
programme-dependent release (Chapter 50) is better still.

---

## 51.2 True peak

Chapter 4 established that a reconstructed waveform can exceed the values of its samples —
**inter-sample peaks**. A limiter that only examines samples will let through material that
overshoots after reconstruction, and then:

- The listener's DAC clips.
- An MP3 or AAC encoder clips, because lossy encoding shifts sample values.
- A downstream sample-rate converter clips.

**Broadcast and streaming standards therefore specify true-peak limits**, typically −1 dBTP
(EBU R128) or −2 dBTP for some platforms.

### Measuring true peak

Oversample by 4× and measure the peak of the *reconstructed* signal:

```cpp
class TruePeakDetector
{
public:
    void prepare(double sr)
    {
        // A 4x polyphase interpolator. ITU-R BS.1770 specifies at least 4x.
        os_.prepare(4, sr, 48);
    }

    float measure(float x)
    {
        float maxPeak = 0.0f;
        os_.processSample(x, [&](float upsampled) {
            maxPeak = std::max(maxPeak, std::fabs(upsampled));
            return upsampled;
        });
        return maxPeak;
    }

private:
    Oversampler os_;
};
```

**Typical difference between sample peak and true peak**: 0.3 to 1.5 dB on normal material, up to
3 dB on heavily limited material — precisely the material most likely to be near the ceiling.

**This is why a "0 dBFS" master can fail a broadcast spec.** It measures 0 dBFS sample peak and
+1.2 dBTP true peak.

**Practical ceilings:**

| Destination | Ceiling |
|---|---|
| CD / lossless | −0.3 dBTP |
| Streaming (lossy encoding) | **−1.0 dBTP** |
| Broadcast (EBU R128) | **−1.0 dBTP** |
| Film / cinema | −3.0 dBTP or lower |
| Anything that will be transcoded again | −2.0 dBTP |

---

## 51.3 Gates and expanders

The opposite of compression: reduce what is **below** a threshold.

```cpp
float gateGain(float levelDb)
{
    if (levelDb > thresholdDb_)
        return 1.0f;                               // open

    const float under = thresholdDb_ - levelDb;

    // A gate is an expander with a very high ratio.
    const float reduceDb = -under * (ratio_ - 1.0f);

    return static_cast<float>(dbToGain(std::max(reduceDb, floorDb_)));
}
```

| Ratio | Name | Effect |
|---|---|---|
| 1.5:1 – 3:1 | **Expander** | Gentle; pushes quiet material down a little |
| 10:1+ | **Gate** | Hard; silences anything below threshold |

**A gate needs more parameters than a compressor:**

| Parameter | Purpose |
|---|---|
| **Threshold** | The open/close level |
| **Hysteresis** | Closes at a *lower* level than it opens — prevents chattering |
| **Attack** | How fast it opens (fast, or you clip transients) |
| **Hold** | Minimum open time — prevents chattering on sustained-but-modulated material |
| **Release** | How fast it closes (slow, or it sounds like a switch) |
| **Floor / range** | How much reduction, rather than full silence |

**Hysteresis and hold exist to stop chattering.** Material sitting right at the threshold will
open and close rapidly, producing a stuttering artefact. Opening at −30 dB but closing at −35 dB,
with a 50 ms minimum hold, eliminates it.

**Use a floor rather than full silence** unless you specifically want the effect. A gate that
reduces by 20 dB sounds natural; one that produces absolute digital silence sounds like an edit,
because real backgrounds never stop.

**Common uses:** removing bleed between drum microphones, cleaning up noisy dialogue between
lines, and — creatively — the gated reverb of 1980s drums (a big reverb followed by a fast gate,
which cuts the tail abruptly).

---

## 51.4 Sidechain input

Feed the detector from a **different signal** than the one being processed.

```cpp
void processWithSidechain(AudioBuffer& main, const AudioBuffer& sidechain)
{
    for (int i = 0; i < main.numFrames(); ++i)
    {
        // Detect from the SIDECHAIN, apply to the MAIN signal.
        const float detect = std::fabs(sidechain.channel(0)[i]);
        const float env    = detector_.process(detect);
        const float gain   = computeGain(env);

        for (int c = 0; c < main.numChannels(); ++c)
            main.channel(c)[i] *= gain;
    }
}
```

**Uses, in order of importance for cinematic work:**

**1. Dialogue ducking.** Music and effects are compressed by the dialogue signal, so they step
back automatically whenever someone speaks. This is done on essentially every film and every
broadcast, and it is why you can hear dialogue over an action sequence. Chapter 89 covers the
settings.

**2. De-essing.** Detect from a band-passed copy (5–8 kHz), apply to the full signal. Sibilance
triggers reduction; everything else does not. A **dynamic EQ** version applies the reduction only
to that band, which is more transparent.

**3. Kick-and-bass.** The bass is ducked by the kick drum so they do not fight for the same
frequency space — Chapter 3's masking, managed deliberately.

**4. Pumping as an effect.** A pad sidechained to a four-on-the-floor kick produces the rhythmic
breathing that defines several genres.

**5. Frequency-selective control.** High-pass the sidechain so the detector ignores bass
(Chapter 50); band-pass it to target a specific problem region.

---

## 51.5 Multiband dynamics

Split into frequency bands, compress each independently, recombine.

```
   in ──► [ crossover ] ──┬─► low  band ──► comp 1 ──┐
                          ├─► mid  band ──► comp 2 ──┼──► SUM ──► out
                          └─► high band ──► comp 3 ──┘
```

**Why bother:** a full-band compressor is triggered by whatever is loudest — usually the bass.
A kick drum then ducks the vocals, the cymbals, everything. Multiband lets the bass compress
itself without touching the rest.

**The crossover is the hard part.** It must sum back to flat, or you have built an EQ you did not
intend.

| Crossover | Sums flat? | Latency | Notes |
|---|---|---|---|
| **Linkwitz-Riley 4th order** | **Yes (magnitude)** | None | The standard. Bands are 180° out at the crossover, so invert alternate bands |
| Butterworth | No — +3 dB bump | None | Avoid |
| **Linear-phase FIR** | **Yes (exactly)** | High | Mastering; pre-rings |
| All-pass compensated | Yes | None | Complex to design |

**Linkwitz-Riley is two cascaded Butterworth filters** of half the order — which is why Chapter
23's cascade Q values matter here. A 4th-order LR crossover is two 2nd-order Butterworth stages.

```cpp
// A Linkwitz-Riley 4th-order crossover: two cascaded Butterworth 2nd-order.
lowBand  = lpA.process(lpB.process(input));
highBand = hpA.process(hpB.process(input));
// At the crossover both are -6 dB and 180 degrees apart, so:
output = lowBand - highBand;        // note the SUBTRACTION
```

**Typical multiband uses:**

| Application | Bands | Purpose |
|---|---|---|
| Mastering | 3–4 | Control problem regions independently |
| De-essing | 2 | High band only |
| Dialogue | 3 | Control chest boom, presence and sibilance separately |
| Bass control | 2 | Tighten low end without touching the rest |
| Broadcast processing | 5+ | Consistent spectral balance regardless of source |

**The caution:** multiband compression can flatten the spectral *contrast* of material — if every
band is held at a constant level, the mix's tonal movement disappears and it sounds lifeless.
This is exactly the cinematic anti-pattern from Chapter 50. Use few bands, gentle ratios, and
only where a specific problem needs solving.

---

## 51.6 Transient shapers

A different mechanism worth knowing, because it does something compression cannot.

Instead of a threshold, compare a **fast** envelope with a **slow** envelope. Where the fast one
exceeds the slow one, there is a transient.

```cpp
float process(float x)
{
    const float rectified = std::fabs(x);

    fast_ = std::max(rectified, fast_ * fastCoeff_);     // ~1 ms
    slow_ = std::max(rectified, slow_ * slowCoeff_);     // ~50 ms

    // Positive during attacks, negative during decays.
    const float difference = fast_ - slow_;

    const float gain = 1.0f
                     + attackAmount_  * std::max(difference, 0.0f) * 4.0f
                     + sustainAmount_ * std::min(difference, 0.0f) * 4.0f;

    return x * std::clamp(gain, 0.0f, 4.0f);
}
```

**No threshold at all.** A transient shaper responds to the *shape* of the envelope rather than
its level, so it works identically on loud and quiet material. That is its advantage: you can
make every drum hit punchier without having to ride a threshold.

**Two controls:** attack (boost or cut the transient) and sustain (boost or cut the body).

- **Attack up, sustain down** → punchy, tight, dry
- **Attack down, sustain up** → soft, sustained, roomy
- **Attack up, sustain up** → bigger everything

**For cinematic work this is extremely useful on impacts** (Chapter 85), because you can sharpen
the hit and extend the body independently — which is precisely the two-part structure an impact
needs.

---

## 51.7 Exercises

**51.1** Build the lookahead limiter. Verify no sample exceeds the ceiling on material driven
20 dB into it.

**51.2** *Deliberate breakage.* Remove the gain smoothing. Listen for the distortion, and look at
the gain signal.

**51.3** Compare a limiter with 0 ms lookahead and 2 ms lookahead on drums driven hard. How many
samples get past the 0 ms version?

**51.4** Implement true-peak detection with 4× oversampling. Measure a heavily limited master's
sample peak and true peak. What is the difference?

**51.5** Build a gate. Test it on noisy dialogue with and without hysteresis and hold. Count the
chattering events.

**51.6** Implement gated reverb: a large reverb into a gate with a fast release. Render a snare.

**51.7** Build dialogue ducking: compress a music bed with a dialogue sidechain. Tune the attack,
release and depth until the transition is inaudible.

**51.8** Build a de-esser with a band-passed sidechain. Compare with a dynamic EQ version that
reduces only the band.

**51.9** Build a 3-band Linkwitz-Riley crossover. Sum the bands with no processing and measure the
difference from the input. Is it flat? What happens if you forget to subtract the high band?

**51.10** Build a transient shaper. Apply it to a drum loop at four settings from §51.6. Then
apply it to the same loop at −30 dB and confirm it behaves identically.

---

### Chapter summary

- A **limiter** is an infinite-ratio compressor with **lookahead**: delay the audio while the
  detector sees the un-delayed signal, so reduction is already in place when the peak arrives.
- **Smooth the gain** over roughly the lookahead period. A stepped gain is a discontinuity, which
  is distortion. **Report the latency** so parallel paths can be compensated.
- **True peak** measures the *reconstructed* waveform via 4× oversampling. Sample peak understates
  it by 0.3–3 dB, which is why a "0 dBFS" master can clip a streaming encoder. Target
  **−1 dBTP** for streaming and broadcast.
- **Gates** need **hysteresis** and **hold** to prevent chattering, and a **floor** rather than
  full silence — real backgrounds never stop.
- **Sidechaining** drives the detector from another signal. The most important use in cinematic
  work is **dialogue ducking**; also de-essing, kick-and-bass separation, and rhythmic pumping.
- **Multiband** stops the loudest band from ducking everything else. The crossover must sum flat
  — use **Linkwitz-Riley 4th order** and remember the **subtraction** at recombination. Use few
  bands and gentle ratios, or you flatten the spectral contrast.
- **Transient shapers** compare a fast and a slow envelope, so they have **no threshold** and work
  identically at any level. Attack and sustain are independent — exactly what cinematic impacts
  need.

**Next:** [Chapter 52 — Distortion, Saturation, and Waveshaping](52-distortion-and-saturation.md)
