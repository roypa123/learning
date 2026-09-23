# Chapter 71 — Adaptive Music: Layers, Transitions, Stingers

> Music that responds to what is happening, without sounding like it is being switched. This
> chapter closes Part VI with the techniques games use — and which increasingly appear in
> interactive and generative film work.

---

## 71.1 The problem

Linear music is composed for a known timeline. Interactive music is not: the player might fight
for ten seconds or ten minutes, and the transition to "danger" must happen *now*, not at the next
convenient bar.

The requirements:

| Requirement | Why |
|---|---|
| Respond to state changes | The music must follow the action |
| Transition musically | A hard cut mid-bar sounds like a mistake |
| Never repeat audibly | Players hear the same music for hours |
| Support many states | Explore, combat, stealth, victory, death |
| Low latency where it matters | A stinger on a kill must be immediate |

**These conflict.** Musical transitions want to wait for a bar line; responsiveness wants to
happen now. Every technique below is a different resolution of that tension.

---

## 71.2 Vertical layering

The most-used technique, and the most forgiving.

**Compose several stems that play simultaneously, in sync, and fade layers in and out.**

```
   Layer 4: lead / melody      ────────────▓▓▓▓▓▓▓▓────────
   Layer 3: percussion         ──────▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓──────
   Layer 2: harmony            ──▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓──
   Layer 1: pad / bed          ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓
                               calm ───────► intense ──────►
```

```cpp
class LayeredMusic
{
public:
    void addLayer(std::shared_ptr<SampleData> stem, double threshold, double fadeRange)
    {
        layers_.push_back({ stem, threshold, fadeRange, 0.0f });
    }

    void setIntensity(double intensity)
    {
        for (auto& layer : layers_)
        {
            // Each layer fades in over its own range, so they arrive
            // progressively rather than all at once.
            const double t = (intensity - layer.threshold) / layer.fadeRange;
            layer.targetGain = static_cast<float>(std::clamp(t, 0.0, 1.0));
        }
    }

    void process(AudioBuffer& out, int numFrames)
    {
        for (auto& layer : layers_)
        {
            // Smooth, or intensity changes click (Chapter 61).
            for (int i = 0; i < numFrames; ++i)
            {
                layer.gain += (layer.targetGain - layer.gain) * fadeCoeff_;

                const float s = layer.read();
                out.channel(0)[i] += s * layer.gain;
                out.channel(1)[i] += s * layer.gain;
            }
        }
    }

private:
    struct Layer { std::shared_ptr<SampleData> stem;
                   double threshold, fadeRange;
                   float gain, targetGain; };

    std::vector<Layer> layers_;
    float fadeCoeff_ = 0.0001f;        // ~2 s fade at 48 kHz
};
```

**All layers play continuously**, in sync, whether audible or not. Only the gain changes.

**Advantages:** transitions are instant and always musical, because everything is already in
time. No transition logic at all.

**Disadvantages:** all layers must be the same length and tempo, the harmony cannot change, and
you pay CPU and memory for silent layers.

**Staggered thresholds are the important detail.** If every layer fades in over the same range,
intensity 0.5 gives you every layer at half volume — which sounds like a quiet full arrangement,
not a moderate one. Staggering them means the arrangement genuinely *grows*:

```
   Layer 1 (bed):     threshold 0.0,  range 0.2
   Layer 2 (harmony): threshold 0.15, range 0.25
   Layer 3 (percussion): threshold 0.4, range 0.3
   Layer 4 (lead):    threshold 0.7,  range 0.3
```

---

## 71.3 Horizontal re-sequencing

Different musical sections, switched at musically sensible points.

```cpp
class HorizontalSequencer
{
public:
    enum class SyncPoint { Immediate, NextBeat, NextBar, NextPhrase, EndOfSegment };

    void requestTransition(const std::string& targetSegment, SyncPoint sync)
    {
        pendingTarget_ = targetSegment;
        pendingSync_   = sync;
    }

    void update(const Transport& transport, int numFrames)
    {
        if (pendingTarget_.empty()) return;

        const int offset = findSyncOffset(transport, numFrames, pendingSync_);
        if (offset < 0) return;                 // not in this block

        // Play the transition segment if one is defined for this pair.
        if (auto* t = findTransition(currentSegment_, pendingTarget_))
            queueSegment(t->segment, offset, pendingTarget_);
        else
            queueSegment(pendingTarget_, offset, "");

        pendingTarget_.clear();
    }

private:
    std::string currentSegment_, pendingTarget_;
    SyncPoint   pendingSync_ = SyncPoint::NextBar;
};
```

**The sync point is the whole design decision:**

| Sync | Latency at 120 bpm | Use |
|---|---|---|
| Immediate | 0 | Death, dramatic cuts — accepts the musical break |
| Next beat | 0–500 ms | Percussion changes |
| **Next bar** | **0–2 s** | **The usual choice** |
| Next phrase (4 bars) | 0–8 s | Section changes |
| End of segment | 0–30 s | Only when latency does not matter |

**Transition segments** are short pieces composed specifically to join two states — a two-bar
fill from explore to combat. They are what make horizontal sequencing sound composed rather than
edited.

**Composing them is the expensive part.** With `n` states you potentially need `n²` transitions.
The practical approach is a small set of generic transitions plus specific ones for the pairs
that matter most.

---

## 71.4 Stingers

Short musical events layered over whatever is playing: a kill, a discovery, a door opening.

```cpp
void playStinger(const std::string& name, Transport& transport)
{
    const Stinger& s = stingers_[name];

    int offset = 0;

    if (s.quantise != SyncPoint::Immediate)
        offset = transport.nextGridOffset(s.quantiseBeats, blockSize_);

    // Duck the main music briefly so the stinger cuts through.
    duckAmount_ = s.duckDb;
    duckTimer_  = s.duckDurationSamples;

    triggerSample(s.sample, offset);
}
```

**Two things make stingers work:**

**1. Harmonic compatibility.** A stinger must be in the same key as the underlying music, or in
a neutral key. The usual solution: compose stingers for each key the score uses, or use
percussive/atonal stingers that fit anywhere.

**2. Ducking.** Briefly reducing the underlying music by 3–6 dB lets the stinger read clearly
without raising its level. This is Chapter 51's sidechain, applied musically — and it is the same
insight as Chapter 3's masking: make room rather than competing.

---

## 71.5 Parameter-driven scoring

Instead of discrete states, expose continuous parameters that the game drives:

```cpp
struct MusicParameters
{
    float tension  = 0.0f;      // 0 = safe, 1 = imminent danger
    float combat   = 0.0f;      // 0 = none, 1 = heavy
    float health   = 1.0f;      // 1 = full, 0 = near death
    float progress = 0.0f;      // 0 = start, 1 = objective complete
    float mystery  = 0.0f;      // exploration, discovery
};
```

Each parameter maps to multiple musical elements:

```cpp
void applyParameters(const MusicParameters& p)
{
    // Tension: register and dissonance, not just volume.
    stringsHigh_.setGain(p.tension);
    dissonanceAmount_ = p.tension * p.tension;      // nonlinear, arrives late

    // Combat: percussion and low brass.
    percussion_.setGain(p.combat);
    brassLow_.setGain(p.combat * 0.8f);

    // Health: a heartbeat pulse that speeds up as health drops.
    heartbeat_.setGain(1.0f - p.health);
    heartbeat_.setRate(0.8 + (1.0 - p.health) * 1.4);

    // Mystery: high, sparse, ethereal texture.
    celeste_.setGain(p.mystery);
    reverbSend_ = 0.2f + p.mystery * 0.5f;

    // Tension ALSO reduces the mix's low end, which makes it feel unstable.
    lowShelf_.setGain(-6.0 * p.tension);
}
```

**The low-shelf detail is the kind of thing that makes this work.** Removing low frequencies
makes a mix feel ungrounded and anxious — an effect Chapter 3 would attribute to removing the
physical, felt component of the sound. It is not obvious and it is very effective.

**One parameter driving several coupled elements**, with nonlinear curves where the feeling
demands them. Chapter 40's principle, again.

---

## 71.6 Middleware

Commercial games use dedicated tools rather than hand-written systems.

| Tool | Notes |
|---|---|
| **Wwise** (Audiokinetic) | The industry standard. Deep, complex, free for small projects |
| **FMOD** | More approachable. Free below a revenue threshold |
| **Elias** | Specialised for adaptive music specifically |
| Unity/Unreal built-in | Adequate for simple needs |

**What middleware provides** beyond what you would build:

- A composer-facing editor, so the audio team works without programmers
- Built-in layering, transitions, stingers and parameter mapping
- Platform-specific optimisation and streaming
- Memory budgeting and voice management
- Profiling
- The DSP: reverb, occlusion, spatialisation

**Build your own when:** you need something unusual, licensing is a problem, or the project is
small enough that the integration cost exceeds the benefit.

**Use middleware when:** you are shipping a commercial game. The composer-facing editor alone
justifies it — the alternative is every music change requiring a programmer.

---

## 71.7 Adaptive techniques in linear film

Increasingly relevant, and worth knowing even if your work is linear.

**Interactive trailers and title sequences** respond to input. **VR film** cannot know where the
viewer is looking. **Variable-length cuts** — different edits of the same piece for different
platforms — need music that can stretch and compress musically.

**And the reverse transfer is genuinely useful:** the *technique* of composing in stems that can
be recombined is valuable for linear work too. A cue delivered as five stems can be re-balanced
in the mix without going back to the composer, which is worth a great deal when the edit changes
at the last minute — which it always does.

**Chapter 89 develops this**: delivering stems rather than a stereo mix is now standard practice
in film, precisely because it preserves the flexibility that adaptive music was designed for.

---

## 71.8 Exercises

**71.1** Build the layered music system with four stems. Automate intensity from 0 to 1 over 60
seconds.

**71.2** *Deliberate breakage.* Give every layer the same threshold and fade range. Compare with
staggered thresholds at intensity 0.5.

**71.3** Implement the horizontal sequencer with all five sync points. Trigger a transition at a
random moment and measure the latency for each.

**71.4** Compose two segments and a transition between them. Compare transitioning with and
without it.

**71.5** Implement stingers with ducking. Compare 0, 3 and 6 dB of duck.

**71.6** Build a stinger in a key that clashes with the underlying music. How obvious is it?

**71.7** Implement the parameter-driven system from §71.5. Drive it with a simulated gameplay
sequence.

**71.8** Implement the low-shelf-reduction-with-tension detail. A/B it — how much does it
contribute?

**71.9** Build a heartbeat layer whose rate increases as health drops. Find the rates that feel
calm and panicked.

**71.10** Take a linear cue and split it into five stems. Rebalance them for a different edit
length.

---

### Chapter summary

- Adaptive music must respond immediately *and* transition musically. Every technique is a
  different resolution of that tension.
- **Vertical layering**: stems playing simultaneously in sync, faded by intensity. Transitions
  are free because everything is already in time. **Stagger the layer thresholds**, or intensity
  0.5 gives you a quiet full arrangement rather than a moderate one.
- **Horizontal re-sequencing**: switch between segments at a **sync point**. Next bar is the
  usual compromise. **Transition segments** make it sound composed rather than edited — but `n`
  states potentially need `n²` of them.
- **Stingers** need **harmonic compatibility** and **ducking** (3–6 dB) — Chapter 3's masking
  insight applied musically: make room rather than competing.
- **Parameter-driven scoring** maps continuous inputs to many coupled musical elements with
  nonlinear curves. Details like **reducing the low shelf as tension rises** (which makes a mix
  feel ungrounded) are what make it work.
- **Wwise and FMOD** are the industry standards, and the composer-facing editor alone justifies
  them on a commercial project.
- The technique transfers to linear work: **delivering stems rather than a stereo mix** preserves
  flexibility when the edit changes — which it always does.

---

## Part VI is complete

You can now handle musical time and structure: tuning systems beyond equal temperament,
sample-accurate tempo and transport, sequencing with microtiming that sounds played rather than
programmed, sample playback and streaming at library scale, generative music that is constrained
enough to be musical, and adaptive scoring that responds without sounding switched.

**Part VII is about space** — psychoacoustics, localisation, panning, binaural rendering,
distance and ambisonics. It is where the cinematic material properly begins.

**Next:** [Chapter 72 — Psychoacoustics: Critical Bands, Masking, Loudness](72-psychoacoustics.md)
