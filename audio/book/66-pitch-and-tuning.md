# Chapter 66 — Pitch, Tuning Systems, and Cents

> Chapter 6 gave you `midiToFrequency` and moved on. This chapter explains what that formula is
> a compromise between, why the compromise was necessary, and what you gain by stepping outside
> it — which for cinematic work is more than you might expect.

---

## 66.1 Intervals are ratios

Chapter 2 established that a doubling of frequency is an octave and feels like "the same note".
That generalises: **musical intervals are frequency ratios**, not differences.

The simplest ratios sound the most consonant, and the reason is Chapter 2's harmonic series. Two
notes a perfect fifth apart (3:2) share many harmonics; two notes a tritone apart (45:32) share
almost none. Shared harmonics mean fewer beating partials (Chapter 2's §2.6), and fewer beats
means more consonance.

| Interval | Just ratio | Cents | Equal temperament | Error |
|---|---|---|---|---|
| Unison | 1:1 | 0 | 0 | 0 |
| Minor second | 16:15 | 111.7 | 100 | −11.7 |
| Major second | 9:8 | 203.9 | 200 | −3.9 |
| Minor third | 6:5 | 315.6 | 300 | **−15.6** |
| **Major third** | **5:4** | **386.3** | **400** | **+13.7** |
| Perfect fourth | 4:3 | 498.0 | 500 | +2.0 |
| Tritone | 45:32 | 590.2 | 600 | +9.8 |
| **Perfect fifth** | **3:2** | **702.0** | **700** | **−2.0** |
| Minor sixth | 8:5 | 813.7 | 800 | −13.7 |
| Major sixth | 5:3 | 884.4 | 900 | +15.6 |
| Minor seventh | 16:9 | 996.1 | 1000 | +3.9 |
| Major seventh | 15:8 | 1088.3 | 1100 | +11.7 |
| Octave | 2:1 | 1200 | 1200 | 0 |

**The major third is 13.7 cents sharp in equal temperament.** That is clearly audible — Chapter 3
established a resolution of about 3 cents for trained listeners. Every major chord you have ever
heard on a piano has a noticeably sharp third, and we have collectively decided not to mind.

---

## 66.2 Cents

The universal unit for small pitch differences:

```
   cents = 1200 · log₂(f₂ / f₁)
   ratio = 2^(cents / 1200)
```

100 cents is a semitone; 1200 is an octave.

```cpp
inline double centsToRatio(double cents) { return std::pow(2.0, cents / 1200.0); }
inline double ratioToCents(double ratio) { return 1200.0 * std::log2(ratio); }
```

**Why cents rather than hertz** — the point Chapter 34 made about modulation, restated: a 10 Hz
error at 100 Hz is 165 cents (a wildly wrong note); at 5 kHz it is 3.5 cents (imperceptible).
Cents are perceptually uniform, hertz are not.

**Perceptual thresholds worth knowing:**

| Cents | Perception |
|---|---|
| 1–3 | Inaudible in isolation; detectable by beating against a reference |
| 5 | Just noticeable; a pleasant "thickness" when detuning |
| 10–15 | Clearly detuned but still musical — classic chorus/unison |
| 20 | Out of tune |
| 30+ | Wrong |
| 50 | A quarter tone |

---

## 66.3 Why equal temperament

**Just intonation** uses the pure ratios in §66.1. In one key it is beautiful — chords lock
together with no beating at all.

**The problem is modulation.** Tune a keyboard justly in C, then play in F#, and the intervals
are catastrophically wrong, because the ratios that were exact for C are not for F#.

**The deeper problem is that the maths does not close.** Twelve perfect fifths should return you
to the starting note seven octaves up:

```
   (3/2)^12 = 129.746
   2^7      = 128.0
```

They differ by a ratio of 1.0136 — **23.5 cents**, called the **Pythagorean comma**. You cannot
build a twelve-note scale from pure fifths that returns to its starting point. Something must
give.

**Equal temperament's answer:** make every semitone exactly `2^(1/12)`. Every key is equally
slightly wrong, which means every key is equally usable.

```cpp
double equalTemperament(int midiNote, double referenceA4 = 440.0)
{
    return referenceA4 * std::pow(2.0, (midiNote - 69) / 12.0);
}
```

**What it costs:** the fifth is 2 cents flat (fine), but the major third is 13.7 cents sharp
(audible) and the minor third is 15.6 cents flat. Equal-tempered thirds beat noticeably, which is
why barbershop quartets and string ensembles — who can adjust — do not use equal temperament.

---

## 66.4 Other tuning systems

Worth knowing, because several are directly useful for cinematic work.

**Just intonation.** Pure ratios from a fixed tonic. Perfect in one key.

**Pythagorean.** Built from pure fifths. Good fifths, very sharp thirds (+21.5 cents). Medieval
European music.

**Meantone.** Flattens the fifths slightly to make the thirds pure. Excellent in the central
keys, unusable in remote ones — the famous "wolf fifth" is 35 cents off.

**Well temperament** (the Bach/Werckmeister family). Each key is slightly different but all are
usable, so each key has its own *character*. This is what "The Well-Tempered Clavier" is
demonstrating — not equal temperament, which is often assumed.

**Stretched tuning.** Pianos are tuned with octaves slightly *wider* than 2:1 — about 30 cents by
the top of the keyboard — to match the string inharmonicity from Chapter 36. A piano tuned
mathematically sounds flat in the treble.

```cpp
// The Railsback curve, approximately.
double stretchedPiano(int midiNote)
{
    const double base = equalTemperament(midiNote);
    const double octavesFromMiddle = (midiNote - 60) / 12.0;
    const double stretchCents = 3.0 * octavesFromMiddle * std::fabs(octavesFromMiddle);
    return base * centsToRatio(stretchCents);
}
```

**Non-Western systems:** Indonesian slendro and pelog (5 and 7 unequal steps), Arabic maqam
(quarter tones), Indian shruti (22 microtonal steps). These are not approximations of Western
tuning — they are different systems with their own logic.

**Microtonal / xenharmonic:** 19-EDO, 31-EDO, 53-EDO (equal divisions of the octave). 31-EDO has
famously good thirds; 53-EDO approximates just intonation extremely closely.

---

## 66.5 Tuning tables

A general tuning system is a lookup, not a formula:

```cpp
class TuningTable
{
public:
    void setEqualTemperament(int divisions = 12, double referenceHz = 440.0)
    {
        for (int note = 0; note < 128; ++note)
            frequencies_[note] = referenceHz
                * std::pow(2.0, (note - 69) / static_cast<double>(divisions));
    }

    // A scale given as ratios within one octave, repeating.
    void setScale(const std::vector<double>& ratios, int rootNote,
                  double rootHz)
    {
        const size_t n = ratios.size();

        for (int note = 0; note < 128; ++note)
        {
            const int offset = note - rootNote;
            const int octave = static_cast<int>(std::floor(
                                   static_cast<double>(offset) / n));
            const size_t degree = static_cast<size_t>(offset - octave * static_cast<int>(n));

            frequencies_[note] = rootHz * ratios[degree] * std::pow(2.0, octave);
        }
    }

    double frequency(int note) const
    {
        return frequencies_[std::clamp(note, 0, 127)];
    }

private:
    std::array<double, 128> frequencies_{};
};
```

**The Scala format** (`.scl`) is the standard interchange format for tunings, with thousands of
historical and experimental scales freely available. Supporting it is about forty lines of
parsing and makes an instrument genuinely more interesting.

**MIDI Tuning Standard (MTS)** lets a host send tuning tables to an instrument at runtime, and
**MTS-ESP** is a modern widely-adopted system for sharing tuning between plugins in a session.

---

## 66.6 Pitch detection

Determining the fundamental of a recorded sound. Harder than it appears, mostly because of
Chapter 3's missing fundamental.

**Autocorrelation** (Chapter 21) is the standard starting point: correlate the signal with a
shifted copy of itself and find the first strong peak after zero.

```cpp
double detectPitchAutocorrelation(const std::vector<float>& x, double sr,
                                  double minHz = 50.0, double maxHz = 2000.0)
{
    const size_t minLag = static_cast<size_t>(sr / maxHz);
    const size_t maxLag = static_cast<size_t>(sr / minHz);

    auto r = autocorrelate(x, maxLag + 1);

    // Find the highest peak in the valid lag range.
    size_t bestLag = minLag;
    float  best    = -1.0f;

    for (size_t lag = minLag; lag <= maxLag && lag < r.size(); ++lag)
        if (r[lag] > best) { best = r[lag]; bestLag = lag; }

    // Parabolic interpolation for sub-sample accuracy (Chapter 36).
    if (bestLag > 0 && bestLag + 1 < r.size())
    {
        const double a = r[bestLag - 1], b = r[bestLag], c = r[bestLag + 1];
        const double denom = a - 2.0 * b + c;
        if (std::fabs(denom) > 1e-12)
            return sr / (bestLag + 0.5 * (a - c) / denom);
    }

    return sr / bestLag;
}
```

**The classic failure is octave errors** — reporting half or double the true pitch, because the
autocorrelation has strong peaks at multiples of the period.

**YIN** (de Cheveigné & Kawahara, 2002) fixes this with three refinements: use the *difference*
function rather than autocorrelation, normalise it cumulatively, and apply an absolute threshold.
It is substantially more reliable and is what most modern pitch trackers use.

**Practical guidance:**

- **Autocorrelation/YIN** in the time domain for monophonic material — accurate and low latency
- **FFT peak-picking** with parabolic interpolation for polyphonic estimation
- **Neural approaches** (CREPE and similar) are now the most accurate, at a considerable CPU cost
- **Latency is unavoidable**: you need at least two periods to measure one, so detecting 50 Hz
  requires 40 ms minimum

---

## 66.7 Tuning in cinematic sound

Three places where tuning choices do real work.

**1. Tuning ambience to the music.** A drone, an engine, or a room tone has a pitch. Tuning it to
the key of the cue makes the whole soundscape cohere; leaving it a semitone off makes everything
feel subtly wrong. Many sound designers pitch-shift ambiences specifically to match.

**This is the single most under-used technique in film sound**, and it is nearly free: detect the
ambience's dominant pitch, detect the cue's key, shift by the difference.

**2. Deliberate mistuning for unease.** Chapter 3: inharmonic content cannot be resolved into a
single pitch, which the auditory system experiences as wrongness. Detuning layers by 30–50 cents
— too much to read as chorus, not enough to read as a different note — produces a specific
queasiness. Horror scores use this constantly.

**Quarter-tone clusters** are the extreme version: several pitches 50 cents apart, which the ear
cannot organise at all.

**3. Just intonation for purity.** The opposite move. A chord in pure ratios has no beating at
all, which sounds unnaturally still and clean — useful for anything meant to feel transcendent,
alien or artificial. Equal-tempered chords always beat slightly; removing that beating is
immediately noticeable even to listeners who cannot say why.

```cpp
// A pure major triad: no beating at all.
const double ratios[3] = { 1.0, 5.0/4.0, 3.0/2.0 };
```

Chapter 84 uses both extremes.

---

## 66.8 Exercises

**66.1** Compute the equal-tempered and just frequencies for a C major triad. How many cents apart
is each note?

**66.2** Render both triads and listen. Count the beats per second in the equal-tempered version's
major third.

**66.3** Verify the Pythagorean comma numerically: multiply 3/2 twelve times and compare with
2^7.

**66.4** Implement the tuning table with Scala-style ratio input. Load a just major scale and a
Pythagorean scale and compare.

**66.5** Implement stretched piano tuning. Render an octave at the top of the keyboard with and
without stretch. Which sounds in tune?

**66.6** Implement autocorrelation pitch detection. Test it on sines, sawtooths, a voice and a
piano note. Where does it make octave errors?

**66.7** Add parabolic interpolation and measure the accuracy improvement in cents.

**66.8** Implement 19-EDO and 31-EDO. Render a major triad in each and compare with 12-EDO. Which
has the purest thirds?

**66.9** Take an ambience recording, detect its dominant pitch, and shift it to match a musical
key. Compare the before and after under a chord.

**66.10** Build a quarter-tone cluster of five pitches 50 cents apart. Then build the same five
notes in just intonation. Describe the difference in feeling.

---

### Chapter summary

- **Intervals are ratios.** Simple ratios are consonant because they share harmonics, so fewer
  partials beat.
- **Cents** (`1200·log₂(ratio)`) are the perceptually uniform unit. 3 cents is the detection
  threshold; 5–15 is musical detuning; 20+ is out of tune.
- **Equal temperament is a compromise** forced by the **Pythagorean comma** — twelve pure fifths
  overshoot seven octaves by 23.5 cents, so a twelve-note scale cannot close. ET makes every key
  equally slightly wrong, at the cost of a **+13.7 cent major third**.
- Other systems: just (pure in one key), Pythagorean (pure fifths), meantone (pure thirds, wolf
  fifth), **well temperament** (each key has character — what Bach meant), and **stretched piano
  tuning** to match string inharmonicity.
- Use a **tuning table**, not a formula, so any system works. Scala files and MTS-ESP are the
  interchange standards.
- **Pitch detection** by autocorrelation with parabolic interpolation; **YIN** fixes the octave
  errors. At least two periods of latency is unavoidable.
- Cinematic uses: **tune ambience to the cue's key** (under-used and nearly free), **detune
  30–50 cents for unease** (inharmonicity the ear cannot resolve), and **just intonation for
  unnatural purity** — no beating at all, which reads as transcendent or alien.

**Next:** [Chapter 67 — Tempo and Sample-Accurate Timing](67-tempo-and-timing.md)
