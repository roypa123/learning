# Chapter 53 — Equalisation: Shelves, Bells, and Linear Phase

> An equaliser is a set of filters with a user interface. Chapter 23 built every filter type it
> needs. This chapter is about what to do with them — which is mostly a question of judgement,
> so it is the most opinionated chapter in Part IV.

---

## 53.1 The band types

| Type | Controls | Use |
|---|---|---|
| **Low shelf** | Frequency, gain, Q | Broad tonal change below a point |
| **High shelf** | Frequency, gain, Q | "Air", brightness |
| **Peak / bell** | Frequency, gain, Q | Surgical or musical, at a point |
| **High-pass** | Frequency, slope, Q | Remove rumble — the most-used band |
| **Low-pass** | Frequency, slope, Q | Remove hiss, create distance |
| **Notch** | Frequency, Q | Kill a specific resonance or hum |
| **Band-pass** | Frequency, Q | Isolate — telephone effects, sidechains |
| **Tilt** | Frequency, gain | Rotate the whole spectrum about a pivot |

All of these are Chapter 23's cookbook, and `BiquadCascade` handles the steeper slopes.

**Tilt** is worth implementing because it is not in the cookbook and it is extremely useful: a
low shelf and a high shelf at the same frequency with opposite gains. One control that makes
everything darker or brighter while keeping the overall level roughly constant.

```cpp
void setTilt(double pivotHz, double gainDb)
{
    lowShelf_.setFilter(FilterType::LowShelf,  pivotHz, 0.707, -gainDb);
    highShelf_.setFilter(FilterType::HighShelf, pivotHz, 0.707, +gainDb);
}
```

---

## 53.2 The parametric EQ

```cpp
struct EQBand
{
    bool       enabled = false;
    FilterType type    = FilterType::Peaking;
    double     freqHz  = 1000.0;
    double     gainDb  = 0.0;
    double     Q       = 0.707;
    int        slope   = 2;          // order, for HP/LP
};

class ParametricEQ : public Processor
{
public:
    void prepare(double sr, int) override
    {
        sr_ = sr;
        for (auto& f : filters_) f.prepare(sr, 0);
        updateAll();
    }

    void setBand(size_t index, const EQBand& band)
    {
        bands_[index] = band;
        updateBand(index);
    }

    void process(AudioBuffer& buffer) override
    {
        for (size_t b = 0; b < bands_.size(); ++b)
        {
            if (!bands_[b].enabled) continue;       // skip disabled bands
            filters_[b].process(buffer);
        }
    }

    // The combined response: sum the dB contributions of every band.
    double magnitudeDb(double freqHz) const
    {
        double total = 0.0;
        for (size_t b = 0; b < bands_.size(); ++b)
            if (bands_[b].enabled)
                total += filters_[b].coefficients().magnitudeDb(freqHz, sr_);
        return total;
    }

private:
    std::array<EQBand, 8>  bands_;
    std::array<Biquad, 8>  filters_;
    double sr_ = kDefaultRate;
};
```

**Summing dB values to get the combined response** works because cascading filters multiplies
their transfer functions, and `log(a·b) = log(a) + log(b)`. That is Chapter 11's logarithm
property doing real work, and it means the display curve is trivial to compute.

**Skipping disabled bands** matters: an 8-band EQ with two bands in use should cost two biquads,
not eight. A band at 0 dB gain still costs full CPU if you process it.

---

## 53.3 Cut versus boost

The most consequential piece of EQ advice, and it has a technical basis.

> **Cutting is more transparent than boosting.**

Three reasons:

**1. Boosting adds energy where there may be nothing useful.** Boosting 10 kHz on a dull source
raises noise, not detail. Cutting 400 Hz to reveal the top is different in kind.

**2. Boosting with a resonant filter rings.** A +12 dB peak at Q = 4 has a pole close to the unit
circle (Chapter 24), and it rings audibly on transients. A −12 dB cut at the same Q rings far
less, because the cut *attenuates* the resonance it creates.

**3. Boosting eats headroom.** +6 dB at 100 Hz is +6 dB of peak level on bass-heavy material.

**The practical technique** — sometimes called "subtractive EQ":

1. Identify what you *want* more of.
2. Find what is masking it (Chapter 3).
3. **Cut that**, rather than boosting what you want.
4. Make up the overall level.

**When boosting is right:** broad, gentle shelves (±3 dB at low Q) are musical and transparent.
It is narrow, high-gain boosts that cause problems.

---

## 53.4 The frequencies, and what they mean

A working reference. These are perceptual regions, not hard boundaries.

| Range | Name | Boost gives | Cut gives |
|---|---|---|---|
| **20–60 Hz** | Sub | Weight, physical feel | Clarity, headroom |
| **60–120 Hz** | Bass | Fullness, power | Tightness |
| **120–250 Hz** | Low mid | Warmth, body | Removes mud |
| **250–500 Hz** | Mud zone | **Boxiness** | **Clarity** — the most common cut |
| **500 Hz–1 kHz** | Mid | Body, honk | Scoop, "hi-fi" |
| **1–3 kHz** | Presence | Forward, aggressive | Distant, soft |
| **3–6 kHz** | Definition | **Intelligibility**, attack | Smoothness, less fatigue |
| **6–10 kHz** | Brilliance | Detail, sibilance | Warmth, de-essing |
| **10–20 kHz** | Air | Openness, expensive-sounding | Darkness, vintage |

**Two regions deserve special attention for cinematic work.**

**250–500 Hz is where mixes die.** Nearly every instrument has energy here, and it accumulates.
When a mix sounds cluttered and you cannot identify why, this is almost always the region. A
broad 2–3 dB cut here on several elements clears more space than anything else you can do.

**3–6 kHz is dialogue's territory and it is not negotiable.** Chapter 3: the ear-canal resonance
puts maximum sensitivity here, and consonants — which carry intelligibility — live here. Film
mixers carve this band out of music and effects so dialogue can occupy it. Chapter 89 implements
this as automatic frequency-slotting.

---

## 53.5 Linear phase, minimum phase, and dynamic EQ

**Minimum phase** (a normal IIR EQ): phase shifts accompany the magnitude changes. This is what
analogue EQs do and what everyone is used to. Zero latency.

**Linear phase** (FIR, Chapter 22): every frequency delayed equally, so transient shape is
preserved exactly.

| | Minimum phase | Linear phase |
|---|---|---|
| Latency | **None** | `M/2` samples (often 10–50 ms) |
| Transient shape | Altered | **Preserved** |
| Pre-ringing | None | **Present** — and unnatural |
| CPU | Low | High at low frequencies |
| Parallel/crossover summing | Requires care | **Sums exactly** |
| Sounds "natural" | Usually | Sometimes clinical |

**When linear phase genuinely matters:**

- Multiband crossovers that must recombine flat (Chapter 51)
- Parallel processing where the paths are summed
- Mastering, where transient shape on a full mix is being preserved
- Matching one recording to another

**When it is a liability:**

- **Pre-ringing** on sharp transients. A linear-phase filter rings *before* the event, which is
  physically impossible and audible as a soft smear ahead of a drum hit. Many engineers
  specifically prefer minimum-phase EQ on percussion for this reason.
- Live use, because of the latency.
- Low-frequency work, where the tap count explodes.

**Dynamic EQ** is a third option and often the right one: a bell whose gain depends on the level
in that band.

```cpp
float processDynamicBand(float x)
{
    // Measure the level in the band without changing the signal.
    const float banded = bandpass_.processSample(x, 1);
    const float env    = detector_.process(banded);
    const float levelDb = static_cast<float>(gainToDb(env));

    // Only engage above a threshold.
    const float over = levelDb - thresholdDb_;
    const float gainDb = (over > 0.0f) ? -over * (1.0f - 1.0f / ratio_) : 0.0f;

    if (++counter_ >= 16)
    {
        counter_ = 0;
        band_.setFilter(FilterType::Peaking, freqHz_, Q_, gainDb);
    }

    return band_.processSample(x, 0);
}
```

**Dynamic EQ is more transparent than either static EQ or multiband compression** for
problem-solving, because it only acts when the problem is present. A harsh 3 kHz resonance that
appears only on loud notes can be cut *only on those notes*, leaving the quiet ones untouched.

For dialogue in particular this is the right tool: a static cut makes every line duller; a dynamic
cut only tames the shouts.

---

## 53.6 Matching and analysis EQ

**Match EQ** analyses a reference's average spectrum and a target's, then builds a filter to
transform one into the other.

```cpp
std::vector<float> buildMatchCurve(const std::vector<float>& reference,
                                   const std::vector<float>& target,
                                   size_t fftSize, double sampleRate)
{
    // Average the magnitude spectra over many frames (Chapter 27).
    auto refAvg    = averageSpectrum(reference, fftSize);
    auto targetAvg = averageSpectrum(target,    fftSize);

    // The correction is the ratio.
    std::vector<double> correction(refAvg.size());
    for (size_t k = 0; k < refAvg.size(); ++k)
        correction[k] = (targetAvg[k] > 1e-9) ? refAvg[k] / targetAvg[k] : 1.0;

    // SMOOTH IT. An unsmoothed match curve is a mess of narrow
    // resonances that will ring horribly.
    smoothLogarithmically(correction, 1.0 / 3.0);   // 1/3 octave

    // LIMIT IT. Unbounded correction tries to boost a silent band by 60 dB.
    for (auto& c : correction)
        c = std::clamp(c, dbToGain(-12.0), dbToGain(12.0));

    return designFIRFromMagnitude(correction, fftSize);
}
```

**The two clamps are the whole difficulty.** A raw match curve:

- Contains narrow peaks and dips from the specific notes in the material, not from its tonal
  character. Smoothing over 1/3 octave (which matches the ear's critical bands, Chapter 3)
  removes them.
- Tries to correct bands where one source has essentially nothing, producing enormous gains.
  Limiting to ±12 dB prevents it.

**Match EQ is genuinely useful for**: matching dialogue recorded on different days or in
different locations (extremely common in film), matching a mix to a reference, and correcting a
known monitoring or room response.

**It is not useful for** making one song sound like another. Spectral balance is a small part of
what makes a record sound the way it does.

---

## 53.7 EQ in cinematic mixing

Three techniques that are specific to sound for picture.

**1. Frequency slotting.** Assign each element a primary band and cut that band from everything
else. Dialogue gets 3–6 kHz; music and effects are dipped there. Sub-bass goes to impacts and
music; dialogue is high-passed at 80–100 Hz. Chapter 89 automates this.

**2. Distance EQ.** Chapter 2: air absorbs high frequencies. A low-pass makes a sound recede
without changing its level, which is a far more convincing distance cue than volume alone.
Roughly:

| Apparent distance | Low-pass |
|---|---|
| Close | none |
| 10 m | 12 kHz |
| 50 m | 6 kHz |
| 200 m | 3 kHz |
| 1 km | 1.2 kHz |

Chapter 77 derives this from air absorption properly.

**3. Perspective and worldising.** Sound heard through a wall, a phone, a radio or a helmet gets
a characteristic filter:

| Perspective | Filter |
|---|---|
| Through a wall | Low-pass ~400 Hz, plus a low-frequency boost |
| Telephone | Band-pass 300–3400 Hz, steep |
| Radio / walkie | Band-pass 400–3000 Hz + distortion + noise |
| Underwater | Low-pass ~800 Hz + heavy modulation |
| Inside a helmet | Band-pass + a resonance around 1 kHz + reverb |
| Adjacent room | Low-pass ~1 kHz + the room's reverb only |

**The wall case is worth noting because it is not just a low-pass.** Chapter 2: low frequencies
diffract around and through obstacles while high ones are blocked. So sound through a wall has
*relatively more* bass than the original, not just less treble — which is why a low-pass alone
sounds muffled rather than blocked, and adding a low shelf boost fixes it.

---

## 53.8 Exercises

**53.1** Build the 8-band parametric EQ. Verify the combined response by summing the bands' dB
contributions and comparing with a measured sweep.

**53.2** Compare a +12 dB bell at Q = 4 with a −12 dB bell at the same Q on a drum transient. Look
at both impulse responses. Which rings more?

**53.3** Implement tilt EQ. Sweep it ±6 dB about 1 kHz on a full mix and confirm the overall level
stays roughly constant.

**53.4** Take a cluttered mix and apply a broad −3 dB cut at 350 Hz to several elements. Measure
how much clarity you gain compared with boosting 3 kHz by the same amount.

**53.5** Build a linear-phase EQ with a 2001-tap FIR. Apply a +12 dB bell to a drum hit and look
for pre-ringing. Compare with the minimum-phase version.

**53.6** Build a dynamic EQ band. Apply it to a vocal with an intermittent 3 kHz harshness.
Compare with a static cut of the same depth.

**53.7** Implement match EQ. Then remove the smoothing and observe what the curve looks like.
Then remove the limiting and observe the gains it asks for.

**53.8** Build the distance EQ table from §53.7. Render the same footstep at each distance and
check that it reads as distance rather than volume.

**53.9** Build the "through a wall" filter with and without the low-shelf boost. Which sounds like
a wall and which sounds like a blanket?

**53.10** Implement automatic frequency slotting: detect the level in 3–6 kHz on a dialogue track
and apply a dynamic cut in that band to a music track.

---

### Chapter summary

- An EQ is Chapter 23's filters with an interface. **Tilt** — opposite shelves at one frequency —
  is not in the cookbook and is very useful.
- The combined response is the **sum of the bands' dB values**, because cascading multiplies
  transfer functions. **Skip disabled bands** or you pay for them.
- **Cutting is more transparent than boosting**: boosting raises noise, rings (a high-gain
  resonant pole sits near the unit circle), and eats headroom. Find what is masking what you want
  and cut *that*.
- **250–500 Hz is where mixes die** — a broad small cut there clears more space than any boost.
  **3–6 kHz belongs to dialogue** and must be carved out of everything else.
- **Linear phase** preserves transient shape and sums exactly (essential for crossovers and
  parallel paths) but costs latency and **pre-rings**, which is audible and unnatural on
  percussion.
- **Dynamic EQ** is often the right answer: it acts only when the problem is present, so a harsh
  resonance on loud notes can be tamed without dulling the quiet ones.
- **Match EQ** needs two safeguards or it is useless: **smooth over ~1/3 octave** (matching the
  ear's critical bands) and **limit to ±12 dB**.
- Cinematic techniques: **frequency slotting**, **distance EQ** (a low-pass reads as distance far
  better than a level change), and **perspective filters** — noting that "through a wall" needs a
  low-shelf *boost* as well as a low-pass, because low frequencies diffract through.

**Next:** [Chapter 54 — Pitch Shifting and Time Stretching](54-pitch-and-time.md)
