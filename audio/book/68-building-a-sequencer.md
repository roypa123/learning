# Chapter 68 — Building a Sequencer

> Chapter 67 gave you the clock. This chapter schedules events against it — sample-accurately,
> without allocating, and with the small deviations from the grid that make sequenced music
> sound like it was played rather than computed.

---

## 68.1 The step sequencer

The simplest useful form: a fixed grid of steps, each on or off.

```cpp
struct Step
{
    bool    active    = false;
    int     note      = 60;
    float   velocity  = 0.8f;
    double  lengthBeats = 0.25;       // gate time
    double  microshift  = 0.0;        // deviation from the grid, in beats
    float   probability = 1.0f;       // chance of firing
};

class StepSequencer
{
public:
    void setPattern(std::vector<Step> steps, double stepBeats = 0.25)
    {
        steps_ = std::move(steps);
        stepBeats_ = stepBeats;
    }

    // Called once per block. Fills `events` with sample offsets.
    void process(const Transport& transport, int numFrames,
                 std::vector<TimedEvent>& events)
    {
        if (steps_.empty()) return;

        const double startBeat = transport.positionInBeats();
        const double endBeat   = transport.beatAtOffset(numFrames);

        // Which step indices fall inside this block?
        const long firstStep = static_cast<long>(std::ceil(startBeat / stepBeats_));
        const long lastStep  = static_cast<long>(std::floor(endBeat / stepBeats_));

        for (long s = firstStep; s <= lastStep; ++s)
        {
            const size_t index = static_cast<size_t>(
                ((s % static_cast<long>(steps_.size())) + steps_.size())
                % steps_.size());

            const Step& step = steps_[index];
            if (!step.active) continue;

            if (step.probability < 1.0f
                && rng_.nextFloat() * 0.5f + 0.5f > step.probability)
                continue;

            const double stepBeat = s * stepBeats_ + step.microshift;
            const int offset = transport.offsetForBeat(stepBeat, numFrames);

            if (offset < 0 || offset >= numFrames) continue;

            events.push_back({ offset, TimedEvent::NoteOn, step.note, step.velocity });

            // Schedule the note off. It may land in a LATER block, so it
            // goes into a pending list rather than this block's events.
            pending_.push_back({ stepBeat + step.lengthBeats, step.note });
        }

        // Emit any pending note-offs that fall in this block.
        emitPendingNoteOffs(transport, numFrames, events);
    }

private:
    std::vector<Step>      steps_;
    std::vector<PendingOff> pending_;
    double stepBeats_ = 0.25;
    FastRandom rng_{ 6789 };
};
```

**The `firstStep`/`lastStep` calculation** finds which grid positions fall inside this block. It
handles blocks that contain zero, one or several steps, and it works regardless of block size —
which matters because Chapter 57 established that block size can vary.

**The double modulo** `((s % n) + n) % n` handles negative step indices correctly, which happens
when the transport is rewound past zero. C++'s `%` on negatives returns a negative, and a
negative array index is Chapter 7's worst kind of bug.

**Note-offs must be scheduled separately.** A note starting near the end of a block with a
quarter-beat gate ends in a later block. Keeping a pending list is the only correct approach —
and it must be pre-allocated (Chapter 59).

---

## 68.2 Microtiming

A perfectly quantised sequence sounds mechanical. Human players do not land exactly on the grid,
and the deviations are systematic, not random.

| Technique | Amount | Effect |
|---|---|---|
| **Swing** | 50–70% | Delays even-numbered subdivisions |
| **Push** | −5 to −20 ms | Consistently early — urgency, drive |
| **Lay back** | +5 to +30 ms | Consistently late — relaxed, heavy |
| **Humanise** | ±2 to ±15 ms random | Variation; can sound sloppy |
| **Per-instrument offset** | varies | Bass slightly late, hats slightly early |

```cpp
// Swing: delay the odd-numbered subdivisions.
// 0.5 = straight, 0.667 = triplet swing, 0.75 = hard swing.
double applySwing(long stepIndex, double stepBeats, double swingAmount)
{
    if (stepIndex % 2 == 0) return 0.0;        // downbeats are unmoved

    // The odd step moves from the midpoint toward the next downbeat.
    return stepBeats * (swingAmount - 0.5) * 2.0;
}
```

**Swing amounts:**

| Value | Ratio | Feel |
|---|---|---|
| 0.50 | 1:1 | Straight |
| 0.54 | 1.17:1 | Subtle, "not quite straight" |
| 0.58 | 1.38:1 | Light swing |
| **0.667** | **2:1** | **Triplet swing — jazz, shuffle** |
| 0.75 | 3:1 | Hard swing, dotted feel |

**Per-instrument offsets are more important than random humanisation.** A drummer's kick is not
randomly early or late — it is *consistently* placed relative to the hi-hat in a way that defines
their feel. Random jitter sounds like a bad player; consistent offsets sound like a particular
player.

```cpp
struct TrackFeel
{
    double timingOffsetMs = 0.0;      // consistent push or lay-back
    double jitterMs       = 0.0;      // random variation, small
    float  velocityJitter = 0.0f;     // dynamics vary too
};

// A plausible starting point:
//   kick:   +2 ms, jitter 1 ms      (slightly behind, very steady)
//   snare:  +6 ms, jitter 3 ms      (laid back)
//   hats:   -3 ms, jitter 2 ms      (pushing)
//   bass:   +4 ms, jitter 2 ms      (locked to the kick, slightly behind)
```

**Velocity variation matters as much as timing.** A drum machine with perfect velocity sounds
mechanical even with perfect microtiming. Accenting the downbeats and varying everything else by
±10% does more than any timing adjustment.

---

## 68.3 Polyrhythm and Euclidean patterns

**Euclidean rhythms** distribute `k` hits as evenly as possible across `n` steps, and the
resulting patterns are startlingly musical — they reproduce traditional rhythms from a great many
cultures.

```cpp
std::vector<bool> euclideanRhythm(int hits, int steps)
{
    std::vector<bool> pattern(static_cast<size_t>(steps), false);
    if (hits <= 0 || steps <= 0) return pattern;

    // Bresenham's line algorithm, which is the same problem.
    int bucket = 0;
    for (int i = 0; i < steps; ++i)
    {
        bucket += hits;
        if (bucket >= steps)
        {
            bucket -= steps;
            pattern[static_cast<size_t>(i)] = true;
        }
    }
    return pattern;
}
```

| E(k, n) | Pattern | Where it is from |
|---|---|---|
| E(3,8) | `x..x..x.` | Cuban tresillo; ubiquitous |
| E(5,8) | `x.xx.xx.` | Cuban cinquillo |
| E(2,5) | `x.x..` | Korean, Persian |
| E(3,4) | `x.xx` | Trinidad, Persia |
| E(5,16) | `x..x..x..x..x...` | Bossa nova |
| E(7,16) | `x..x.x.x..x.x.x.` | Brazilian samba |
| E(9,16) | `x.xx.x.x.xx.x.x.` | West African |

**That these fall out of an evenness algorithm is remarkable**, and it makes Euclidean patterns a
genuinely good generative tool: you get musically plausible rhythms from two integers.

**Polyrhythm** is simply two sequences of different lengths running simultaneously:

```cpp
// A 3-step and a 5-step pattern realign every 15 steps.
StepSequencer seqA;  seqA.setPattern(patternOf(3), 0.25);
StepSequencer seqB;  seqB.setPattern(patternOf(5), 0.25);
```

The **cycle length is the least common multiple** of the pattern lengths. Three against five
repeats every fifteen steps; seven against eleven every seventy-seven. Long cycles produce
patterns that feel like they are evolving.

**This is a cheap way to make cinematic ostinatos** that stay interesting for minutes — Chapter
94 uses it.

---

## 68.4 Event scheduling

A general scheduler for events that are not on a grid.

```cpp
struct ScheduledEvent
{
    double beatPosition;
    int    type;
    int    data1, data2;

    bool operator<(const ScheduledEvent& o) const
    { return beatPosition > o.beatPosition; }     // reversed: min-heap
};

class Scheduler
{
public:
    void prepare(size_t maxEvents)
    {
        events_.reserve(maxEvents);               // pre-allocate (Ch 59)
    }

    // Called from the MAIN thread; events reach the audio thread by queue.
    void schedule(const ScheduledEvent& e)
    {
        events_.push_back(e);
        std::push_heap(events_.begin(), events_.end());
    }

    // Called on the AUDIO thread, once per block.
    void collectDue(const Transport& transport, int numFrames,
                    std::vector<TimedEvent>& out)
    {
        const double endBeat = transport.beatAtOffset(numFrames);

        int emitted = 0;
        while (!events_.empty()
               && events_.front().beatPosition < endBeat
               && emitted < kMaxEventsPerBlock)     // BOUNDED (Ch 59)
        {
            const auto& e = events_.front();
            const int offset = transport.offsetForBeat(e.beatPosition, numFrames);

            out.push_back({ std::clamp(offset, 0, numFrames - 1),
                            e.type, e.data1, e.data2 });

            std::pop_heap(events_.begin(), events_.end());
            events_.pop_back();
            ++emitted;
        }
    }

private:
    static constexpr int kMaxEventsPerBlock = 128;
    std::vector<ScheduledEvent> events_;          // a binary heap
};
```

**A binary heap** gives `O(log n)` insertion and `O(1)` access to the earliest event. With
`reserve()` it never allocates, which satisfies Chapter 59.

**The bounded emission count** is Chapter 59's rule about unbounded loops. A pathological pattern
that schedules a thousand events in one block would otherwise blow the deadline; dropping the
excess is the lesser evil.

**`std::clamp(offset, 0, numFrames-1)`** handles events that are slightly in the past — which
happens when the transport jumps, or when an event is scheduled from the main thread just after
the block started. Firing them immediately is better than dropping them.

---

## 68.5 Pattern chaining and song structure

```cpp
struct PatternSlot
{
    int    patternIndex;
    int    repeats = 1;
    double transposeSemitones = 0.0;
    float  velocityScale = 1.0f;
};

class Arrangement
{
public:
    void setSequence(std::vector<PatternSlot> slots) { slots_ = std::move(slots); }

    const PatternSlot* slotAtBar(int bar) const
    {
        int accumulated = 0;
        for (const auto& s : slots_)
        {
            if (bar < accumulated + s.repeats) return &s;
            accumulated += s.repeats;
        }
        return loop_ ? &slots_[0] : nullptr;
    }

private:
    std::vector<PatternSlot> slots_;
    bool loop_ = true;
};
```

**Transposition and velocity scaling per slot** are what turn a handful of patterns into an
arrangement. The same eight-bar pattern at three transpositions with different velocity scaling
is a verse, a chorus and a bridge.

---

## 68.6 Recording and quantisation

```cpp
// Snap a beat position to the nearest grid line, partially.
double quantise(double beats, double gridBeats, double strength = 1.0)
{
    const double nearest = std::round(beats / gridBeats) * gridBeats;
    return beats + (nearest - beats) * strength;   // strength 0 = off, 1 = full
}
```

**Partial quantisation (`strength` around 0.5–0.8) is almost always better than full.** It pulls
the performance toward the grid while preserving the player's feel. Full quantisation removes
exactly the microtiming that §68.2 spends effort putting back.

**Groove templates** extract the timing deviations from one performance and apply them to
another:

```cpp
struct GrooveTemplate
{
    std::vector<double> timingOffsets;    // per step, in beats
    std::vector<float>  velocityScales;   // per step

    double applyTiming(long step, double gridPosition) const
    {
        return gridPosition + timingOffsets[step % timingOffsets.size()];
    }
};
```

This is how a programmed part inherits a real drummer's feel, and it is more effective than any
amount of randomisation.

---

## 68.7 Sequencing for cinematic work

Three techniques that differ from music sequencing.

**1. Ostinatos with long cycles.** A pattern that does not repeat for thirty seconds keeps
tension without becoming boring. Polyrhythm (§68.3) and probability (§68.1) both do this:

```cpp
// A 7-step and an 11-step pattern: a 77-step cycle.
// At sixteenth notes and 120 bpm, that is 19 seconds before it repeats.
```

**2. Accelerating and decelerating patterns.** A sequence whose step rate increases builds
tension mechanically:

```cpp
// The step interval halves over 8 bars: 1/4 -> 1/8 -> 1/16 -> 1/32
double stepBeatsAt(double barPosition, double totalBars)
{
    const double t = barPosition / totalBars;
    return 0.25 * std::pow(0.125, t);      // exponential acceleration
}
```

This is the rhythmic equivalent of Chapter 84's riser, and combining the two is the standard
trailer build.

**3. Sequencing sound effects, not notes.** A sequencer does not have to drive an instrument. A
pattern of impacts, whooshes and hits — placed on a grid derived from the picture (Chapter 67's
`tempoForHitPoint`) — is how a trailer's percussion section is assembled.

**The grid derived from picture is the important part.** The sequence locks to the edit rather
than to an arbitrary tempo, so cuts and hits coincide without anyone consciously noticing.

---

## 68.8 Exercises

**68.1** Build the step sequencer. Verify it fires correctly across block boundaries at several
block sizes.

**68.2** *Deliberate breakage.* Remove the double modulo. Rewind the transport past zero and
observe.

**68.3** Implement note-off scheduling with a pending list. Verify gates longer than one block
work correctly.

**68.4** Implement swing. Render a hi-hat pattern at 0.5, 0.58, 0.667 and 0.75 and compare.

**68.5** Implement per-instrument timing offsets with the values in §68.2. Compare with perfectly
quantised and with random jitter of the same magnitude.

**68.6** Implement Euclidean rhythms. Generate E(3,8), E(5,8) and E(7,16) and verify they match
the table.

**68.7** Build a 3-against-5 polyrhythm. How many steps before it repeats? Verify by listening.

**68.8** Implement the heap scheduler. Verify it never allocates by using Chapter 59's detector.

**68.9** Implement partial quantisation. Record a sloppy performance and apply strengths of 0,
0.5, 0.8 and 1.0. Which sounds best?

**68.10** Build an accelerating pattern from quarter notes to 32nds over 8 bars. Combine it with a
rising filter sweep.

---

### Chapter summary

- A step sequencer computes which grid positions fall inside each block, handling zero, one or
  several — and any block size.
- **Note-offs must be scheduled into a pending list**, because a note can outlast its block.
- **Microtiming is systematic, not random.** Consistent **per-instrument offsets** (kick slightly
  behind, hats pushing) sound like a particular player; random jitter sounds like a bad one.
  **Velocity variation matters as much as timing.**
- **Swing** delays the odd subdivisions: 0.667 is triplet swing, 0.54 is "not quite straight".
- **Euclidean rhythms** distribute `k` hits evenly across `n` steps via Bresenham's algorithm, and
  reproduce traditional rhythms from many cultures. Two integers, musically plausible output.
- **Polyrhythm cycles at the LCM** of the pattern lengths — 7 against 11 repeats every 77 steps,
  which is a cheap way to keep an ostinato alive for minutes.
- Use a **pre-allocated binary heap** for general scheduling, with a **bounded** emission count
  per block (Chapter 59).
- **Partial quantisation (0.5–0.8) beats full**, and **groove templates** transfer a real
  performance's feel to a programmed part.
- Cinematic sequencing: **long polyrhythmic cycles**, **accelerating patterns** as rhythmic
  risers, and **sequencing effects on a grid derived from the picture** so the music locks to the
  edit.

**Next:** [Chapter 69 — Sample Playback and Disk Streaming](69-sample-playback.md)
