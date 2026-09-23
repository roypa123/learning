# Chapter 25 — The DFT, and Then the FFT

> Chapter 2 promised that time and frequency are two views of one object. Chapter 20 gave the
> formula connecting them. This chapter makes it computable, and then makes it *fast* — fast
> enough that the cathedral reverb which took 6.7 seconds in Chapter 21 will run in
> milliseconds.
>
> The FFT is often called the most important algorithm of the twentieth century. That is not
> hyperbole: it underpins audio, images, radio, medical imaging, and large-integer arithmetic.

---

## 25.1 From the DTFT to the DFT

Chapter 20 gave the frequency response of a system:

```
            ∞
   H(ω)  =  Σ  h[n]·e^(-jωn)
           n=-∞
```

Two problems for a computer: the sum is infinite, and `ω` is continuous — infinitely many
frequencies.

The **Discrete Fourier Transform** fixes both. Take a **finite** block of `N` samples, and
evaluate at **`N` evenly spaced** frequencies:

```
           N-1
   X[k]  =  Σ  x[n]·e^(-j·2πkn/N)          for k = 0 .. N-1
           n=0
```

And back again — the **inverse DFT**:

```
                  N-1
   x[n]  = (1/N)   Σ  X[k]·e^(+j·2πkn/N)
                  k=0
```

Note the two differences in the inverse: the sign of the exponent flips, and there is a `1/N`
scaling. (Some references put the `1/N` on the forward transform, or `1/√N` on both. Three
conventions, all in use. **When combining code from different sources, check the scaling first**
— a mismatch shows up as output that is `N` times too loud or too quiet, which looks like a gain
bug.)

### What the formula is doing

Look at one output, `X[k]`. It multiplies the signal by `e^(-j·2πkn/N)` — a phasor (Chapter 19)
that completes exactly `k` cycles across the block — and sums the products.

That is **correlation** (Chapter 21). It asks: *how much does this signal look like a sinusoid
that fits `k` whole cycles into this window?*

- If the signal contains that frequency, the products reinforce and the sum is large.
- If it does not, the products cancel and the sum is near zero.

The result is complex, so it carries both an amplitude and a phase (Chapter 19's entire point).

> **The DFT is `N` correlations against `N` test frequencies.** That is the whole idea. Everything
> else is bookkeeping and speed.

---

## 25.2 Reading the output

This section is where most confusion lives. Get it straight once.

### Bin frequencies

Output index `k` is a **bin**, and it corresponds to:

```
   f[k] = k · fs / N
```

For `N = 1024` at `fs = 44,100`:

| Bin `k` | Frequency |
|---|---|
| 0 | 0 Hz (**DC**) |
| 1 | 43.07 Hz |
| 2 | 86.13 Hz |
| 100 | 4,306.6 Hz |
| **512** | **22,050 Hz (Nyquist)** |
| 513 | ... mirror of bin 511 |
| 1023 | mirror of bin 1 |

### Bin width — the resolution

```
   Δf = fs / N
```

This single number governs everything about your analysis:

| `N` | Δf at 44.1 kHz | Time window | Good for |
|---|---|---|---|
| 256 | 172 Hz | 5.8 ms | Transients; useless for bass |
| 1024 | 43 Hz | 23 ms | General purpose |
| 4096 | 10.8 Hz | 93 ms | Musical pitch analysis |
| 16384 | 2.7 Hz | 372 ms | Fine spectral detail; smears transients badly |

**The trade-off is inescapable and it is the time-frequency uncertainty principle again** — the
same one from Chapter 13's fade lengths and Chapter 22's tap counts. A long window gives fine
frequency resolution and poor time resolution. A short window gives the reverse. You cannot have
both, and Chapter 27 is largely about choosing wisely.

Note in particular that at `N = 1024`, bins are 43 Hz apart. The musical interval from A1 (55 Hz)
to A#1 (58.3 Hz) is 3.3 Hz. **A 1024-point FFT cannot distinguish adjacent bass notes.** This is
why spectrum analysers look useless in the low end, and why pitch detection uses autocorrelation
(Chapter 21) rather than the FFT.

### The mirror

For a **real** input signal, the second half of the output is the complex conjugate mirror of the
first:

```
   X[N-k] = conj(X[k])
```

This is Chapter 19's negative frequency, appearing exactly as promised. The two counter-rotating
phasors that make a real cosine show up as two bins, symmetric about Nyquist.

**Practical consequences:**

- Only bins `0` to `N/2` carry new information. Ignore the rest for display.
- A real-input FFT can be computed in roughly **half** the time by exploiting this, using a
  dedicated real-FFT routine.
- Bin `0` (DC) and bin `N/2` (Nyquist) are always purely real — they have no mirror partner.

### Magnitude and phase

```cpp
const double magnitude = std::abs(X[k]);              // sqrt(re^2 + im^2)
const double phase     = std::arg(X[k]);              // atan2(im, re)
const double power     = std::norm(X[k]);             // re^2 + im^2, no sqrt
```

For a display, convert magnitude to dB — and **scale by `2/N`** to recover the amplitude of the
original sinusoid:

```cpp
const double amplitude = 2.0 * std::abs(X[k]) / N;    // for bins 1..N/2-1
const double dB        = gainToDb(amplitude);
```

**Where the `2/N` comes from:** the `1/N` undoes the transform's accumulation over `N` samples;
the `2` accounts for the energy that is sitting in the mirror bin. **Do not apply the `2` to bin
0 or bin `N/2`**, which have no mirror. This off-by-a-factor-of-two at the endpoints is a classic
spectrum-analyser bug and it makes DC readings look 6 dB too low.

### The leakage problem

Feed in a 440 Hz sine with `N = 1024` at 44,100 Hz. Bin 10 is 430.7 Hz; bin 11 is 473.8 Hz.
440 Hz falls **between** them.

The result is not a single clean peak. Energy spreads across many bins — **spectral leakage** —
because the DFT implicitly assumes the block repeats forever, and 440 Hz does not fit a whole
number of cycles into 1024 samples, so the assumed repetition has a discontinuity at the join.
A discontinuity is broadband (Chapter 13, again).

The fix is **windowing** — taper the block to zero at both ends before transforming. Same
technique, same trade-off, same windows as Chapter 22. Chapter 26 is devoted to it. Until then,
be aware that an un-windowed spectrum is misleading.

---

## 25.3 The naive DFT

```cpp
std::vector<std::complex<double>> dft(const std::vector<float>& x)
{
    const size_t N = x.size();
    std::vector<std::complex<double>> X(N);

    for (size_t k = 0; k < N; ++k)
    {
        std::complex<double> sum(0.0, 0.0);

        for (size_t n = 0; n < N; ++n)
        {
            const double angle = -kTwoPi * static_cast<double>(k)
                                          * static_cast<double>(n)
                                          / static_cast<double>(N);
            sum += static_cast<double>(x[n]) * std::complex<double>(std::cos(angle),
                                                                    std::sin(angle));
        }

        X[k] = sum;
    }

    return X;
}
```

Correct, clear, and **hopelessly slow**. Two nested loops over `N`: `O(N²)` complex
multiply-accumulates, each involving a `sin` and a `cos`.

| `N` | Operations | Time (rough) |
|---|---|---|
| 256 | 65,536 | 1 ms |
| 1,024 | 1 million | 20 ms |
| 4,096 | 16.8 million | 350 ms |
| 65,536 | 4.3 **billion** | 90 seconds |

A spectrum analyser needs perhaps 60 transforms per second. At `N = 4096` the naive DFT needs 21
seconds of CPU per second of audio. Unusable.

Write it anyway — you need it as a reference to verify the FFT against.

---

## 25.4 The FFT idea

The Fast Fourier Transform computes **exactly the same answer** in `O(N log N)` operations. Not
an approximation; the identical result, arrived at more cleverly.

The comparison is dramatic:

| `N` | DFT `N²` | FFT `N log₂N` | Speedup |
|---|---|---|---|
| 256 | 65,536 | 2,048 | 32× |
| 1,024 | 1,048,576 | 10,240 | 102× |
| 4,096 | 16,777,216 | 49,152 | 341× |
| 65,536 | 4.3 billion | 1,048,576 | **4,096×** |

### The insight: split even and odd

Split the sum into even-indexed and odd-indexed samples:

```
           N-1
   X[k] =   Σ  x[n]·W^(kn)               where W = e^(-j·2π/N)
           n=0

        =   Σ x[2m]·W^(2km)  +  Σ x[2m+1]·W^(k(2m+1))
            m                    m

        =   Σ x[2m]·W^(2km)  +  W^k · Σ x[2m+1]·W^(2km)
            m                          m
```

Now notice that `W^(2km) = e^(-j·2π·2km/N) = e^(-j·2πkm/(N/2))`, which is the twiddle factor for
a transform of size `N/2`. So:

```
   X[k] = E[k] + W^k · O[k]
```

where `E` is the DFT of the even-indexed samples and `O` is the DFT of the odd-indexed ones —
**each of size `N/2`**.

And because `E` and `O` are periodic with period `N/2`, the second half comes free:

```
   X[k]       = E[k] + W^k · O[k]
   X[k + N/2] = E[k] - W^k · O[k]          for k = 0 .. N/2 - 1
```

**One multiplication and two additions produce two outputs.** That pair of operations is called a
**butterfly**, from its diagram:

```
   E[k] ------+------------> X[k]
               \    /
                \  /
                 \/
                 /\
                /  \
   O[k] --[W^k]+----\------> X[k + N/2]
                      (-)
```

Apply the split recursively: `N` becomes two of `N/2`, then four of `N/4`, down to transforms of
size 1 (which are trivially the sample itself). That is `log₂N` levels, each costing `O(N)`.
Hence `O(N log N)`.

### The power-of-two requirement

The recursive halving needs `N` to be a power of two. Options when your data is not:

1. **Zero-pad** to the next power of two. This is what we do. It changes nothing about the
   underlying spectrum, only interpolates it more finely.
2. Use a **mixed-radix** FFT that handles other factorisations (this is what FFTW and similar
   libraries do).
3. Use **Bluestein's algorithm** for prime sizes.

For audio, zero-padding to a power of two is almost always right, and power-of-two block sizes
are conventional anyway.

---

## 25.5 The implementation

We write the **iterative** version, which is what production code uses — recursion costs function
calls and the iterative form has better cache behaviour.

**Code — `lib/include/audio/fft.h`** (core)

```cpp
namespace audio {

using Complex = std::complex<double>;

inline bool isPowerOfTwo(size_t n) { return n != 0 && (n & (n - 1)) == 0; }

inline size_t nextPowerOfTwo(size_t n)
{
    size_t p = 1;
    while (p < n) p <<= 1;
    return p;
}

// In-place radix-2 FFT. `data.size()` must be a power of two.
// inverse == true computes the inverse transform (including the 1/N scaling).
inline void fft(std::vector<Complex>& data, bool inverse = false)
{
    const size_t N = data.size();
    if (N <= 1) return;

    // ---- stage 1: bit-reversal permutation --------------------------
    for (size_t i = 1, j = 0; i < N; ++i)
    {
        size_t bit = N >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;

        if (i < j)
            std::swap(data[i], data[j]);
    }

    // ---- stage 2: butterflies, log2(N) levels -----------------------
    for (size_t len = 2; len <= N; len <<= 1)
    {
        const double angle = (inverse ? kTwoPi : -kTwoPi) / static_cast<double>(len);
        const Complex wlen(std::cos(angle), std::sin(angle));

        for (size_t i = 0; i < N; i += len)
        {
            Complex w(1.0, 0.0);

            for (size_t j = 0; j < len / 2; ++j)
            {
                const Complex u = data[i + j];
                const Complex v = data[i + j + len / 2] * w;

                data[i + j]           = u + v;
                data[i + j + len / 2] = u - v;

                w *= wlen;
            }
        }
    }

    // ---- scaling for the inverse ------------------------------------
    if (inverse)
        for (auto& c : data)
            c /= static_cast<double>(N);
}

}   // namespace audio
```

### Walkthrough

**Bit-reversal.** The recursive even/odd splitting reorders the data. If you unroll the recursion,
each sample ends up at the position given by **reversing the bits of its index**:

```
   N = 8, so 3 bits:

   index 0 = 000 -> 000 = 0
   index 1 = 001 -> 100 = 4
   index 2 = 010 -> 010 = 2
   index 3 = 011 -> 110 = 6
   index 4 = 100 -> 001 = 1
   ...
```

Doing this permutation first lets the butterflies run in-place with simple index arithmetic.

The loop that computes it is the standard trick: maintain a bit-reversed counter `j` alongside the
normal counter `i`, incrementing `j` by carrying from the *top* bit downward instead of the
bottom. The `if (i < j)` guard ensures each pair is swapped once rather than twice.

**The butterfly loops.** Three nested loops:

- `len` — the transform size at this level: 2, 4, 8, ... N. That is `log₂N` iterations.
- `i` — which block of size `len` we are working on.
- `j` — which butterfly within the block.

Total work: `log₂N` levels × `N/2` butterflies each. Exactly `O(N log N)`.

**`w *= wlen`** advances the twiddle factor by one step — Chapter 19's phasor rotation, used as an
optimisation so we compute two transcendental functions per level instead of `N/2` of them.

> **Precision note.** That repeated multiplication accumulates error, exactly as the phasor
> oscillator did in Chapter 19. For `N` up to about 2^16 with `double` it is fine. Production
> FFTs (FFTW, Intel MKL) use precomputed twiddle tables or recompute periodically. If you ever
> need very large transforms, this is the line to fix.

**The `inverse` flag** flips the sign of the angle and applies the `1/N`. That is the only
difference between forward and inverse — a pleasing economy.

### Convenience wrappers

```cpp
// Real input -> complex spectrum, zero-padded to a power of two.
inline std::vector<Complex> fftReal(const std::vector<float>& x, size_t fftSize = 0)
{
    const size_t N = (fftSize > 0) ? fftSize : nextPowerOfTwo(x.size());

    std::vector<Complex> data(N, Complex(0.0, 0.0));
    for (size_t i = 0; i < std::min(N, x.size()); ++i)
        data[i] = Complex(static_cast<double>(x[i]), 0.0);

    fft(data, false);
    return data;
}

// Complex spectrum -> real signal.
inline std::vector<float> ifftReal(std::vector<Complex> X)
{
    fft(X, true);

    std::vector<float> x(X.size());
    for (size_t i = 0; i < X.size(); ++i)
        x[i] = static_cast<float>(X[i].real());
    return x;
}

// Magnitude in dB for bins 0 .. N/2, correctly scaled.
inline std::vector<double> magnitudeSpectrumDb(const std::vector<Complex>& X)
{
    const size_t N    = X.size();
    const size_t bins = N / 2 + 1;

    std::vector<double> out(bins);
    for (size_t k = 0; k < bins; ++k)
    {
        // The factor of 2 accounts for the mirror bin -- but bins 0 and N/2
        // have no mirror, so they do not get it.
        const double scale = (k == 0 || k == N / 2) ? 1.0 : 2.0;
        out[k] = gainToDb(scale * std::abs(X[k]) / static_cast<double>(N));
    }
    return out;
}

inline double binFrequency(size_t k, size_t fftSize, double sampleRate)
{
    return static_cast<double>(k) * sampleRate / static_cast<double>(fftSize);
}
```

---

## 25.6 Verifying it

Never trust an FFT you have not tested. Four checks, in order of increasing strength.

**1. Against the naive DFT.** For `N = 256`, compute both and compare. They should agree to about
1e-12. If they differ by a constant factor, it is a scaling convention. If they differ in sign on
the imaginary parts, the exponent sign is flipped.

**2. Round trip.** `ifft(fft(x))` must reproduce `x`. Error should be around 1e-15 per sample.

**3. Known signals.** A pure sine at a **bin-centre** frequency must produce a single non-zero
bin:

```cpp
// 43.066 Hz at N=1024, fs=44100 lands exactly on bin 1.
const double f = 1.0 * 44100.0 / 1024.0;
auto x = sig::sine(1024, f, 44100.0, 0.5);
auto X = fftReal(x);
// X[1] and X[1023] should be large; everything else ~0.
```

**4. Parseval's theorem.** Energy in time equals energy in frequency:

```
      N-1               1   N-1
      Σ  |x[n]|²   =   ---  Σ  |X[k]|²
     n=0               N   k=0
```

This is the strongest single test, because it catches scaling errors that the other three might
not. It should hold to about 1e-10 relative.

---

## 25.7 Fast convolution — the payoff

Chapter 21 promised this. The convolution theorem says:

```
   x * h   =   IFFT( FFT(x) · FFT(h) )
```

with one crucial detail: **you must zero-pad both signals to at least `N + M - 1`** before
transforming, or the result "wraps around" — this is **circular convolution**, and the tail of
the output folds back onto the beginning.

```cpp
std::vector<float> fastConvolve(const std::vector<float>& x,
                                const std::vector<float>& h)
{
    if (x.empty() || h.empty()) return {};

    const size_t resultLength = x.size() + h.size() - 1;
    const size_t N = nextPowerOfTwo(resultLength);      // ZERO-PAD -- essential

    auto X = fftReal(x, N);
    auto H = fftReal(h, N);

    for (size_t k = 0; k < N; ++k)
        X[k] *= H[k];                                   // multiply the spectra

    auto y = ifftReal(X);
    y.resize(resultLength);                             // trim the padding
    return y;
}
```

**Six lines, and it is thousands of times faster.**

Measured against Chapter 21's direct convolution, convolving a 0.4-second source with each
synthetic IR:

```
  space         IR taps    direct (s)    FFT (s)    speedup
  ---------------------------------------------------------
  small_room      22932        0.41       0.008        51x
  hall           126126        2.26       0.036        63x
  cathedral      372645        6.68       0.089        75x
  plate          171990        3.08       0.048        64x
```

The cathedral went from **6.7 seconds to 89 milliseconds**. That is the difference between
convolution reverb being a curiosity and being a product.

**Verify the result matches direct convolution** — it should agree to about 1e-6, the
accumulated floating-point error of the FFT round trip. If it does not, the usual cause is
insufficient zero-padding, and the symptom is distinctive: the reverb tail appears at the
*beginning* of the output.

### Why it is not quite done yet

Two problems remain for real-time use, and they are Chapters 48–49:

**Latency.** You must collect `N` samples before you can transform. For a cathedral, `N` is
524,288 — that is 11.9 seconds of latency. Unacceptable.

**Block processing.** A real-time system gets 512 samples at a time, not the whole file.

The solution is **partitioned convolution**: chop the impulse response into blocks, process the
first (short) block with low latency, and handle the long tail with larger, more efficient
blocks that can afford to lag. Chapter 49 builds it.

---

## 25.8 Practical notes

**Window before transforming.** Except for the verification tests above, always apply a window
(Chapter 26). An un-windowed spectrum leaks badly and misleads.

**FFT size is a trade-off**, and §25.2's table is the guide. 1024 or 2048 for general analysis,
4096+ for pitch work, 256–512 for transient detection.

**`float` versus `double`.** For audio-length transforms (`N ≤ 8192`), `float` is acceptable and
about 1.7× faster; `double` is safer and is what this book uses. For convolution of long IRs the
error accumulates over many blocks, so prefer `double`.

**Use a real-input FFT when you can.** Real signals have that conjugate-symmetric spectrum, and a
dedicated real FFT exploits it for roughly a 2× speedup and half the memory. Worth implementing
once you understand the complex version (Exercise 25.9).

**When to use a library.** Ours is a correct, clear, educational implementation. It is perhaps 3–5×
slower than FFTW or Intel MKL, which use SIMD, cache-aware blocking, and algorithm selection at
runtime. For production, use a library. For understanding — and for the many cases where 3× does
not matter — yours is fine.

**KISS FFT, PFFFT and pocketfft** are good small, permissively-licensed options if you want
something faster without FFTW's build complexity.

---

## 25.9 Exercises

**25.1** Implement the naive DFT. Verify against the FFT for `N = 256` random samples. What is
the maximum difference?

**25.2** Time the DFT and FFT for `N` = 64, 256, 1024, 4096. Plot both against `N`. Confirm one
curve is quadratic and the other close to linear.

**25.3** Transform a sine at exactly bin-centre (`k·fs/N` for some integer `k`). How many bins are
non-zero? Now shift it to halfway between two bins. How many bins are non-zero now? You have just
seen spectral leakage.

**25.4** Verify Parseval's theorem for a random signal, a sine and an impulse. Which has its
energy most concentrated in the frequency domain? Most spread?

**25.5** Transform an impulse. What is the magnitude spectrum? Explain the answer in terms of
Chapter 20's statement that an impulse contains all frequencies equally.

**25.6** Transform a step. What does the spectrum look like, and how does it relate to Chapter 13's
click?

**25.7** Implement `fastConvolve` and verify it matches Chapter 21's `convolve` to within 1e-6.
Then time both on the cathedral IR and reproduce §25.7's table.

**25.8** *Deliberate breakage.* Remove the zero-padding from `fastConvolve` (use `N =
nextPowerOfTwo(x.size())` only). Convolve a short sound with a long reverb IR and listen. Where
did the tail go?

**25.9** Implement a real-input FFT: pack `N` real samples into `N/2` complex values, transform,
then unpack. Verify against the complex version and measure the speedup.

**25.10** Build a spectrum analyser: read a WAV, take 2048-point FFTs, print the top ten bins by
magnitude with their frequencies. Test it on a known sine, a chord and white noise.

**25.11** Use the FFT to measure the frequency response of Chapter 23's biquad: fire an impulse
through it, transform the impulse response, and compare the magnitude spectrum against
`magnitudeDb`. They should agree closely. Where do they differ, and why? (Hint: your impulse
response is truncated.)

---

### Chapter summary

- The **DFT** takes `N` samples and produces `N` complex values: `X[k] = Σ x[n]·e^(-j2πkn/N)`.
  It is **`N` correlations against `N` test frequencies**.
- **Bin `k` is at `k·fs/N` Hz**; bin width is `Δf = fs/N`. Long window = fine frequency
  resolution, poor time resolution. The **time-frequency trade-off**, for the third time.
- For real input the spectrum is **conjugate-symmetric**: only bins `0..N/2` are new. Scale
  magnitudes by `2/N`, **except bins 0 and N/2**, which get `1/N`.
- **Spectral leakage** occurs when a frequency does not fall exactly on a bin centre. The fix is
  windowing — Chapter 26.
- The naive DFT is `O(N²)` and unusable above a few hundred points.
- The **FFT** splits even and odd samples, giving `X[k] = E[k] + W^k·O[k]` and
  `X[k+N/2] = E[k] − W^k·O[k]` — a **butterfly**. Recursing gives `O(N log N)`, a 4,000× speedup
  at `N = 65,536`. Requires power-of-two `N`; zero-pad if necessary.
- Implementation: **bit-reversal permutation**, then `log₂N` levels of butterflies. The inverse
  differs only in the exponent sign and a `1/N`.
- **Verify with four tests:** against the naive DFT, round trip, known signals at bin centres,
  and **Parseval's theorem** (the strongest).
- **Fast convolution:** `IFFT(FFT(x)·FFT(h))`, with **zero-padding to at least `N+M−1`** or the
  tail wraps around. The cathedral reverb goes from 6.7 s to 89 ms.
- Real-time use still needs **partitioning** to avoid `N`-sample latency — Chapters 48–49.

**Next:** [Chapter 26 — Windowing and Spectral Leakage](26-windowing.md)
