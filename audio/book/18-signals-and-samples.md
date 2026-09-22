# Chapter 18 — Signals and Samples: The Notation You Need

> Part II is the steepest part of this book. It is also the part after which you can read any
> DSP paper and build any effect you find described. This first chapter is the vocabulary: a
> small set of symbols that lets the next eleven chapters be precise instead of vague.
>
> Nothing here is difficult. It is just unfamiliar, and unfamiliar notation is what makes DSP
> look harder than it is.

---

## 18.1 Why bother with notation at all?

You have already written a low-pass filter (Chapter 14's DC blocker), an envelope generator, and
five oscillators. You did it without any of the notation in this chapter. So why learn it?

Because **prose does not scale**. Here is a description of a filter in words:

> Take the current input sample. Add the previous input sample. Subtract half of the input from
> two samples ago. Then add 0.7 times the previous output and subtract 0.2 times the output from
> two samples ago.

And here is the same thing in notation:

```
   y[n] = x[n] + x[n-1] - 0.5·x[n-2] + 0.7·y[n-1] - 0.2·y[n-2]
```

The second version is not merely shorter. It is *manipulable*: you can substitute, rearrange,
and transform it, and doing so tells you the filter's frequency response, its stability, and its
phase behaviour. The prose version tells you nothing except how to implement it.

Every DSP textbook, paper and forum post uses this notation. Learning it is the difference
between being able to use published filter designs and having to invent everything yourself.

---

## 18.2 The core notation: `x[n]`

A **discrete-time signal** is a sequence of numbers indexed by an integer.

```
   x[n]
```

- `x` is the name of the signal.
- `n` is the sample index — an **integer**.
- `x[n]` is the value of that signal at sample `n`.

**Square brackets mean discrete time.** Round brackets, `x(t)`, mean *continuous* time — a
function defined for every real `t`. This distinction is universal and it is the fastest way to
tell which world a formula lives in.

```
   x(t)   continuous: defined for every real t, infinitely many values
   x[n]   discrete:   defined only at integers, one value per sample
```

The relationship is the sampling of Chapter 4:

```
   x[n] = x(n · Ts) = x(n / fs)
```

Sample `n` is the value of the continuous signal at time `n/fs` seconds.

### In code

```cpp
std::vector<float> x(1000);
float value = x[n];
```

The notation maps directly onto C++ array indexing. That is not a coincidence — both come from
the same idea of an indexed sequence.

### Signals extend in both directions

In mathematics, `x[n]` is usually defined for **all** integers `n`, from −∞ to +∞. A finite
recording is treated as a signal that is zero everywhere outside its length:

```
   x[n] = 0   for n < 0  and  n >= N
```

This convention matters enormously in practice. When a filter at sample 0 asks for `x[-1]`, the
mathematical answer is "zero", and your code must agree — which is exactly why every filter class
in this book initialises its state to zero and provides `reset()`. A filter whose state is left
over from previous audio is answering `x[-1]` with garbage, and you hear it as a click or a
transient at the start.

**Naming conventions**, nearly universal:

| Symbol | Means |
|---|---|
| `x[n]` | Input signal |
| `y[n]` | Output signal |
| `h[n]` | Impulse response of a system (Chapter 20) |
| `n`, `m`, `k` | Sample indices |
| `N`, `M` | Lengths |
| `fs` | Sample rate |

---

## 18.3 The building-block signals

Five signals appear constantly. Each is trivial; each has a specific job.

### The unit impulse, `δ[n]`

```
   δ[n] = 1   when n = 0
        = 0   otherwise
```

```
      1 |     |
        |     |
      0 |__ __|__ __ __ __ __
        -3 -2 -1  0  1  2  3   n
```

One sample of value 1, surrounded by silence. The Greek letter is **delta** (also called the
Kronecker delta, or the unit sample).

**This is the single most important signal in DSP**, and Chapter 20 explains why: if you know
what a system does to `δ[n]`, you know what it does to *everything*. You built one in Exercise
9.3 without knowing it.

In code:

```cpp
std::vector<float> impulse(1024, 0.0f);
impulse[0] = 1.0f;
```

**A caution about hearing it.** An impulse is not a click in the ordinary sense — it is the
*shortest possible* event, so its spectrum is perfectly flat across all frequencies. Played
alone it sounds like a faint tick, because although its peak is 0 dBFS its energy is tiny
(Chapter 11 measured its crest factor at 46 dB).

### The unit step, `u[n]`

```
   u[n] = 1   when n >= 0
        = 0   when n < 0
```

```
      1 |        ___________
        |       |
      0 |__ __ _|
        -3 -2 -1  0  1  2  3   n
```

Silence, then a constant. This is the "switch it on" signal, and it is exactly what caused the
click in Chapter 13: an abrupt step has energy at every frequency.

The step and the impulse are related by difference and summation:

```
   δ[n] = u[n] - u[n-1]           the difference of a step is an impulse
   u[n] = Σ (from k=-inf to n) δ[k]    the running sum of impulses is a step
```

That pair — difference and running sum — is the discrete analogue of derivative and integral,
and it will reappear in Chapter 20.

### The exponential

```
   x[n] = a^n
```

| `a` | Behaviour |
|---|---|
| `a > 1` | Grows without limit. **Unstable.** |
| `a = 1` | Constant. Marginally stable. |
| `0 < a < 1` | Decays toward zero. **Stable.** |
| `a < 0` | Alternates sign while growing or decaying. |

This is the shape of every decay you have written: the envelope in Chapter 13, the DC blocker's
feedback in Chapter 14, and every reverb tail to come. The `a > 1` row is the instability you
produced deliberately in Experiment 14.1, and Chapter 24 makes the rule precise.

### The sinusoid

```
   x[n] = A · sin(2π f n / fs + φ)      or      A · sin(ω n + φ)
```

Chapter 10's oscillator, now in standard notation. Note that `ω` here is in **radians per
sample**, not per second:

```
   ω = 2π f / fs
```

This is the normalised angular frequency, and it is what your `phaseIncrement` variable has been
all along. Its useful range is `0` to `π`, because `ω = π` corresponds to `f = fs/2` — **Nyquist
is `ω = π`**. That fact is worth memorising; it is why filter plots in textbooks run from 0 to π
on the horizontal axis and why you must divide by π to get back to hertz.

### Noise

Not a formula but a statistical description — Chapter 15. Written as `w[n]` (for "white") when it
appears in equations.

---

## 18.4 Operations on signals

Five operations, all of which you have already performed in code.

### Shift (delay)

```
   y[n] = x[n - k]
```

**`x[n-k]` is `x` delayed by `k` samples.** This is the operation people most often get backwards,
so here is the reasoning: to get the output at time `n`, you reach back to the input at time
`n-k`. Reaching *backwards* in the input means the output appears *later*. Minus means delay.

```
   x[n]:     0  1  2  3  0  0  0
   x[n-2]:   0  0  0  1  2  3  0       shifted RIGHT (delayed) by 2
   x[n+2]:   2  3  0  0  0  0  0       shifted LEFT (advanced) by 2
```

`x[n+k]` is an *advance*, which requires knowing the future. A system that needs it is
**non-causal** and cannot run in real time — though it is perfectly implementable offline, which
is exactly how linear-phase filters and lookahead limiters work (Chapters 53 and 51).

In code, a delay is reading from an earlier position:

```cpp
y[n] = x[n - k];                      // needs a bounds check when n < k
```

which, done with a circular buffer, is Chapter 42.

### Scale

```
   y[n] = a · x[n]
```

A gain. Chapter 11.

### Sum

```
   y[n] = x1[n] + x2[n]
```

Mixing. Chapter 14.

### Product (modulation)

```
   y[n] = x1[n] · x2[n]
```

Multiplying two signals sample by sample. You have done this twice already: applying an envelope
(Chapter 13) and applying a gain that changes over time.

**This one is special and deserves a flag.** Multiplication in the time domain does something
dramatic in the frequency domain — it *shifts* frequencies around, creating sums and differences.
That is:

- Why an abrupt envelope creates a click (Chapter 13's rectangular window).
- Why tremolo creates sidebands.
- How ring modulation and amplitude modulation work.
- Why windowing an FFT frame smears the spectrum (Chapter 26).

Chapter 21 proves the general statement: **multiplication in one domain is convolution in the
other**. Hold that as a promissory note.

### Time reversal

```
   y[n] = x[-n]
```

Play it backwards. Rarely used directly, but it appears inside the definition of convolution, and
it is the difference between convolution and correlation (Chapter 21).

---

## 18.5 Summation notation

The one symbol that makes DSP formulas look intimidating, and the one that is simplest.

```
        N-1
   y =  Σ   x[k]
        k=0
```

Reads as: "the sum, for `k` from 0 to `N-1`, of `x[k]`". In code:

```cpp
double y = 0.0;
for (int k = 0; k < N; ++k)
    y += x[k];
```

**Σ is a for-loop that accumulates.** That is the entire content of the symbol.

The parts:

- **Σ** (capital sigma) — "sum of".
- **Below** — the index variable and its starting value.
- **Above** — the ending value, **inclusive**. `k = 0` to `N-1` is `N` terms.
- **To the right** — the thing being summed, once per value of `k`.

Note the inclusive upper limit: `Σ from k=0 to N-1` corresponds to `for (k = 0; k < N; ++k)`, with
`<` rather than `<=`. Mixing these up is the notational version of Chapter 6's off-by-one.

### Examples you already know

**Mean:**
```
           N-1
   mean = (1/N) Σ x[n]
           n=0
```
That is `dcOffset()` from Chapter 11.

**RMS:**
```
              N-1
   rms = sqrt((1/N) Σ x[n]²)
              n=0
```
That is `rms()`.

**Energy:**
```
         N-1
   E  =  Σ  x[n]²
         n=0
```

### Infinite sums

```
         ∞
   y  =  Σ   x[k]
        k=-∞
```

Mathematically the sum runs over all integers. In practice, signals are zero outside a finite
range, so the infinite sum has only finitely many non-zero terms. When you see `-∞` to `∞` in a
DSP formula, read it as "over the whole signal, and remember everything outside is zero".

### Double sums

Occasionally you will see two nested Σ. It is two nested loops. Nothing more.

---

## 18.6 Energy, power, and why we care

Two quantities that formalise what Chapter 11 measured.

**Energy** of a signal:
```
        ∞
   E =  Σ  |x[n]|²
       n=-∞
```

A finite recording has finite energy. An infinitely long tone has infinite energy, which is why
we also need:

**Average power**:
```
                     N-1
   P = lim (1/N)  ·   Σ  |x[n]|²
      N->inf         n=0
```

Which is RMS squared. Power is the useful measure for continuous signals; energy for finite ones
(a drum hit, an impulse response, a sound effect).

The `|·|` bars are absolute value; for real signals `|x|² = x²`. They are written that way
because in Chapter 19 signals become complex, and then the bars matter.

**Parseval's theorem**, which we will meet properly in Chapter 25, says that the energy computed
in the time domain equals the energy computed in the frequency domain. Its practical use is as a
correctness check: if you FFT a signal, do something, inverse-FFT it, and the energy has changed
when it should not have, you have a scaling bug. This catches a large fraction of FFT
implementation errors.

---

## 18.7 Systems

A **system** takes an input signal and produces an output signal:

```
   y[n] = T{ x[n] }
```

where `T` is the transformation. In block diagram form:

```
            +-------+
   x[n] --->|   T   |---> y[n]
            +-------+
```

Every processor you have written is a system: a gain, an envelope multiplier, the DC blocker, a
clipper.

### Block diagram vocabulary

DSP papers use a small set of symbols. Learn them and you can read any signal-flow diagram.

```
   Adder                 Multiplier              Unit delay
        x1                    x                      x
         |                    |                      |
   x2 -->(+)--> y        a -->(x)--> y            [z^-1]
                                                     |
                                                     v
                                                   x[n-1]
```

**`z^-1` means "delay by one sample".** The notation comes from the z-transform (Chapter 24); for
now, treat `z^-1` as a one-sample memory. In code it is a variable holding the previous value.

Here is Chapter 14's DC blocker as a diagram:

```
   x[n] ---+------------------>(+)------+----> y[n]
           |                    ^       |
        [z^-1]                  |       |
           |                    |    [z^-1]
           v                    |       |
         (x -1) ----------------+       |
                                ^       |
                                |       v
                                +----(x R)
```

And as an equation:

```
   y[n] = x[n] - x[n-1] + R·y[n-1]
```

And as code:

```cpp
y = x - x1 + R * y1;
x1 = x;
y1 = y;
```

**Three representations of one thing.** Being able to move between them fluently is what Part II
is training you to do. When you read a paper, you will get the diagram or the equation; you need
to produce the code.

### Reading a difference equation into code

The general form:

```
   y[n] = b0·x[n] + b1·x[n-1] + b2·x[n-2] + ...
        - a1·y[n-1] - a2·y[n-2] - ...
```

- `b` coefficients multiply **inputs** (the *feedforward* path).
- `a` coefficients multiply **outputs** (the *feedback* path).
- **The `a` terms are conventionally subtracted.** This trips people up constantly: a published
  coefficient set with `a1 = -1.8` means you compute `y = ... - (-1.8)·y[n-1]`, i.e. you *add*
  1.8 times the previous output. Some references flip this sign convention. If a filter you
  implemented from a paper is unstable or sounds inverted, **check the `a` sign convention
  first** — it is the most common cause.

Translated to code, mechanically:

```cpp
float process(float x)
{
    const double y = b0*x + b1*x1 + b2*x2 - a1*y1 - a2*y2;

    x2 = x1;  x1 = x;      // shift the input history
    y2 = y1;  y1 = y;      // shift the output history

    return static_cast<float>(y);
}
```

That is the **biquad**, the workhorse of Chapter 23. You now know how to read one, and the only
remaining question is where the coefficients come from.

---

## 18.8 Properties of systems

Four properties, each of which either holds or does not, and each of which has practical
consequences.

### Causal

A system is **causal** if the output at time `n` depends only on inputs at times `n` and earlier.

```
   y[n] = x[n] + x[n-1]           causal
   y[n] = x[n] + x[n+1]           NON-causal: needs the future
```

**Every real-time system must be causal.** Offline processing need not be: you have the whole
file, so "the future" is available. This is why offline mastering can use linear-phase EQ and
true lookahead limiting while real-time versions must introduce latency to fake it.

### Stable

A system is **BIBO stable** (Bounded Input, Bounded Output) if every bounded input produces a
bounded output.

Unstable systems produce output that grows without limit. You made one in Experiment 14.1. In
audio an unstable filter means full-scale noise, which is a hearing hazard, which is why every
real-time experiment in this book is preceded by "volume down".

Chapter 24 gives the exact test.

### Memoryless

A system is **memoryless** if `y[n]` depends only on `x[n]`.

```
   y[n] = 2·x[n]                  memoryless (a gain)
   y[n] = tanh(x[n])              memoryless (a waveshaper)
   y[n] = x[n] + x[n-1]           has memory (a filter)
```

Memoryless systems need no state and no `reset()`. Anything with memory needs both, and needs
them managed correctly when voices are reused or playback restarts.

### Linear and time-invariant

The two big ones. They get Chapter 20 to themselves, because almost all of DSP theory applies
only to systems that have both, and knowing whether yours does tells you which tools you may use.

Briefly: a **linear** system obeys superposition (doubling the input doubles the output; the
response to a sum is the sum of the responses). A **time-invariant** system behaves the same way
regardless of when you feed it.

A gain is linear and time-invariant. A filter with fixed coefficients is linear and
time-invariant. A distortion is **not linear**. A filter whose cutoff is being swept is **not
time-invariant**. Both of those are extremely useful and extremely common — the point is that
the Chapter 20–29 toolkit does not directly apply to them, which is exactly why distortion needs
oversampling (Chapter 29) and why a swept filter can behave in ways its static frequency response
does not predict.

---

## 18.9 A `Signal` helper

Putting the operations into code, mostly so that later chapters can be written compactly.

**Code — `lib/include/audio/signal.h`** (excerpt)

```cpp
namespace audio {
namespace sig {

// --- generators ----------------------------------------------------

inline std::vector<float> impulse(size_t length, size_t position = 0)
{
    std::vector<float> x(length, 0.0f);
    if (position < length) x[position] = 1.0f;
    return x;
}

inline std::vector<float> step(size_t length, size_t position = 0)
{
    std::vector<float> x(length, 0.0f);
    for (size_t n = position; n < length; ++n) x[n] = 1.0f;
    return x;
}

inline std::vector<float> exponential(size_t length, double a)
{
    std::vector<float> x(length);
    double v = 1.0;
    for (size_t n = 0; n < length; ++n) { x[n] = static_cast<float>(v); v *= a; }
    return x;
}

// --- operations ----------------------------------------------------

// y[n] = x[n - k].  Positive k delays; negative k advances.
inline std::vector<float> shift(const std::vector<float>& x, int k)
{
    std::vector<float> y(x.size(), 0.0f);
    for (size_t n = 0; n < x.size(); ++n)
    {
        const long src = static_cast<long>(n) - k;
        if (src >= 0 && src < static_cast<long>(x.size()))
            y[n] = x[static_cast<size_t>(src)];
    }
    return y;
}

inline std::vector<float> scale(const std::vector<float>& x, float a)
{
    std::vector<float> y(x.size());
    for (size_t n = 0; n < x.size(); ++n) y[n] = a * x[n];
    return y;
}

inline std::vector<float> add(const std::vector<float>& a, const std::vector<float>& b)
{
    std::vector<float> y(std::max(a.size(), b.size()), 0.0f);
    for (size_t n = 0; n < a.size(); ++n) y[n] += a[n];
    for (size_t n = 0; n < b.size(); ++n) y[n] += b[n];
    return y;
}

inline std::vector<float> multiply(const std::vector<float>& a, const std::vector<float>& b)
{
    std::vector<float> y(std::min(a.size(), b.size()));
    for (size_t n = 0; n < y.size(); ++n) y[n] = a[n] * b[n];
    return y;
}

inline std::vector<float> reverse(std::vector<float> x)
{
    std::reverse(x.begin(), x.end());
    return x;
}

// --- measurements --------------------------------------------------

inline double energy(const std::vector<float>& x)
{
    double e = 0.0;
    for (float s : x) e += static_cast<double>(s) * s;
    return e;
}

}}   // namespace audio::sig
```

Note `add` handles differing lengths by zero-padding, and `multiply` truncates to the shorter.
Those are choices, not laws — but they are the conventional ones, and the header documents them
so that nobody has to guess.

---

## 18.10 Exercises

**18.1** Write out, in `x[n]`/`y[n]` notation, the difference equations for: a gain of 0.5; a
two-sample delay; a simple averaging filter of the current and previous sample; Chapter 14's DC
blocker.

**18.2** For `x[n] = {2, 4, 6, 8}` starting at `n = 0` (zero elsewhere), write out `x[n-2]`,
`x[n+1]`, `2·x[n]`, and `x[-n]`.

**18.3** Convert to code:
```
   y[n] = 0.5·x[n] + 0.5·x[n-1]
```
Feed it an impulse and print the first ten outputs. Then feed it a step. Explain both results.

**18.4** Convert this to code, being careful with the sign convention in §18.7:
```
   y[n] = x[n] - 1.8·y[n-1] - 0.81·y[n-2]
```
Feed it an impulse. Is it stable? Now change `-1.8` to `+1.8` and try again. What happened, and
why does a sign flip matter so much? (Volume at zero.)

**18.5** Write `Σ` expressions for: the sum of the first `N` samples; the sum of the squares of
all samples; the mean of samples 100 through 200 inclusive. Then write each as a C++ loop and
check that the loop bounds match.

**18.6** Which of these are causal? Which are memoryless? Which are linear?
```
   (a) y[n] = 3·x[n]
   (b) y[n] = x[n]²
   (c) y[n] = x[n] + x[n-1]
   (d) y[n] = x[n] + x[n+1]
   (e) y[n] = n · x[n]
   (f) y[n] = |x[n]|
```
For each "not linear", give a concrete counterexample with numbers.

**18.7** Show numerically that `δ[n] = u[n] - u[n-1]` by generating both and subtracting.

**18.8** Compute the energy of: an impulse; 1000 samples of a step; 1000 samples of a sine at
amplitude 1.0. Which has the most? Relate the answers to Chapter 11's crest-factor measurements.

**18.9** Implement `shift` using `std::vector`'s own insert/erase instead of the loop above, and
compare the two for a 10-million-sample buffer. Which is faster, and why?

**18.10** Draw the block diagram for `y[n] = 0.5·x[n] + 0.5·x[n-1] + 0.9·y[n-1]`, using the
adder, multiplier and `z^-1` symbols from §18.7.

---

### Chapter summary

- `x[n]` is a discrete signal; **square brackets mean discrete time**, round brackets mean
  continuous. Signals are zero outside their defined range — which is why filter state must be
  zeroed.
- Building blocks: the **unit impulse `δ[n]`** (the most important signal in DSP), the unit step
  `u[n]`, the exponential `a^n` (stable iff `|a| < 1`), and the sinusoid.
- **`ω` is radians per sample**, `ω = 2πf/fs`, and **Nyquist is `ω = π`**.
- Operations: **shift** (`x[n-k]` is a *delay* of `k`), scale, sum, product, reversal.
  Multiplication in time does something dramatic in frequency — Chapter 21.
- **Σ is a for-loop that accumulates.** The upper limit is inclusive, so `Σ k=0..N-1` is
  `for (k=0; k<N; ++k)`.
- Energy is `Σ|x[n]|²`; average power is RMS squared.
- A system is `y[n] = T{x[n]}`. Read block diagrams: adder, multiplier, and `z^-1` = one-sample
  delay.
- The general difference equation has `b` coefficients on inputs and `a` coefficients on outputs,
  **with the `a` terms subtracted** — check that sign convention first when a published filter
  misbehaves.
- Properties: **causal** (required for real time), **stable**, **memoryless**, and
  **linear + time-invariant** — the last of which unlocks the whole toolkit, and which distortion
  and swept filters do not have.

**Next:** [Chapter 19 — Complex Numbers and Phasors](19-complex-numbers-and-phasors.md)
