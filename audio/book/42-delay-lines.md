# Chapter 42 — Delay Lines and Circular Buffers

> Part IV processes sound rather than creating it, and almost all of it is built on one
> component. Echo, chorus, flanger, phaser, reverb, pitch shifting, Doppler and comb filtering
> are all delay lines with different parameters. Get this right and the next thirteen chapters
> are variations.

---

## 42.1 The circular buffer

A delay line stores the last `N` samples so you can read one from the past. The naive
implementation shifts the whole buffer each sample — `O(N)` per sample, absurd.

The right structure is a **circular buffer**: a fixed array with a moving write position that
wraps around.

```
   write ──┐
           ▼
   [ .. .. X .. .. .. .. .. ]
        ▲
      read (some distance behind)

   Both indices advance each sample and wrap at the end.
```

```cpp
class DelayLine
{
public:
    void prepare(double sampleRate, double maxDelaySeconds)
    {
        sr_ = sampleRate;
        // +4 gives interpolation room at the ends (Chapter 28's Hermite
        // reads two samples either side).
        const size_t n = static_cast<size_t>(maxDelaySeconds * sampleRate) + 4;
        buffer_.assign(n, 0.0f);
        writeIndex_ = 0;
    }

    void write(float x)
    {
        buffer_[writeIndex_] = x;
        if (++writeIndex_ >= buffer_.size()) writeIndex_ = 0;
    }

    // Read `delaySamples` in the past. Fractional delays allowed.
    float read(double delaySamples) const
    {
        const double n = static_cast<double>(buffer_.size());
        double pos = static_cast<double>(writeIndex_) - delaySamples;
        while (pos < 0.0) pos += n;
        while (pos >= n)  pos -= n;

        const size_t i0 = static_cast<size_t>(pos);
        const size_t i1 = (i0 + 1) % buffer_.size();
        const double frac = pos - static_cast<double>(i0);

        return static_cast<float>(buffer_[i0] * (1.0 - frac) + buffer_[i1] * frac);
    }

    float readSeconds(double seconds) const { return read(seconds * sr_); }

    void clear() { std::fill(buffer_.begin(), buffer_.end(), 0.0f); writeIndex_ = 0; }

private:
    std::vector<float> buffer_;
    size_t writeIndex_ = 0;
    double sr_ = kDefaultRate;
};
```

**Three details that matter.**

**Write then read, or read then write?** Reading *before* writing gives a minimum delay of one
sample; reading after writing allows a delay of zero. For feedback loops you almost always want
read-then-write, because a zero-sample feedback delay is an algebraic loop that cannot be
computed. Be deliberate about it and document which you chose.

**The wrap uses `while`, not `if`**, for the same reason as Chapter 30's phase: a modulated
delay time can move by more than the buffer length in pathological cases.

**Interpolation headroom.** The `+4` matters if you switch to Hermite interpolation, which reads
`i-1` through `i+2`. Without it you index out of bounds at the wrap point — an intermittent
crash that only happens when the read position lands near the boundary, which is the worst kind
of bug.

### The power-of-two optimisation

If the buffer length is a power of two, the wrap becomes a mask (Chapter 8):

```cpp
writeIndex_ = (writeIndex_ + 1) & mask_;      // mask_ = size - 1
```

One AND instead of a comparison and a branch. On a modern CPU the branch predictor handles the
`if` well, so the saving is small — but in a reverb with sixteen delay lines read several times
each per sample, it adds up. Most production reverbs use power-of-two buffers for exactly this
reason, accepting that the delay lengths are then not freely choosable.

---

## 42.2 Interpolation: which one, and when

Chapter 28 covered the methods. Here is the delay-specific guidance, because the answer depends
on whether the delay time *moves*.

**Static delay time** (an echo, a fixed comb filter): the delay is an integer number of samples,
so **no interpolation is needed at all**. Just read the integer index.

**Slowly modulated** (chorus, flanger, vibrato): **linear interpolation**. The modulation is
mostly in the low mids where linear is transparent, and the effect is deliberately coloured
anyway. This is what almost every chorus does.

**Fast modulation or wide pitch shifts** (Doppler, tape emulation, pitch shifting):
**Hermite**. Linear's high-frequency loss becomes a modulated low-pass filter, which sounds like
a dull wobble.

**The trap nobody warns you about:** linear interpolation's frequency response *depends on the
fractional part*. At frac = 0 it is exactly transparent; at frac = 0.5 it is at its dullest. So a
delay whose time sweeps through fractional values has a **brightness that wobbles at the
modulation rate** — an amplitude modulation of the high frequencies that you did not ask for.

On a chorus this is inaudible and arguably part of the character. On a long, slowly-swept delay it
is a clearly audible artefact. Hermite reduces it by about 25 dB.

---

## 42.3 The comb filter

Mix a signal with a delayed copy of itself and you get a **comb filter** — so called because its
frequency response looks like the teeth of a comb.

```cpp
// Feedforward comb
y[n] = x[n] + g · x[n - D]
```

The frequency response has **peaks** where the delayed copy arrives in phase and **notches**
where it arrives out of phase:

```
   |H(f)|
     2 |  /\    /\    /\    /\    /\
       | /  \  /  \  /  \  /  \  /  \
     0 |/    \/    \/    \/    \/    \
       +--------------------------------> f
         0   fs/D  2fs/D 3fs/D
```

- **Notches** at `f = (2k+1)·fs/(2D)` — odd multiples of half the comb frequency
- **Peaks** at `f = k·fs/D`
- Spacing between notches: `fs/D` Hz

**This single fact explains an enormous number of things:**

| Phenomenon | The delay involved |
|---|---|
| A microphone near a reflective surface sounds hollow | The reflection path difference |
| Two mics on one source sound thin when mixed | The distance between them |
| A flanger's swoosh | A short, swept delay (Chapter 44) |
| Regularly-spaced reverb reflections sound metallic | Equal delay lengths (Chapter 46) |
| Granular synthesis with regular grains rings | The grain period (Chapter 37) |
| A speaker and its floor bounce cancel at certain frequencies | The path difference |

**The practical rule that follows:** anywhere you sum a signal with a delayed version of itself,
you have made a comb filter. If you did not want one, either avoid the summing or make the delay
long enough (> 50 ms) that the teeth are too close together to be heard as coloration — at which
point they read as an echo instead (Chapter 3's precedence effect).

### The feedback comb

```cpp
// Feedback comb -- an IIR filter
y[n] = x[n] + g · y[n - D]
```

Now the delayed signal is the *output*, so it recirculates. The peaks become much sharper and
taller (poles rather than zeros — Chapter 24), and the response rings.

```cpp
class CombFilter
{
public:
    float process(float x)
    {
        const float delayed = delay_.read(delaySamples_);
        const float out     = x + feedback_ * delayed;
        delay_.write(out);                       // write the OUTPUT (feedback)
        return out;
    }

private:
    DelayLine delay_;
    double delaySamples_ = 1000.0;
    float  feedback_     = 0.5f;
};
```

**Stability:** `|feedback| < 1`. At exactly 1 it rings forever; above 1 it grows without bound —
Chapter 24's unit circle, again, with the poles at radius `|g|^(1/D)`.

**Decay time** follows directly:

```
   RT60 = D / fs × 60 / (-20·log10(|g|))     seconds
```

A 50 ms delay with `g = 0.7` gives `50ms × 60/3.098 = 0.97 s`. This formula is how reverb decay
controls are actually implemented (Chapter 46).

---

## 42.4 Multi-tap delays

One buffer, many read positions. This is the cheapest way to get complex rhythmic or spatial
effects, because the expensive part (the buffer) is shared.

```cpp
struct Tap { double delayMs; float gain; float pan; };

class MultiTapDelay
{
public:
    void setTaps(std::vector<Tap> taps) { taps_ = std::move(taps); }

    void process(float in, float& left, float& right)
    {
        double l = 0.0, r = 0.0;

        for (const auto& t : taps_)
        {
            const float s = delay_.read(t.delayMs * 0.001 * sr_) * t.gain;
            l += s * (1.0f - t.pan);
            r += s * t.pan;
        }

        delay_.write(in + feedback_ * static_cast<float>(l + r) * 0.5f);

        left  = static_cast<float>(l);
        right = static_cast<float>(r);
    }

private:
    DelayLine delay_;
    std::vector<Tap> taps_;
    float feedback_ = 0.0f;
    double sr_ = kDefaultRate;
};
```

Chapter 21 noted that a multi-tap delay **is** a convolution with a sparse impulse response. That
means the early reflections of a reverb (Chapter 46) and a rhythmic delay are the same object,
differently parameterised — a 12-tap delay with irregular times and decaying gains *is* a room's
early reflection pattern.

**Useful tap patterns:**

| Pattern | Taps | Effect |
|---|---|---|
| Ping-pong | 2, alternating pan, feedback crossed | Bouncing L/R |
| Dotted eighth | 1 at `beat × 0.75` | The U2 delay |
| Early reflections | 8–16 at irregular prime-ish times, decaying | Room ambience |
| Rhythmic | Taps at musical subdivisions | Polyrhythmic echoes |
| Diffuse | 20+ at dense random times | Smearing, pre-reverb |

---

## 42.5 Memory and latency

**Memory:** `maxDelaySeconds × sampleRate × 4` bytes per channel. A 2-second stereo delay at
48 kHz is 768 KB. A 10-second looper is 3.8 MB. Not large by modern standards, but a reverb with
sixteen delay lines per channel adds up, and cache behaviour (Chapter 63) becomes the real
constraint rather than capacity.

**Allocate in `prepare()`, never in `process()`.** Chapter 59's rule, and delay lines are the
most common place people break it — "the user increased the delay time, so reallocate the
buffer" is exactly the wrong response. Allocate for the maximum up front and only change the
*read offset*.

**Changing the delay time.** Jumping the read position is a discontinuity, which is a click
(Chapter 13). Three approaches:

1. **Smooth the delay time** with a one-pole. The delay then behaves like tape — the pitch bends
   as it moves, because you are reading at a varying rate. Musical, and what most delays do.
2. **Crossfade** between the old and new positions over 10–30 ms. No pitch bend, but a brief
   doubling. What "digital mode" delays do.
3. **Only change at zero crossings.** Rarely worth the complexity.

**Which you pick is an audible design decision**, not an implementation detail. Tape-style
smoothing is why analogue delay emulations sound the way they do when you turn the knob.

---

## 42.6 Exercises

**42.1** Implement the delay line and verify: write an impulse, read at 1000 samples, confirm it
appears exactly 1000 samples later.

**42.2** *Deliberate breakage.* Remove the `+4` interpolation headroom and use Hermite
interpolation. Sweep the delay time across the buffer wrap point until it crashes.

**42.3** Build a feedforward comb at 5 ms. Measure its frequency response (Chapter 22's
`frequencyResponse` on the impulse response). Where are the notches? Check against `fs/(2D)`.

**42.4** Build a feedback comb and verify the RT60 formula for `g` = 0.5, 0.7, 0.9, 0.95.

**42.5** Sweep a comb's delay from 0.1 ms to 50 ms over 10 seconds on pink noise. Note where it
stops sounding like coloration and starts sounding like an echo.

**42.6** Compare linear and Hermite interpolation on a delay whose time sweeps sinusoidally at
0.5 Hz over ±3 ms, applied to a bright source. Can you hear the linear version's brightness
wobble?

**42.7** Build a ping-pong delay with crossed feedback. Verify the echoes alternate channels.

**42.8** Build a 12-tap early-reflection pattern using prime-ish times between 11 and 89 ms with
decaying gains. Compare with Chapter 21's synthetic IR.

**42.9** Implement both delay-time-change strategies (smoothed and crossfaded). Sweep the delay
knob and compare.

**42.10** Implement the power-of-two masked wrap and benchmark against the branching version over
16 delay lines read 4 times each per sample.

---

### Chapter summary

- A **circular buffer** gives `O(1)` delay: a fixed array with a wrapping write index.
- Decide **read-then-write** (minimum delay 1 sample, safe for feedback) versus write-then-read,
  and document it. Wrap with `while`. Leave **interpolation headroom** at the ends.
- **Static delays need no interpolation**; slow modulation wants **linear**; fast modulation or
  pitch shifting wants **Hermite**. Linear's response depends on the fractional part, so a swept
  delay gets a **brightness wobble** at the modulation rate.
- **Summing a signal with a delayed copy makes a comb filter** — notches every `fs/(2D)` Hz. This
  explains hollow microphone placement, thin multi-mic mixes, flangers, metallic reverbs and
  ringing granular clouds.
- **Feedback combs** are IIR: stable for `|g| < 1`, with
  `RT60 = (D/fs) × 60 / (−20·log10|g|)`. That formula is how reverb decay controls work.
- **Multi-tap delays** share one buffer among many read positions, and a multi-tap delay *is* a
  convolution with a sparse IR — the same object as a room's early reflections.
- **Allocate in `prepare()`, never in `process()`.** Changing the delay time is an audible design
  decision: **smoothing gives tape-style pitch bend**, crossfading gives clean digital changes.

**Next:** [Chapter 43 — Echo, Feedback, and Stability](43-echo-and-feedback.md)
