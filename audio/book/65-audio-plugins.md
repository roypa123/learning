# Chapter 65 — Audio Plugins: VST3, AU, and CLAP

> A plugin is your DSP in a shared library that a host loads, with a standard interface for
> audio, parameters, MIDI and state. This chapter closes Part V with what the formats are, what
> they all require, and the details that cause most plugin bugs.

---

## 65.1 The formats

| Format | Owner | Platforms | Licence | Notes |
|---|---|---|---|---|
| **VST3** | Steinberg | Win/Mac/Linux | GPLv3 or proprietary | The most widely supported |
| **AU (v2/v3)** | Apple | macOS/iOS only | Free | Required for Logic and GarageBand |
| **AAX** | Avid | Win/Mac | **Requires Avid approval** | Pro Tools only |
| **CLAP** | Bitwig/u-he | All | **MIT — genuinely free** | Modern, growing support |
| **LV2** | Open | Mostly Linux | ISC | The Linux standard |
| VST2 | Steinberg | All | **Discontinued** | No new licences; still ubiquitous |

**CLAP deserves attention.** It was designed recently with knowledge of where VST3 is awkward: it
has clean per-note expression, proper thread-pool support for multi-threaded plugins,
non-destructive parameter modulation, and an MIT licence with no agreement to sign. Bitwig,
Reaper, FL Studio and others support it.

**The practical answer for shipping:** VST3 and AU cover the overwhelming majority of users; add
CLAP because it is easy and free; add AAX only if Pro Tools users are your market.

---

## 65.2 Use a framework

Writing five format implementations is weeks of work that has nothing to do with audio.

| Framework | Licence | Covers |
|---|---|---|
| **JUCE** | GPL or commercial | VST3, AU, AAX, LV2, standalone + GUI + DSP |
| **iPlug2** | Permissive | VST3, AU, AAX, Web |
| **DPF** | ISC | VST, LV2, JACK |
| **nih-plug** | Rust | VST3, CLAP |
| **clap-wrapper** | MIT | Wraps CLAP into VST3/AU |

**JUCE is the default choice** for commercial plugins and has been for fifteen years. Its
licensing is the main consideration: GPLv3 for open-source, or a commercial licence (free below a
revenue threshold, paid above it).

A JUCE plugin's core is small:

```cpp
class MyPlugin : public juce::AudioProcessor
{
public:
    void prepareToPlay(double sampleRate, int maxBlockSize) override
    {
        // ALL allocation happens here.
        engine_.prepare(sampleRate, maxBlockSize);
    }

    void processBlock(juce::AudioBuffer<float>& buffer,
                      juce::MidiBuffer& midi) override
    {
        juce::ScopedNoDenormals noDenormals;        // Chapter 59

        // Sample-accurate MIDI: split the block at each event (Chapter 62).
        int position = 0;
        for (const auto metadata : midi)
        {
            const int offset = metadata.samplePosition;
            if (offset > position)
            {
                engine_.process(buffer, position, offset - position);
                position = offset;
            }
            engine_.handleMidi(metadata.getMessage());
        }
        engine_.process(buffer, position, buffer.getNumSamples() - position);
    }

    void releaseResources() override { engine_.reset(); }

    double getTailLengthSeconds() const override { return 4.0; }   // reverb tail
    int    getLatencySamples()    const          { return engine_.latency(); }

private:
    MyEngine engine_;
};
```

**`ScopedNoDenormals` is JUCE's version of Chapter 59's RAII guard**, and its presence in every
JUCE example tells you how universal that problem is.

---

## 65.3 What every format requires

Regardless of the format, a host needs the same things from you.

### Parameters

Parameters must be **normalised to 0.0–1.0** for automation, with conversions to and from the
real value:

```cpp
class Parameter
{
public:
    // Normalised (0..1) -> real value.
    float denormalise(float n) const
    {
        switch (skew_)
        {
            case Skew::Linear:
                return min_ + n * (max_ - min_);

            case Skew::Logarithmic:
                // For frequency, time, and anything perceived logarithmically
                // (Chapter 11). A linear frequency parameter is unusable.
                return min_ * std::pow(max_ / min_, n);

            case Skew::Exponential:
                return min_ + (max_ - min_) * (std::exp(n * skewFactor_) - 1.0f)
                            / (std::exp(skewFactor_) - 1.0f);
        }
        return min_;
    }

    juce::String getText(float normalised) const
    {
        const float v = denormalise(normalised);
        return juce::String(v, decimals_) + " " + units_;
    }
};
```

**The skew is not cosmetic.** A frequency parameter mapped linearly from 20 Hz to 20 kHz puts
everything below 2 kHz in the bottom 10% of the control — unusable. Logarithmic mapping is
mandatory for frequency, time, and level.

**Parameters must also:**
- Have a **stable ID** that never changes between versions, or saved automation breaks
- Report whether they are automatable, and whether they can be changed while playing
- Provide text display and text parsing
- Notify the host when changed from your own GUI (`beginChangeGesture` / `endChangeGesture`)

### State

```cpp
void getStateInformation(juce::MemoryBlock& destData) override
{
    auto state = parameters_.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());

    xml->setAttribute("version", kStateVersion);       // ALWAYS version it
    copyXmlToBinary(*xml, destData);
}

void setStateInformation(const void* data, int sizeInBytes) override
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (!xml) return;

    const int version = xml->getIntAttribute("version", 1);

    if (version < kStateVersion)
        migrateState(*xml, version);        // handle old sessions

    parameters_.replaceState(juce::ValueTree::fromXml(*xml));
}
```

**Version the state format from version 1.** Chapter 17 said this about presets and it matters
more here: a user's project from two years ago must still open. Adding a parameter without
versioning means old sessions load it as zero, which may be silence or may be a scream.

### Bus layouts

```cpp
bool isBusesLayoutSupported(const BusesLayout& layouts) const override
{
    const auto& out = layouts.getMainOutputChannelSet();

    // Accept mono and stereo only.
    if (out != juce::AudioChannelSet::mono()
     && out != juce::AudioChannelSet::stereo())
        return false;

    // Input must match output for an effect.
    if (layouts.getMainInputChannelSet() != out)
        return false;

    return true;
}
```

**Be honest about what you support.** A plugin that claims to support 7.1 and then processes only
the first two channels produces a silent surround mix — a real and frequently-shipped bug.

### Latency

```cpp
setLatencySamples(engine_.latencySamples());
```

**Report it, and report it again if it changes.** Chapter 64 explained what happens when a plugin
misreports latency: everything summed in parallel with it comb-filters. This is the single most
common cause of "my mix sounds phasey with this plugin".

**If the latency changes** (switching a linear-phase mode on), call `setLatencySamples` again so
the host recompensates.

### Tail length

```cpp
double getTailLengthSeconds() const override { return 4.0; }
```

How long the plugin continues producing sound after input stops. The host uses this to decide how
much extra audio to render when bouncing. A reverb that reports 0 gets its tail cut off at the
end of every export.

---

## 65.4 The threading model

```
   AUDIO THREAD                   MESSAGE THREAD (GUI)
   ────────────                   ────────────────────
   processBlock()                 GUI events
   - Chapter 59's rules apply     - Parameter changes from the UI
   - no allocation                - Repainting
   - no locks                     - File dialogs
   - no GUI access                - Preset loading

                  ▲                        │
                  └──── atomics + ─────────┘
                       lock-free queues
                       (Chapter 60)
```

**The host owns both threads**, and it may call `processBlock` from a different thread than you
expect — some hosts use a thread pool and the audio thread identity changes between calls.

**Never touch the GUI from `processBlock`.** Not to update a meter, not to trigger a repaint. Use
an atomic that a timer on the message thread reads:

```cpp
// Audio thread
currentLevel_.store(blockPeak, std::memory_order_relaxed);

// GUI, on a 30 Hz timer
void timerCallback() { meter_.setLevel(currentLevel_.load()); repaint(); }
```

**`prepareToPlay` can be called repeatedly** — on sample-rate change, buffer-size change, or
simply when the host feels like it. It must be safe to call at any time and must fully
reinitialise.

**`processBlock` may be called with zero samples.** Handle it.

---

## 65.5 Testing

Plugins fail in hosts you do not own, on systems you cannot see. Systematic testing is not
optional.

**pluginval** (free, open source) is the standard automated validator. It checks parameter
handling, state save/restore, bus layouts, threading, and deliberately abuses the plugin in ways
hosts do.

```bash
pluginval --strictness-level 10 --validate MyPlugin.vst3
```

**Level 10 is aggressive** — it randomises parameters during processing, calls methods out of
order, and changes the sample rate mid-stream. Passing it means the plugin is robust.

**Apple's `auval`** is required for AU distribution:

```bash
auval -v aufx Xmpl Manu
```

**Manual testing across hosts** remains necessary, because hosts differ:

| Host | Known for |
|---|---|
| Reaper | Permissive; runs almost anything |
| Ableton Live | Strict about parameter behaviour and state |
| Logic | Strict AU validation |
| Pro Tools | Very strict; AAX only |
| FL Studio | Unusual threading in places |
| Bitwig | Good CLAP and modulation support |

**The test checklist:**

- Load, save, reload a session — is the state identical?
- Automate every parameter through its full range — any clicks? crashes?
- Change the sample rate and buffer size while playing
- Bypass and unbypass repeatedly
- Multiple instances (16+)
- Every supported bus layout
- Offline bounce versus real-time playback — do they match sample-for-sample?
- Denormals: leave it running with silence and watch the CPU

**That last one catches a genuine and common bug.** If CPU rises during silence, Chapter 59's
flush-to-zero is missing.

---

## 65.6 Distribution

Practical realities, briefly.

**Code signing.** macOS requires signing and notarisation or Gatekeeper blocks installation.
Windows requires an EV certificate to avoid SmartScreen warnings. Both cost money annually and
both are effectively mandatory.

**Installers.** Plugins go in standard locations per platform and format:

```
   macOS:   ~/Library/Audio/Plug-Ins/VST3/    (and Components/ for AU)
   Windows: C:\Program Files\Common Files\VST3\
   Linux:   ~/.vst3/
```

**Architecture.** macOS needs universal binaries (x86-64 and arm64) — a single-architecture
plugin fails for half your users.

**Licensing/copy protection** is a business decision with a technical cost. Every scheme is
breakable; the realistic goal is to make casual sharing inconvenient without inconveniencing
paying customers. Heavy-handed protection generates more support burden than it prevents piracy.

---

## 65.7 Exercises

**65.1** Build a minimal JUCE gain plugin. Load it in a host.

**65.2** Add three parameters with linear, logarithmic and exponential skew. Automate each and
confirm the logarithmic one feels right for frequency.

**65.3** Implement state save/restore with versioning. Save a session, add a parameter, bump the
version, and verify the old session still loads.

**65.4** *Deliberate breakage.* Add a lookahead limiter but do not call `setLatencySamples`.
Duplicate the track, bypass the plugin on one copy, and listen to the comb filtering.

**65.5** Add `ScopedNoDenormals` to a reverb plugin. Measure the CPU during silence with and
without.

**65.6** Implement sample-accurate MIDI handling. Compare with block-quantised on a fast trill.

**65.7** Run `pluginval` at strictness 10. Fix everything it reports.

**65.8** Test in three different hosts. Document every difference you find.

**65.9** Compare an offline bounce with a real-time recording of the same material, sample for
sample. Are they identical? If not, why not?

**65.10** Port a plugin from VST3 to CLAP using `clap-wrapper` and compare the two.

---

### Chapter summary

- **VST3** and **AU** cover most users; **CLAP** is modern, MIT-licensed and free to implement;
  **AAX** needs Avid approval; VST2 is discontinued.
- **Use a framework.** JUCE is the default; iPlug2 and nih-plug are alternatives. Writing five
  format backends is not audio work.
- **Parameters** must be normalised 0–1 with **logarithmic skew for frequency, time and level** —
  linear frequency controls are unusable. They need **stable IDs**, text display, and gesture
  notifications.
- **Version the state format from version 1.** A user's two-year-old session must still open.
- **Report latency accurately and update it when it changes** — misreporting causes comb
  filtering in every parallel path, and is the most common source of "this plugin sounds phasey".
- **Report the tail length**, or exports cut off your reverb.
- **Be honest about bus layouts.** Claiming surround support and processing two channels ships a
  silent mix.
- The host owns both threads. **Never touch the GUI from `processBlock`**; use atomics and a
  timer. `prepareToPlay` can be called repeatedly and must fully reinitialise.
- **Validate with `pluginval` at strictness 10** and `auval`, then test in several hosts. **CPU
  rising during silence means missing flush-to-zero.**

---

## Part V is complete

Your DSP now runs in real time: you understand the callback's hard deadline and why the worst
case is what matters, can open a device and handle its negotiations, know the seven real-time
rules and why each exists, can communicate between threads without locks, can smooth every
parameter so nothing clicks, can receive MIDI with sample accuracy, know where to optimise and in
what order, can build and schedule an audio graph with correct latency compensation, and can ship
it as a plugin.

**Part VI** is about time and structure — pitch, tempo, sequencing and adaptive music.

**Next:** [Chapter 66 — Pitch, Tuning Systems, and Cents](66-pitch-and-tuning.md)
