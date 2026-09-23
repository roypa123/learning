# Chapter 28 — Resampling and Interpolation

> How do you read a sample at position 1234.7? There is no sample there. This chapter answers
> that question, and the answer turns out to underpin sample playback, pitch shifting, chorus,
> flanging, Doppler, and every sample-rate conversion you will ever do.

---

## 28.1 Why you need fractional positions

Four situations, all common, all requiring a value between two samples.

**1. Sample rate conversion.** A 48 kHz file in a 44.1 kHz project. The ratio is 1.0884, so
output sample 1 needs input position 1.0884, output sample 2 needs 2.1769, and so on. Almost none
of those are integers.

**2. Playing a sample at a different pitch.** A sampler playing a C4 recording as C#4 must read
the source 1.0595× faster. Chapter 69.

**3. Modulated delay.** Chorus, flanger and vibrato work by continuously varying a delay time.
The delay is a fractional number of samples, and it must move *smoothly* — Chapter 44.

**4. Doppler.** A moving source means a continuously varying delay, which is case 3 again.
Chapter 77.

In every case: **you need `x[n]` where `n` is not an integer.**

---

## 28.2 The interpolation ladder

### Nearest neighbour (drop sample)

```cpp
float nearest(const std::vector<float>& x, double pos)
{
    const size_t i = static_cast<size_t>(pos + 0.5);
    return (i < x.size()) ? x[i] : 0.0f;
}
```

Free, and awful. Rounding the position introduces an error of up to half a sample, which is a
time jitter of up to 11 µs at 44.1 kHz. That jitter is broadband noise, and the resulting
distortion sits around −20 to −30 dB. You hear it as a gritty, aliased crunch.

Use only for deliberate lo-fi effects — it is exactly the sound of an early 8-bit sampler.

### Linear interpolation

```cpp
float linear(const std::vector<float>& x, double pos)
{
    const size_t i    = static_cast<size_t>(pos);
    const double frac = pos - static_cast<double>(i);

    if (i + 1 >= x.size()) return 0.0f;

    return static_cast<float>(x[i] * (1.0 - frac) + x[i + 1] * frac);
}
```

Draw a straight line between the two neighbouring samples and read off the value. One multiply,
one add. **This is the workhorse** — it is what most delay-based effects use.

**But it is not transparent.** A straight line is not the band-limited curve that Chapter 4 said
passes through the samples, so linear interpolation acts as a gentle low-pass filter *and*
introduces distortion. Its frequency response is:

```
   |H(f)| = |sinc(f / fs)|²  ... roughly
```

which is −3.9 dB at Nyquist. In practice:

| Signal frequency | Linear interpolation error |
|---|---|
| Below `fs/10` (< 4.4 kHz) | Negligible, below −60 dB |
| `fs/8` (5.5 kHz) | about −45 dB |
| `fs/4` (11 kHz) | about −25 dB |
| Near Nyquist | Severe: audible dulling and distortion |

**So linear is fine for bass and low mids, and poor for treble.** That is exactly why it is
acceptable in a chorus (which mostly modulates content you are not scrutinising) and unacceptable
in a sample-rate converter.

> **The oversampling trick:** linear interpolation's error is a function of frequency *relative
> to the sample rate*. Run at 4× the sample rate and the same audio content sits four times lower
> relative to Nyquist, so the error drops enormously. This is why some delay effects oversample —
> it is cheaper than a better interpolator.

### Cubic (Hermite) interpolation

Fit a cubic through four points — two on each side — matching both value and slope:

```cpp
float hermite(const std::vector<float>& x, double pos)
{
    const long   i    = static_cast<long>(pos);
    const double frac = pos - static_cast<double>(i);

    auto at = [&](long k) -> double {
        return (k >= 0 && k < static_cast<long>(x.size()))
             ? static_cast<double>(x[static_cast<size_t>(k)]) : 0.0;
    };

    const double xm1 = at(i - 1), x0 = at(i), x1 = at(i + 1), x2 = at(i + 2);

    // Catmull-Rom form: slopes estimated from neighbouring points.
    const double c0 = x0;
    const double c1 = 0.5 * (x1 - xm1);
    const double c2 = xm1 - 2.5 * x0 + 2.0 * x1 - 0.5 * x2;
    const double c3 = 0.5 * (x2 - xm1) + 1.5 * (x0 - x1);

    return static_cast<float>(((c3 * frac + c2) * frac + c1) * frac + c0);
}
```

Four multiplies, four adds (using Horner's method, as written). Roughly **20–30 dB better than
linear** across the midrange, and the standard choice when linear is not good enough and a full
sinc is too expensive.

This is what most samplers and pitch-shifters use.

### Windowed-sinc interpolation

Chapter 4 said the correct reconstruction is a sum of sincs. Chapter 22 said a truncated sinc
must be windowed. Put them together:

```cpp
float sincInterp(const std::vector<float>& x, double pos, int halfWidth = 16)
{
    const long   i    = static_cast<long>(std::floor(pos));
    const double frac = pos - static_cast<double>(i);

    double acc = 0.0;

    for (int k = -halfWidth + 1; k <= halfWidth; ++k)
    {
        const long idx = i + k;
        if (idx < 0 || idx >= static_cast<long>(x.size())) continue;

        const double t = frac - static_cast<double>(k);

        double s;
        if (std::fabs(t) < 1e-9)
            s = 1.0;                                    // sinc(0) -- the trap again
        else
            s = std::sin(kPi * t) / (kPi * t);

        // Window it, or the truncation ripples (Chapter 22).
        const double w = 0.5 + 0.5 * std::cos(kPi * t / halfWidth);

        acc += static_cast<double>(x[static_cast<size_t>(idx)]) * s * w;
    }

    return static_cast<float>(acc);
}
```

**This is essentially exact** — with `halfWidth = 16` the error is below −100 dB — and it costs
32 multiply-adds plus 32 `sin` calls per sample. Far too slow as written.

The production version **precomputes the sinc values into a table** indexed by the fractional
part, quantised to (say) 512 steps with linear interpolation between table entries. That reduces
it to 32 multiply-adds and two table lookups, which is entirely practical.

### The comparison

| Method | Cost/sample | THD+N (midrange) | Use |
|---|---|---|---|
| Nearest | 0 | −25 dB | Lo-fi effect only |
| Linear | 2 ops | −60 dB (low), −25 dB (high) | Modulated delays, chorus, Doppler |
| Cubic/Hermite | 8 ops | −80 dB | Samplers, pitch shift, general purpose |
| Sinc (16-tap, tabled) | ~34 ops | −100 dB | Sample-rate conversion, mastering |
| Sinc (64-tap) | ~130 ops | −140 dB | Offline, archival |

---

## 28.3 Sample-rate conversion done properly

Resampling is not just interpolation. There is a filtering step that people omit, and omitting it
causes aliasing.

### Downsampling (e.g. 96 kHz → 48 kHz)

The new Nyquist is **lower**. Any content between the new Nyquist and the old one will **fold
down** and alias (Chapter 4). It is irreversible.

```
   1. LOW-PASS FILTER at the NEW Nyquist (or slightly below)
   2. Then take every Mth sample
```

**The filter must come first.** Decimating first and filtering after removes nothing — the alias
is already mixed into the legitimate content.

### Upsampling (e.g. 44.1 kHz → 88.2 kHz)

Inserting zeros between samples creates **spectral images** — copies of the spectrum reflected
around the original Nyquist. They must be removed.

```
   1. Insert (L-1) zeros between each sample ("zero stuffing")
   2. LOW-PASS FILTER at the OLD Nyquist
   3. Multiply by L to compensate for the energy lost to the zeros
```

The images are inaudible in the sense that they sit above 22.05 kHz — but they are *not*
harmless: any subsequent nonlinear process (distortion, compression) will fold them back down
into the audible band. This is why upsampling without filtering causes problems that appear
two stages later.

### Arbitrary ratios

For a ratio like 44100/48000, the textbook method is: upsample by `L`, filter, downsample by `M`,
where `L/M` is the ratio in lowest terms. For 44.1↔48 that is 147/160 — an intermediate rate of
7.056 MHz. Correct, and absurd.

The practical method is **polyphase resampling**: only compute the output samples you actually
need, using a bank of pre-computed filter phases. The cost is the same as one filter pass.

For this book we use a simpler approach that is entirely adequate for offline work:

```cpp
std::vector<float> resample(const std::vector<float>& x,
                            double fromRate, double toRate,
                            int quality = 16)
{
    if (x.empty() || fromRate <= 0 || toRate <= 0) return {};

    const double ratio = toRate / fromRate;
    std::vector<float> source = x;

    // DOWNsampling: band-limit FIRST or the content above the new
    // Nyquist folds down irreversibly.
    if (ratio < 1.0)
    {
        const double newNyquist = toRate / 2.0 * 0.92;   // a little margin
        const size_t taps = kaiserTapCount(80.0, toRate * 0.04, fromRate);
        auto lp = designLowPass(newNyquist, fromRate, taps,
                                WindowType::Kaiser, kaiserBeta(80.0));
        source = fastConvolve(source, lp);

        // Compensate for the filter's latency.
        const size_t latency = (lp.size() - 1) / 2;
        source.erase(source.begin(),
                     source.begin() + static_cast<long>(latency));
    }

    const size_t outLength = static_cast<size_t>(
        static_cast<double>(x.size()) * ratio);

    std::vector<float> out(outLength);
    for (size_t n = 0; n < outLength; ++n)
        out[n] = sincInterp(source, static_cast<double>(n) / ratio, quality);

    return out;
}
```

The windowed-sinc interpolation handles the reconstruction; the explicit low-pass handles the
anti-aliasing when going down. For upsampling, the sinc interpolation *is* the image-rejection
filter, so no separate step is needed.

---

## 28.4 Verifying a resampler

Resampler bugs are subtle and this is the test that catches them: a **swept sine**.

Take a 20 Hz → 20 kHz exponential sweep (Chapter 10), resample it, and make a spectrogram
(Chapter 27). A correct resampler shows one clean diagonal line. A broken one shows:

| What you see | What is wrong |
|---|---|
| A second diagonal running **downward** | Aliasing — no anti-alias filter, or it is too high |
| The line stopping early at the top | The filter's cutoff is too low; you are losing treble |
| Faint diagonals at odd angles | Images from upsampling, not removed |
| A haze around the line | Poor interpolation quality (linear at high frequencies) |
| The line wobbling | Position accumulation error — see below |

**The accumulation bug.** Do not compute positions incrementally:

```cpp
pos += step;                         // WRONG: error accumulates over millions of samples
```

Compute them from the index:

```cpp
pos = static_cast<double>(n) / ratio;    // RIGHT: no accumulation
```

The incremental version drifts. Over a three-minute file at 44.1 kHz that is 8 million additions,
and even in `double` the drift becomes measurable — audible as very slow pitch instability. This
is exactly the precision-accumulation problem from Chapter 6, in yet another guise.

And a numerical check:

```
   Resample 44.1k -> 48k -> 44.1k. Compare with the original.
   A good resampler: error below -90 dB.
   Linear interpolation: error around -40 dB.
```

---

## 28.5 The audio-quality politics of resampling

Brief, because it is a subject with more heat than light.

Sample-rate conversion is one of the few places where measurable differences between
implementations genuinely exist, and where the differences are sometimes audible. The reasons are
real:

- **Filter steepness.** A gentle filter loses treble; a steep one rings.
- **Stopband rejection.** Poor rejection lets aliases through.
- **Phase response.** Linear-phase converters pre-ring; minimum-phase ones smear differently.

Independent measurements of commercial converters have shown genuine differences of 30+ dB in
alias rejection. So this is not audiophile folklore; it is measurable engineering.

**That said:** a well-implemented windowed-sinc converter with 80+ dB rejection is transparent by
any reasonable standard, and the differences between good converters are far below the
differences caused by, say, a 0.5 dB EQ move. Spend your attention accordingly.

**Practical advice:** avoid resampling when you can (work at the delivery rate), do it once
rather than repeatedly, and use a good implementation when you must. Repeated conversions
accumulate error; a single conversion does not.

---

## 28.6 Exercises

**28.1** Implement all four interpolation methods. Resample a 1 kHz sine from 44.1 to 48 kHz with
each, then back, and measure the error in dB.

**28.2** Repeat 28.1 with a 10 kHz sine. Which methods degrade, and by how much? Relate the
result to §28.2's table.

**28.3** Make spectrograms of a resampled sweep using each method. Find the aliasing in the
nearest-neighbour version.

**28.4** *Deliberate breakage.* Downsample 96 kHz → 24 kHz **without** the low-pass filter. Make
a spectrogram. Where did the content above 12 kHz go?

**28.5** Implement the `pos += step` accumulation bug and measure the drift after 10 million
samples. How many samples of error? Convert that to cents of pitch error.

**28.6** Build a table-driven sinc interpolator: precompute 512 fractional phases × 32 taps, and
interpolate linearly between table entries. Compare its speed and accuracy with the direct
version.

**28.7** Implement a variable-rate reader: `readAt(double position)` with a smoothly changing
rate. Use it to build a tape-stop effect (rate ramping from 1.0 to 0.0 over two seconds).

**28.8** Measure the frequency response of linear interpolation empirically: resample a swept
sine by a ratio of 1.0001 (so almost nothing changes) and measure the level at each frequency.
Plot the roll-off.

**28.9** Implement 2× oversampling (upsample, process, downsample) around a `tanh` waveshaper.
Compare the aliasing with and without, using a spectrogram of a swept input. This previews
Chapter 29.

---

### Chapter summary

- Reading between samples is needed for sample-rate conversion, pitch shifting, modulated delays
  and Doppler — which is to say, constantly.
- **Nearest neighbour** is free and awful. **Linear** is the workhorse: fine below `fs/10`, poor
  near Nyquist. **Cubic/Hermite** is ~25 dB better for four times the cost. **Windowed sinc** is
  essentially exact and should be table-driven in production.
- Linear interpolation's error scales with frequency relative to the sample rate, so
  **oversampling makes linear interpolation much better** — sometimes cheaper than a better
  interpolator.
- **Downsampling: filter FIRST, then decimate.** Filtering afterwards cannot remove an alias.
- **Upsampling: zero-stuff, then filter** to remove the spectral images, then scale by `L`.
  Unremoved images are inaudible until a later nonlinear stage folds them down.
- **Compute positions from the index (`n / ratio`), never by accumulating (`pos += step`)** —
  accumulation drifts audibly over long files.
- Verify with a **swept sine plus a spectrogram**. Aliasing shows as a second diagonal running
  the wrong way.
- Resample as little as possible, once rather than repeatedly, with a good implementation.

**Next:** [Chapter 29 — Aliasing In Depth, and Oversampling](29-aliasing-and-oversampling.md)
