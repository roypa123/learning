# Chapter 47 — Reverb II: Feedback Delay Networks

> Chapter 46's combs each feed back into themselves. A feedback delay network lets every delay
> feed back into *every other* delay, through a matrix. That one change produces exponentially
> greater echo density, removes the metallic ringing, and gives you independent control over
> decay at different frequencies. It is the architecture behind most high-quality algorithmic
> reverbs.

---

## 47.1 The structure

```
              ┌──────────── FEEDBACK MATRIX (N×N) ────────────┐
              │                                                │
              ▼                                                │
   in ──► [+]──► delay 1 (m1) ──► damping 1 ──┬────────────────┤──► out
          [+]──► delay 2 (m2) ──► damping 2 ──┤                │
          [+]──► delay 3 (m3) ──► damping 3 ──┤                │
          [+]──► delay 4 (m4) ──► damping 4 ──┘────────────────┘
```

Each delay's output is mixed into **every** delay's input through a matrix `A`:

```
   input to delay i  =  in·b[i]  +  Σ  A[i][j] · output of delay j
                                    j
```

**With `N` delays, each echo splits into `N` echoes on every pass.** After `k` passes there are
`N^k` echo paths. With 8 delays and 20 passes that is 8²⁰ ≈ 10¹⁸ — density that Schroeder's
parallel combs cannot approach, because a comb's echo only ever feeds back into itself.

---

## 47.2 The matrix

The matrix `A` is where the design lives, and the requirement is precise.

**For the reverb to be stable and lossless, `A` must be *unitary*** (orthogonal, for real
matrices):

```
   A · Aᵀ = I
```

Unitary means the matrix **preserves total energy** — it redistributes energy among the delay
lines without creating or destroying any. Then the *only* place energy is lost is in the damping
filters and the decay gain, which means decay is controlled entirely by parameters you chose
rather than emerging from matrix arithmetic.

**If `A` is not unitary**, you get either runaway (energy grows) or an uncontrolled decay that
changes when you retune delay lengths. Both are hard to debug.

### Householder

```
   A = I - (2/N) · 1·1ᵀ
```

where `1` is a column of ones. For `N = 4`:

```
   A = 0.5 ·  [ -1   1   1   1 ]
              [  1  -1   1   1 ]
              [  1   1  -1   1 ]
              [  1   1   1  -1 ]
```

The implementation is beautifully cheap — no multiplies at all:

```cpp
// Householder feedback, O(N) instead of O(N^2).
void householder(std::array<float, 4>& v)
{
    const float sum = (v[0] + v[1] + v[2] + v[3]) * 0.5f;    // 2/N with N=4
    for (auto& x : v) x = sum - x;
}
```

**Three additions, one multiply, four subtractions** — for what is nominally a 4×4 matrix
multiply. That is why Householder is the default choice.

### Hadamard

```
   H4 = 0.5 · [ 1  1  1  1 ]
              [ 1 -1  1 -1 ]
              [ 1  1 -1 -1 ]
              [ 1 -1 -1  1 ]
```

Also multiply-free, using the fast Hadamard transform:

```cpp
void hadamard4(std::array<float, 4>& v)
{
    const float a = v[0] + v[1], b = v[0] - v[1];
    const float c = v[2] + v[3], d = v[2] - v[3];

    v[0] = (a + c) * 0.5f;
    v[1] = (b + d) * 0.5f;
    v[2] = (a - c) * 0.5f;
    v[3] = (b - d) * 0.5f;
}
```

**Hadamard mixes more evenly than Householder**, which gives a slightly smoother tail at the cost
of being slightly more "washed out". Householder retains a little more of the individual delay
lines' character. Both are used; the choice is aesthetic.

**Hadamard requires `N` to be a power of two.** Householder works for any `N`.

### Other choices

**Random orthogonal matrices** give the smoothest tails but cost `O(N²)` and must be generated
carefully (Gram-Schmidt on random vectors, then verify orthogonality).

**Block-circulant matrices** allow tuning the diffusion characteristics more precisely, at some
complexity.

**Velvet-noise matrices** are sparse orthogonal matrices — mostly zeros — which keeps the cost
near `O(N)` while behaving like a random matrix.

---

## 47.3 Delay lengths

**Mutually prime, as always** (Chapters 45, 46). But for an FDN there is more to it.

**The total delay determines the modal density.** A room's modes are spaced by roughly
`c³/(4πV f²)` — and in an FDN, the number of modes is set by the total number of delay samples:

```
   modal density ≈ total delay samples / sample rate   modes per Hz
```

**The rule of thumb:** for a smooth, non-ringing tail you want at least **0.15 modes per hertz**
at low frequencies, which means:

```
   total delay (samples) ≥ 0.15 × sampleRate
```

At 44.1 kHz that is 6,615 samples total. With 8 delay lines, averaging 827 samples (19 ms) each.
Fewer than that and the tail is audibly sparse; the "flutter" of Chapter 46.

**Practical lengths** for an 8-line FDN at 44.1 kHz, prime and spread over roughly 3:1:

```cpp
constexpr int kDelays[8] = { 1129, 1523, 1789, 2053, 2311, 2647, 2903, 3163 };
// All prime. Total: 17,518 samples (0.40 s) -> 0.40 modes/Hz. Comfortable.
```

**Why spread over 3:1 rather than Chapter 46's 1.5:1?** Because the matrix already mixes
everything; a wider spread of delay lengths gives a wider spread of echo times, which increases
density in a way that parallel combs cannot exploit.

---

## 47.4 Damping and decay control

The elegance of the FDN is that **decay is set per delay line** and can be frequency-dependent.

For a target RT60 at a given frequency, the gain for delay line `i` of length `m[i]` samples is:

```
   g[i] = 10^( -3·m[i] / (RT60 · fs) )
```

```cpp
float decayGain(double delaySamples, double rt60Seconds, double sampleRate)
{
    return static_cast<float>(
        std::pow(10.0, -3.0 * delaySamples / (rt60Seconds * sampleRate)));
}
```

**Each line gets a different gain because each has a different length** — the longer line needs
less attenuation per pass to reach the same RT60, since it passes round fewer times per second.
Using the same gain for all lines gives a decay that depends on length, which is wrong.

### Frequency-dependent decay

Real rooms decay faster at high frequencies (Chapter 2). Put a filter in each feedback path:

```cpp
// A one-pole low-pass shelf whose gain matches the target RT60 at DC
// and at high frequency independently.
class DampingFilter
{
public:
    void set(double delaySamples, double rt60Low, double rt60High,
             double crossoverHz, double sampleRate)
    {
        const double gLow  = std::pow(10.0, -3.0 * delaySamples / (rt60Low  * sampleRate));
        const double gHigh = std::pow(10.0, -3.0 * delaySamples / (rt60High * sampleRate));

        // A one-pole shelving filter: gain gLow at DC, gHigh at Nyquist.
        const double k = std::tan(kPi * crossoverHz / sampleRate);
        a_ = (k - 1.0) / (k + 1.0);
        gLow_  = static_cast<float>(gLow);
        gHigh_ = static_cast<float>(gHigh);
    }

    float process(float x)
    {
        const double lp = a_ * (x - y1_) + x1_;
        x1_ = x;
        y1_ = lp;

        // Blend: low frequencies get gLow, high frequencies get gHigh.
        return static_cast<float>(gHigh_ * x + (gLow_ - gHigh_) * lp);
    }

private:
    double a_ = 0.0, x1_ = 0.0, y1_ = 0.0;
    float  gLow_ = 0.9f, gHigh_ = 0.5f;
};
```

**This gives independent RT60 at low and high frequencies**, which is exactly how real rooms are
specified acoustically and exactly what a mixing engineer wants to control.

Typical values:

| Space | RT60 low (below 1 kHz) | RT60 high (above) |
|---|---|---|
| Studio live room | 0.6 s | 0.4 s |
| Concert hall | 2.2 s | 1.6 s |
| Cathedral | 8.0 s | 3.5 s |
| Cave | 4.0 s | 1.2 s |
| Tiled bathroom | 1.5 s | 1.8 s (brighter than low!) |

**Note the bathroom.** Hard tile reflects high frequencies *better* than low ones (which leak
through the walls), so its high-frequency RT60 is *longer*. A reverb that can only make the tail
darker cannot produce that, and bathroom reverbs from such algorithms always sound wrong.

---

## 47.5 The complete FDN

```cpp
class FDNReverb
{
public:
    void prepare(double sampleRate)
    {
        sr_ = sampleRate;
        const double scale = sampleRate / 44100.0;

        for (int i = 0; i < N; ++i)
        {
            lengths_[i] = kDelays[i] * scale;
            delays_[i].prepare(sampleRate, 0.5);
        }

        // Input diffusion: all-pass stages before the network, so the
        // onset is smooth rather than a burst of discrete echoes (Ch 45).
        const double diffLen[4] = { 142, 107, 379, 277 };
        for (int i = 0; i < 4; ++i)
        {
            diffusionL_[i].prepare(sampleRate, 0.05);
            diffusionL_[i].setDelaySamples(diffLen[i] * scale);
            diffusionL_[i].setGain(0.7f);
            diffusionR_[i].prepare(sampleRate, 0.05);
            diffusionR_[i].setDelaySamples((diffLen[i] + 23) * scale);
            diffusionR_[i].setGain(0.7f);
        }

        preDelay_.prepare(sampleRate, 0.5);
        updateDecay();
    }

    void setRT60(double low, double high, double crossoverHz)
    {
        rt60Low_ = low; rt60High_ = high; crossover_ = crossoverHz;
        updateDecay();
    }

    void setPreDelayMs(double ms) { preDelaySamples_ = ms * 0.001 * sr_; }

    void process(float inL, float inR, float& outL, float& outR)
    {
        // --- pre-delay ---------------------------------------------
        const float mono = (inL + inR) * 0.5f;
        preDelay_.write(mono);
        float x = preDelay_.read(preDelaySamples_);

        // --- input diffusion ----------------------------------------
        float dl = x, dr = x;
        for (int i = 0; i < 4; ++i)
        {
            dl = diffusionL_[i].process(dl);
            dr = diffusionR_[i].process(dr);
        }

        // --- read the delay lines -----------------------------------
        std::array<float, N> v{};
        for (int i = 0; i < N; ++i)
            v[i] = damping_[i].process(delays_[i].read(lengths_[i]));

        // Collect the output BEFORE the matrix, alternating sign for
        // decorrelation between channels.
        double l = 0.0, r = 0.0;
        for (int i = 0; i < N; ++i)
        {
            if (i % 2 == 0) l += v[i]; else r += v[i];
        }

        // --- feedback matrix -----------------------------------------
        householder(v);

        // --- write back, injecting the input ------------------------
        for (int i = 0; i < N; ++i)
        {
            const float inject = (i % 2 == 0) ? dl : dr;
            float w = v[i] + inject * inputGain_;

            // Slight modulation of the delay length breaks up residual
            // periodicity (Chapter 45). Keep it very small.
            if (!std::isfinite(w)) w = 0.0f;
            delays_[i].write(w);
        }

        const double norm = 1.0 / std::sqrt(static_cast<double>(N) * 0.5);

        outL = inL * dry_ + static_cast<float>(l * norm) * wet_;
        outR = inR * dry_ + static_cast<float>(r * norm) * wet_;
    }

private:
    void updateDecay()
    {
        for (int i = 0; i < N; ++i)
            damping_[i].set(lengths_[i], rt60Low_, rt60High_, crossover_, sr_);
    }

    static constexpr int N = 8;

    std::array<DelayLine, N>      delays_;
    std::array<DampingFilter, N>  damping_;
    std::array<double, N>         lengths_{};
    std::array<AllpassDelay, 4>   diffusionL_, diffusionR_;
    DelayLine preDelay_;

    double sr_ = kDefaultRate;
    double rt60Low_ = 2.0, rt60High_ = 1.2, crossover_ = 2000.0;
    double preDelaySamples_ = 900.0;
    float  inputGain_ = 0.5f, wet_ = 0.3f, dry_ = 0.7f;
};
```

**Three design choices worth noting.**

**Input diffusion before the network.** Without it, the first few milliseconds are a handful of
discrete echoes — the network needs time to build density. Four all-pass stages smear the onset
so it sounds continuous from the start.

**Output taken before the matrix, with alternating channel assignment.** Taking even-indexed
lines to the left and odd to the right gives naturally decorrelated channels, because those lines
have different lengths and therefore different echo patterns.

**Slight delay modulation.** Even with prime lengths, a static FDN can develop a faint ringing at
very long decays. A sub-millisecond, slow, random modulation of each delay length eliminates it.
The same tuning problem as Chapter 45: too much and sustained notes wobble.

---

## 47.6 FDN versus Schroeder

| | Schroeder/Freeverb | FDN |
|---|---|---|
| Echo density growth | Linear per comb | **Exponential (`N^k`)** |
| Metallic ringing | Present at long decays | **Largely absent** |
| Frequency-dependent decay | One damping control | **Independent RT60 per band** |
| CPU (8 lines) | ~40 ops/sample | ~80 ops/sample |
| Tuning difficulty | Moderate | **Higher** — matrix, lengths and damping interact |
| Quality ceiling | Good | **Excellent** |

**The FDN costs about twice as much and sounds considerably better.** For anything where reverb
quality matters — which in cinematic work is always — it is the right algorithmic choice.

**Where Schroeder still wins:** very tight CPU budgets, and as a cheap diffusion stage inside
something larger.

**Where neither wins:** when you need a *specific real space*. No algorithm reproduces a
particular cathedral. That is Chapter 48.

---

## 47.7 Exercises

**47.1** Implement Householder and Hadamard mixing. Verify both are unitary by checking that the
sum of squares of a vector is preserved.

**47.2** Build a 4-line FDN. Feed an impulse and count the echoes in the first 100 ms. Compare
with Chapter 46's four parallel combs.

**47.3** *Deliberate breakage.* Replace the matrix with the identity (each line feeds only
itself). What have you built? How does it sound?

**47.4** *Deliberate breakage.* Use a non-unitary matrix (e.g. all entries 0.5). Measure the
output level over 10 seconds.

**47.5** Verify the decay gain formula: set RT60 to 2 seconds and measure the actual RT60 of the
impulse response.

**47.6** Set RT60 low = 4 s and high = 0.8 s. Then reverse them (low = 0.8, high = 4). Which
sounds like a cathedral and which like a tiled room?

**47.7** Reduce the total delay length below the 0.15 modes/Hz threshold. Listen for flutter in
the tail.

**47.8** Compare Householder and Hadamard on the same delay lengths and decay. Describe the
difference.

**47.9** Remove the input diffusion. Listen to the first 30 ms of the impulse response.

**47.10** Add slow random modulation to the delay lengths. Find the depth at which ringing
disappears and the depth at which a sustained piano note wobbles.

---

### Chapter summary

- An **FDN** feeds every delay line back into every other through a matrix. Echo paths grow as
  **`N^k`** rather than linearly — density Schroeder's parallel combs cannot reach.
- **The matrix must be unitary** (`A·Aᵀ = I`) so it redistributes energy without creating or
  destroying it. Then decay is controlled *only* by the gains and damping filters you set.
- **Householder** (`A = I − (2/N)·1·1ᵀ`) and **Hadamard** are both **multiply-free** and `O(N)`.
  Householder works for any `N`; Hadamard needs a power of two and mixes slightly more evenly.
- **Delay lengths: mutually prime, spread ~3:1.** Total delay must give at least **0.15 modes
  per hertz** (`total ≥ 0.15 × fs`) or the tail flutters.
- **Decay gain is per line**: `g[i] = 10^(−3·m[i]/(RT60·fs))`. Longer lines need less attenuation
  per pass.
- A **shelving damping filter per line** gives **independent RT60 at low and high frequencies** —
  which is how real rooms are specified, and the only way to get a tiled bathroom (where the
  *high* RT60 is longer) right.
- Add **input diffusion** before the network so the onset is smooth, take the **output before the
  matrix** with alternating channel assignment for decorrelation, and apply **very slight delay
  modulation** to kill residual ringing.
- FDN costs about **2× Schroeder** and sounds considerably better. Neither can reproduce a
  *specific* real space — that is Chapter 48.

**Next:** [Chapter 48 — Reverb III: Convolution](48-reverb-convolution.md)
