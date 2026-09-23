# Chapter 62 — MIDI Input

> MIDI is a 1983 protocol running at 31,250 baud that has outlasted everything designed to
> replace it. It is small, ugly in places, and universal. This chapter covers what you need to
> receive it, parse it, and act on it with sample accuracy.

---

## 62.1 The wire format

MIDI messages are one to three bytes. The first is a **status byte** (high bit set); the rest are
**data bytes** (high bit clear).

```
   Status byte:  1sssnnnn      sss = message type, nnnn = channel (0-15)
   Data byte:    0ddddddd      7 bits, so 0-127
```

**Data bytes are 7-bit** — which is why MIDI velocity, controller values and note numbers all
run 0–127. That limitation shapes a great deal of what follows.

### The messages

| Status | Name | Data 1 | Data 2 |
|---|---|---|---|
| `0x8n` | **Note Off** | note (0–127) | velocity |
| `0x9n` | **Note On** | note | velocity (**0 = note off**) |
| `0xAn` | Poly Aftertouch | note | pressure |
| `0xBn` | **Control Change** | controller | value |
| `0xCn` | Program Change | program | — |
| `0xDn` | Channel Aftertouch | pressure | — |
| `0xEn` | **Pitch Bend** | LSB | MSB |
| `0xFn` | System | varies | varies |

**Note On with velocity 0 means Note Off.** This is not a quirk to work around — it is how
"running status" (§62.3) allows a stream of notes without repeating the status byte, and
essentially every device does it. **Handling only `0x8n` means notes hang forever**, and it is
the most common MIDI bug there is.

```cpp
void handleMessage(uint8_t status, uint8_t d1, uint8_t d2)
{
    const uint8_t type    = status & 0xF0;
    const uint8_t channel = status & 0x0F;

    switch (type)
    {
        case 0x90:
            if (d2 > 0) noteOn(channel, d1, d2 / 127.0f);
            else        noteOff(channel, d1);      // velocity 0 = note off
            break;

        case 0x80:
            noteOff(channel, d1);
            break;

        case 0xB0:
            controlChange(channel, d1, d2);
            break;

        case 0xE0:
            // 14-bit, LSB first. Centre is 8192.
            pitchBend(channel, ((d2 << 7) | d1) - 8192);
            break;
    }
}
```

**Pitch bend is the one 14-bit value** in standard MIDI, giving 16,384 steps. It is assembled
LSB-first, and its centre is 8192, not 0.

---

## 62.2 Control Change

The general-purpose controller message, and the one with the most conventions.

| CC | Name | Notes |
|---|---|---|
| 1 | **Modulation wheel** | The most-used expressive control |
| 7 | Channel volume | |
| 10 | Pan | |
| 11 | Expression | A secondary volume, often a pedal |
| **64** | **Sustain pedal** | ≥64 = down, <64 = up |
| 66 | Sostenuto | |
| 67 | Soft pedal | |
| 71 | Resonance | By convention |
| 74 | **Brightness / cutoff** | By convention; the standard filter control |
| 120 | All Sound Off | Immediate, ignores release |
| 121 | Reset All Controllers | |
| **123** | **All Notes Off** | Respects release |

**CC 64 uses a threshold, not a value.** Values 0–63 are "up" and 64–127 are "down". Treating it
as continuous gives you a sustain pedal that half-works.

**CC 120 and 123 are panic messages** and you must implement them. When a MIDI cable is unplugged
mid-note, or a sequencer stops, these are what clears the hanging notes. A synth that ignores
them will eventually be stuck with notes sounding and no way to stop them.

**Handle "All Notes Off" as a release, not a kill** — that is what respecting the envelope means,
and it is why there are two messages.

### High-resolution CC

Some controllers pair a coarse CC (0–31) with a fine one (32–63) to give 14-bit resolution:

```cpp
// CC 1 (mod wheel MSB) and CC 33 (mod wheel LSB)
if (cc < 32)       { coarse_[cc] = value; updateHighRes(cc); }
else if (cc < 64)  { fine_[cc - 32] = value; updateHighRes(cc - 32); }

float highResValue(int cc)
{
    return ((coarse_[cc] << 7) | fine_[cc]) / 16383.0f;
}
```

**7-bit resolution is audibly insufficient** for slow filter sweeps — 128 steps across a
20 Hz–20 kHz range is about 6.5 steps per octave, and you hear the stepping. Either use 14-bit
where available, or smooth heavily (Chapter 61), or both.

---

## 62.3 Running status and parsing a stream

To save bandwidth on a 31,250 baud link, a sender may omit the status byte when it is the same
as the previous message:

```
   90 3C 64      Note On, note 60, velocity 100
   3E 64         (status omitted) Note On, note 62, velocity 100
   40 64         (status omitted) Note On, note 64, velocity 100
```

A parser must remember the last status byte:

```cpp
class MidiParser
{
public:
    void parseByte(uint8_t byte, int sampleOffset)
    {
        if (byte & 0x80)                      // status byte
        {
            if (byte >= 0xF8)                 // real-time: can appear ANYWHERE,
            {                                 // even inside another message
                handleRealTime(byte, sampleOffset);
                return;                       // does NOT reset running status
            }

            if (byte < 0xF0)
                runningStatus_ = byte;        // channel message: remember it
            else
                runningStatus_ = 0;           // system common: clears it

            status_ = byte;
            dataCount_ = 0;
            expected_ = expectedDataBytes(byte);
        }
        else                                   // data byte
        {
            if (status_ == 0)
            {
                if (runningStatus_ == 0) return;   // no context: discard
                status_ = runningStatus_;
                expected_ = expectedDataBytes(status_);
                dataCount_ = 0;
            }

            data_[dataCount_++] = byte;

            if (dataCount_ >= expected_)
            {
                dispatch(status_, data_[0], data_[1], sampleOffset);
                dataCount_ = 0;
                status_ = 0;                   // ready for running status
            }
        }
    }

private:
    uint8_t status_ = 0, runningStatus_ = 0;
    uint8_t data_[2]{};
    int dataCount_ = 0, expected_ = 0;
};
```

**Real-time messages (`0xF8`–`0xFF`) can appear between any two bytes**, including in the middle
of a three-byte message. They must be handled immediately and must **not** disturb the parse
state. This is genuinely fiddly and it is where most hand-written parsers are wrong.

**Most platform APIs give you complete messages**, so you rarely need a byte parser — except when
reading SysEx, MIDI files, or a raw serial stream.

---

## 62.4 Timing

**MIDI events arrive between audio callbacks**, and their timing relative to the audio matters.

```
   callback n              callback n+1
   ├──────────────────────┤├──────────────────────┤
        ▲        ▲                  ▲
       note     note                note
```

**The naive approach** applies all pending events at the start of the next block. That quantises
every event to the block boundary — 11.6 ms at 512 frames, which is audible as timing looseness
and makes fast passages sound sloppy.

**Sample-accurate MIDI** timestamps each event with its offset within the block and splits the
block at each event — the identical structure to Chapter 61's automation splitting:

```cpp
void processBlock(AudioBuffer& buffer, std::vector<TimedMidiEvent>& events)
{
    int position = 0;
    size_t eventIndex = 0;

    while (position < buffer.numFrames())
    {
        int nextEvent = buffer.numFrames();
        if (eventIndex < events.size())
            nextEvent = std::min(events[eventIndex].sampleOffset, buffer.numFrames());

        if (nextEvent > position)
        {
            synth_.process(buffer, position, nextEvent - position);
            position = nextEvent;
        }

        while (eventIndex < events.size()
               && events[eventIndex].sampleOffset <= position)
        {
            handleMessage(events[eventIndex]);
            ++eventIndex;
        }
    }
}
```

**Where the timestamps come from:** the platform API. On Windows, `midiInProc` gives you a
millisecond timestamp; converting to a sample offset means correlating the MIDI clock with the
audio clock, which drifts (Chapter 57's clock problem, again). Plugin hosts do this for you and
hand you sample offsets directly, which is one of several good reasons to write plugins rather
than standalone applications.

---

## 62.5 Receiving MIDI on Windows

```cpp
#include <windows.h>

void CALLBACK midiInCallback(HMIDIIN, UINT msg, DWORD_PTR instance,
                             DWORD_PTR param1, DWORD_PTR param2)
{
    if (msg != MIM_DATA) return;

    auto* engine = reinterpret_cast<Engine*>(instance);

    const uint8_t status = static_cast<uint8_t>(param1 & 0xFF);
    const uint8_t data1  = static_cast<uint8_t>((param1 >> 8)  & 0xFF);
    const uint8_t data2  = static_cast<uint8_t>((param1 >> 16) & 0xFF);
    const DWORD timestampMs = static_cast<DWORD>(param2);

    // THIS IS NOT THE AUDIO THREAD. Push to a lock-free queue
    // (Chapter 60); do not touch the synth directly.
    engine->midiQueue.push({ status, data1, data2, timestampMs });
}

bool openMidiInput(int deviceIndex, Engine* engine)
{
    HMIDIIN handle;
    if (midiInOpen(&handle, static_cast<UINT>(deviceIndex),
                   reinterpret_cast<DWORD_PTR>(midiInCallback),
                   reinterpret_cast<DWORD_PTR>(engine),
                   CALLBACK_FUNCTION) != MMSYSERR_NOERROR)
        return false;

    midiInStart(handle);
    return true;
}
```

**The MIDI callback runs on its own thread**, not the audio thread. Touching synth state from it
is a data race. Push to a lock-free queue and drain it in the audio callback — Chapter 60's
pattern exactly.

**Cross-platform alternatives:** `RtMidi` (small, MIT, matches RtAudio), `libremidi`, or JUCE's
`MidiInput`. All are preferable to writing platform code.

---

## 62.6 MIDI 2.0 and MPE

**MPE (MIDI Polyphonic Expression)** is a convention on top of MIDI 1.0: each note gets its own
channel, so per-note pitch bend, pressure and timbre become possible.

```
   Channel 1:        the "master" channel
   Channels 2-16:    one note each, rotating
```

Per-note pitch bend is the point — it enables continuous pitch gestures on individual notes,
which standard MIDI cannot express. Instruments like the ROLI Seaboard and LinnStrument use it.

**Implementing MPE support** mostly means: allocate voices per channel rather than per note, apply
pitch bend and CC 74 per channel, and respect the configured pitch bend range (usually ±48
semitones rather than ±2).

**MIDI 2.0** (2020) addresses the real limitations: 32-bit resolution instead of 7-bit,
per-note controllers as a first-class feature, bidirectional negotiation, and property exchange.
Adoption has been slow because MIDI 1.0 works and everything supports it.

**Practical position:** support MIDI 1.0 properly, add MPE if your instrument is expressive
enough to benefit, and treat MIDI 2.0 as something to watch.

---

## 62.7 Velocity curves

Raw MIDI velocity 0–127 mapped linearly to amplitude feels wrong, for reasons you already know.

```cpp
float velocityToGain(int velocity, float curve = 2.0f)
{
    const float v = velocity / 127.0f;
    return std::pow(v, curve);
}
```

| Curve | Feel |
|---|---|
| 0.5 | Very sensitive; hard to play quietly |
| 1.0 | Linear — feels unresponsive at the top |
| **2.0** | **Roughly perceptually linear** |
| 3.0 | Requires firm playing |

**Why the exponent:** Chapter 11 established that loudness is roughly a power law. Mapping
velocity to amplitude with an exponent around 2 makes equal velocity steps feel like equal
loudness steps.

**And route velocity to more than amplitude** — Chapter 33's point. Real instruments get brighter
as well as louder when struck harder, so velocity should also open the filter, increase the FM
index, or shorten the attack.

---

## 62.8 Exercises

**62.1** Build a MIDI monitor that prints every incoming message in hex and decoded form. Play a
keyboard and observe.

**62.2** *Deliberate breakage.* Handle only `0x8n` for note off, not velocity-0 note on. Play a
few notes and observe the hanging notes.

**62.3** Implement CC 123 (All Notes Off) and CC 120 (All Sound Off). Verify one respects the
release and the other does not.

**62.4** Implement the running-status parser. Test it with a byte stream that omits status bytes
and includes real-time messages mid-message.

**62.5** Implement sample-accurate event handling. Play a fast trill and compare with block-
quantised handling. Can you hear the difference?

**62.6** Connect MIDI input to Chapter 41's synth through a lock-free queue. Verify with a thread
sanitizer that nothing races.

**62.7** Implement pitch bend with a configurable range. Verify that ±8192 gives exactly the
configured number of semitones.

**62.8** Implement the sustain pedal correctly (Chapter 41's held-by-sustain flag). Test: pedal
down, play and release ten notes, pedal up.

**62.9** Compare velocity curves 1.0, 2.0 and 3.0 by playing the same phrase. Which feels most
natural?

**62.10** Implement high-resolution CC using a coarse/fine pair, and compare a slow filter sweep
at 7-bit and 14-bit resolution.

---

### Chapter summary

- MIDI messages are 1–3 bytes: a **status byte** (high bit set) and **7-bit data bytes**, which
  is why everything is 0–127.
- **Note On with velocity 0 means Note Off.** Handling only `0x8n` makes notes hang, and it is
  the most common MIDI bug.
- **Pitch bend is 14-bit**, assembled LSB-first, centred at 8192.
- CC conventions worth knowing: 1 (mod wheel), **64 (sustain — a threshold at 64, not a
  continuous value)**, 74 (brightness), and **120/123 (panic messages you must implement)**.
- **7-bit CC resolution is audibly insufficient** for slow sweeps. Use 14-bit pairs where
  available, and smooth heavily regardless.
- **Running status** omits repeated status bytes. **Real-time messages can appear between any two
  bytes** and must not disturb the parse state — this is where hand-written parsers go wrong.
- **Sample-accurate MIDI** splits the block at each event, exactly like Chapter 61's automation.
  Block-quantised events are audibly loose on fast passages.
- **The MIDI callback is not the audio thread.** Push to a lock-free queue (Chapter 60).
- **MPE** gives per-note expression by allocating one channel per note. **MIDI 2.0** fixes the
  resolution limits; adoption is slow.
- Map velocity with an **exponent around 2** for perceptual linearity, and route it to
  **brightness as well as loudness**.

**Next:** [Chapter 63 — Optimisation: Profiling, SIMD, and Cache](63-optimisation-and-simd.md)
