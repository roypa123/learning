# Chapter 3 — How Hearing Works

> Still no code. This chapter is where the *cinematic* half of the book secretly begins,
> because cinematic sound is applied psychoacoustics. Everything an audio programmer does
> is ultimately aimed at a listener, and the listener is a very strange instrument.

---

## 3.1 Why a programmer should care about anatomy

You could, in principle, write audio software knowing only the physics of Chapter 2. People
do. Their code is correct and their results are lifeless, and they cannot explain why.

The reason is that the human auditory system is not a measuring instrument. It is an
interpretation engine, tuned by evolution for a specific job: *work out what is happening
around me, right now, especially if it is dangerous*. Everywhere that job differs from faithful
measurement, hearing departs from physics — sometimes drastically. It discards information,
invents information, and weights things in ways no meter would.

Every one of those departures is a lever a sound designer can pull. Here are five that this
chapter will explain, each of which contradicts naïve physics:

1. Doubling a sound's power does **not** make it sound twice as loud. You need roughly **ten
   times** the power for "twice as loud".
2. A loud sound can render a quieter one **completely inaudible** even though the quieter one
   is physically still there, unchanged. This is how MP3 works, and how film mixers stop an
   explosion from burying dialogue.
3. Remove the fundamental frequency from a note entirely and people still hear that pitch.
   This is why a phone speaker can convey a bass line it physically cannot reproduce.
4. Two sounds 5 ms apart are heard as one sound coming from the direction of the first; 50 ms
   apart, as two sounds. The transition is abrupt and it governs how reverb is designed.
5. Frequencies around 3 kHz seem far louder than frequencies at 100 Hz of the same physical
   level — so "make the sound scarier" and "boost 3 kHz" are often the same instruction.

None of those is a fact about air. All of them are facts about you.

---

## 3.2 The journey of a sound into your head

### Outer ear: the pinna and the ear canal

The visible, folded flap of cartilage is the **pinna**. It looks arbitrary. It is not.

Those ridges and valleys reflect incoming sound into the ear canal along multiple paths of
slightly different length. Those paths interfere (Chapter 2's superposition again), producing
notches and peaks in the spectrum — and critically, **the pattern of notches depends on the
direction the sound came from**, particularly its elevation.

Your brain has learned your own pinna's signature over a lifetime. When a sound arrives from
above, it acquires a particular spectral fingerprint; from below, a different one. That is how
you can tell up from down with only two ears, which is otherwise geometrically impossible.

This has an enormous practical consequence for us: **3D audio over headphones works by
artificially applying these direction-dependent spectral fingerprints.** The complete
description of what a direction does to a sound on its way to your eardrum is called a
**Head-Related Transfer Function (HRTF)**, and Chapter 76 implements binaural rendering with
measured HRTF data. It also explains why generic HRTFs never sound perfect: you are wearing
someone else's ears.

The **ear canal** is a tube about 2.5 cm long, closed at one end. From Chapter 2: a closed tube
resonates at a wavelength four times its length. 4 × 2.5 cm = 10 cm, and `f = 343 / 0.10 ≈
3,400 Hz`. Your ear canal is a resonator with a broad gain peak of roughly 10–15 dB centred
around 3 kHz.

That single anatomical accident is one of the most exploited facts in all of audio. It is a
large part of why:

- The ear is most sensitive around 2–4 kHz.
- Babies' cries, screams, and alarms are concentrated there — evolution put the distress signal
  where the amplifier is.
- Boosting 3 kHz makes anything sound closer, more urgent, more present.
- Excessive 3 kHz is "harsh" and fatiguing, because you are hammering a resonant peak.
- Dialogue intelligibility lives in this band, so film mixers protect it ruthlessly: music and
  effects get dipped right there so speech can occupy it. This is **frequency slotting**, and
  Chapter 89 implements it.

### Middle ear: the impedance matcher

The eardrum (tympanic membrane) vibrates with the pressure changes. Attached to it is a chain of
three tiny bones — malleus, incus, stapes, the smallest bones in the body — which transmit that
vibration to a membrane called the oval window.

Why bother with a linkage? Because the inner ear is filled with **fluid**, and sound crossing
from air into water is almost entirely reflected — this is why everything sounds muffled with
your head underwater. Roughly 99.9% of the energy would bounce off. The middle ear solves this
by concentrating the force from the large eardrum onto the much smaller oval window, plus a
small lever action from the bones, giving about 30 dB of gain. It is a mechanical impedance
matching transformer, and without it you would be nearly deaf.

The middle ear also has a protective reflex: loud sounds cause small muscles to tense and stiffen
the chain, reducing transmission by 10–20 dB. Two limitations matter. It takes 50–100 ms to
engage, so it cannot protect you from a gunshot. And it fatigues, so it cannot protect you from
a two-hour concert. This is why hearing damage is so easy to acquire and impossible to undo —
see §3.10, and please read it.

### Inner ear: the cochlea, where the magic is

The **cochlea** is a fluid-filled spiral tube about 35 mm long if you unrolled it, coiled like a
snail. Running down its length is the **basilar membrane**, and this is the component that makes
hearing what it is.

The basilar membrane is not uniform. At the base (near the oval window) it is **narrow and
stiff**; at the apex (the far end of the spiral) it is **wide and floppy**. From Chapter 2,
stiff things resonate at high frequencies, floppy things at low.

So when a vibration enters, it travels along the membrane, and at each point the membrane
responds best to a particular frequency:

```
   base (stiff)                                          apex (floppy)
   |------------------------------------------------------------|
   20 kHz    10k     5k     2k     1k     500   200   100    20 Hz

   A 1 kHz tone makes THIS region peak:            ^^^
   A 5 kHz tone makes THIS region peak:     ^^^
```

**The cochlea performs a mechanical frequency analysis.** It is a bank of about 3,500
overlapping filters, converting a single time-domain pressure signal into a spatial map of
which frequencies are present. It is, quite literally, a biological spectrum analyser — a
real-time, analogue, power-free Fourier-like transform, evolved a few hundred million years
before Fourier.

Sitting on the membrane are roughly 16,000 **hair cells**. When their region of membrane moves,
tiny stereocilia bend, ion channels open, and the cell fires the auditory nerve. Position along
the membrane encodes frequency; firing rate encodes intensity. This place-coding is called
**tonotopic organisation**, and it is preserved all the way up into the auditory cortex — the
brain keeps a frequency map.

Two further facts with large consequences:

**The mapping is logarithmic.** Equal distances along the membrane correspond to equal *ratios*
of frequency, not equal differences. The span 100→200 Hz occupies about as much membrane as
1,000→2,000 Hz. This is the anatomical reason that octaves feel equal, that pitch is
logarithmic, that we use decibels, and that every frequency control in every audio application
you have ever used is a logarithmic slider. When you draw a spectrum with a linear frequency
axis, you are drawing something the ear does not recognise; Chapter 27 always uses log axes,
for this reason.

**Outer hair cells are active amplifiers.** About 12,000 of the 16,000 hair cells do not merely
sense — they contract in response to motion, pumping energy back into the membrane and
sharpening its response, up to 50 dB of gain at low levels. Your cochlea has automatic gain
control built into the hardware. This has two consequences: the ear's frequency resolution and
its compression behaviour both change with level (so "how the ear responds" is not one fixed
curve), and damage to outer hair cells produces both hearing loss *and* poor frequency
discrimination — which is why hearing aids that just amplify are disappointing.

---

## 3.3 Loudness: the ear's response to level

### Loudness is not amplitude

Physical level is measured in pascals or decibels SPL. Perceived loudness is something else, and
the relationship is neither linear nor even fixed.

The rough rule established by psychoacoustics (Stevens' power law, for a 1 kHz tone):

> To make something sound **twice as loud**, you need about **ten times the power**, which is
> **+10 dB**.

Some implications that surprise people:

- +3 dB is a doubling of *power*. It is "just noticeably louder".
- +6 dB is a doubling of *amplitude*. Clearly louder, nowhere near twice as loud.
- +10 dB is about twice as loud perceptually, and it costs ten times the power.
- To make an explosion sound twice as big as a gunshot, you cannot simply turn it up — you run
  out of headroom long before you run out of perceived scale. You must change its *content*:
  more low end, longer decay, more spectral spread. This is the central problem of cinematic
  sound design and Chapter 83 is devoted to it.

### Equal-loudness contours

Sensitivity varies enormously with frequency, and — crucially — the variation itself changes
with level. The **equal-loudness contours** (historically Fletcher–Munson, now standardised as
ISO 226) map out which physical levels sound equally loud at each frequency.

```
  dB SPL
  120 |__                                                    __
      |  ''--..__                                      __..-'
  100 |          ''--..______                  ______.-'        100 phon
      |                      '''----______----'''
   80 |__                                                  __
      |  ''--..__                                    __..-'
   60 |          ''--..______            ______.--'''           60 phon
      |                      '''--____--'''
   40 |  \__                                              __
      |     '--..__                              ___..--'
   20 |            '''---.._____      ______..--'''             20 phon
      |                        '-----'
    0 +--------------------------------------------------------> Hz
      20    50   100   200   500   1k    2k    4k   8k   16k
                                    ^^^^^^^^
                                most sensitive region
```

Read the graph like this: each curve is a set of tones that all *sound* equally loud. Where the
curve is high, you need a lot of physical level to achieve that loudness — i.e. the ear is
insensitive there.

Key readings:

- The dip around 2–4 kHz is the ear-canal resonance. Maximum sensitivity.
- Low frequencies need far more level. At 20 phon (quiet listening), a 50 Hz tone needs about
  30 dB more SPL than a 1 kHz tone to match it.
- **The curves flatten as level rises.** At high levels, bass and treble catch up.

That last point is the important one for anyone who mixes, and it explains three everyday
phenomena:

1. **Music sounds better loud.** At high level the curve is flatter, so you hear more bass and
   treble relative to mids. It is not that loud is objectively better; it is that loud is
   *flatter*.
2. **The "loudness" button** on old hi-fi amplifiers boosted bass and treble at low volume to
   compensate. It was a real compensation curve, not a gimmick.
3. **You must check mixes at multiple volumes.** A mix balanced while loud will sound thin and
   bass-light when played quietly. Cinema mixers work at a calibrated reference level (85 dB
   SPL per channel) precisely so that the equal-loudness situation is known and reproducible in
   every cinema. Chapter 89 covers calibration and why streaming loudness normalisation exists.

### Loudness over time: integration

The ear does not judge loudness instantaneously. It integrates over roughly 100–200 ms. A
5 ms click at full scale sounds much quieter than a 500 ms tone at full scale, even though the
peak is identical.

This is the entire reason peak meters are useless for judging loudness, why the broadcast world
moved to **LUFS** (a time-integrated, frequency-weighted loudness measure), and why heavy
compression makes things sound louder at the same peak level — compression raises the average
without raising the peak, and average is what you perceive. Chapter 50 builds the compressor;
Chapter 89 builds the LUFS meter.

---

## 3.4 Masking: the most exploitable fact in audio

**Masking** is when one sound makes another inaudible.

### Frequency (simultaneous) masking

Play a loud 1 kHz tone. Now play a quiet 1.1 kHz tone at the same time. You will not hear the
second one at all — not "faintly", not "hard to pick out". Gone.

The mechanism is physical. The loud tone excites a *region* of the basilar membrane, not a
single point. A quieter tone whose region overlaps is swamped: the hair cells are already
saturated by the louder signal.

The masking region has a characteristic asymmetric shape:

```
   level
     |          LOUD MASKER
     |              |
     |            .-|-.
     |         .-'  |  '----.____              <- masking threshold
     |      .-'     |            '''--..___
     |   .-'        |                       '''---...
     +---------------------------------------------------> frequency
              (masks little below)   (masks a LOT above)
```

**Masking spreads upward in frequency much more than downward.** A loud low tone masks higher
tones readily; a loud high tone barely masks lower ones. Hence:

- Bass-heavy content eats everything above it. If your mix is muddy, the culprit is almost
  always excessive low-mid energy masking the rest.
- A rumbling low-frequency bed under a scene will bury detail. Film mixers high-pass aggressively
  to keep the low end clear for the things that actually need it.
- In a mix, you do not make something audible only by turning it up; you make room by turning
  down what is masking it. This is the single most useful mixing insight in this book, and it is
  a direct consequence of cochlear mechanics.

### Temporal masking

Masking also works across time, which is stranger:

- **Forward masking**: a loud sound masks quiet sounds for 100–200 ms *after* it. Intuitive
  enough — the system is saturated and recovering.
- **Backward masking**: a loud sound masks quiet sounds for 5–20 ms *before* it. This seems to
  violate causality, and it does not: the louder sound's neural response travels faster through
  the system and catches up with the quieter one.

Backward masking is used deliberately in cinematic sound. You can put a quiet, ugly artefact
immediately before a huge impact and nobody will hear it. More usefully, it explains why
pre-delay on a reverb works, and why a tiny bit of "pre-lap" before a cut is inaudible but
subconsciously prepares the listener.

### Masking is why MP3 exists

Perceptual audio codecs (MP3, AAC, Opus) analyse the signal, compute which parts are masked,
and simply **do not store them**. They also hide quantization noise underneath masking curves.
The result is a tenth of the data with little audible difference — not because the ear is
imprecise, but because we can predict exactly what it will fail to notice.

Chapter 72 makes all of this quantitative with critical bands; Chapter 89 applies it to mixing.

---

## 3.5 Pitch: stranger than you think

Pitch is *mostly* frequency, but the exceptions are illuminating.

### The missing fundamental

Play a sound containing 200, 300, 400, and 500 Hz — but no 100 Hz at all. What pitch do you
hear? **100 Hz.** Your brain sees a harmonic series whose fundamental would be 100 Hz, and
reports that pitch, despite there being no energy at that frequency whatsoever.

This is not an illusion in any dismissible sense; it is how pitch perception works. The auditory
system performs pattern recognition on the harmonic structure, not simple peak-picking.

Consequences:

- Small speakers can convey bass lines they cannot reproduce. Your phone speaker produces
  essentially nothing below 300 Hz, yet you still perceive the bass line — because the harmonics
  are there.
- This is directly exploitable. **Harmonic bass enhancement** (the "MaxxBass" family of
  processors) synthesises harmonics of the bass content so small speakers imply the missing low
  end. Chapter 52 implements it with a waveshaper, and it is one of the highest-value tricks for
  anything that will be heard on laptops or phones.
- In cinematic mixing, the sub-bass and its harmonics do different jobs: the sub is *felt* in a
  cinema and *absent* on a laptop, so the harmonic content has to carry the weight in the fold-
  down mix. Chapter 83 deals with this trade-off explicitly.

### Pitch depends slightly on level

Play a pure low tone and increase its level: it seems to go slightly flat. High tones go
slightly sharp. The effect is small (a few percent) and mostly matters for pure tones, but it
is a reminder that pitch is computed, not measured.

### Pitch resolution is extraordinary

Trained listeners can reliably detect frequency differences of about **0.2%** in the midrange —
around 2 Hz at 1 kHz, roughly 3 cents of a semitone. This is far finer than the raw mechanical
tuning of the basilar membrane would allow, and the brain achieves it partly by using the
*timing* of neural firing (phase locking) in addition to place coding.

Practical upshot: tuning errors matter. Detuning by 5 cents is a perceptible richness; 20 cents
is out of tune; 50 cents is wrong. Chapter 66 gives you the cent as a working unit.

---

## 3.6 Time: how fast is hearing?

The auditory system's temporal resolution is much finer than the visual system's, and finer
than most people assume.

| Task | Resolution |
|---|---|
| Detecting a gap in noise | ~2–3 ms |
| Detecting that two clicks are separate | ~2 ms |
| Interaural time difference (which ear first) | **~10 microseconds** |
| Judging which of two sounds came first | ~20 ms |
| Fusing an echo with the original (precedence) | up to ~30–40 ms |

That 10 microseconds is not a typo. At 44,100 samples per second, one sample is 22.7
microseconds — so your ability to localise sound by arrival time is **finer than one sample
period**. This is not a problem (the brain compares continuous waveforms at the two ears, not
samples), but it tells you that sample-accurate timing between channels genuinely matters, and
it is why sloppy stereo delay handling smears the image audibly.

### The precedence effect (Haas effect)

Play a sound from the left, and the same sound from the right 5 ms later. You do not hear two
sounds; you hear **one** sound, located to the **left** — at the first arrival — even if the
second is up to about 10 dB louder.

The brain suppresses the echo's directional information and fuses it with the original. This is
obviously adaptive: in any real room the first arrival is the true direction and everything
after it is reflections. Without this mechanism, every room would be a directional chaos.

The thresholds, roughly:

- **0–1 ms**: the two are perceived as one sound whose position is pulled between them
  (this is how delay-based stereo widening works).
- **1–30 ms**: fused into one sound, localised at the first arrival, but the later arrival adds
  loudness and spaciousness. This is the Haas zone and it is the basis of reverb's early
  reflections.
- **30–50 ms**: begins to break apart; the sound gets a "doubled" quality.
- **>50 ms**: heard as a distinct echo.

Every reverb design decision in Chapters 46–49 is informed by those numbers, and so is the
classic mixing technique of delaying one channel by 10–20 ms to widen a sound without changing
its apparent location.

---

## 3.7 Localisation: how you know where things are

A preview of Chapter 73, because it is the heart of spatial audio.

Two ears, three-dimensional space. The brain uses four cues:

**1. Interaural Time Difference (ITD).** Sound from the left reaches the left ear up to about
0.7 ms earlier. Dominant below roughly 800 Hz, where the wavelength is bigger than the head and
phase comparison is unambiguous.

**2. Interaural Level Difference (ILD).** Above about 1.5 kHz the head shadows the far ear,
producing level differences of up to 20 dB. Dominant at high frequencies, where wavelengths are
smaller than the head.

Together these two are the **duplex theory**: timing at low frequencies, level at high. Notice
that this falls straight out of the wavelength table in Chapter 2 — the anatomy and the physics
agree.

**3. Spectral cues from the pinna.** ITD and ILD alone cannot distinguish front from back or up
from down — there is a whole "cone of confusion" of directions with identical ITD and ILD. The
pinna's direction-dependent filtering resolves it. This is why elevation perception is the
hardest thing to fake in headphone 3D audio and why front/back confusions are the classic
failure mode of cheap spatialisers.

**4. Head movement.** Rotate your head slightly and the cues change in a way that instantly
disambiguates front from back. This is so powerful that head-tracked binaural audio is
dramatically more convincing than static binaural — often more so than using a better HRTF.
Chapter 76 discusses it, and it is the main reason VR audio feels solid while headphone music
usually does not.

**Distance** is judged separately, from: overall level, the **ratio of direct to reverberant
sound** (the strongest cue in a room, and the one most under your control in code), high-
frequency loss from air absorption, and for very close sources, low-frequency boost from the
near-field proximity effect. Chapter 77 implements all four.

---

## 3.8 Auditory scene analysis: the brain builds objects

Your cochlea delivers one thing: a frequency map that changes over time. Out of that, the brain
reconstructs *discrete sound sources* — a voice, a car, a violin, a door. This is **auditory
scene analysis** (the psychologist Albert Bregman's framework), and it is the perceptual
process that cinematic sound design manipulates most directly.

The brain groups components into one source using heuristics:

- **Common onset.** Frequency components that start at the same moment probably belong together.
  This is very strong. It is also why a sloppy attack on a layered sound design element makes
  the layers fall apart into separate sounds instead of fusing into one object — see Chapter 82,
  where aligning transients to within a millisecond is the difference between "one impact" and
  "several samples playing at once".
- **Harmonicity.** Components in a harmonic series fuse into one note. Inharmonic components
  refuse to fuse, which is why struck metal sounds like several things at once and why horror
  design loves it.
- **Common fate.** Components that move together in frequency or amplitude (the same vibrato,
  the same swell) belong together. This is why applying one shared LFO across layers glues them,
  and why a shared reverb glues a mix.
- **Continuity.** If a tone is interrupted by a burst of noise, you hear it continue *through*
  the noise — the brain fills in the gap. (This is the continuity illusion, and it is a real
  perceptual restoration, not a guess after the fact.)
- **Spatial position.** Components from the same direction group together. This is why moving
  an element in the stereo field can make it audible without changing its level at all.

The practical summary, which is worth writing on a wall:

> **To fuse layers into one sound:** align their onsets tightly, share modulation, share
> reverb, place them in the same position, keep them harmonically related.
>
> **To separate elements in a mix:** offset onsets slightly, differentiate modulation, use
> different spaces, pan apart, separate them in frequency.

Chapter 82 is essentially a long application of those two sentences.

---

## 3.9 What this means for how we write code

Drawing the practical thread together. Because hearing is logarithmic in both level and
frequency:

- **Volume controls must be logarithmic.** A linear fader feels wrong: most of the useful range
  is bunched at the bottom. Code operates on linear amplitude, users think in dB, and you must
  convert. Chapter 11.
- **Frequency controls must be logarithmic.** A linear frequency slider gives you the top
  octave over half its travel and squeezes all the bass into a centimetre.
- **Spectrum displays should use log frequency and dB magnitude**, or they misrepresent what you
  hear. Chapter 27.
- **Interpolate parameters in perceptual units.** Fading between 100 Hz and 1,000 Hz should be
  geometric, not arithmetic; the midpoint should be ~316 Hz, not 550 Hz.
- **Measure loudness with a time-integrated, frequency-weighted meter**, not a peak meter.
  Chapter 89.
- **Design for masking.** Do not fight for a frequency band that something louder already owns.
- **Sample-accurate inter-channel timing matters**, because 10 µs of ITD is audible.
- **Onsets are sacred.** The first 20 ms of a sound carries its identity and its grouping.

And the overarching one, which separates people who make tools from people who make experiences:

> **You are not writing code to produce a correct signal. You are writing code to produce a
> particular experience in a nervous system. When those two goals conflict, the nervous system
> wins.**

A mathematically perfect linear-phase filter can sound worse than a "flawed" analogue-modelled
one. A physically accurate room simulation can be less convincing than a stylised one. A 
faithfully recorded punch sounds like nothing. Physics is the means; perception is the end.

---

## 3.10 Protecting your hearing — please read this

Your hair cells do not regenerate. Mammalian cochlear hair cells, once destroyed, are gone
permanently. Every loud exposure spends a resource you cannot replace, and audio programmers
are at unusual risk because we generate unexpected full-scale noise as a routine part of the
job.

The exposure limits (NIOSH), which are cumulative over a day:

| Level | Safe exposure time |
|---|---|
| 85 dB | 8 hours |
| 88 dB | 4 hours |
| 91 dB | 2 hours |
| 94 dB | 1 hour |
| 100 dB | 15 minutes |
| 103 dB | 7.5 minutes |
| 106 dB | 3.75 minutes |
| 112 dB | under 1 minute |
| 120 dB | seconds |

Every 3 dB halves the safe time. Headphones at maximum on a typical phone reach 100–110 dB.
A full-scale digital square wave through headphones can exceed 120 dB.

**The rules for this book:**

1. **Turn the volume down before running any new program.** Every time. Build the habit now,
   before it is tested.
2. **Never wear headphones while debugging an unstable filter or a feedback loop.** Use
   speakers at low level, or render to a file and inspect the numbers first.
3. **Add a limiter or a hard clamp to any real-time experiment.** Chapter 51 gives you one. It
   costs three lines and it has saved a lot of hearing.
4. **Mix quietly.** 75–85 dB SPL is the professional reference range. Working quietly makes you
   *better* at mixing, not worse, because the equal-loudness curves are less flattering and
   problems hide less.
5. **Take breaks.** Ear fatigue is real; after two hours your judgement is measurably degraded
   and you will make decisions you undo tomorrow.
6. **Tinnitus after a session is a warning, not a badge.** It means you exceeded a limit.

I am not being dramatic. Hearing loss and tinnitus end audio careers, and they arrive
gradually enough that nobody notices the day it happened. The cost of caution is zero.

---

## 3.11 Discussion: the gap between measurement and experience

It is worth dwelling on how deep the gap between physics and perception goes, because it shapes
the whole discipline.

Consider a simple question: "how loud is this sound?" A physicist answers in pascals. An
engineer answers in dB SPL. A broadcaster answers in LUFS. A mixer answers "too loud for the
scene". These are not four levels of rigour on one scale — they are four different questions,
and each is the correct one in its context.

Or: "are these two sounds the same?" Sample-identical files are the same to a computer. A file
and its 320 kbps MP3 are different to a computer and identical to nearly every listener. A
recording and its copy with 0.5 dB more at 3 kHz are almost identical to a computer and
obviously different to a listener. Similarity is defined by the measuring instrument, and our
measuring instrument is a nervous system with opinions.

This is why audio software is full of things that look wrong on paper and work in practice, and
things that measure perfectly and sound terrible. It is also why a certain humility is useful:
when a sound engineer tells you something sounds different and your meter says it does not, the
honest response is to find out which perceptual mechanism is involved — not to assume they are
imagining it, and not to assume they are right either. Roughly half the time there is a real
effect with a real explanation, and finding it teaches you something. The other half is
expectation bias, which is also real and also worth understanding.

A healthy working attitude: **measure everything you can, listen to everything you measure, and
when they disagree, that disagreement is the interesting part.**

---

## 3.12 Exercises

**3.1** Your ear canal resonates near 3 kHz. Using `λ = c/f` and the quarter-wavelength rule for
a tube closed at one end, what canal length would give a resonance at 2.5 kHz instead? Does the
result seem anatomically plausible? (This is roughly why individual sensitivity peaks differ.)

**3.2** A sound is at 60 dB SPL. What level is needed for it to sound "twice as loud"? Four times
as loud?

**3.3** Explain, in terms of basilar membrane excitation, why a loud 200 Hz tone masks a quiet
400 Hz tone far more than a loud 400 Hz tone masks a quiet 200 Hz tone.

**3.4** A sound contains energy at 300, 450, 600, 750 and 900 Hz. What pitch will most listeners
report? Why?

**3.5** You are designing a game. A sound emitter is 3 m to your left. List every cue you would
need to render for it to be convincingly located, and note which of them requires knowing the
listener's head orientation.

**3.6** *Listening.* Play a piece of music at a comfortable level, then at a very quiet level.
Write down what disappears first. Predict, from the equal-loudness contours, what *should*
disappear first, and compare.

**3.7** *Listening.* Find a recording with a prominent bass line. Play it on a phone speaker.
You still hear the bass line. Explain why, and then explain what a processor could do to
strengthen the effect.

**3.8** A dialogue line is being buried by a music cue. Give three different fixes, only one of
which involves changing the dialogue level. Rank them by how natural the result will sound.

---

### Chapter summary

- The pinna filters sound direction-dependently (the HRTF), enabling elevation and front/back
  perception; the ear canal resonates near 3 kHz, which is why that band means "urgent, close,
  present".
- The cochlea is a mechanical, logarithmically-spaced spectrum analyser with active gain
  control. Place along the basilar membrane encodes frequency.
- Loudness is not level: roughly +10 dB for "twice as loud", and the frequency response of
  hearing flattens as level rises (equal-loudness contours). Loudness is integrated over
  ~100–200 ms, so peak meters do not measure it.
- Masking hides quiet sounds near loud ones, spreading upward in frequency far more than
  downward, and extending ~200 ms forward and ~20 ms backward in time. It underlies both
  perceptual codecs and mixing strategy.
- Pitch is computed from harmonic patterns, not measured — hence the missing fundamental, and
  hence harmonic bass enhancement.
- Temporal resolution is very fine (10 µs for interaural timing); the precedence effect fuses
  echoes within ~30 ms into the first arrival, which is the foundation of reverb design.
- The brain builds auditory objects by common onset, harmonicity, common fate, continuity and
  position — the exact levers used to fuse or separate layers in a mix.
- Hearing damage is permanent and easy to cause. Volume down before every run; limiter on every
  experiment.

**Next:** [Chapter 4 — From Air to Numbers: Sampling and Quantization](04-from-air-to-numbers.md)
