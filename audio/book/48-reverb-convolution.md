# Chapter 48 — Reverb III: Convolution

> Chapters 46 and 47 built spaces that sound plausible. This chapter puts your sound inside a
> *specific* real room — a named cathedral, a particular studio, a guitar cabinet that exists —
> by measuring its impulse response and convolving with it. Chapter 20 proved this works;
> Chapter 25 made it fast enough.

---

## 48.1 The premise, restated

Chapter 20's result: an LTI system is completely described by its impulse response. A room is
approximately LTI. Therefore:

```
   output = input * roomImpulseResponse
```

**The file *is* the room.** Load a different IR and you are in a different place. No parameters
to tune, no algorithm to design, no approximation of what a space sounds like — the actual
measured behaviour of the actual space.

This is both the enormous strength and the fundamental limitation, and being clear about both is
the point of the chapter.

---

## 48.2 Measuring an impulse response

Chapter 20 outlined the problems: an impulse has too little energy, you cannot make a real one,
and speakers distort. The standard solution is the **exponential swept sine** (Farina's method).

### The method

**1. Generate the sweep.** Exponential (logarithmic) from 20 Hz to 20 kHz over 5–30 seconds:

```cpp
std::vector<float> exponentialSweep(double f1, double f2, double seconds, double sr)
{
    const size_t n = static_cast<size_t>(seconds * sr);
    std::vector<float> out(n);

    const double T = seconds;
    const double K = T * 2.0 * kPi * f1 / std::log(f2 / f1);
    const double L = std::log(f2 / f1) / T;

    for (size_t i = 0; i < n; ++i)
    {
        const double t = static_cast<double>(i) / sr;
        out[i] = static_cast<float>(std::sin(K * (std::exp(t * L) - 1.0)));
    }

    // Fade the ends, or the start and stop are broadband clicks (Ch 13).
    const size_t fade = static_cast<size_t>(0.02 * sr);
    for (size_t i = 0; i < fade; ++i)
    {
        const float g = static_cast<float>(i) / static_cast<float>(fade);
        out[i] *= g;
        out[n - 1 - i] *= g;
    }

    return out;
}
```

**2. Play it in the space and record the result.**

**3. Build the inverse filter.** Time-reverse the sweep and apply an amplitude envelope that
compensates for the sweep's `1/f` energy distribution:

```cpp
std::vector<float> inverseFilter(const std::vector<float>& sweep,
                                 double f1, double f2, double seconds, double sr)
{
    std::vector<float> inv(sweep.rbegin(), sweep.rend());   // time-reversed

    // The exponential sweep spends more time at low frequencies, so it
    // deposits more energy there. Compensate with a -6 dB/octave ramp.
    const double L = std::log(f2 / f1) / seconds;
    const size_t n = inv.size();

    for (size_t i = 0; i < n; ++i)
    {
        const double t = static_cast<double>(i) / sr;
        inv[i] *= static_cast<float>(std::exp(-t * L));
    }

    return inv;
}
```

**4. Convolve the recording with the inverse filter** (Chapter 25's `fastConvolve`). The sweep
collapses back to an impulse, and the recording collapses to the impulse response.

### The elegant part

Because the sweep's frequency rises with time, **any harmonic distortion the loudspeaker produces
arrives at a different time than the fundamental**. In the deconvolved result, the distortion
products appear as separate, *earlier* impulses, cleanly separated from the true impulse response.

```
   deconvolved result:

   ... [3rd harm] ... [2nd harm] ...... [THE IMPULSE RESPONSE] ...
        (discard)      (discard)          (keep this)
   ────────────────────────────────────►  time
```

You simply window off everything before the main peak. **A measurement technique that isolates
its own distortion** — genuinely beautiful engineering, and the reason this method replaced all
the alternatives.

### Practical notes

- **Longer sweeps give better signal-to-noise.** 10 s is typical; 30 s for very quiet spaces.
- **Multiple measurements averaged** reduce noise further, but only if nothing moves between them.
- **Trim the result** to where the tail reaches the noise floor, then apply a gentle fade (Chapter
  13) so convolved sounds do not click.
- **Normalise** so the direct sound is at a known level, or every IR has a different loudness.
- **The measurement captures the speaker and microphone too.** For room work that is usually fine;
  for precise work you deconvolve the speaker's own response out as well.

---

## 48.3 Real-time convolution

Chapter 25 gave fast convolution:

```
   y = IFFT( FFT(x) · FFT(h) )
```

with zero-padding to at least `N + M − 1`. That works offline. For real time there are two
problems:

**Latency.** You must collect `N` samples before transforming. A 9-second cathedral IR needs
`N = 524,288`, which is **11.9 seconds of latency**. Unusable.

**Block processing.** A real-time callback provides 256 or 512 samples at a time, not the whole
file.

**Overlap-add** (Chapter 21) solves the block problem:

```cpp
class FFTConvolver
{
public:
    void prepare(const std::vector<float>& ir, size_t blockSize)
    {
        blockSize_ = blockSize;
        fftSize_   = nextPowerOfTwo(blockSize * 2);

        // Pre-transform the IR once. This never changes at run time.
        irSpectrum_ = fftReal(ir, fftSize_);

        overlap_.assign(fftSize_, 0.0f);
        scratch_.assign(fftSize_, Complex(0.0, 0.0));
    }

    void process(const float* in, float* out, size_t numSamples)
    {
        // Load the block, zero-pad to fftSize.
        for (size_t i = 0; i < fftSize_; ++i)
            scratch_[i] = (i < numSamples) ? Complex(in[i], 0.0) : Complex(0.0, 0.0);

        fft(scratch_, false);

        for (size_t k = 0; k < fftSize_; ++k)
            scratch_[k] *= irSpectrum_[k];

        fft(scratch_, true);

        // Output = this block's head + the previous block's tail.
        for (size_t i = 0; i < numSamples; ++i)
            out[i] = static_cast<float>(scratch_[i].real()) + overlap_[i];

        // Save this block's tail for next time.
        for (size_t i = 0; i < fftSize_ - numSamples; ++i)
            overlap_[i] = static_cast<float>(scratch_[numSamples + i].real())
                        + ((i + numSamples < fftSize_) ? overlap_[i + numSamples] : 0.0f);
    }

private:
    std::vector<Complex> irSpectrum_, scratch_;
    std::vector<float>   overlap_;
    size_t blockSize_ = 512, fftSize_ = 1024;
};
```

**But this only works when the IR fits in one FFT.** For an IR longer than the block size — which
is every reverb — you need the IR split into blocks, and that is Chapter 49's partitioned
convolution.

**For now, the useful case:** IRs shorter than the block size work directly. Guitar cabinet IRs
(20 ms, ~880 samples) fit comfortably in a 2048-point FFT, so cabinet simulation is a solved
problem with the code above.

---

## 48.4 Working with impulse responses

### Stereo and true stereo

| Type | Channels | What it captures |
|---|---|---|
| **Mono** | 1 | One source position, one mic position |
| **Stereo** | 2 | One source, two mic positions — gives a stereo tail from a mono source |
| **True stereo** | 4 | L→L, L→R, R→L, R→R — preserves the input's stereo image correctly |

**True stereo matters more than people expect.** With a plain stereo IR, a sound panned hard left
in the input produces the same reverb as one panned hard right — the space does not know where
the source was. True stereo convolution uses four IRs and keeps that information:

```cpp
outL = convolve(inL, irLL) + convolve(inR, irRL);
outR = convolve(inL, irLR) + convolve(inR, irRR);
```

Four convolutions instead of two, and the reverb now moves with the source. For cinematic work,
where a sound's position in the space is meaningful, this is worth the cost.

### Modifying IRs

An IR is just audio, so you can process it — and doing so gives you the parameters that
convolution otherwise lacks:

| Modification | Effect |
|---|---|
| **Truncate + fade** | Shorter decay. Cheapest way to shorten a reverb. |
| **Apply an exponential envelope** | Change the decay rate. Faster decay = smaller room. |
| **Reverse** | The famous reverse reverb — a swell into the sound |
| **EQ the IR** | Change the room's tone permanently |
| **Time-stretch the IR** | Change the apparent room size (Chapter 37) |
| **Remove the first N ms** | Delete the direct sound and early reflections, leaving only the tail |
| **Convolve an IR with itself** | Doubles the RT60 (Chapter 21's associativity) |
| **Add pre-delay** | Insert silence at the start |

**Truncating with a fade is the single most useful one**, because IR length is what costs CPU.
Halving a 6-second IR to 3 seconds halves the cost, and if the tail was below the noise floor
anyway you have lost nothing.

**Removing the direct sound** is how you get a pure "send" reverb from an IR that was measured
with the source in the room.

---

## 48.5 Strengths and limitations

### What convolution does that algorithms cannot

**Specific real spaces.** Abbey Road Studio 2, the Sydney Opera House, a stairwell you recorded.
No algorithm reproduces a particular room.

**Accurate early reflections.** Chapter 46 noted that Schroeder reverbs are poor at early
reflections. A measured IR has the *actual* reflection pattern of the actual geometry, which is
what tells your brain the room's shape and size.

**Non-room objects.** Guitar cabinets, microphones, plate and spring reverbs, analogue EQs, vinyl
chains — anything approximately LTI can be captured.

**No tuning.** It sounds right because it *is* right.

### What it cannot do

**Parameter changes.** You cannot make the room bigger. IR modification helps, but changing the
decay by stretching the IR also changes its spectral character in ways that are not physical.

**Modulation.** Algorithmic reverbs modulate their delays for smoothness and movement.
Convolution is static, which some people hear as "sterile" on sustained material — it lacks the
subtle life that modulation provides.

**Non-linear or time-varying systems.** Chapter 20's table: you cannot capture a compressor, a
tape machine's wow and flutter, or a spring reverb's nonlinear behaviour in a single IR. Products
that claim to do so use multiple IRs at different levels with interpolation, or a hybrid model.

**Cost.** Even with FFT convolution, a long true-stereo IR is expensive — and it does not scale
down gracefully. An algorithmic reverb can be made cheaper by using fewer delay lines;
convolution's cost is set by the IR length.

**Memory.** A 10-second true-stereo IR at 48 kHz is 7.7 MB. A library of fifty is 385 MB.

---

## 48.6 Hybrid approaches

The best of both, and what most high-end reverbs actually do:

```
   in ──┬──► [ convolution: the first 80-150 ms of a measured IR ] ──┐
        │       (accurate early reflections, real geometry)           │
        │                                                            ├──► out
        └──► [ FDN: the late tail ]  ───────────────────────────────┘
                (modulated, parameterised, cheap, smooth)
```

**Why this works:** early reflections carry the room's *identity* and are short (so cheap to
convolve). The late tail carries the room's *size* and is long (so expensive to convolve) but is
statistically smooth — which is exactly what an FDN is good at.

You get accurate room character, adjustable decay time, modulation for smoothness, and a fraction
of the CPU cost. The crossover between the two is typically 80–150 ms, with a short crossfade so
the join is inaudible.

This is what Chapter 91's cinematic engine uses.

---

## 48.7 Exercises

**48.1** Generate an exponential sweep and its inverse filter. Convolve them together — the result
should be an impulse. Measure how close to a perfect impulse it is.

**48.2** Measure a real impulse response: play a sweep through a speaker in a room, record it,
deconvolve. (A phone speaker and a phone mic will work well enough to demonstrate it.)

**48.3** In the deconvolved result from 48.2, find the harmonic distortion products before the
main peak. How far below the main peak are they?

**48.4** Implement the FFT convolver for short IRs. Verify it matches direct convolution.

**48.5** Truncate a long IR to 50%, 25% and 10% of its length with a fade. At what point does it
stop sounding like the same space?

**48.6** Reverse an IR and apply it to a percussive sound. This is reverse reverb.

**48.7** Take a mono IR and use it as a true-stereo IR (all four the same). Then decorrelate the
L→R and R→L paths by delaying them slightly. Compare the stereo image.

**48.8** Convolve an IR with itself. Measure the RT60 before and after. Does it double?

**48.9** Apply an exponential decay envelope to an IR to halve its RT60. Compare with simple
truncation.

**48.10** Build the hybrid reverb from §48.6: convolve the first 120 ms of a measured IR, and use
Chapter 47's FDN for the tail, crossfaded. Compare CPU cost with full convolution.

---

### Chapter summary

- Convolution reverb puts a sound in a **specific real space**: `output = input * measuredIR`.
  The file *is* the room.
- IRs are measured with an **exponential swept sine** and deconvolved with a **time-reversed,
  amplitude-corrected** copy. Because frequency rises with time, **the speaker's own distortion
  appears as separate earlier impulses** that can simply be windowed off.
- **Fade the sweep's ends**, trim and fade the resulting IR, and normalise it.
- Real-time needs **overlap-add**; IRs longer than the block size need **partitioning**
  (Chapter 49). IRs shorter than the block — guitar cabinets — work directly.
- **True stereo** uses four IRs (L→L, L→R, R→L, R→R) so the reverb knows where the source was.
  Plain stereo does not, and the difference matters when position is meaningful.
- IRs are audio and can be **modified**: truncate (cheapest way to shorten), envelope (change
  decay), reverse, EQ, stretch, remove the direct sound, or convolve with itself (doubles RT60).
- **Strengths**: specific spaces, accurate early reflections, non-room objects, no tuning.
  **Limitations**: no parameter changes, no modulation (can sound sterile), cannot capture
  nonlinear or time-varying systems, expensive and memory-hungry.
- **Hybrid** — convolved early reflections plus an FDN tail — gives accurate room identity,
  adjustable decay, modulation, and a fraction of the cost. This is what good reverbs do.

**Next:** [Chapter 49 — Reverb IV: Partitioned Low-Latency Convolution](49-reverb-partitioned.md)
