# Chapter 67 — Tempo and Sample-Accurate Timing

> Musical time and sample time are different coordinate systems, and most sequencer bugs are
> conversion errors between them. This chapter establishes the conversions, the accumulation
> traps, and how to detect where the beats are in audio that did not come with a tempo map.

---

## 67.1 The conversions

```cpp
double secondsPerBeat(double bpm)          { return 60.0 / bpm; }
double samplesPerBeat(double bpm, double sr) { return sr * 60.0 / bpm; }
double beatsToSamples(double beats, double bpm, double sr)
                                           { return beats * sr * 60.0 / bpm; }
double samplesToBeats(double samples, double bpm, double sr)
                                           { return samples * bpm / (sr * 60.0); }
```

At 120 bpm, 44.1 kHz: one beat is 0.5 s, or **22,050 samples**.

**Subdivisions**, expressed in beats, because that is the unit that survives tempo changes:

| Name | Beats | At 120 bpm |
|---|---|---|
| Whole | 4.0 | 2000 ms |
| Half | 2.0 | 1000 ms |
| Quarter | 1.0 | 500 ms |
| Dotted quarter | 1.5 | 750 ms |
| Quarter triplet | 0.667 | 333 ms |
| Eighth | 0.5 | 250 ms |
| **Dotted eighth** | **0.75** | **375 ms** |
| Eighth triplet | 0.333 | 167 ms |
| Sixteenth | 0.25 | 125 ms |
| 32nd | 0.125 | 62.5 ms |

```cpp
enum class Division { Whole, Half, Quarter, Eighth, Sixteenth, ThirtySecond };

double divisionInBeats(Division d, bool dotted = false, bool triplet = false)
{
    double beats = 4.0;
    switch (d)
    {
        case Division::Whole:        beats = 4.0;   break;
        case Division::Half:         beats = 2.0;   break;
        case Division::Quarter:      beats = 1.0;   break;
        case Division::Eighth:       beats = 0.5;   break;
        case Division::Sixteenth:    beats = 0.25;  break;
        case Division::ThirtySecond: beats = 0.125; break;
    }

    if (dotted)  beats *= 1.5;         // a dot adds half the value
    if (triplet) beats *= 2.0 / 3.0;   // three in the space of two

    return beats;
}
```

---

## 67.2 The transport

The authoritative clock, and the thing every timed component reads.

```cpp
class Transport
{
public:
    void prepare(double sampleRate) { sr_ = sampleRate; }

    void setBPM(double bpm)
    {
        bpm_ = bpm;
        beatsPerSample_ = bpm / (60.0 * sr_);
    }

    void setTimeSignature(int numerator, int denominator)
    {
        beatsPerBar_ = numerator * 4.0 / denominator;
    }

    void play()  { playing_ = true; }
    void stop()  { playing_ = false; }
    void rewind(){ positionInBeats_ = 0.0; }

    // Advance by one block. Called once per callback.
    void advance(int numFrames)
    {
        if (playing_)
            positionInBeats_ += numFrames * beatsPerSample_;
    }

    double positionInBeats() const   { return positionInBeats_; }
    double positionInSeconds() const { return positionInBeats_ * 60.0 / bpm_; }
    double positionInSamples() const { return positionInSeconds() * sr_; }

    int    bar()      const { return static_cast<int>(positionInBeats_ / beatsPerBar_); }
    double beatInBar() const { return std::fmod(positionInBeats_, beatsPerBar_); }

    // The position of a given sample offset within this block.
    double beatAtOffset(int offset) const
    {
        return positionInBeats_ + offset * beatsPerSample_;
    }

    // Does a grid line of the given division fall inside this block?
    // Returns the sample offset, or -1.
    int nextGridOffset(double divisionBeats, int numFrames) const
    {
        const double start = positionInBeats_;
        const double end   = start + numFrames * beatsPerSample_;

        const double nextGrid = std::ceil(start / divisionBeats) * divisionBeats;

        if (nextGrid >= end) return -1;

        return static_cast<int>((nextGrid - start) / beatsPerSample_);
    }

private:
    double sr_ = kDefaultRate, bpm_ = 120.0;
    double beatsPerSample_ = 120.0 / (60.0 * kDefaultRate);
    double positionInBeats_ = 0.0, beatsPerBar_ = 4.0;
    bool   playing_ = false;
};
```

**Position is stored in beats, not samples.** That way a tempo change does not require rewriting
history — the musical position is unchanged, only the rate of advance.

**`nextGridOffset` is the function that makes sample-accurate sequencing possible.** It answers
"does a sixteenth-note boundary fall within this block, and at which sample?" — which is exactly
what a sequencer needs to schedule events with Chapter 61's block-splitting.

---

## 67.3 The accumulation trap

```cpp
// WRONG: 44,100 additions per second, each rounding.
positionInBeats_ += numFrames * beatsPerSample_;
```

A `double` has ~15 significant digits, and with position values in the thousands the error per
addition is around 10⁻¹². Over a ten-hour session that is about 10⁻⁷ beats — utterly negligible.

**So for a `double` this is fine.** The trap is real in two specific situations:

**1. If you use `float`.** Chapter 6's measurement applies directly: the error after an hour is
audible as drift against other timed material.

**2. If you accumulate in samples and convert.** Rounding the sample position to an integer each
block accumulates an error of up to half a sample per block — 86 times a second, which is 43
samples per second of drift.

**The robust alternative** is to compute the position from an absolute sample counter:

```cpp
// No accumulation error at all.
totalSamplesElapsed_ += numFrames;
positionInBeats_ = totalSamplesElapsed_ * beatsPerSample_;
```

**With tempo changes this needs care**, because `beatsPerSample_` is no longer constant. The
standard solution is a **tempo map**: a list of (sample position, tempo) points, with the beat
position computed by integrating across them.

---

## 67.4 Tempo maps and ramps

Real music changes tempo.

```cpp
struct TempoPoint
{
    double positionInBeats;
    double bpm;
    bool   ramp;          // linear ramp to the next point, or a step
};

class TempoMap
{
public:
    void addPoint(double beats, double bpm, bool ramp = false)
    {
        points_.push_back({ beats, bpm, ramp });
        std::sort(points_.begin(), points_.end(),
                  [](const TempoPoint& a, const TempoPoint& b)
                  { return a.positionInBeats < b.positionInBeats; });
    }

    double bpmAt(double beats) const
    {
        if (points_.empty()) return 120.0;

        size_t i = 0;
        while (i + 1 < points_.size() && points_[i + 1].positionInBeats <= beats)
            ++i;

        if (!points_[i].ramp || i + 1 >= points_.size())
            return points_[i].bpm;

        // Linear ramp in BPM between the two points.
        const double t = (beats - points_[i].positionInBeats)
                       / (points_[i + 1].positionInBeats - points_[i].positionInBeats);

        return points_[i].bpm + t * (points_[i + 1].bpm - points_[i].bpm);
    }

    // Converting beats to seconds with a ramp requires INTEGRATION,
    // because the tempo is changing continuously.
    double beatsToSeconds(double beats) const
    {
        double seconds = 0.0;
        double current = 0.0;
        const double step = 0.01;              // integrate in small steps

        while (current < beats)
        {
            const double chunk = std::min(step, beats - current);
            seconds += chunk * 60.0 / bpmAt(current + chunk * 0.5);
            current += chunk;
        }
        return seconds;
    }

private:
    std::vector<TempoPoint> points_;
};
```

**The integration is the subtle part.** With a constant tempo, `seconds = beats × 60/bpm`. With a
ramp, the tempo differs at every instant, so you must integrate. A closed form exists for a
linear BPM ramp, but numerical integration in small steps is simpler and accurate enough.

**Note that a linear ramp in *BPM* is not a linear ramp in *tempo period*.** Ramping 60→120 bpm
linearly in BPM spends more time at the slow end than ramping linearly in seconds-per-beat would.
Both are used; DAWs differ. Be explicit about which you mean.

---

## 67.5 Synchronising to a host

A plugin does not own the transport; the host does.

```cpp
void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
{
    if (auto* playHead = getPlayHead())
    {
        if (auto position = playHead->getPosition())
        {
            if (auto bpm = position->getBpm())           transport_.setBPM(*bpm);
            if (auto ppq = position->getPpqPosition())   transport_.setPosition(*ppq);

            const bool playing = position->getIsPlaying();

            // Detect a jump: the user moved the playhead.
            if (std::fabs(*ppq - lastPpq_) > 0.1 && playing)
                resetTimedState();           // clear delay lines, reset LFO phases

            lastPpq_ = *ppq;
        }
    }
    // ...
}
```

**Detecting a position jump matters.** When the user drags the playhead, a tempo-synced delay
whose buffer still contains audio from the old position will play it back — which sounds like a
glitch. Detecting the discontinuity and resetting timed state prevents it.

**"PPQ" means pulses per quarter note** and in most APIs is simply the position in quarter notes
as a `double`. The name is historical.

**Freewheeling / offline rendering** is a mode where the host processes faster than real time.
Anything that depends on wall-clock time rather than sample position will be wrong. Use the
transport, never `std::chrono`, for anything musical.

---

## 67.6 Beat detection

Finding the tempo and beat positions in audio that has no tempo map.

**The pipeline:**

```
   audio → onset detection → onset envelope → autocorrelation → tempo
                                                    │
                                                    └──► phase → beat positions
```

**1. Onset detection function.** Measure how much the spectrum changes between STFT frames.
**Spectral flux** — the sum of positive magnitude increases — is the standard:

```cpp
double spectralFlux(const std::vector<double>& current,
                    const std::vector<double>& previous)
{
    double flux = 0.0;
    for (size_t k = 0; k < current.size(); ++k)
    {
        const double diff = current[k] - previous[k];
        if (diff > 0.0) flux += diff;      // only INCREASES count as onsets
    }
    return flux;
}
```

**Only counting increases is the key detail.** A note ending is a decrease and is not an onset;
counting it produces spurious detections at every note release.

**2. Peak-pick the flux** with an adaptive threshold — a local median or moving average plus a
margin — so that quiet passages still yield onsets and loud ones do not over-trigger.

**3. Autocorrelate the onset envelope** (Chapter 21) to find the periodicity. Peaks appear at the
beat period and its multiples.

**4. Resolve the octave ambiguity.** Autocorrelation cannot distinguish 60 bpm from 120 bpm from
240 bpm. Standard heuristics: prefer tempos near 120 bpm (the centre of human preference), check
which candidate's phase aligns with more onsets, and check whether the implied subdivisions are
musically plausible.

**5. Find the phase** — *where* the beats fall — by cross-correlating a pulse train at the
detected tempo against the onset envelope and taking the best alignment.

**Realistic accuracy:** for steady electronic music, 95%+ tempo accuracy. For expressive live
performance with rubato, considerably worse, and the tempo is genuinely varying so a single
number is the wrong answer. This is a well-studied problem where the remaining difficulty is
musical rather than technical.

---

## 67.7 Timing in cinematic work

Film sound has a different relationship with time than music does.

**Hit points.** Specific frames where something must land — a cut, an impact, a reveal. These are
absolute times, not musical ones, and everything must be positioned relative to them.

**The tempo is often derived from the picture**, not the reverse. Find a tempo where the bars
align with the scene's hit points, and the music locks to the edit without anyone noticing why.
This is a standard film-scoring technique and it is arithmetic:

```cpp
// A hit point at 14.3 s. What tempo puts a downbeat there?
// bars × beatsPerBar × 60/bpm = 14.3
double tempoForHitPoint(double seconds, int bars, double beatsPerBar)
{
    return bars * beatsPerBar * 60.0 / seconds;
}
// 7 bars of 4/4 in 14.3 s -> 117.5 bpm
```

**Frame rates matter.** Film is 24 fps, most video 25 or 29.97 (which is genuinely 30000/1001,
not 30). At 24 fps one frame is 41.67 ms — about 1,837 samples at 44.1 kHz and exactly 2,000 at
48 kHz.

**This is why film audio uses 48 kHz** (Chapter 4): whole numbers of samples per frame. At
44.1 kHz, 24 fps gives 1,837.5 samples per frame, and the accumulated half-sample drift is a real
nuisance in sync-critical work.

**Drop-frame timecode** exists because 29.97 fps is not 30: after an hour, 30 fps timecode has
drifted 3.6 seconds ahead of wall-clock time. Drop-frame skips certain frame *numbers* (not
frames) to compensate. It is a source of persistent confusion and Chapter 90 covers it.

---

## 67.8 Exercises

**67.1** Implement the conversions and verify: at 140 bpm and 48 kHz, how many samples is a dotted
eighth?

**67.2** Build the transport. Verify `nextGridOffset` correctly identifies sixteenth-note
boundaries across block boundaries.

**67.3** *Deliberate breakage.* Store the position in `float` and run for an hour of simulated
time. How far has it drifted in samples?

**67.4** Accumulate the position in integer samples with rounding each block. Measure the drift
over an hour.

**67.5** Implement the tempo map with ramps. Verify the integration by checking that a ramp from
120 to 60 bpm over 8 beats takes the correct number of seconds.

**67.6** Compare a linear ramp in BPM with a linear ramp in seconds-per-beat over the same range.
How different are the resulting durations?

**67.7** Implement spectral flux onset detection. Test it on a drum loop and count false positives
and misses.

**67.8** *Deliberate breakage.* Count negative differences as well as positive in the flux. How
many spurious onsets appear?

**67.9** Implement tempo detection by autocorrelating the onset envelope. Test on several tracks
and note the octave errors.

**67.10** Write `tempoForHitPoint` and find three plausible tempos that put a downbeat at 23.5
seconds.

---

### Chapter summary

- Musical time and sample time are different coordinate systems.
  **`samplesPerBeat = sr × 60/bpm`.** Express subdivisions in **beats**, because beats survive
  tempo changes.
- **Store position in beats, not samples**, so tempo changes do not rewrite history.
- **`nextGridOffset`** — does a grid line fall inside this block, and where — is what makes
  sample-accurate sequencing possible.
- Accumulating in `double` is fine; accumulating in **`float` drifts audibly**, and rounding to
  integer samples each block drifts ~43 samples per second. Computing from an absolute sample
  counter avoids both.
- **Tempo ramps require integration** to convert beats to seconds. A linear ramp in BPM is not a
  linear ramp in seconds-per-beat; be explicit about which.
- A plugin reads the host's transport. **Detect position jumps** and reset timed state, or a
  tempo-synced delay replays audio from the old position.
- **Beat detection**: spectral flux (counting **only increases**), adaptive peak-picking,
  autocorrelation for tempo, cross-correlation for phase. Octave ambiguity needs heuristics.
- Cinematic timing: **hit points are absolute**, and the tempo is often **derived from the
  picture** so bars align with the edit. **48 kHz gives whole samples per video frame**, which is
  why film uses it.

**Next:** [Chapter 68 — Building a Sequencer](68-building-a-sequencer.md)
