# Chapter 21 — Convolution From Scratch

> Chapter 20 derived the convolution sum. This chapter makes it concrete: you will work one
> through by hand, implement it, verify its properties, and then use it to put a sound inside a
> synthetic cathedral. By the end you will also understand why direct convolution is far too
> slow for real work — which is the motivation for the FFT four chapters from now.

---

## 21.1 The formula, and what it is telling you to do

```
          ∞
   y[n] = Σ   x[k] · h[n - k]
         k=-∞
```

Translate it literally. To compute output sample `n`:

1. For every input sample `x[k]`...
2. ...multiply it by `h[n-k]` — the impulse response value at the distance between `k` and `n`.
3. Add up all those products.

The quantity `n - k` is "how long ago sample `k` happened, relative to now". So the formula says:

> **Every input sample contributes to this output, weighted by how the system responds to
> something that happened that long ago.**

An echo 10,000 samples into the impulse response means that whatever happened 10,000 samples ago
is still contributing to what you hear now. That is the whole of reverb, in one sentence.

### The equivalent form

Substituting `m = n - k` gives:

```
          ∞
   y[n] = Σ   h[m] · x[n - m]
         m=-∞
```

Identical result, different loop. The first form says "spread each input over the future"; the
second says "gather the past into each output". Both appear in the literature and both are
useful:

- **Gather** (second form) is what you implement in a real-time filter: for each output, look
  back over the last `M` inputs. It is the FIR filter of Chapter 22.
- **Scatter** (first form) is what you implement when processing offline in blocks, and it is the
  basis of overlap-add (§21.8).

---

## 21.2 Flip and slide

The standard mental picture, and it is worth having because it explains the "minus" in `h[n-k]`.

`h[n-k]`, seen as a function of `k`, is `h` **reversed** and then **shifted** to position `n`.
So convolution is:

1. **Flip** the impulse response left-to-right.
2. **Slide** it along the input.
3. At each position, **multiply overlapping samples and sum**.

```
   x:        1   2   3   0   0   0
   h:        4   5

   Flipped h:    5   4

   n=0:      1   2   3
                 5   4                 only h[0] overlaps:  1*4 = 4
                     ^

   n=1:      1   2   3
             5   4                     1*5 + 2*4 = 5 + 8 = 13

   n=2:      1   2   3
                 5   4                 2*5 + 3*4 = 10 + 12 = 22

   n=3:      1   2   3
                     5   4             3*5 = 15

   y = { 4, 13, 22, 15 }
```

If that flipping bothers you, here is the intuition: the *most recent* input sample should be
multiplied by the *earliest* part of the impulse response, because the impulse response has only
just started responding to it. The oldest input sample gets multiplied by the latest part of `h`,
because the system has had the longest to respond to it. The flip is the bookkeeping for that.

### Why the result got longer

Input length 3, impulse response length 2, output length 4.

```
   output length = N + M - 1
```

This is not an implementation artefact; it is real. A 1-second sound through a 3-second reverb
produces 4 seconds of audio. Leaving no room for the tail is a genuine bug, and it sounds like
the reverb being abruptly cut off.

---

## 21.3 A worked example, by hand

Do this one with a pencil before reading the answer. It takes two minutes and it makes the
formula permanent.

```
   x = { 1, 2, 3 }
   h = { 1, 0, -1 }         (a crude "difference over 2 samples" filter)
```

Output length is `3 + 3 - 1 = 5`.

```
   y[0] = x[0]·h[0]                             = 1·1                  =  1
   y[1] = x[0]·h[1] + x[1]·h[0]                 = 1·0 + 2·1            =  2
   y[2] = x[0]·h[2] + x[1]·h[1] + x[2]·h[0]     = 1·(-1) + 2·0 + 3·1   =  2
   y[3] =             x[1]·h[2] + x[2]·h[1]     = 2·(-1) + 3·0         = -2
   y[4] =                         x[2]·h[2]     = 3·(-1)               = -3

   y = { 1, 2, 2, -2, -3 }
```

Notice the pattern in the indices: in every term, **the two indices add up to `n`**. That is the
whole structure of convolution, and it is a good check when you are writing the loop — if your
indices do not sum to `n`, you have a bug.

> **The polynomial connection.** If you have multiplied polynomials, this is exactly that
> operation. `(1 + 2z + 3z²)(1 + 0z − z²)` gives coefficients `1, 2, 2, −2, −3`. Convolution *is*
> polynomial multiplication. That is not a coincidence — Chapter 24's z-transform makes the
> connection explicit, and it is why "multiply the transfer functions" means "convolve the
> impulse responses".

---

## 21.4 The implementation

**Code — `lib/include/audio/convolve.h`**

```cpp
#pragma once

#include <vector>
#include <cstddef>

namespace audio {

// Direct (time-domain) convolution.
// Output length is x.size() + h.size() - 1.
// Cost: O(N*M). Fine for short impulse responses; see Chapter 48 for the
// FFT-based version needed for real reverbs.
inline std::vector<float> convolve(const std::vector<float>& x,
                                   const std::vector<float>& h)
{
    if (x.empty() || h.empty())
        return {};

    const size_t N = x.size();
    const size_t M = h.size();
    std::vector<float> y(N + M - 1, 0.0f);

    for (size_t n = 0; n < N; ++n)
    {
        const double xn = static_cast<double>(x[n]);
        if (xn == 0.0)                      // cheap and often a big win
            continue;

        for (size_t m = 0; m < M; ++m)
            y[n + m] += static_cast<float>(xn * static_cast<double>(h[m]));
    }

    return y;
}

}   // namespace audio
```

### Walkthrough

**This is the scatter form.** For each input sample, we add its scaled copy of the *entire*
impulse response into the output, starting at position `n`. Read it as: "sample `n` arrives,
and the system responds with `h` scaled by that sample, beginning now."

Compare with the gather form:

```cpp
for (size_t n = 0; n < N + M - 1; ++n)
{
    double acc = 0.0;
    for (size_t m = 0; m < M; ++m)
    {
        const long k = static_cast<long>(n) - static_cast<long>(m);
        if (k >= 0 && k < static_cast<long>(N))
            acc += static_cast<double>(x[static_cast<size_t>(k)]) * h[m];
    }
    y[n] = static_cast<float>(acc);
}
```

Same answer. The scatter form is faster here because the inner loop has no bounds check and
writes contiguously; the gather form is what you need for streaming, because it computes one
output at a time.

**`y[n + m] +=` never goes out of range** because `n < N` and `m < M`, so `n + m < N + M - 1`.
Worth confirming to yourself; this is the kind of index arithmetic where an off-by-one is silent.

**`if (xn == 0.0) continue;`** — comparing a float to zero exactly, which Chapter 6 warned
against. It is legitimate here: we are not testing "is this approximately zero", we are testing
"is this exactly the value that would make the inner loop a no-op". Silence in audio is often
genuinely exactly zero, and skipping it can cut the work dramatically when convolving sparse
signals such as drum hits or velvet noise (Chapter 15).

**Accumulating in `double`** — Chapter 7's rule. A 400,000-tap convolution sums 400,000 products
into each output. In `float` the error is clearly audible as a raised noise floor.

### Edge modes

Three conventions for what to return, borrowed from numerical libraries:

| Mode | Length | Meaning |
|---|---|---|
| **full** | `N + M - 1` | Everything, including the tail. Our default. |
| **same** | `N` | Same length as the input, centred. Common in image processing. |
| **valid** | `N - M + 1` | Only positions where `h` fully overlaps `x`. No edge effects. |

For audio, **full** is nearly always right, because the tail is the reverb and you want it.

---

## 21.5 The properties, and why each one matters

Convolution obeys three algebraic laws, and each has a direct practical consequence.

### Commutative: `x * h = h * x`

The result does not depend on which signal you call the input.

```cpp
convolve(x, h) == convolve(h, x)      // identical, to floating-point precision
```

**Consequence:** "reverb the guitar" and "guitar the reverb" give the same answer. More usefully,
it means you can convolve the *shorter* signal into the longer one, which is sometimes faster,
and it means the order of a chain of linear filters is irrelevant — which is Chapter 20's point 6,
now proved.

### Associative: `(x * h1) * h2 = x * (h1 * h2)`

**Consequence, and it is a big one:** you can pre-combine filters. If you always run a low-pass
then a high-pass, convolve their impulse responses *once*, offline, and then apply the single
combined response at run time. You get a band-pass for the cost of one filter instead of two.

This is how multi-stage IR chains are optimised in practice, and why a "cab + mic + room" IR can
be shipped as one file.

### Distributive: `x * (h1 + h2) = (x * h1) + (x * h2)`

**Consequence:** parallel paths can be merged. Two reverbs in parallel, summed, are equivalent to
one reverb whose IR is the sum of theirs. This is how "blend two rooms" presets are implemented
efficiently, and it is why you can build a complex IR by adding simpler ones together (§21.7).

### The identity: `x * δ = x`

Convolving with a single impulse changes nothing. And:

```
   x * (a·δ[n-k])  =  a · x[n-k]
```

Convolving with a scaled, shifted impulse is a gain and a delay. So **a delay line is a
convolution**, and a multi-tap delay is a convolution with a sparse impulse response. That
equivalence is worth holding onto: it means the early reflections of a reverb (Chapter 46) and a
multi-tap delay (Chapter 42) are literally the same operation, differently parameterised.

---

## 21.6 The cost problem

Direct convolution costs `N × M` multiply-accumulates.

For real-time processing, per second of audio at 44.1 kHz, with an impulse response of `M` taps:

| Impulse response | `M` | MACs per second | Feasible? |
|---|---|---|---|
| 32-tap FIR filter | 32 | 1.4 million | Trivially |
| 512-tap FIR | 512 | 22.6 million | Easily |
| Guitar cabinet, 20 ms | 880 | 39 million | Yes |
| Small room, 0.5 s | 22,050 | 972 million | Marginal, one channel |
| Concert hall, 2 s | 88,200 | **3.9 billion** | No |
| Cathedral, 9 s | 396,900 | **17.5 billion** | Absolutely not |

A modern CPU core manages a few billion multiply-adds per second with good vectorisation. The
concert hall is already out of reach, for one channel, using the entire core.

**This is why the FFT exists in audio.** Chapter 25 builds it; Chapter 48 uses it. Fast
convolution reduces the cost from `O(N·M)` to roughly `O(N log N)`, which turns the cathedral from
impossible into a few percent of one core.

For now, direct convolution is perfectly good for impulse responses up to a thousand taps or so,
and for any offline rendering where you do not mind waiting.

---

## 21.7 Building a reverb with convolution

The payoff. We do not have a real impulse response, so we will synthesise one — and the recipe
is instructive in itself.

### What a room's impulse response looks like

From Chapter 2:

```
   level
     |  | direct sound
     |  |
     |  |    |  |   |     early reflections (sparse, discrete)
     |  |    |  |   |  |
     |  |    |  |   |  |||||||.,,,___   late reverb (dense, exponentially decaying noise)
     +----------------------------------> time
        0   10   20  30  40  50  60 ms
```

So a plausible synthetic IR is:

1. An impulse at time zero (the direct sound).
2. A handful of scaled impulses at irregular times over the next 10–80 ms (early reflections).
3. **Exponentially decaying noise** thereafter (late reverb).

Step 3 is the interesting one. Late reverberation genuinely *is* noise — thousands of
overlapping reflections with essentially random arrival times and amplitudes. That is why
"filtered noise with an exponential envelope" is not a cheap approximation of a reverb tail; it
is a fair description of what one is.

**Code — `code/ch21/convreverb.cpp`** (excerpt)

```cpp
// Build a synthetic room impulse response.
std::vector<float> makeRoomIR(double rt60Seconds,
                              double preDelayMs,
                              double sampleRate,
                              double dampingCoeff = 0.35,
                              uint32_t seed = 4242)
{
    const size_t length = static_cast<size_t>(rt60Seconds * 1.3 * sampleRate);
    std::vector<float> h(length, 0.0f);

    FastRandom rng(seed);

    // --- 1. direct sound -----------------------------------------
    const size_t preDelay = static_cast<size_t>(preDelayMs * 0.001 * sampleRate);
    h[0] = 1.0f;

    // --- 2. early reflections: sparse, irregular -----------------
    const double reflectionTimesMs[] = { 11, 17, 23, 29, 37, 41, 53, 61, 73, 89 };
    for (double ms : reflectionTimesMs)
    {
        const size_t pos = preDelay + static_cast<size_t>(ms * 0.001 * sampleRate);
        if (pos < length)
            h[pos] += static_cast<float>(0.7 * std::exp(-ms / 45.0)
                                         * (rng.nextFloat() > 0.0f ? 1.0 : -1.0));
    }

    // --- 3. late reverb: exponentially decaying noise ------------
    // 60 dB over rt60 seconds -> per-sample decay factor
    const double decay = std::pow(10.0, -60.0 / (20.0 * rt60Seconds * sampleRate));
    double gain = 1.0;

    // A one-pole low-pass makes the tail get darker as it decays,
    // which is what air and soft surfaces actually do (Chapter 2).
    double lpState = 0.0;

    const size_t lateStart = preDelay + static_cast<size_t>(0.02 * sampleRate);

    for (size_t n = lateStart; n < length; ++n)
    {
        const double white = rng.nextFloat();
        lpState = lpState * dampingCoeff + white * (1.0 - dampingCoeff);

        h[n] += static_cast<float>(lpState * gain * 0.35);
        gain *= decay;
        if (gain < 1e-9) break;
    }

    // Fade the very end so the IR does not stop abruptly (Chapter 13).
    const size_t fade = std::min(length / 10, static_cast<size_t>(0.05 * sampleRate));
    for (size_t i = 0; i < fade; ++i)
        h[length - 1 - i] *= static_cast<float>(i) / static_cast<float>(fade);

    return h;
}
```

**Walkthrough of the design decisions**

**Irregular reflection times.** `11, 17, 23, 29, 37...` — these are prime-ish numbers,
deliberately not multiples of each other. Regularly spaced reflections would produce audible
comb filtering at `1/spacing` Hz — a metallic, pitched ringing. Chapter 15's rule that "nature is
irregular" applies to room geometry too, and Chapter 46 formalises it with mutually prime delay
lengths.

**Random polarity on the reflections.** Real reflections can arrive inverted, depending on the
surface. Mixing polarities makes the early cluster sound less like a repeating echo.

**The one-pole low-pass on the noise** makes the tail darker over time. This matters more than
it sounds: air absorbs high frequencies (Chapter 2), and soft surfaces absorb them on every
bounce. A tail with constant brightness sounds artificial and "metallic". Changing
`dampingCoeff` from 0.1 to 0.7 takes you from a bright tiled bathroom to a heavily curtained
hall, and it is the single most expressive parameter here.

**The exponential decay factor** is the same "60 dB in RT60 seconds" formula as Chapter 13's
envelope. Reverb tails and envelope decays are the same maths, which is Chapter 20's point about
`a^n` appearing everywhere.

**The fade at the end** is Chapter 13 again: if the IR stops abruptly, every convolved sound ends
with a click.

### Using it

```cpp
int main()
{
    const double sr = 44100.0;

    // A short, dry source: a percussive synth note.
    auto dry = makeDrySource(sr);

    struct Space { const char* name; double rt60; double preDelay; double damping; };
    const Space spaces[] = {
        { "small_room", 0.4,  8.0, 0.55 },
        { "hall",       2.2, 25.0, 0.40 },
        { "cathedral",  6.5, 45.0, 0.30 },
        { "plate",      3.0,  0.0, 0.15 },      // bright, no pre-delay
    };

    for (const auto& s : spaces)
    {
        auto h = makeRoomIR(s.rt60, s.preDelay, sr, s.damping);

        const auto t0 = std::chrono::steady_clock::now();
        auto wet = convolve(dry, h);
        const auto t1 = std::chrono::steady_clock::now();

        // Mix dry and wet. The wet signal is much longer, so start from it.
        std::vector<float> out = wet;
        const double wetGain = 0.35, dryGain = 0.8;
        for (auto& v : out) v = static_cast<float>(v * wetGain);
        for (size_t i = 0; i < dry.size(); ++i)
            out[i] += static_cast<float>(dry[i] * dryGain);

        normaliseInPlace(out, 0.9f);
        writeWav(std::string("reverb_") + s.name + ".wav", out, (int)sr, 1, true);

        const double seconds = std::chrono::duration<double>(t1 - t0).count();
        const double macs = static_cast<double>(dry.size()) * h.size();

        std::cout << std::setw(12) << std::left << s.name << std::right
                  << "  IR " << std::setw(7) << h.size() << " taps"
                  << "  " << std::setw(6) << std::fixed << std::setprecision(2)
                  << h.size() / sr << " s"
                  << "  convolve took " << std::setw(7) << seconds << " s"
                  << "  (" << std::setprecision(1) << macs / seconds / 1e9
                  << " GMAC/s)\n";
    }
}
```

**Expected output**

```
small_room    IR   22932 taps    0.52 s  convolve took    0.41 s  (1.2 GMAC/s)
hall          IR  126126 taps    2.86 s  convolve took    2.26 s  (1.2 GMAC/s)
cathedral     IR  372645 taps    8.45 s  convolve took    6.68 s  (1.2 GMAC/s)
plate         IR  171990 taps    3.90 s  convolve took    3.08 s  (1.2 GMAC/s)
```

**Listen.** These are real reverbs, built from an impulse response and a double loop. The
cathedral genuinely sounds like a cathedral.

**And note the timings.** Convolving a 0.4-second dry sound with the cathedral IR took **6.7
seconds**. That is roughly 16 times slower than real time, for a single channel. The GMAC/s
figure is about as good as a scalar loop gets on a modern core, so this is not a
poorly-written-code problem — it is an algorithmic one. Chapter 48 fixes it properly.

**Experiment 21.1.** Set `dampingCoeff` to 0.05 and then 0.8. Listen to the difference between a
tiled room and a curtained one. Which sounds bigger? Which sounds *further away*?

**Experiment 21.2.** Remove the early reflections entirely (comment out step 2). The tail is
still there, but the sense of a specific room largely vanishes. Early reflections carry the room
*shape*; the tail carries the room *size*.

**Experiment 21.3.** Use the same RT60 but change the pre-delay from 0 to 80 ms. Notice that
longer pre-delay makes the source sound *closer* while the room stays large — because a long gap
before the reverb implies you are near the source and far from the walls. This is one of the most
useful controls in cinematic mixing and Chapter 89 returns to it.

---

## 21.8 Streaming convolution: the overlap problem

A real-time system cannot wait for the whole input. It gets 512 samples at a time.

The naive approach — convolve each block independently and concatenate — is **wrong**, and it is
worth understanding why. Convolving a 512-sample block with a 1,000-tap IR produces 1,511
samples. The last 999 of those belong to the *next* block's time range. Throw them away and you
have chopped off every sound's tail at every block boundary, 86 times a second. The result is a
gated, buzzing mess.

The fix is **overlap-add**:

```
   Block 0:  [--- 512 in ---]
             convolve -> [------ 1511 out ------]
                          |<-512->|<- 999 tail ->|
                          output      SAVE

   Block 1:            [--- 512 in ---]
                       convolve -> [------ 1511 out ------]
                                    |<-512->|<- 999 tail ->|
                                       +
                              saved tail from block 0
                                       =
                                   correct output
```

1. Convolve each input block with the full IR.
2. Output the first `blockSize` samples **plus** the saved tail from the previous block.
3. Save this block's tail for next time.

```cpp
class OverlapAddConvolver
{
public:
    void prepare(const std::vector<float>& impulseResponse, size_t blockSize)
    {
        h = impulseResponse;
        tail.assign(h.size() - 1, 0.0f);
        block = blockSize;
    }

    void process(const float* in, float* out, size_t numSamples)
    {
        auto full = convolve(std::vector<float>(in, in + numSamples), h);

        for (size_t i = 0; i < numSamples; ++i)
            out[i] = full[i] + (i < tail.size() ? tail[i] : 0.0f);

        // Shift the tail: drop what we just used, keep the rest, add the new.
        std::vector<float> newTail(h.size() - 1, 0.0f);
        for (size_t i = 0; i + numSamples < tail.size(); ++i)
            newTail[i] = tail[i + numSamples];
        for (size_t i = numSamples; i < full.size(); ++i)
            newTail[i - numSamples] += full[i];

        tail = std::move(newTail);
    }

private:
    std::vector<float> h, tail;
    size_t block = 0;
};
```

This is correct but not yet fast — it still does direct convolution inside. Chapter 48 replaces
that inner convolution with an FFT, and Chapter 49 partitions the IR so that the first block can
be processed with zero latency while the long tail is handled with larger, more efficient
blocks. That combination is what commercial convolution reverbs do.

> **Note for Chapter 59:** this implementation allocates inside `process()` — three vectors per
> call. That is forbidden in a real-time audio callback. The real version pre-allocates
> everything in `prepare()` and uses a circular tail buffer. It is written this way here for
> clarity, and fixing it is Exercise 21.9.

---

## 21.9 Correlation, and the difference

**Cross-correlation** looks almost identical:

```
   Convolution:   y[n] = Σ x[k]·h[n-k]        (h is flipped)
   Correlation:   r[n] = Σ x[k]·h[k-n]        (h is NOT flipped)
```

The only difference is the flip. Correlation asks **"how similar are these two signals when one
is shifted by `n`?"** The peak of the correlation tells you the shift at which they best line up.

Its uses in audio are genuinely important:

- **Delay estimation.** Correlate a microphone signal with the source to find the exact latency
  of a loop-back — the basis of every audio interface's latency-compensation calibration.
- **Pitch detection.** Correlate a signal with a shifted copy of *itself* (autocorrelation); the
  first strong peak after zero is the period. Chapter 54.
- **Beat and tempo detection.** Autocorrelate an energy envelope and look for peaks at musical
  intervals. Chapter 67.
- **IR measurement.** The swept-sine deconvolution of Chapter 20 is a correlation.

A useful identity: **correlation is convolution with one signal reversed.**

```cpp
auto correlation = convolve(x, sig::reverse(h));
```

Which means you get fast correlation for free once you have fast convolution.

Note that if `h` is symmetric — as a windowed-sinc filter is (Chapter 22) — flipping changes
nothing, and convolution and correlation are identical. That is one reason symmetric filters are
convenient.

---

## 21.10 The convolution theorem (a promise)

The single most important fact in DSP, stated here and proved in Chapter 25:

```
   Convolution in the time domain  <===>  MULTIPLICATION in the frequency domain
   Multiplication in the time domain  <===>  Convolution in the frequency domain
```

In symbols, where `F{}` is the Fourier transform:

```
   F{ x * h }  =  F{x} · F{h}
```

Two consequences, one theoretical and one practical:

**Theoretical.** It explains Chapter 13's click. Multiplying a signal by an abrupt gate in time
*convolves* its spectrum with the gate's spectrum, and the gate's spectrum is broad — so a single
frequency gets smeared across the whole spectrum. That is the click, explained properly at last.

**Practical.** It gives you fast convolution. Instead of `N × M` operations:

1. FFT the input and the impulse response — `O(N log N)`.
2. Multiply the spectra — `O(N)`.
3. Inverse FFT — `O(N log N)`.

For the cathedral IR, that is a speed-up of several thousand times. It converts 6.7 seconds into
a few milliseconds, and it is the entire reason convolution reverb is a product you can buy
rather than a curiosity.

---

## 21.11 Exercises

**21.1** By hand, convolve `{1, 2, 1}` with `{1, -1}`. Then check with code. What does this filter
do to a constant input? To an alternating `+1, -1` input?

**21.2** Verify commutativity numerically: generate two random signals of length 100 and 30, and
confirm `convolve(a,b)` equals `convolve(b,a)` to within 1e-6. Why is the tolerance not zero?

**21.3** Verify associativity with three signals. Then verify that pre-combining two filters
(`convolve(h1,h2)` once) and applying the result gives the same answer as applying them in
sequence — and time both approaches over 100 runs.

**21.4** Convolve any signal with `{0,0,0,1}`. Predict the result before running it. What is this
filter?

**21.5** Write `convolveSame` returning only the centre `N` samples. When would that be the right
choice, and when would it destroy something you wanted?

**21.6** Build an IR consisting of exactly three impulses at 0, 11,025 and 22,050 samples with
amplitudes 1.0, 0.6, 0.36. Convolve a short sound with it. What have you built, and what is its
echo time in milliseconds?

**21.7** Measure the RT60 of your synthetic IRs using Exercise 20.7's function. Does it match the
`rt60Seconds` you asked for? If not, by how much, and where does the discrepancy come from?

**21.8** Implement `autocorrelate(x)` and use it to find the period of a 220 Hz sine at 44.1 kHz.
Does the first peak land at 200.45 samples? How would you get sub-sample accuracy?

**21.9** Rewrite `OverlapAddConvolver` so that `process()` performs **no allocations**: preallocate
everything in `prepare()` and use a circular buffer for the tail. Verify it produces
bit-identical output to the allocating version.

**21.10** Time direct convolution for impulse responses of 100, 1,000, 10,000 and 100,000 taps
against a fixed 44,100-sample input. Plot time against `M`. Confirm the relationship is linear in
`M`, then extrapolate to the 9-second cathedral and compare with the measured figure.

**21.11** Take your synthetic cathedral IR and convolve it with *itself*. What does the result
sound like, and what is its RT60? (This is the associativity property predicting the answer
before you listen.)

---

### Chapter summary

- `y[n] = Σ x[k]·h[n-k]`. **Flip the impulse response, slide it along the input, multiply and
  sum.** In every term the two indices add to `n`.
- **Output length is `N + M - 1`.** Leaving no room for the tail truncates the reverb.
- Two loop forms: **scatter** (spread each input into the future — faster offline) and **gather**
  (collect the past into each output — needed for streaming and for FIR filters).
- Properties and their uses: **commutative** (cascade order is irrelevant), **associative**
  (pre-combine filters into one IR), **distributive** (merge parallel paths), and
  **`x * δ = x`** (a delay line *is* a convolution).
- **Cost is `O(N·M)`.** A 2-second hall needs 3.9 billion MACs per second per channel; a
  cathedral 17.5 billion. Direct convolution is fine to about a thousand taps and hopeless
  beyond.
- A convincing synthetic room IR is: a direct impulse, a handful of **irregularly** spaced early
  reflections, then **low-passed noise with an exponential decay** — with a fade at the end.
  Early reflections carry room *shape*; the tail carries room *size*; pre-delay controls apparent
  *distance from the walls*.
- Streaming needs **overlap-add**: convolve each block with the full IR, output the head plus the
  previous block's saved tail, and save the new tail.
- **Correlation is convolution with one signal reversed.** It answers "how similar, at what
  shift?" — used for latency measurement, pitch detection and tempo detection.
- **The convolution theorem:** convolution in time is multiplication in frequency. It explains
  the click from Chapter 13, and it is what makes fast convolution — and therefore convolution
  reverb — possible.

**Next:** [Chapter 22 — FIR Filters and Windowed-Sinc Design](22-fir-filters.md)
