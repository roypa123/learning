# Chapter 70 — Algorithmic and Generative Music

> Music that composes itself. This chapter covers the techniques that produce output worth
> listening to, and — more usefully — the constraints that separate generative music from random
> notes.

---

## 70.1 The central problem

Random notes are not music. Fully determined notes are not generative. Everything interesting
lives between, and the design question is always:

> **What is fixed, what is random, and what constrains the randomness?**

The techniques below are all answers to that question.

---

## 70.2 Constrained randomness

The simplest useful approach: random choices from a restricted set.

```cpp
class ScaleConstrainedGenerator
{
public:
    // Scale degrees as semitone offsets from the root.
    void setScale(std::vector<int> intervals, int rootNote)
    {
        scale_ = std::move(intervals);
        root_  = rootNote;
    }

    int nextNote()
    {
        // A random walk, not independent choices: melodies move by steps
        // far more often than they leap.
        const int stepChoices[] = { -2, -1, -1, 0, 1, 1, 2 };
        const int step = stepChoices[randomIndex(7)];

        degree_ = std::clamp(degree_ + step, 0, static_cast<int>(scale_.size()) * 3);

        const int octave = degree_ / static_cast<int>(scale_.size());
        const int inScale = degree_ % static_cast<int>(scale_.size());

        return root_ + scale_[static_cast<size_t>(inScale)] + octave * 12;
    }

private:
    std::vector<int> scale_;
    int root_ = 60, degree_ = 0;
};
```

**The random walk is doing the work**, not the scale. Independent random choices from a scale
still sound random. A walk with small steps produces contour — which is what melody is.

**The weighting matters too.** `{-2,-1,-1,0,1,1,2}` favours steps of one degree, allows repeats,
and permits occasional leaps. That distribution is roughly what real melodies do.

**Useful scales:**

| Scale | Intervals | Character |
|---|---|---|
| Major | 0 2 4 5 7 9 11 | Bright, resolved |
| Natural minor | 0 2 3 5 7 8 10 | Dark, resolved |
| **Dorian** | 0 2 3 5 7 9 10 | Minor but hopeful — very common in film |
| Phrygian | 0 1 3 5 7 8 10 | Dark, Spanish/Middle Eastern |
| **Lydian** | 0 2 4 6 7 9 11 | Wonder, awe — the "magic" scale |
| Mixolydian | 0 2 4 5 7 9 10 | Heroic, folk |
| **Aeolian + raised 4** | 0 2 3 6 7 8 10 | Ominous, unresolved |
| Pentatonic minor | 0 3 5 7 10 | Cannot sound wrong |
| Whole tone | 0 2 4 6 8 10 | **No tonic — floating, dreamlike** |
| Octatonic | 0 1 3 4 6 7 9 10 | Unsettling, symmetrical |

**Lydian and whole tone are the two most useful for cinematic work.** Lydian's raised fourth
produces the "wonder" quality of a great deal of film music. The whole-tone scale has no perfect
fifth and no leading tone, so it has **no tonal centre** — nothing resolves, which is exactly the
feeling of floating or dreaming.

---

## 70.3 Markov chains

Learn the transition probabilities from existing music, then generate from them.

```cpp
class MarkovChain
{
public:
    void train(const std::vector<int>& sequence, int order = 2)
    {
        order_ = order;
        for (size_t i = order; i < sequence.size(); ++i)
        {
            std::vector<int> context(sequence.begin() + i - order,
                                     sequence.begin() + i);
            transitions_[context].push_back(sequence[i]);
        }
    }

    int next(const std::vector<int>& context)
    {
        auto it = transitions_.find(context);
        if (it == transitions_.end() || it->second.empty())
            return fallback();

        const auto& options = it->second;
        return options[randomIndex(options.size())];
    }

private:
    std::map<std::vector<int>, std::vector<int>> transitions_;
    int order_ = 2;
};
```

**The order is the whole trade-off:**

| Order | Behaviour |
|---|---|
| 1 | Almost random; captures only note frequency |
| **2–3** | **Local coherence, genuine novelty** |
| 4–6 | Starts reproducing training phrases |
| 8+ | Essentially replays the training data |

**Order 2–3 is the sweet spot**, and the failure at high orders is instructive: the model has
memorised rather than learned. This is the same overfitting problem that appears throughout
machine learning, visible here with fifteen lines of code.

**The limitation** is that Markov chains have no long-term structure. They produce plausible
local motion and wander aimlessly over minutes — there is no sense of departure and return,
because the model cannot represent anything beyond the last few notes.

---

## 70.4 Generative grammars

Rules that expand symbols into sequences, giving hierarchical structure that Markov chains lack.

```cpp
// A rule set:  PHRASE -> MOTIF MOTIF_VARIED
//              MOTIF  -> note note note rest
//              MOTIF_VARIED -> transpose(MOTIF, +2)

struct Rule
{
    std::string symbol;
    std::vector<std::vector<std::string>> expansions;   // alternatives
};

std::vector<std::string> expand(const std::string& symbol,
                                const std::map<std::string, Rule>& rules,
                                int depth)
{
    if (depth <= 0) return { symbol };

    auto it = rules.find(symbol);
    if (it == rules.end()) return { symbol };        // a terminal

    const auto& options = it->second.expansions;
    const auto& chosen  = options[randomIndex(options.size())];

    std::vector<std::string> result;
    for (const auto& s : chosen)
    {
        auto sub = expand(s, rules, depth - 1);
        result.insert(result.end(), sub.begin(), sub.end());
    }
    return result;
}
```

**Grammars give hierarchy**, which is what produces musical form: a phrase repeats with
variation, sections have relationships, and the whole has shape.

**L-systems** are a related formalism, originally for modelling plant growth, where all symbols
expand simultaneously. They produce self-similar structures and are well suited to generating
material that feels organically related at multiple time scales.

---

## 70.5 Cellular automata and rule-based systems

**Rule 110** and its relatives produce complex, non-repeating patterns from trivially simple
rules:

```cpp
std::vector<bool> stepCellularAutomaton(const std::vector<bool>& state, int rule)
{
    const size_t n = state.size();
    std::vector<bool> next(n);

    for (size_t i = 0; i < n; ++i)
    {
        const bool left   = state[(i + n - 1) % n];
        const bool centre = state[i];
        const bool right  = state[(i + 1) % n];

        const int pattern = (left ? 4 : 0) | (centre ? 2 : 0) | (right ? 1 : 0);
        next[i] = (rule >> pattern) & 1;
    }
    return next;
}
```

**Map cells to pitches or to rhythmic triggers.** Rule 30 produces chaos; Rule 110 produces
structured complexity; Rule 90 produces fractal (Sierpiński) patterns.

**The appeal is that they are deterministic but unpredictable** — the same seed always gives the
same output, but you cannot tell what it will be without running it. That combination is useful:
reproducible for rendering, surprising for composition.

---

## 70.6 Systems music and process pieces

The tradition that produces the most listenable generative music, and it uses the simplest
mechanisms.

**Phasing** (Steve Reich): two identical patterns at slightly different tempos drift apart and
back into alignment.

```cpp
// Two loops, one 0.5% slower. They realign after 200 repetitions.
seqA.setBPM(120.0);
seqB.setBPM(119.4);
```

**The interest is entirely in the drift.** At the start they are unison; as they separate you
hear new composite patterns emerge that neither loop contains; eventually they realign. Nothing
is random, and the result is genuinely absorbing.

**Tintinnabuli** (Arvo Pärt): one voice moves stepwise, a second sounds only notes of the tonic
triad, chosen as the nearest available.

```cpp
int tintinnabuliVoice(int melodyNote, const std::vector<int>& triad, int root)
{
    int best = triad[0] + root;
    int bestDistance = 128;

    for (int t : triad)
        for (int octave = -2; octave <= 2; ++octave)
        {
            const int candidate = root + t + octave * 12;
            const int distance = std::abs(candidate - melodyNote);
            if (distance < bestDistance && candidate < melodyNote)
            {
                bestDistance = distance;
                best = candidate;
            }
        }
    return best;
}
```

**Twenty lines, and the output is unmistakably Pärt.** It is the clearest demonstration available
that a simple rule, chosen well, produces more musical output than an elaborate one chosen
arbitrarily.

**Ambient generative** (Eno's *Music for Airports*): loops of different lengths, which realign
only at their LCM.

```cpp
// Loops of 17.1, 19.7, 23.3, 29.1 and 31.5 seconds.
// The combination repeats after... a very long time.
```

**This is Chapter 68's polyrhythm at a larger scale**, and it is the most reliable technique for
material that must run for hours without becoming tedious.

---

## 70.7 Harmony

Melody generation without harmonic context wanders. Constraining to a chord progression
immediately improves it.

```cpp
struct Chord
{
    int root;
    std::vector<int> intervals;        // e.g. {0, 4, 7} for major
    double durationBeats = 4.0;
};

// Common cinematic progressions, in scale degrees.
const std::vector<std::vector<int>> progressions = {
    { 0, 5, 3, 4 },        // I-vi-IV-V   : classic, resolved
    { 0, 3, 4, 0 },        // I-IV-V-I    : the simplest
    { 5, 3, 0, 4 },        // vi-IV-I-V   : the "sensitive" progression
    { 0, 6, 3, 0 },        // i-VII-IV-i  : modal, heroic
    { 0, 5, 1, 4 },        // i-VI-ii-V   : minor, moving
    { 0, 0, 3, 3 },        // static, two chords -- ominous, unresolved
};
```

**The two-chord static progression is worth noting.** A great deal of tension music alternates
between two chords for minutes. It does not resolve, which is exactly the point — resolution
releases tension, so withholding it sustains.

**Voice leading** — moving each voice by the smallest possible interval between chords — is what
makes a progression sound smooth rather than like a series of unrelated stabs:

```cpp
std::vector<int> voiceLead(const std::vector<int>& previous,
                           const std::vector<int>& targetPitchClasses)
{
    std::vector<int> result;

    for (int prev : previous)
    {
        int best = prev, bestDistance = 128;

        for (int pc : targetPitchClasses)
            for (int octave = -1; octave <= 1; ++octave)
            {
                const int candidate = (prev / 12 + octave) * 12 + pc;
                const int d = std::abs(candidate - prev);
                if (d < bestDistance) { bestDistance = d; best = candidate; }
            }

        result.push_back(best);
    }
    return result;
}
```

**Good voice leading is more audible than good chord choice.** A mediocre progression with smooth
voice leading sounds better than a sophisticated one with every voice leaping.

---

## 70.8 Generative music for cinematic work

The requirements differ from musical generative art.

**1. Must not draw attention.** Film music is mostly background. A generative system that
produces striking material is the wrong tool; one that produces sustained, non-repeating,
unobtrusive material is right.

**2. Must be controllable in intensity.** The scene changes; the music must follow. This means a
parameter — tension, danger, hope — that drives density, register, dissonance and instrumentation
coherently.

```cpp
void setIntensity(double t)     // 0 = calm, 1 = maximum
{
    noteDensity_     = 0.5 + 4.5 * t;        // notes per beat
    registerSpread_  = 12 + 36 * t;          // semitones
    dissonanceAmount_= t * t;                // nonlinear: rises late
    dynamicRange_    = 0.3 + 0.7 * t;
    rhythmicActivity_= t;

    // The scale itself changes with intensity.
    if (t < 0.3)      setScale(kLydian);       // wonder
    else if (t < 0.7) setScale(kAeolian);      // unease
    else              setScale(kOctatonic);    // alarm
}
```

**One input, many coupled outputs** — Chapter 40's parameter-interface principle applied to
music. Designing those couplings is the work.

**3. Must be able to hit marks.** Chapter 67: the picture has hit points. A generative system
must be able to arrive at a cadence, a swell or a silence at a specified time. That means
planning backwards from the target, which is a fundamentally different structure from
step-by-step generation.

**4. Must sustain for a long time without repeating.** Chapter 68's long-cycle polyrhythm and
§70.6's incommensurate loop lengths both solve this.

---

## 70.9 Exercises

**70.1** Build the scale-constrained random walk. Compare it with independent random choices from
the same scale.

**70.2** Render melodies in Lydian, Aeolian and whole tone with identical rhythm. Describe the
emotional difference.

**70.3** Train a Markov chain on a melody you type in. Generate with orders 1, 2, 4 and 8. Where
does it start reproducing the input?

**70.4** Implement the tintinnabuli rule. Generate a two-voice piece and compare with a
harmonically random second voice.

**70.5** Build five loops of incommensurate lengths (17.1, 19.7, 23.3, 29.1, 31.5 s). Render 20
minutes and listen for repetition.

**70.6** Implement Rule 110 and map it to a pentatonic scale. Compare with Rule 30 and Rule 90.

**70.7** Implement voice leading. Render a progression with and without it.

**70.8** Build the intensity parameter from §70.8 with all its couplings. Automate it from 0 to 1
over two minutes.

**70.9** Implement phasing: two identical 8-note patterns at 120 and 119.4 bpm. Render until they
realign.

**70.10** Build a system that must arrive at a cadence at exactly 47.0 seconds. How does planning
backwards change the architecture?

---

### Chapter summary

- The design question is always: **what is fixed, what is random, and what constrains the
  randomness?**
- **A random walk with weighted small steps** produces contour — which is melody. Independent
  random choices from a scale still sound random.
- Scales carry meaning: **Lydian** for wonder, **whole tone** for floating (no tonal centre),
  octatonic for unease.
- **Markov chains** at order **2–3** give local coherence with novelty; higher orders memorise.
  They have **no long-term structure**.
- **Grammars and L-systems** supply the hierarchy Markov chains lack — phrases, variation, form.
- **Cellular automata** are deterministic but unpredictable: reproducible for rendering,
  surprising for composition.
- **Systems music** gives the most listenable results from the simplest rules: **phasing**,
  **tintinnabuli** (twenty lines, unmistakably Pärt), and **incommensurate loop lengths** for
  material that runs for hours.
- **Voice leading is more audible than chord choice.** Move each voice by the smallest interval.
- Cinematic generative music must: **not draw attention**, be **controllable by one intensity
  parameter** driving many coupled outputs, be able to **hit marks** (which requires planning
  backwards), and **sustain without repeating**.

**Next:** [Chapter 71 — Adaptive Music: Layers, Transitions, Stingers](71-adaptive-music.md)
