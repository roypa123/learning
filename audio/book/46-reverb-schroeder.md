# Chapter 46 — Reverb I: Schroeder, Comb Filters, and Freeverb

> Reverb is the effect that tells the listener *where they are*. Four chapters are devoted to it
> because it is the most important effect in cinematic sound and because there are four genuinely
> different ways to build one. This chapter covers the classic algorithmic approach: cheap,
> controllable, and still in wide use sixty years after it was invented.

---

## 46.1 What reverb has to do

Chapter 2 described a room's impulse response:

```
   level
     |  | direct sound
     |  |
     |  |    |  |   |     early reflections (sparse, discrete, 5-80 ms)
     |  |    |  |   |  |
     |  |    |  |   |  |||||||.,,,___   late reverb (dense, smooth, decaying)
     +----------------------------------> time
```

So a reverb algorithm must produce:

| Requirement | Why |
|---|---|
| **Increasing echo density** with time | Real rooms go from a few reflections to thousands |
| **Smooth exponential decay** | Energy loss is proportional to energy present |
| **Flat frequency response** overall | A room should not sound like a tuned pipe |
| **High-frequency damping over time** | Air and surfaces absorb treble (Chapter 2) |
| **Decorrelated stereo output** | Two ears receiving identical signals sounds like headphones, not a room |
| **No audible periodicity** | Regular echoes are heard as metallic ringing |

The last two are where naive implementations fail.

---

## 46.2 Schroeder's design (1962)

Manfred Schroeder's insight was to split the job between two components you now know well:

**Parallel feedback combs** provide the decay and the bulk of the energy.
**Series all-passes** provide diffusion — increasing the echo density without colouring the
spectrum (Chapter 45).

```
                ┌──► comb 1 ──┐
                ├──► comb 2 ──┤
   in ──────────┼──► comb 3 ──┼──► SUM ──► allpass 1 ──► allpass 2 ──► out
                └──► comb 4 ──┘
```

**Why parallel combs, and why four?** Each comb produces echoes at a regular interval with a
strongly coloured response (Chapter 42). One comb alone is a metallic ringing. Four combs with
**mutually prime delay lengths** produce echo patterns that rarely coincide, so their sum is much
denser and their individual colorations partially fill each other's notches.

**Why series all-passes?** They multiply the density further while leaving the spectrum flat
(Chapter 45). Putting them in *series* is essential — each one processes the output of the last,
so densities multiply rather than add.

### Schroeder's original delay lengths

```
   Combs (ms):     29.7   37.1   41.1   43.7
   Allpasses (ms):  5.0    1.7
```

In samples at 44.1 kHz: 1310, 1636, 1813, 1927 for the combs. **These are chosen to be mutually
prime** — no two share a common factor. Chapter 45 explained why: coincident echoes reintroduce
periodicity.

**The ratio between the longest and shortest comb matters too.** Schroeder found around 1.5:1
works well. Too narrow a spread and the combs' resonances cluster, producing coloration. Too wide
and the short comb's echoes are audibly separate from the long one's.

---

## 46.3 Freeverb

Jezar's Freeverb (2000) is public domain, remarkably effective for its size, and has been ported
into essentially every audio framework. It is the practical version of Schroeder's design with
three improvements.

**Improvement 1: eight combs instead of four.** More density.

**Improvement 2: a low-pass inside each comb's feedback path.**

```cpp
class LowpassCombFilter
{
public:
    float process(float x)
    {
        const float output = buffer_[index_];

        // The damping filter -- a one-pole low-pass INSIDE the feedback loop.
        // Each pass round the loop filters again, so high frequencies decay
        // faster than low ones. This is Chapter 2's air absorption.
        filterStore_ = output * damp2_ + filterStore_ * damp1_;

        buffer_[index_] = x + filterStore_ * feedback_;

        if (++index_ >= buffer_.size()) index_ = 0;
        return output;
    }

    void setDamping(float d)
    {
        damp1_ = d;
        damp2_ = 1.0f - d;
    }

private:
    std::vector<float> buffer_;
    size_t index_ = 0;
    float  feedback_ = 0.84f;
    float  filterStore_ = 0.0f;
    float  damp1_ = 0.2f, damp2_ = 0.8f;
};
```

**This single filter is what makes Freeverb sound like a room rather than a machine.** Without
it, the tail keeps all its high frequencies as it decays — which sounds metallic and artificial,
because no real space does that.

**Improvement 3: stereo by offsetting the delay lengths.**

```cpp
constexpr int kStereoSpread = 23;      // samples

// The right channel uses the same delays PLUS 23 samples.
combL[i].setSize(combTuning[i]);
combR[i].setSize(combTuning[i] + kStereoSpread);
```

23 samples is 0.5 ms — far too short to hear as a delay, but enough to make the two channels'
echo patterns different. The result is a **decorrelated** stereo tail, which is what makes the
reverb sound like a space you are inside rather than a sound in the middle of your head.

### Freeverb's tuning

```cpp
// Comb delay lengths, in samples at 44.1 kHz
constexpr int combTuning[8] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };

// Allpass delay lengths
constexpr int allpassTuning[4] = { 556, 441, 341, 225 };

constexpr float kFixedGain     = 0.015f;
constexpr float kScaleDamp     = 0.4f;
constexpr float kScaleRoom     = 0.28f;
constexpr float kOffsetRoom    = 0.7f;
constexpr float kAllpassFeedback = 0.5f;
```

**These numbers were found by ear**, not derived. That is normal for reverb: the theory tells you
the *structure*, and tuning the constants is craft. Anyone who tells you reverb tuning is purely
analytical has not tuned one.

**Note that the delays must scale with sample rate.** Freeverb's constants are for 44.1 kHz;
at 48 kHz they must be multiplied by 48000/44100 or the reverb is 8.8% shorter and differently
tuned. This is a genuinely common porting bug.

---

## 46.4 The complete structure

```cpp
class Freeverb
{
public:
    void prepare(double sampleRate)
    {
        const double scale = sampleRate / 44100.0;    // scale the tuning

        for (int i = 0; i < 8; ++i)
        {
            combL_[i].setSize(static_cast<size_t>(combTuning[i] * scale));
            combR_[i].setSize(static_cast<size_t>((combTuning[i] + kStereoSpread) * scale));
        }
        for (int i = 0; i < 4; ++i)
        {
            allpassL_[i].setSize(static_cast<size_t>(allpassTuning[i] * scale));
            allpassR_[i].setSize(static_cast<size_t>((allpassTuning[i] + kStereoSpread) * scale));
        }
    }

    void setRoomSize(float s) { roomSize_ = s * kScaleRoom + kOffsetRoom; update(); }
    void setDamping(float d)  { damping_  = d * kScaleDamp;               update(); }
    void setWidth(float w)    { width_    = w;                            update(); }

    void process(float inL, float inR, float& outL, float& outR)
    {
        const float input = (inL + inR) * kFixedGain;

        float l = 0.0f, r = 0.0f;

        // Combs in PARALLEL: accumulate.
        for (int i = 0; i < 8; ++i)
        {
            l += combL_[i].process(input);
            r += combR_[i].process(input);
        }

        // Allpasses in SERIES: each processes the previous output.
        for (int i = 0; i < 4; ++i)
        {
            l = allpassL_[i].process(l);
            r = allpassR_[i].process(r);
        }

        // Width control: blend between mono and full stereo.
        const float wet1 = wet_ * (width_ * 0.5f + 0.5f);
        const float wet2 = wet_ * ((1.0f - width_) * 0.5f);

        outL = l * wet1 + r * wet2 + inL * dry_;
        outR = r * wet1 + l * wet2 + inR * dry_;
    }

private:
    std::array<LowpassCombFilter, 8> combL_, combR_;
    std::array<AllpassDelay, 4>      allpassL_, allpassR_;
    float roomSize_ = 0.5f, damping_ = 0.5f, width_ = 1.0f;
    float wet_ = 0.3f, dry_ = 0.7f;
};
```

---

## 46.5 The parameters, and what they physically mean

| Parameter | Implementation | Physical meaning |
|---|---|---|
| **Room size** | Comb feedback gain | Decay time. Larger = longer RT60 |
| **Damping** | Comb low-pass cutoff | Surface and air absorption. More = darker tail |
| **Width** | L/R blend | Stereo spread of the tail |
| **Wet/dry** | Output mix | Distance from the source (Ch 3's direct/reverberant ratio) |
| **Pre-delay** | Delay before the reverb | Distance from the *walls* — see below |

**Room size does not change the delay lengths**, which is a shortcut. In a real room a larger
space means longer gaps between early reflections *and* a longer decay. Freeverb changes only the
decay. That is why its "large hall" setting sounds like a small room with a long tail rather than
a genuinely large space, and it is the algorithm's main limitation.

**Pre-delay is not in Freeverb and should be.** It is a delay between the dry signal and the
reverb input, typically 10–80 ms, and it is one of the most useful controls in cinematic mixing:

| Pre-delay | Perception |
|---|---|
| 0 ms | Source is against the wall; reverb smothers it |
| 20 ms | Natural, moderate room |
| 40–60 ms | Source is close to *you*, room is large behind it |
| 80+ ms | Reverb reads as a distinct echo |

**Longer pre-delay makes the source sound closer while keeping the room large** — because the gap
implies you are near the source and far from the walls. This is Chapter 3's precedence effect
being used deliberately, and it is how film mixers keep dialogue intelligible in a cathedral.

---

## 46.6 What is wrong with Schroeder reverbs

Honest assessment, because it motivates Chapter 47.

**Metallic ringing at long decays.** The comb resonances are still there; at high feedback they
ring audibly. Adding combs helps but does not cure it.

**The tail is not truly dense.** Echo density grows but never reaches the thousands-per-second of
a real hall. On sustained material you can hear the granularity.

**Room size does not change the geometry**, as noted above.

**Poor early reflections.** The all-passes produce diffusion, not the specific reflection pattern
of a real room shape. Schroeder reverbs are good at *tails* and poor at *rooms*.

**No modal behaviour.** A real room has resonant modes at frequencies set by its dimensions
(Chapter 2). A Schroeder reverb has comb resonances set by arbitrary delay lengths. They do not
correspond to anything physical.

**Where it is still the right choice:** CPU-constrained situations (games, embedded), when you
want a *pleasant* rather than *accurate* space, and as a diffusion stage inside a larger design.
Chapter 47's FDN fixes most of these problems for about twice the cost.

---

## 46.7 Tuning a reverb by ear

Since the constants are found by ear, here is how to do it. This is genuinely a skill and the
order matters.

**1. Start with an impulse or a short percussive sound.** A clap, a rim shot, a short noise burst.
Sustained material hides problems.

**2. Set the decay to something long** (RT60 ~4 s) and listen for ringing. If you hear a pitch in
the tail, your delay lengths share common factors or are too close together. Adjust them to be
mutually prime and spread over roughly a 1.5:1 ratio.

**3. Listen for "flutter"** — a rapid, periodic amplitude fluctuation in the tail. This means the
echo density is too low. Add all-pass stages, or nest them.

**4. Set the damping** so the tail's brightness decays naturally. Compare with a real space:
record a handclap in a stairwell and match the way its brightness falls.

**5. Check the onset.** The first 30 ms should not sound like a discrete burst of echoes. If it
does, add a short diffusion stage before the combs.

**6. Check in mono.** If the reverb largely disappears when summed to mono, your two channels are
correlated in a way that cancels — the stereo spread is doing something wrong (Chapter 75).

**7. Check with a full mix.** A reverb that sounds gorgeous on a solo instrument can be a muddy
disaster on a dense arrangement. The high-pass on the reverb input (see Chapter 89) usually fixes
this.

---

## 46.8 Exercises

**46.1** Build a single feedback comb at 30 ms with feedback 0.85. Feed it an impulse and listen.
Describe the coloration.

**46.2** Build four parallel combs at Schroeder's lengths. Compare with one comb. Then use four
combs at lengths that share a common factor (e.g. 1000, 2000, 3000, 4000) and compare.

**46.3** Add two series all-passes after the combs. Count the echoes in the impulse response
before and after.

**46.4** Implement full Freeverb. Verify the delay lengths scale correctly at 48 and 96 kHz.

**46.5** *Deliberate breakage.* Remove the low-pass from inside the combs (damping = 0). Listen to
a long tail. What does it sound like, and why?

**46.6** Add pre-delay. Render the same source at 0, 20, 40 and 80 ms pre-delay with everything
else constant. Which sounds closest? Which sounds like the largest room?

**46.7** Measure the RT60 of your reverb (Chapter 20's function) at room sizes 0.2, 0.5, 0.8 and
0.95. Plot the relationship.

**46.8** Set the room size to 0.99 and listen for metallic ringing. Find the frequency of the ring
and relate it to a comb delay length.

**46.9** Tune a reverb by ear using §46.7's procedure. Spend an hour on it. Compare your tuning
with Freeverb's.

**46.10** Check your reverb in mono. How much level is lost? Where did it go?

---

### Chapter summary

- Reverb must provide: **increasing echo density**, **smooth exponential decay**, **flat overall
  response**, **HF damping over time**, **decorrelated stereo**, and **no audible periodicity**.
- **Schroeder's design**: **parallel feedback combs** (decay and energy) into **series
  all-passes** (diffusion without coloration). Parallel adds density; series **multiplies** it.
- Delay lengths must be **mutually prime** and spread over roughly **1.5:1**, or echoes coincide
  and the tail rings.
- **Freeverb** adds: eight combs, a **low-pass inside each comb's feedback loop** (the thing that
  makes it sound like a room rather than a machine — Chapter 2's air absorption), and **stereo
  decorrelation by offsetting the right channel's delays by 23 samples**.
- **Delay tunings must scale with sample rate** — a very common porting bug.
- **Pre-delay** (10–80 ms) is essential and missing from Freeverb. Longer pre-delay makes the
  source seem **closer** while the room stays **large** — how film mixers keep dialogue clear in
  a cathedral.
- Weaknesses: metallic ringing at long decays, insufficient density, room size that changes only
  the decay rather than the geometry, poor early reflections, and no modal behaviour.
- The constants are **tuned by ear**. Use an impulse or a clap, listen for ringing and flutter,
  match the brightness decay to a real space, and always check in mono and in a full mix.

**Next:** [Chapter 47 — Reverb II: Feedback Delay Networks](47-reverb-fdn.md)
