# Chapter 20 — Linearity, Time-Invariance, and the Impulse Response

> This is the most important chapter in Part II, and possibly in the book. It contains one
> genuinely astonishing result: for a large and useful class of systems, **poking it once tells
> you everything it will ever do**.
>
> That result is why convolution reverb works, why filters can be designed on paper, and why
> you can capture a cathedral in a file.

---

## 20.1 Linearity

A system is **linear** if it obeys two rules.

### Rule 1: Homogeneity (scaling)

If you scale the input, the output scales by the same factor.

```
   x[n]      ->  y[n]
   a · x[n]  ->  a · y[n]        for any constant a
```

### Rule 2: Additivity (superposition)

The response to a sum of inputs is the sum of the individual responses.

```
   x1[n]          ->  y1[n]
   x2[n]          ->  y2[n]
   x1[n] + x2[n]  ->  y1[n] + y2[n]
```

Together these are the **superposition principle**:

```
   a·x1[n] + b·x2[n]   ->   a·y1[n] + b·y2[n]
```

### What is and is not linear

| System | Linear? | Why |
|---|---|---|
| `y = 3x` | ✓ | Scaling and adding both pass through |
| `y = x[n] + x[n-1]` | ✓ | A sum of scaled, shifted inputs |
| `y = 0.5·x[n] + 0.9·y[n-1]` | ✓ | Feedback is fine; it is still all scaling and adding |
| `y = x²` | ✗ | Double the input, quadruple the output |
| `y = tanh(x)` | ✗ | Saturates; doubling the input does not double the output |
| `y = clamp(x, -1, 1)` | ✗ | Clipping is the definition of non-linear |
| `y = x + 0.1` | ✗ | Fails homogeneity: `0 → 0.1`, not `0 → 0` |
| `y = |x|` | ✗ | `−x` gives the same output as `x` |

That last-but-one case surprises people. Adding a constant feels harmless, but a linear system
**must** map zero to zero. Silence in, silence out. Any system with a DC offset added is
technically non-linear, which is a small part of why DC offsets cause trouble in signal chains.

Notice what is on the non-linear list: **every distortion, every clipper, every compressor,
every saturator.** That is not a coincidence. *Distortion means non-linearity.* It is the
technical definition of the word.

### Why that matters immediately

A linear system **cannot create new frequencies**. Feed it a 440 Hz sine and you get 440 Hz out —
louder, quieter, or phase-shifted, but still 440 Hz. Nothing else can appear.

A non-linear system **always** creates new frequencies. That is what harmonic distortion is, and
why:

- A distortion pedal makes a guitar sound richer — it adds harmonics that were not there.
- Clipping generates odd harmonics (Chapter 14).
- Saturating the low end implies a fundamental that is not present (Chapter 3's missing
  fundamental).
- Non-linear processes **need oversampling** (Chapter 29), because those new harmonics can land
  above Nyquist and alias, whereas a linear filter never generates anything to alias.

That last point is the most practically important thing in this section: **linear processes
cannot alias; non-linear ones can.** It tells you exactly which parts of your signal chain need
protection.

### Testing linearity in code

You can test it empirically, and it is worth doing when you are unsure about a system you did not
write:

```cpp
bool isLinear(System& sys)
{
    auto x1 = sig::sine(1000, 440.0, 44100.0, 0.3);
    auto x2 = sig::sine(1000, 660.0, 44100.0, 0.2);

    sys.reset();  auto y1  = run(sys, x1);
    sys.reset();  auto y2  = run(sys, x2);
    sys.reset();  auto ySum = run(sys, sig::add(x1, x2));

    // Is the response to the sum equal to the sum of the responses?
    return sig::maxDifference(ySum, sig::add(y1, y2)) < 1e-5;
}
```

Run this against `Gain` and it passes. Run it against a `tanh` saturator and it fails
dramatically. **Note the `sys.reset()` calls** — without them the system carries state from the
previous test and the comparison is meaningless. That is Chapter 18's "signals are zero before
they start" rule in practice.

---

## 20.2 Time invariance

A system is **time-invariant** if delaying the input simply delays the output, with no other
change.

```
   x[n]      ->  y[n]
   x[n - k]  ->  y[n - k]        for any integer k
```

Informally: **the system behaves the same way today as it did yesterday.** Its behaviour does not
depend on *when* you use it.

| System | Time-invariant? |
|---|---|
| `y = 3x` | ✓ |
| `y = x[n] + x[n-1]` | ✓ |
| A filter with fixed coefficients | ✓ |
| `y = n · x[n]` | ✗ — the gain depends on the sample index |
| A filter whose cutoff is being swept | ✗ |
| Anything with an LFO modulating it | ✗ |
| A compressor | ✗ (and non-linear too) |

Again, look at what fails: **every modulated effect.** Chorus, flanger, phaser, auto-wah, tremolo
and vibrato are all time-varying by design. So is any synth with an envelope on the filter.

### Does that mean the theory is useless for them?

No, and this is worth being precise about, because it is a common misunderstanding in both
directions.

The standard approach is **quasi-static analysis**: if the parameter changes slowly compared to
the signal, you treat the system as LTI at each instant, with slowly-changing coefficients. A
filter sweeping over a second is, at any given millisecond, essentially a fixed filter.

This works well and it is how virtually all modulated effects are implemented. It breaks down in
two specific circumstances, both of which you will meet:

- **When modulation approaches audio rate.** Modulate a filter cutoff at 200 Hz and you are no
  longer sweeping a filter; you are doing something closer to ring modulation, with sidebands.
  This is exploited deliberately in some sound design.
- **When coefficients change abruptly.** A jump in coefficients is a discontinuity, and Chapter
  13 told you what discontinuities are. This is why Chapter 61 exists: parameter smoothing is not
  a nicety, it is the thing that keeps the quasi-static assumption valid.

---

## 20.3 Why LTI is the magic word

When a system is **both** linear and time-invariant, an enormous toolkit unlocks:

1. It is completely described by its **impulse response** (this chapter).
2. Its output is the **convolution** of input and impulse response (Chapter 21).
3. It has a **frequency response** `H(ω)` — one complex number per frequency (Chapter 19).
4. It can be described by **poles and zeros**, which tell you stability at a glance (Chapter 24).
5. Cascading two of them **multiplies** their frequency responses.
6. The order of cascading **does not matter** (convolution is commutative).
7. It **cannot** create new frequencies, so it cannot alias.

Point 6 is worth dwelling on, because it has a practical consequence people find surprising:
**a low-pass followed by a high-pass gives exactly the same result as a high-pass followed by a
low-pass**, to the last bit. If reordering two of your processors changes the sound, then at
least one of them is non-linear or time-varying — which is exactly why *compressor-then-EQ* and
*EQ-then-compressor* sound different while *EQ-then-EQ* in either order does not.

---

## 20.4 The impulse response

Feed the system a single impulse `δ[n]` — one sample of 1.0, silence before and after. Whatever
comes out is the **impulse response**, written `h[n]`.

```
             +-------+
   δ[n] ---> |  LTI  | ---> h[n]
             +-------+
```

In code:

```cpp
std::vector<float> measureImpulseResponse(System& sys, size_t length)
{
    sys.reset();                                  // essential
    std::vector<float> h(length);
    h[0] = sys.process(1.0f);                     // the impulse
    for (size_t n = 1; n < length; ++n)
        h[n] = sys.process(0.0f);                 // silence thereafter
    return h;
}
```

Some examples:

**A gain of 0.5:**
```
   h[n] = { 0.5, 0, 0, 0, ... }
```

**A two-sample delay:**
```
   h[n] = { 0, 0, 1, 0, 0, ... }
```

**A two-point averager, `y[n] = 0.5·x[n] + 0.5·x[n-1]`:**
```
   h[n] = { 0.5, 0.5, 0, 0, ... }
```

**A one-pole low-pass, `y[n] = 0.1·x[n] + 0.9·y[n-1]`:**
```
   h[n] = { 0.1, 0.09, 0.081, 0.0729, ... }        decaying forever
```

Note how directly the impulse response reads off the difference equation in the first three
cases: the `b` coefficients *are* the impulse response when there is no feedback. With feedback,
the response continues indefinitely — hence **FIR** (finite) versus **IIR** (infinite), as
Chapter 14 introduced.

---

## 20.5 The astonishing result

**If you know `h[n]`, you know everything the system will ever do to any input.**

Here is the argument, and it is worth following closely because it is both simple and the
foundation of everything after it.

### Step 1: Any signal is a sum of scaled, shifted impulses

Take any signal:

```
   x[n] = { 3, -1, 4, 2 }        at n = 0, 1, 2, 3
```

Rewrite it:

```
   x[n] = 3·δ[n] + (-1)·δ[n-1] + 4·δ[n-2] + 2·δ[n-3]
```

Check it: at `n = 0` only the first term is non-zero, giving 3. At `n = 1` only the second,
giving −1. And so on. ✓

In general:

```
          ∞
   x[n] = Σ   x[k] · δ[n - k]
         k=-∞
```

This looks like a profound identity and it is really a triviality dressed up: it says "a signal
is made of its own samples, each sitting at its own position." But writing it this way is the
whole trick.

### Step 2: Apply the system

Now push that decomposition through the system, using the two LTI properties.

**Time invariance** says: if `δ[n] → h[n]`, then `δ[n-k] → h[n-k]`. A shifted impulse gives a
shifted impulse response.

**Linearity** says: the system's response to a sum of scaled things is the sum of the scaled
responses.

Putting them together:

```
          ∞                                    ∞
   x[n] = Σ  x[k]·δ[n-k]      ---->    y[n] =  Σ  x[k]·h[n-k]
         k=-∞                                 k=-∞
```

### Step 3: That is convolution

```
          ∞
   y[n] = Σ   x[k] · h[n - k]        ==      y[n] = (x * h)[n]
         k=-∞
```

The `*` here means **convolution**, not multiplication. (An unfortunate collision of notation.
Context disambiguates: if both operands are signals, it is convolution.)

### What we just proved

> **Poke the system once. Record what comes out. You can now compute its response to any input
> whatsoever, forever, without ever touching the system again.**

You do not need to know what is inside. Not the circuit, not the code, not the room. You need one
measurement.

This is why:

- **Convolution reverb works.** Fire a starting pistol in a cathedral, record the response, and
  you can put any sound into that cathedral. The file *is* the room.
- **Filters can be designed on paper.** Decide on a frequency response, derive the corresponding
  `h[n]`, implement it. Chapter 22.
- **Speaker and microphone correction works.** Measure the impulse response, invert it, convolve.
- **Guitar cabinet simulation works.** A "cab IR" is exactly this measurement, and it is why a
  6 KB file can convincingly replace a microphone in front of a speaker cabinet.

All of it follows from two properties and one measurement.

---

## 20.6 The frequency response

Chapter 19 said that a phasor passes through an LTI system unchanged except for a complex scale
factor `H(ω)`. We can now say what `H(ω)` is.

Feed `x[n] = e^(jωn)` into the convolution sum:

```
          ∞                        ∞
   y[n] = Σ  h[k]·e^(jω(n-k))  =   Σ  h[k]·e^(jωn)·e^(-jωk)
         k=-∞                     k=-∞

                        ∞
        = e^(jωn)  ·    Σ  h[k]·e^(-jωk)
                       k=-∞
          \______/      \_________________/
           input             a constant
```

The input comes out multiplied by a quantity that depends on `ω` but not on `n`. That quantity is
the frequency response:

```
            ∞
   H(ω)  =  Σ  h[n]·e^(-jωn)
           n=-∞
```

**The frequency response is the Fourier transform of the impulse response.** (Specifically, the
discrete-time Fourier transform; Chapter 25 gives the computable version, the DFT.)

Which means:

```
   impulse response  <---- Fourier ---->  frequency response
        h[n]                                   H(ω)
   (what it does in time)              (what it does to each frequency)
```

**These are two views of the same object.** Chapter 2 promised this duality; here it is, made
exact. A reverb's impulse response and its frequency response contain identical information. A
filter's coefficients and its EQ curve contain identical information.

And from `H(ω)`:

- `|H(ω)|` — the **magnitude response**. This is the curve on every EQ plugin you have used.
- `arg(H(ω))` — the **phase response**. Usually hidden, occasionally crucial.

---

## 20.7 Measuring impulse responses for real

In theory: fire an impulse, record. In practice, three problems.

**Problem 1: signal-to-noise ratio.** An impulse has enormous peak and almost no energy
(Chapter 11 measured its crest factor at 46 dB). Recorded in a real space, the tail disappears
into the noise floor long before the reverb has actually decayed.

**Problem 2: you cannot make a real impulse.** A balloon pop or a starting pistol is short, but
it is not one sample, it is not flat in frequency, and it is not repeatable.

**Problem 3: non-linearity.** Loudspeakers distort. Any distortion contaminates the measurement
with harmonics that do not belong to the room.

### The solution: swept-sine deconvolution

The standard technique (Farina's method), and it solves all three at once:

1. Play an **exponential sine sweep** from 20 Hz to 20 kHz over 5–30 seconds. Long duration means
   lots of energy at every frequency, so the signal-to-noise ratio is excellent.
2. Record the result in the space.
3. Convolve the recording with a time-reversed, amplitude-corrected copy of the sweep (the
   "inverse filter"). This collapses the sweep back into an impulse, and the recording collapses
   into the impulse response.

The elegance is in step 3's side effect. Because the sweep's frequency changes with time, any
harmonic distortion produced by the speaker arrives at a *different time* in the deconvolved
result — it appears as separate, earlier artefacts, cleanly separated from the true impulse
response. **You can simply window them off.** A measurement technique that isolates its own
distortion is a genuinely beautiful piece of engineering.

We implement this in Chapter 48 when building the convolution reverb. Until then, know that:

- Any "IR" file you download was almost certainly made this way.
- The technique works for rooms, speakers, microphones, guitar cabinets, analogue EQs, spring
  reverbs and tape machines — anything approximately LTI.
- It does **not** work for anything non-linear or time-varying. You cannot capture a compressor
  in an IR, or a chorus, or a tape machine's wow and flutter. When a product claims to have
  "sampled" a non-linear device, it is doing something more elaborate than a single IR — usually
  a set of IRs at different levels, with interpolation.

---

## 20.8 The program

**Code — `code/ch20/lti.cpp`** (excerpt)

```cpp
// A tiny system interface so we can test several systems uniformly.
struct System
{
    virtual ~System() = default;
    virtual float process(float x) = 0;
    virtual void  reset() = 0;
    virtual const char* name() const = 0;
};

struct GainSys : System {
    float g;
    explicit GainSys(float gain) : g(gain) {}
    float process(float x) override { return g * x; }
    void  reset() override {}
    const char* name() const override { return "gain 0.5"; }
};

struct AveragerSys : System {
    float x1 = 0.0f;
    float process(float x) override { const float y = 0.5f*x + 0.5f*x1; x1 = x; return y; }
    void  reset() override { x1 = 0.0f; }
    const char* name() const override { return "2-point average"; }
};

struct OnePoleSys : System {
    double y1 = 0.0, a = 0.9;
    float process(float x) override
    { y1 = (1.0 - a) * x + a * y1; return static_cast<float>(y1); }
    void  reset() override { y1 = 0.0; }
    const char* name() const override { return "one-pole LP (a=0.9)"; }
};

struct SaturatorSys : System {                    // NOT linear
    float process(float x) override { return std::tanh(3.0f * x); }
    void  reset() override {}
    const char* name() const override { return "tanh saturator"; }
};

struct TimeVaryingSys : System {                  // NOT time-invariant
    long n = 0;
    float process(float x) override
    { const float g = 0.5f + 0.5f * std::sin(static_cast<float>(n) * 0.001f); ++n; return g * x; }
    void  reset() override { n = 0; }
    const char* name() const override { return "tremolo (time-varying)"; }
};
```

And the tests:

```cpp
std::vector<float> run(System& sys, const std::vector<float>& x)
{
    std::vector<float> y(x.size());
    for (size_t n = 0; n < x.size(); ++n)
        y[n] = sys.process(x[n]);
    return y;
}

std::vector<float> impulseResponse(System& sys, size_t length)
{
    sys.reset();
    std::vector<float> h(length);
    for (size_t n = 0; n < length; ++n)
        h[n] = sys.process(n == 0 ? 1.0f : 0.0f);
    return h;
}

bool testLinearity(System& sys)
{
    auto x1 = sig::sine(2000, 440.0, 44100.0, 0.3);
    auto x2 = sig::sine(2000, 660.0, 44100.0, 0.2);

    sys.reset(); auto y1 = run(sys, x1);
    sys.reset(); auto y2 = run(sys, x2);
    sys.reset(); auto ys = run(sys, sig::add(x1, x2));

    return sig::maxDifference(ys, sig::add(y1, y2)) < 1e-5;
}

bool testTimeInvariance(System& sys)
{
    auto x = sig::sine(2000, 440.0, 44100.0, 0.3);
    const int k = 100;

    sys.reset(); auto y        = run(sys, x);
    sys.reset(); auto yShifted = run(sys, sig::shift(x, k));

    // Compare y[n-k] with the response to x[n-k], skipping the first k samples.
    auto expected = sig::shift(y, k);
    double worst = 0.0;
    for (size_t n = static_cast<size_t>(k) + 200; n < y.size(); ++n)
        worst = std::max(worst, std::fabs(static_cast<double>(yShifted[n]) - expected[n]));

    return worst < 1e-5;
}

// THE KEY TEST: does convolution with h reproduce the system exactly?
bool testConvolutionEquivalence(System& sys, size_t hLength)
{
    auto h = impulseResponse(sys, hLength);
    auto x = sig::sine(2000, 300.0, 44100.0, 0.4);

    sys.reset();
    auto direct = run(sys, x);

    // y[n] = sum over k of x[k]*h[n-k]
    std::vector<float> viaConv(x.size(), 0.0f);
    for (size_t n = 0; n < x.size(); ++n)
    {
        double acc = 0.0;
        for (size_t k = 0; k <= n && k < x.size(); ++k)
        {
            const size_t idx = n - k;
            if (idx < h.size())
                acc += static_cast<double>(x[k]) * h[idx];
        }
        viaConv[n] = static_cast<float>(acc);
    }

    return sig::maxDifference(direct, viaConv) < 1e-4;
}
```

**Expected output**

```
system                    linear   time-inv   conv==direct   h[0..5]
------------------------------------------------------------------------------------
gain 0.5                    yes       yes         yes        0.500  0.000  0.000 ...
2-point average             yes       yes         yes        0.500  0.500  0.000 ...
one-pole LP (a=0.9)         yes       yes         yes        0.100  0.090  0.081 ...
tanh saturator               NO       yes          NO        0.995  0.000  0.000 ...
tremolo (time-varying)      yes        NO          NO        0.500  0.000  0.000 ...
```

**Read that table carefully; it is the whole chapter.**

The three LTI systems pass all three tests. Their impulse response fully predicts their
behaviour, and convolution reproduces them exactly.

The **saturator** is time-invariant but not linear. Its "impulse response" is `tanh(3) = 0.995`
followed by zeros — which looks like a gain of 0.995. But convolving with that does **not**
reproduce the saturator, because a quiet input gets a gain of nearly 3 while a loud one gets
much less. **The impulse response of a non-linear system is meaningless**, and measuring one
tells you only what it does at the single level you measured.

The **tremolo** is linear but not time-invariant. Its impulse response depends on *when* you
measure it: poke it at a different moment and you get a different number. Convolution fails for
the same reason.

> This table is worth reproducing yourself, because it converts "LTI is an important property"
> from a slogan into something you have watched fail.

---

## 20.9 Impulse responses you will meet

A sense of scale for the rest of the book:

| System | `h[n]` length | Notes |
|---|---|---|
| Gain | 1 sample | Trivially FIR |
| Delay | `k+1` samples | Mostly zeros |
| Biquad filter (Ch 23) | infinite | IIR, but decays fast |
| FIR low-pass (Ch 22) | 32–1024 taps | Length sets the steepness |
| Guitar cabinet IR | ~1,000 samples (20 ms) | Tiny files, big effect |
| Small room | ~20,000 (0.5 s) | |
| Concert hall | ~90,000 (2 s) | |
| Cathedral | ~400,000 (9 s) | |
| Large canyon / "epic" spaces | 500,000+ (11 s+) | Common in trailer work |

That last row is a hint at a practical problem. Direct convolution costs one multiply-add per
input sample **per impulse-response sample**. A 9-second stereo IR at 44.1 kHz is 400,000 taps;
at 44,100 samples per second that is **17.6 billion multiply-adds per second per channel**. No
CPU does that.

So Chapter 21 shows the direct method, Chapter 25 gives us the FFT, and Chapters 48–49 combine
them into fast convolution — which reduces that cost by a factor of thousands and is what makes
convolution reverb possible at all.

---

## 20.10 Exercises

**20.1** Prove or disprove linearity, on paper and then in code:
`y[n] = x[n] + x[n-1]`, `y[n] = x[n]·x[n-1]`, `y[n] = 2x[n] + 3`, `y[n] = x[n-2]`.

**20.2** Measure the impulse response of Chapter 14's `DCBlocker` over 10,000 samples. Plot or
print every 100th value. How long until it is below −60 dB? Relate that to the `R` coefficient.

**20.3** Show that a two-point averager's impulse response is `{0.5, 0.5}` and then verify by
convolution that it produces the same output as running the filter directly on a sawtooth.

**20.4** Take the one-pole low-pass with `a = 0.9`. Its impulse response is `0.1·0.9^n`. Confirm
this by measurement. Then compute, analytically, the value of `h[50]` and check it.

**20.5** Cascade two systems (A then B) and measure the combined impulse response. Then measure
each separately and convolve `hA` with `hB`. Confirm they match. Now do it in the other order
(B then A) and confirm the result is identical. What does that tell you about the order of
linear processors?

**20.6** Take a non-linear system (the `tanh` saturator) and measure its impulse response at
three different input amplitudes: 0.01, 0.5 and 1.0. How different are they? This is why "IR
capture" of a distortion box does not work.

**20.7** Write a function `double decayTimeSeconds(const std::vector<float>& h, double sr)`
returning the time for the impulse response to fall 60 dB below its peak — the **RT60** of
Chapter 2. Apply it to the one-pole filter for several values of `a`.

**20.8** *Deliberate breakage.* Measure an impulse response **without** calling `reset()` first,
immediately after running some other signal through the system. How wrong is the result? Which
systems are affected and which are not?

**20.9** The frequency response is `H(ω) = Σ h[n]·e^(-jωn)`. Implement it directly (a slow loop
over `n` for each `ω` you want) and plot `|H(ω)|` in dB for the one-pole low-pass at 64 frequency
points from 0 to π. Does it look like a low-pass? At which frequency is it −3 dB?

**20.10** Using the result of 20.9, find the `a` value that puts the −3 dB point at 1 kHz for a
44.1 kHz sample rate. Check your answer against the standard formula `a = e^(-2π·fc/fs)`.

---

### Chapter summary

- **Linear** = scaling and superposition both pass through. Distortion, clipping, compression and
  `|x|` are all non-linear. A linear system maps zero to zero.
- **Linear systems cannot create new frequencies, so they cannot alias.** Non-linear ones always
  do, which is why they need oversampling.
- **Time-invariant** = behaviour does not depend on when. Every modulated effect (chorus, phaser,
  tremolo, swept filter) is time-varying, and is handled by the **quasi-static** assumption —
  which is why parameter smoothing matters.
- **LTI** unlocks: impulse response, convolution, frequency response, poles and zeros,
  and the fact that cascade order does not matter.
- The **impulse response `h[n]`** is what comes out when you feed in a single sample of 1.0.
- **Any signal is a sum of scaled, shifted impulses.** Push that through an LTI system and you
  get the **convolution sum**: `y[n] = Σ x[k]·h[n-k]`.
- **One measurement tells you everything.** That is convolution reverb, cabinet IRs, speaker
  correction, and filter design from a target response.
- **`H(ω) = Σ h[n]·e^(-jωn)`** — the frequency response is the Fourier transform of the impulse
  response. Time view and frequency view, exactly equivalent.
- Real IRs are measured with **exponential swept sines** and deconvolution, which gives excellent
  SNR and cleanly separates the speaker's own distortion.
- The impulse response of a non-linear or time-varying system is **meaningless** — verify this
  yourself with §20.8's table.

**Next:** [Chapter 21 — Convolution From Scratch](21-convolution-from-scratch.md)
