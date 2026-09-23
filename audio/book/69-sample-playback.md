# Chapter 69 — Sample Playback and Disk Streaming

> Playing a recorded sound sounds trivial and is not. This chapter covers the voice architecture,
> looping, multi-sampling, and streaming libraries larger than memory — which is how every
> orchestral library and game audio engine works.

---

## 69.1 The sample voice

```cpp
class SampleVoice
{
public:
    void start(const SampleData* sample, int midiNote, float velocity)
    {
        sample_   = sample;
        position_ = static_cast<double>(sample->startOffset);

        // Playback rate from the pitch difference (Chapter 28).
        const double semitones = midiNote - sample->rootNote;
        rate_ = std::pow(2.0, semitones / 12.0)
              * (sample->sampleRate / hostSampleRate_);   // rate conversion too

        velocity_ = velocity;
        env_.noteOn();
        active_ = true;
    }

    float nextSample()
    {
        if (!active_ || !sample_) return 0.0f;

        // Hermite interpolation: the rate is almost never 1.0 (Chapter 28).
        const float s = interp::hermite(sample_->data, position_);

        position_ += rate_;

        // Loop or end.
        if (sample_->looping && position_ >= sample_->loopEnd)
            position_ -= (sample_->loopEnd - sample_->loopStart);
        else if (position_ >= static_cast<double>(sample_->data.size() - 2))
            active_ = false;

        return s * env_.nextSample() * velocity_;
    }

private:
    const SampleData* sample_ = nullptr;
    double position_ = 0.0, rate_ = 1.0, hostSampleRate_ = kDefaultRate;
    float  velocity_ = 1.0f;
    ADSR   env_;
    bool   active_ = false;
};
```

**Two rate factors multiply**: the pitch shift, and the conversion between the sample's own rate
and the host's. A 44.1 kHz sample played on a 48 kHz host needs a rate of 0.91875 even at unity
pitch — omit it and everything is 8.8% flat, which is Chapter 16's warning made concrete.

**Position is `double`, not `float`.** A 10-minute sample at 44.1 kHz is 26 million samples, and
`float` has only 24 bits of mantissa — so beyond about 16 million, `float` cannot represent
consecutive integers, let alone fractional positions. The playback would quantise and then stop
advancing.

**Hermite interpolation** because the rate is almost never exactly 1.0 (Chapter 28).

---

## 69.2 Looping

To sustain indefinitely, loop a section. Getting it seamless is the difficulty.

**The requirements for a clean loop:**

1. **Amplitude must match** at the loop points, or you get a click
2. **Waveform phase should match**, or you get a click even with matching amplitude
3. **Spectral content should match**, or you hear a timbral jump
4. **Loop at a zero crossing** in the same direction, which helps with 1 and 2

```cpp
// Search for the loop end that best matches the loop start.
size_t findBestLoopPoint(const std::vector<float>& data,
                         size_t loopStart, size_t searchFrom,
                         size_t searchRange, size_t windowSize)
{
    double bestScore = -1e30;
    size_t bestEnd   = searchFrom;

    for (size_t end = searchFrom; end < searchFrom + searchRange; ++end)
    {
        // Correlate the audio BEFORE the loop end against the audio
        // AFTER the loop start -- they must join seamlessly.
        double correlation = 0.0;
        for (size_t i = 0; i < windowSize; ++i)
            correlation += static_cast<double>(data[end - windowSize + i])
                         * static_cast<double>(data[loopStart + i]);

        if (correlation > bestScore) { bestScore = correlation; bestEnd = end; }
    }
    return bestEnd;
}
```

**This is Chapter 21's cross-correlation again** — the same tool as SOLA in Chapter 54, used for
the same reason: find the alignment where two pieces of audio join constructively.

### Crossfade looping

When no good match exists — which is normal for anything with vibrato or evolving timbre —
**crossfade** across the loop point:

```cpp
float readLooped(double position)
{
    const double loopLength = loopEnd_ - loopStart_;

    // Distance into the crossfade region at the end of the loop.
    const double distanceFromEnd = loopEnd_ - position;

    if (distanceFromEnd < crossfadeLength_)
    {
        const double t = 1.0 - distanceFromEnd / crossfadeLength_;

        const float a = interp::hermite(data_, position);
        const float b = interp::hermite(data_, position - loopLength);

        // Equal-power crossfade (Chapter 74), because the two are
        // uncorrelated and a linear fade would dip.
        return static_cast<float>(a * std::cos(t * kHalfPi)
                                + b * std::sin(t * kHalfPi));
    }

    return interp::hermite(data_, position);
}
```

**Equal-power rather than linear** because the two sides of the crossfade are uncorrelated
(Chapter 11), so a linear fade produces a 3 dB dip in the middle — audible as a pulse at the loop
rate.

**Crossfade lengths:** 20–100 ms for sustained tones, 200–500 ms for evolving textures and
ambiences. Longer is smoother and costs more memory in the region.

**Bidirectional (ping-pong) looping** plays forwards then backwards. It is always seamless at the
turnaround — the waveform is continuous by construction — but reversed audio sounds obviously
reversed on anything with a transient. It works for smooth sustained material only.

---

## 69.3 Multi-sampling

One recording stretched across the keyboard sounds wrong, for two reasons: the formants shift
(Chapter 54's chipmunk effect, applying to instruments as much as voices), and real instruments
change timbre with register in ways that pitch shifting cannot reproduce.

**The solution: record many samples and map them.**

```cpp
struct SampleZone
{
    int lowNote = 0, highNote = 127, rootNote = 60;
    int lowVelocity = 0, highVelocity = 127;
    const SampleData* sample = nullptr;
    float gain = 1.0f;
};

const SampleZone* findZone(const std::vector<SampleZone>& zones,
                           int note, int velocity)
{
    for (const auto& z : zones)
        if (note >= z.lowNote && note <= z.highNote
         && velocity >= z.lowVelocity && velocity <= z.highVelocity)
            return &z;
    return nullptr;
}
```

**Mapping density:**

| Spacing | Quality | Library size |
|---|---|---|
| Every semitone | Perfect, no stretching | Enormous |
| **Every minor third (3)** | **Very good — the usual compromise** | Large |
| Every perfect fourth (5) | Acceptable | Moderate |
| Every octave (12) | Audibly stretched | Small |

**Velocity layers** are as important as pitch zones. A piano struck softly is not a quiet loud
note — it has a completely different spectrum. Four to eight velocity layers is typical; serious
libraries use sixteen or more.

**Round-robin** cycles between several recordings of the same note to avoid the **machine-gun
effect** — the unmistakable sound of the identical sample repeating:

```cpp
const SampleData* nextRoundRobin(ZoneGroup& group)
{
    const auto* s = group.samples[group.nextIndex];
    group.nextIndex = (group.nextIndex + 1) % group.samples.size();
    return s;
}
```

**Random round-robin is worse than cyclic** for small counts, because randomness produces
repeats. Cycling guarantees maximum spacing between reuses. Better still: cycle but exclude the
most recently used.

**Crossfading between velocity layers** avoids an audible step at the layer boundary — a note at
velocity 63 and one at 64 should not sound like different instruments.

---

## 69.4 Disk streaming

An orchestral library is hundreds of gigabytes. It cannot be resident.

**The architecture** (Chapter 60's ring buffer, applied):

```
   Each active voice has a ring buffer.
   A background thread keeps them topped up.
   The audio thread only reads.

   DISK THREAD              RING BUFFERS           AUDIO THREAD
   ───────────              ────────────           ────────────
   read chunks    ──push──►  [====....]  ──pop──►  play
```

```cpp
class StreamingVoice
{
public:
    void start(const StreamingSample* sample, int note, float velocity)
    {
        sample_ = sample;

        // The first N samples are held in RAM so playback can begin
        // IMMEDIATELY. Disk latency is far too long to wait for.
        preloadPosition_ = 0;
        streamPosition_  = sample->preloadSize;

        ring_.clear();
        needsData_.store(true, std::memory_order_release);

        active_ = true;
    }

    float nextSample()
    {
        if (!active_) return 0.0f;

        float s = 0.0f;

        if (preloadPosition_ < sample_->preloadSize)
        {
            // Still in the preloaded region: read directly from RAM.
            s = sample_->preload[preloadPosition_++];
        }
        else if (!ring_.pop(s))
        {
            // Underrun. Output silence rather than blocking (Chapter 60).
            underruns_.fetch_add(1, std::memory_order_relaxed);
            s = 0.0f;
        }

        // Signal the disk thread when the buffer drops below half.
        if (ring_.sizeApprox() < ring_.capacity() / 2)
            needsData_.store(true, std::memory_order_release);

        return s;
    }

private:
    SPSCRingBuffer<float, 65536> ring_;
    std::atomic<bool> needsData_{ false };
    std::atomic<int>  underruns_{ 0 };
    size_t preloadPosition_ = 0, streamPosition_ = 0;
};
```

**The preload buffer is the essential trick.** Disk latency is 1–10 ms on an SSD and up to
100 ms on a spinning disk — far longer than a note-on can wait. Keeping the first 50–200 ms of
every sample in RAM means playback starts instantly while the streaming catches up.

**Preload size arithmetic:** 100 ms at 48 kHz stereo 24-bit is 28.8 KB per sample. A library with
10,000 samples needs 288 MB of preload — which is why large libraries have noticeable load times
and substantial RAM requirements even though they stream.

**Sizing the ring buffer:** it must cover the worst-case disk latency plus the scheduling jitter
of the disk thread. Two to five seconds is typical. Memory is cheap; an underrun is not.

**Prioritise refills by urgency**, not by request order — a voice whose buffer is 10% full needs
data before one at 60%:

```cpp
std::sort(pending.begin(), pending.end(),
          [](const Request& a, const Request& b)
          { return a.bufferFillRatio < b.bufferFillRatio; });
```

---

## 69.5 Memory management

**Formats and their cost:**

| Format | Bytes/sample | Notes |
|---|---|---|
| 32-bit float | 4 | No conversion needed; largest |
| 24-bit int | 3 | The recording standard; needs conversion |
| **16-bit int** | **2** | Fine for most sample content |
| FLAC | ~1.2 | Lossless, needs decoding — CPU during streaming |
| Opus/Vorbis | ~0.3 | Lossy; acceptable for many game assets |

**For streaming, decoding cost matters as much as size.** FLAC halves your disk bandwidth but
costs CPU on the disk thread. For a spinning disk that is a good trade; for an NVMe SSD it
usually is not.

**Reference counting for shared samples:**

```cpp
class SampleManager
{
public:
    std::shared_ptr<SampleData> load(const std::string& path)
    {
        auto it = cache_.find(path);
        if (it != cache_.end())
            if (auto existing = it->second.lock())
                return existing;           // already loaded

        auto data = std::make_shared<SampleData>(readWav(path));
        cache_[path] = data;
        return data;
    }

private:
    std::unordered_map<std::string, std::weak_ptr<SampleData>> cache_;
};
```

**`weak_ptr` in the cache** means the sample is freed when the last voice using it finishes,
without the cache keeping it alive forever.

**But the audio thread must not touch a `shared_ptr` refcount** — the atomic increment is
contended and `shared_ptr` destruction can free memory (Chapter 59). The pattern is: the main
thread holds the `shared_ptr` and passes a raw pointer to the voice, guaranteeing the lifetime
outlasts the voice by Chapter 60's deferred-deletion mechanism.

---

## 69.6 Sample playback for cinematic work

**Layering at the voice level.** A single "impact" trigger fires five samples simultaneously with
individual pitch, timing and level offsets. Chapter 82 develops this; the sampler must support
one-to-many triggering.

**Reverse playback** is trivial (negative rate) and is a staple: reversed cymbals, reversed piano
and reversed vocal are all standard pre-impact builds.

```cpp
rate_ = -std::pow(2.0, semitones / 12.0);
position_ = static_cast<double>(sample_->data.size() - 2);
```

**Extreme pitch shifting.** A sample at rate 0.1 is ten times slower and two octaves plus a third
lower, with all the timbral consequences. Applied to organic sources — animal calls, metal, ice —
it produces the enormous, unplaceable textures that cinematic sound relies on. The interpolation
quality matters enormously here; Chapter 28's sinc is worth the cost.

**Randomisation is mandatory**, not optional. Chapter 40's rule: any repeated identical sound is
recognised instantly. Every triggered sample should have randomised pitch (±20 cents), level
(±2 dB), start offset (±5 ms) and — where available — round-robin selection.

**Sample start offset randomisation** deserves emphasis: it is the cheapest way to make a
repeated sample sound different, because it changes which part of the attack transient you hear,
and Chapter 3 established that the attack carries most of the identity.

---

## 69.7 Exercises

**69.1** Build the sample voice with Hermite interpolation. Load a WAV and play it at several
pitches.

**69.2** *Deliberate breakage.* Omit the sample-rate conversion factor. Play a 44.1 kHz sample on
a 48 kHz host and measure the pitch error in cents.

**69.3** Use `float` for the playback position and play a 10-minute sample. Where does it break?

**69.4** Implement loop-point search by correlation. Test it on a sustained note and compare with
an arbitrary loop point.

**69.5** Implement crossfade looping. Compare linear and equal-power crossfades on a loop of
uncorrelated material — can you hear the dip?

**69.6** Build a multi-sampled instrument with zones every 3 semitones. Compare with a single
sample stretched across the same range.

**69.7** Implement round-robin. Trigger the same note 20 times rapidly with and without it.

**69.8** Implement random round-robin and count how often the same sample repeats consecutively.
Compare with cyclic.

**69.9** Build the streaming voice with preload. Reduce the preload to zero and measure the delay
before sound appears.

**69.10** Reduce the streaming ring buffer until you get underruns. What is the minimum safe size
on your system with 32 simultaneous voices?

---

### Chapter summary

- A sample voice multiplies **two rate factors**: the pitch shift and the sample-rate conversion.
  Omitting the second makes everything 8.8% flat on a 48 kHz host.
- **Position must be `double`** — `float` cannot represent consecutive integers beyond 16 million
  samples.
- Clean loops need matching **amplitude, phase and spectrum**. Find the best point by
  **cross-correlation** (Chapter 21 again), or **crossfade** with an **equal-power** curve — a
  linear crossfade of uncorrelated material dips 3 dB, audible as a pulse at the loop rate.
- **Multi-sampling**: zones every 3 semitones is the usual compromise, with 4–8 **velocity
  layers** (a soft piano note is not a quiet loud one) and **cyclic round-robin** to defeat the
  machine-gun effect. Cyclic beats random, which produces repeats.
- **Streaming** uses a per-voice ring buffer filled by a background thread, with a **RAM preload
  of the first 50–200 ms** so playback starts instantly. Prioritise refills by how empty each
  buffer is.
- Cache samples with **`weak_ptr`**, but never let the audio thread touch a `shared_ptr` refcount
  — pass raw pointers whose lifetime the main thread guarantees.
- Cinematic use: **one-to-many layered triggering**, **reverse playback**, **extreme pitch
  shifting** of organic sources, and **mandatory randomisation** of pitch, level, start offset
  and round-robin.

**Next:** [Chapter 70 — Algorithmic and Generative Music](70-generative-music.md)
