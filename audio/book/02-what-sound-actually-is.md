# Chapter 2 — What Sound Actually Is

> No code in this chapter. Every concept here becomes a variable name later, so the time you
> spend now is repaid with interest.

---

## 2.1 Start with a single molecule

Forget music, forget speakers, forget computers. Picture a completely still room. The air in
it is not still at all — it is about 10²⁵ molecules per cubic metre, each one flying around at
roughly 500 metres per second, colliding with its neighbours billions of times a second. This
chaos is what we call "air at rest". It has an average pressure: at sea level, about 101,325
pascals (Pa), which is the weight of the entire atmosphere pressing down on every square metre.

Now put a drum in the room and hit it.

The drumhead moves *outwards*. In doing so it shoves the molecules immediately in front of it
slightly closer together than average. Not much — we will see in a moment just how absurdly
little — but measurably. Those crowded molecules now collide with their neighbours a little
more often than usual, which crowds *them*, and so on. A region of slightly-above-average
pressure travels outward from the drum.

Then the drumhead swings back *inwards*, leaving behind slightly more space than average. The
molecules there spread out to fill it, which pulls on their neighbours, and a region of
slightly-below-average pressure follows the first one outward.

The drumhead keeps oscillating. What travels away from it is an alternating pattern:

```
   pressure
   above
   average     ###         ###         ###
              #   #       #   #       #   #
   average ---#-----#-----#-----#-----#-----#---->  distance from drum
                     #   #       #   #       #
   below              ###         ###         ###
   average
             <---- one cycle ---->
```

That travelling pattern of pressure variation is **sound**. That is the whole of it. Everything
else in this book is bookkeeping about the shape of that pattern.

### The most important thing to get straight immediately

**The air does not travel from the drum to your ear.** This is the single most common
misconception about sound, and it will confuse you later if you do not kill it now.

Each molecule jiggles back and forth about its own position by a tiny amount — for ordinary
conversation, roughly a *billionth* of a metre. It does not go anywhere. What travels is the
*pattern of crowding*, passed from molecule to molecule like a rumour.

The standard analogy is a line of dominoes, but a better one is a stadium wave: the people stay
in their seats, the wave crosses the stadium. Or a slinky spring: push one end and a compression
travels down it while the spring stays put.

This matters practically. It is why:

- Sound needs a medium. In vacuum there is nothing to pass the rumour along, so there is
  genuinely no sound in space. (Part VIII discusses why films ignore this, and why they are
  right to.)
- Sound travels faster in denser, stiffer media — steel carries sound at 5,000 m/s versus air's
  343 m/s — because the molecules are packed closer and push each other sooner.
- A gentle breeze does not drown out speech, even though the *air* is moving far faster than
  any molecule in a sound wave.

### Longitudinal, not transverse

The jiggling happens *along* the direction the sound travels. Compress, expand, compress,
expand, in the same axis as the motion. This is called a **longitudinal** wave.

Light and water waves are **transverse** — the wiggling is perpendicular to the direction of
travel. Nearly every picture you will ever see of a sound wave, including the ones in this
book, draws it as a transverse wiggle, because that is the only way to draw it on paper. Keep
in the back of your mind that the up-and-down on the page means "pressure higher / pressure
lower", not "the air moved up and down".

---

## 2.2 How tiny is sound, really?

This is worth a moment because it recalibrates your intuition permanently.

Atmospheric pressure is about **101,325 Pa**.

| Sound | Pressure variation | As a fraction of atmospheric |
|---|---|---|
| Quietest audible sound (threshold of hearing) | 0.00002 Pa | 1 part in 5 billion |
| A quiet room | 0.0006 Pa | 1 part in 170 million |
| Normal conversation at 1 m | 0.02 Pa | 1 part in 5 million |
| Busy street | 0.2 Pa | 1 part in 500,000 |
| Rock concert, front row | 20 Pa | 1 part in 5,000 |
| Threshold of pain | 63 Pa | 1 part in 1,600 |
| Jet engine at 30 m | 200 Pa | 1 part in 500 |
| Instant permanent hearing damage | ~2,000 Pa | 1 part in 50 |

Read the first row again. Your ear can detect a pressure change of **one part in five
billion**. The eardrum's movement at that threshold is smaller than the diameter of a hydrogen
atom. If your ears were slightly more sensitive you would hear the thermal motion of air
molecules as a constant hiss — evolution stopped almost exactly at the physical limit.

Two consequences run through the whole book:

**1. The dynamic range of hearing is enormous.** From 0.00002 Pa to 20 Pa is a ratio of a
million to one in pressure. You cannot sensibly put a million-to-one range on a linear scale —
which is exactly why the decibel exists, and why Chapter 11 is dedicated to it.

**2. "Loud" is a perceptual statement, not a physical one.** Doubling the pressure does not
sound twice as loud. It sounds slightly louder. Chapter 3 quantifies this, and Chapter 11
turns it into code. Sound designers work in perceptual units and constantly translate to
physical ones; knowing which is which will keep you sane.

---

## 2.3 The waveform: turning sound into a graph

Put a microphone in the room. A microphone has a thin membrane that gets pushed by the passing
pressure variations, and electronics that turn that movement into a voltage. Higher pressure,
higher voltage; lower pressure, lower voltage.

Now plot that voltage against time. You get the picture everyone recognises:

```
   +1  |        .-''-.                    .-''-.
       |      .'      '.                .'      '.
    0  |----.'----------'.------------.'----------'.--------> time
       |  .'              '.        .'              '.
   -1  |-'                  '-....-'                  '-
```

This is a **waveform**, and it is the central object of the entire book. Be absolutely clear
about what the two axes are, because beginners routinely get this wrong:

- **Horizontal axis: time.** Left is earlier, right is later.
- **Vertical axis: pressure, relative to average.** Zero means "normal atmospheric pressure",
  i.e. silence. Positive means compressed. Negative means rarefied. In a computer this axis is
  conventionally scaled so that the maximum the system can represent is **+1.0** and the
  minimum is **−1.0**.

The vertical axis is *not* loudness, and it is *not* pitch. It is instantaneous pressure. A
single sample tells you nothing about pitch or loudness — those are properties of how the
value *changes over time*. This trips up everyone once, and once is enough.

> **Sanity check.** If a waveform sits flat on the zero line, is that silence? Yes — no pressure
> variation, nothing for the eardrum to respond to. If it sits flat at +0.5? Also silence,
> perceptually: a constant pressure offset is not a sound, because nothing is changing. Your ear
> responds to *change*. (This constant offset has a name, **DC offset**, and it is a real problem
> in practice: it wastes headroom and can damage speakers. Chapter 14 deals with it.)

---

## 2.4 The five properties of a simple sound

Almost every sound-related parameter in the book is one of these five. Learn them as a set.

### 1. Frequency — how often the cycle repeats

One complete up-and-down of the waveform is a **cycle** (or **period**). The number of cycles
completed per second is the **frequency**, measured in **hertz (Hz)**. 100 Hz means one hundred
complete cycles every second.

Frequency is what we perceive, roughly, as **pitch**. Higher frequency, higher pitch.

The **period** `T` is the duration of one cycle, and it is simply the reciprocal:

```
   T = 1 / f          f = 1 / T
```

At 100 Hz, one cycle lasts 1/100 = 0.01 s = 10 milliseconds. At 1000 Hz, 1 millisecond. At
20 Hz, 50 milliseconds.

Human hearing spans roughly **20 Hz to 20,000 Hz** in a healthy young person. Some reference
points worth memorising, because you will be reaching for them constantly:

| Frequency | What it is |
|---|---|
| 20 Hz | Bottom of hearing. Felt more than heard. The lowest pipe organ notes. |
| 40–60 Hz | Kick drum body, "chest thump", cinematic dread. |
| 82 Hz | Lowest string of a guitar (E2). |
| 110 Hz | A2. Bass guitar territory. |
| 220 Hz | A3. |
| **440 Hz** | **A4. The standard tuning reference. Memorise this one.** |
| 880 Hz | A5. |
| 1,000 Hz | The reference frequency for most acoustic measurement. |
| 2,000–4,000 Hz | Where the ear is most sensitive. Consonants, alarms, screams, "presence". |
| 8,000 Hz | "Air", cymbal shimmer, sibilance. |
| 16,000 Hz | Most adults over 40 cannot hear this. |
| 20,000 Hz | Nominal top of hearing. Essentially nobody past their teens hears it. |

Notice something in that table: every time the frequency **doubles**, the note name stays the
same and the number goes up by one. 110, 220, 440, 880 are all A. A doubling of frequency is an
**octave**, and it is perceived as "the same note, higher". This is the first hint of a huge
theme: *perception of pitch is logarithmic, not linear*. Going from 100 Hz to 200 Hz feels like
the same distance as going from 1,000 Hz to 2,000 Hz, even though one is a difference of 100 Hz
and the other of 1,000 Hz. Chapter 66 builds the whole tuning system on this fact.

### 2. Amplitude — how big the variation is

The **amplitude** is the size of the pressure swing: how far the waveform departs from zero.
Bigger swing, more energy, and (roughly) louder.

There are several different numbers people call "amplitude", and confusing them causes real
bugs:

- **Peak amplitude**: the largest absolute value reached. For the sine above, 1.0.
- **Peak-to-peak**: from the lowest trough to the highest crest. For the sine above, 2.0.
- **RMS (root mean square)**: a kind of average magnitude that corresponds much better to
  perceived loudness. For a sine wave, RMS = peak / √2 ≈ 0.707 × peak. Chapter 11 derives this.

Why does RMS matter? Because two sounds with the same peak can have wildly different perceived
loudness. A single sharp click can peak at 1.0 and be barely audible; a sustained tone peaking
at 1.0 is loud. The peak tells you about *clipping risk*; the RMS tells you about *loudness*.
Mastering engineers live in the gap between those two numbers, and so does the entire loudness
war. Chapter 89 is about that gap.

### 3. Phase — where in the cycle it is at a given moment

Take two identical sine waves and start one slightly later than the other. They have the same
frequency and amplitude but are offset in time. That offset, expressed as a fraction of a
cycle, is **phase**.

Phase is measured in degrees (0–360°) or radians (0–2π). Half a cycle is 180° or π radians.

```
   A:   .-''-.        .-''-.          B lags A by 90 degrees (a quarter cycle)
      .'      '.    .'      '.
   ---'----------'--'----------'---

   B:      .-''-.        .-''-.
         .'      '.    .'      '.
   ------'----------'--'----------'---
```

On its own, phase is inaudible. Play a sine wave, then play the same sine wave starting a
quarter-cycle later: they sound identical. Your ear does not have an absolute phase reference.

But phase becomes enormously audible the moment you have **two or more** sounds, because they
add together. And "adding together" is where all the interesting physics lives.

### 4. Waveform shape (timbre) — what one cycle looks like

Two sounds can have identical frequency and amplitude and still sound completely different — a
flute and a violin both playing A4 at the same loudness. The difference is the *shape* of the
repeating cycle, and the perceptual result is called **timbre** (pronounced "TAM-ber").

```
   Sine              Square            Sawtooth          Triangle
     .-'-.            ,-----,               /|              /\
   .'     '.          |     |              / |             /  \
   ---------  ...  ---'     '-----   ...  /  |    ...     /    \
          '.  .'           |     |       /   |          \/      \
            '-'            '-----'      /    |/                  \/
```

Section 2.6 explains what actually causes timbre, and it is the most important idea in the
chapter.

### 5. Envelope — how it changes over its lifetime

Real sounds are not steady. A piano note starts with a sharp hammer strike, decays fairly
quickly, and sustains faintly. A bowed violin note swells in. A cymbal crash explodes and rings
for seconds.

The overall shape of amplitude over the *whole duration* of a sound is its **envelope**.

```
   amplitude
     |    /\
     |   /  \___
     |  /       \_____
     | /              \____
     |/                    \___
     +---------------------------> time
      A   D      S           R
```

The traditional four-stage breakdown is **Attack, Decay, Sustain, Release** (ADSR), and
Chapter 13 implements it. Do not underestimate the envelope: it carries more identity
information than the waveform shape does. In classic psychoacoustic experiments, stripping the
attack transient off a recorded instrument makes it very hard to identify, while heavily
filtering the steady-state part often leaves it recognisable. The first 20 milliseconds of a
sound tell your brain what made it.

This is also the first cinematic lever. Same sound, faster attack = more aggressive, more
"now". Same sound, slow attack = looming, approaching, uneasy. Chapters 85 and 86 are built on
this.

---

## 2.5 Speed, wavelength, and why room size matters

Sound in air at 20 °C travels at about:

```
   c = 343 metres per second   (about 1,235 km/h, or 1,125 feet/second)
```

It depends on temperature (roughly `c ≈ 331 + 0.6 × T°C`) and negligibly on pressure. It does
not depend on frequency in any way you will notice in air — high and low notes from an
orchestra reach you together, which is why orchestras work at all.

Since the wave moves at `c` and repeats every `T` seconds, each cycle occupies a physical
distance called the **wavelength**, `λ`:

```
   λ = c / f
```

This little formula has huge practical consequences. Some values:

| Frequency | Wavelength | Comparable to |
|---|---|---|
| 20 Hz | 17 m | A house |
| 40 Hz | 8.6 m | A large room |
| 100 Hz | 3.4 m | Room height |
| 343 Hz | 1.0 m | An arm span |
| 1,000 Hz | 34 cm | A forearm |
| 4,000 Hz | 8.6 cm | A hand |
| 10,000 Hz | 3.4 cm | A finger |
| 20,000 Hz | 1.7 cm | A fingertip |

Now several mysteries dissolve at once:

**Why bass goes through walls and treble does not.** A 40 Hz wave is 8.6 m long; a 20 cm wall
is a minor obstacle to something that size, and the wave diffracts around and through it. A
10 kHz wave is 3.4 cm long and the wall is an enormous barrier. This is why you hear only the
bass from the party next door, and it is the physical basis of the **occlusion** modelling in
Chapter 78 — realistic occlusion is a low-pass filter, not a volume reduction.

**Why your head can localise treble but not bass.** Your head is about 20 cm across. For
frequencies whose wavelength is much smaller than that (above ~1.5 kHz), the head casts an
acoustic "shadow", so the far ear hears less — a level difference you can use to work out
direction. For frequencies whose wavelength is much bigger than your head (below ~800 Hz),
the wave wraps around and both ears hear nearly the same level, so the brain switches to using
*arrival time* differences instead. This crossover is the foundation of the **duplex theory**
of localisation in Chapter 73, and it is why subwoofer placement matters much less than people
think.

**Why small rooms sound bad in the bass.** A 3 m room and a 3.4 m wavelength are the same size.
Waves that fit the room dimensions reinforce themselves into **standing waves** (room modes),
creating positions where 100 Hz is enormous and positions a metre away where it nearly
vanishes. You cannot fix this with a graphic equaliser because it is a property of position, not
of the signal. It is why mixing rooms have bass traps.

**Why microphone and speaker size matters.** A transducer works best on wavelengths much larger
than itself. Hence tiny tweeters for treble, 15-inch cones for bass, and why your phone
physically cannot produce 40 Hz.

---

## 2.6 Superposition: the most important idea in the chapter

What happens when two sounds arrive at the same point in space?

**They add.** Instant by instant, the pressure deviation from one plus the pressure deviation
from the other. That is it. No interaction, no interference with each other's propagation, no
mixing fee. This is the **principle of superposition**, and air obeys it very accurately for
ordinary sound levels.

In code, "mixing" two sounds is therefore literally:

```
   mixed[i] = a[i] + b[i];
```

That is not a simplification for beginners. That is what a mixing console does. Chapter 14 adds
only the bookkeeping needed to stop the sum from exceeding ±1.0.

### Constructive and destructive interference

Because they add, phase suddenly becomes audible.

Two identical sine waves **in phase** (0° offset): crest meets crest, trough meets trough. The
result is twice the amplitude. **Constructive interference.**

```
   A:  .-''-.        .-''-.
   B:  .-''-.        .-''-.
   Sum: twice as tall
```

The same two waves **180° out of phase**: crest meets trough, and every instant cancels exactly.
The result is *silence*. **Destructive interference.**

```
   A:   .-''-.
   B:   '-..-'      (inverted)
   Sum: -----       (nothing)
```

Two sine waves of equal amplitude can produce silence. People find this genuinely hard to
believe until they do it — you will, in Chapter 14, in four lines of code. It is not a trick;
the energy goes nowhere because it was never independently there.

This one fact explains an unreasonable number of real-world phenomena:

- **Noise-cancelling headphones** measure incoming sound and play its inverse.
- **Comb filtering**: mix a sound with a slightly delayed copy of itself and some frequencies
  cancel while others reinforce, at regular intervals — the basis of flangers (Chapter 44) and
  the reason a microphone near a reflective surface sounds hollow.
- **Mono compatibility**: if your stereo mix relies on out-of-phase content, summing to mono
  makes parts of it disappear. Broadcast engineers check this obsessively (Chapter 75).
- **A speaker wired backwards** in a stereo pair produces a mix with no centre image; the bass
  largely cancels and the vocal seems to come from nowhere in particular.

### Beating

If two sines are *nearly* the same frequency — say 440 Hz and 443 Hz — they drift in and out of
phase with each other. The result is a single tone that pulses in loudness, at a rate equal to
the frequency difference: 3 Hz, three pulses per second. This is **beating**, and it is how
piano tuners work: they adjust until the beating stops.

Speed the difference up and something remarkable happens. At a difference of 3 Hz you hear
pulsing. At 15 Hz you hear roughness. At 40 Hz you stop hearing one pulsing tone and start
hearing two separate tones. The transition is not in the physics — the physics is identical
throughout — it is in your auditory system. That crossover is the first psychoacoustic boundary
you will meet, and Chapter 72 explains it via critical bands. Cinematic sound uses it
deliberately: detuned layers a few Hz apart are what make a braam feel alive and enormous
rather than synthetic.

---

## 2.7 Harmonics and why timbre exists

Here is the resolution of the timbre question from §2.4, and it is the pivot on which Part II
of this book turns.

Pluck a guitar string. The string is fixed at both ends, so it can only vibrate in patterns
that have a node (a still point) at each end. The simplest such pattern is the whole string
swinging as one arc — this gives the **fundamental** frequency, `f₁`.

But the string can *simultaneously* vibrate in a pattern with a node in the middle, giving two
half-length arcs. Half the length, double the frequency: `2f₁`. And in three sections: `3f₁`.
And four: `4f₁`. All at once, all superimposed.

```
   Mode 1 (fundamental, f):    .-'''''-.          one arc
                             /           \

   Mode 2 (2f):              .-'-.               two arcs, node in middle
                                  '-.-'

   Mode 3 (3f):              .-.     .-.         three arcs
                                '-'
```

These higher components are **harmonics** (or **overtones**, or more generally **partials**).
A guitar string playing A2 produces 110 Hz, 220 Hz, 330 Hz, 440 Hz, 550 Hz... all at the same
time, each with its own amplitude, each decaying at its own rate.

Your ear fuses this stack into a single perceived note, whose pitch is the fundamental, and
whose *character* is determined by the relative strengths of the harmonics.

- Strong fundamental, weak harmonics → soft, round, flute-like.
- Strong odd harmonics (3f, 5f, 7f...) → hollow, clarinet-like. A square wave is pure odd
  harmonics.
- All harmonics present, falling off gently → bright, buzzy, brassy. A sawtooth wave.
- Harmonics that are *not* whole-number multiples → bell-like, metallic, or inharmonic and
  unsettling. Struck metal plates do this, and Chapter 39 exploits it. A great deal of horror
  sound design is deliberately inharmonic content, because our auditory system cannot resolve
  it into a comfortable single pitch.

### The claim that makes all of DSP possible

Now the leap. **Any** repeating waveform, no matter how complicated, can be constructed by
adding together sine waves at harmonic frequencies with the right amplitudes and phases. And
any non-repeating sound can be built from sines too, given a continuum of frequencies.

This is **Fourier's theorem**, and it is not an approximation or an engineering convenience —
it is a mathematical fact. It means:

- The sine wave is the **atom** of sound. Everything decomposes into sines.
- A sound has two complete, equivalent descriptions: as pressure over **time** (the waveform),
  and as a recipe of amplitudes and phases over **frequency** (the spectrum). Neither is more
  real than the other; you can convert freely between them.
- Therefore a filter, which changes how much of each frequency is present, can be understood
  entirely in the frequency domain — even though it is implemented as arithmetic on samples in
  the time domain.

The tool that converts between the two views is the Fourier transform, and we build it by hand
in Chapter 25. It is the single most powerful thing in this book. For now, just hold the idea:
**time view and frequency view are two windows onto the same object.**

```
   TIME DOMAIN                          FREQUENCY DOMAIN
   (waveform: pressure vs time)         (spectrum: level vs frequency)

     /|  /|  /|  /|                      |
    / | / | / | / |                      | |
   /  |/  |/  |/  |                      | | |
                                         | | | | . . .
   sawtooth, 100 Hz                      +-----------------
                                         100 200 300 400 Hz
```

---

## 2.8 What happens to sound as it travels

A sound in a real space is not just a source. Almost everything you hear is shaped by the
journey. This section is the physics behind Chapters 46–49 and 77–78, and it is where
"cinematic" space comes from.

### Spreading loss (the inverse-square law)

Sound from a small source radiates in all directions, spreading its fixed energy over an
ever-larger sphere. The area of a sphere grows as `r²`, so intensity falls as `1/r²`, and
*pressure* (what we measure and what code deals with) falls as `1/r`:

```
   pressure ∝ 1 / distance
```

Double the distance, half the pressure — which is a drop of 6 dB (Chapter 11). This "6 dB per
doubling" is the default distance rule in every game audio engine, and Chapter 77 shows both
how to implement it and the several good reasons real engines deviate from it.

### Air absorption

Air is not a perfect conductor of sound. It converts a little acoustic energy to heat, and it
does so *much* more readily at high frequencies. Over 100 m, treble above 5 kHz is noticeably
reduced; over a kilometre it is essentially gone.

This is why distant thunder is a low rumble while nearby thunder is a sharp crack — same event,
different filtering by distance. It is a colossally important cue: **your brain uses spectral
tilt to judge distance.** Reduce the treble on a sound and it recedes, without any change in
level. Cinematic mixers use this constantly to create depth, and Chapter 77 implements it as a
distance-dependent low-pass filter. Any 3D audio engine that only changes volume with distance
sounds flat and wrong, and now you know exactly why.

### Reflection and absorption

When sound hits a surface, some energy reflects, some is absorbed, some passes through. Hard,
dense, flat surfaces (tile, glass, concrete) reflect; soft, porous, irregular ones (curtains,
carpet, people, acoustic foam) absorb — again, more effectively at high frequencies.

In a room you therefore hear:

1. The **direct sound**, arriving first, along the straight path.
2. **Early reflections** — a handful of distinct echoes off the nearest walls, floor, and
   ceiling, arriving over the next 5–80 ms. These carry the information your brain uses to
   judge *room size and shape*, largely unconsciously.
3. **Late reverberation** — after enough bounces, thousands of overlapping reflections merge
   into a smooth, decaying wash with no individual echoes discernible.

```
   level
     |  |  direct
     |  |
     |  |    |  |   |     early reflections (discrete, sparse)
     |  |    |  |   |  |
     |  |    |  |   |  |||||||.,,,___   late reverb (dense, smooth decay)
     +----------------------------------> time
        0   10   20  30  40  50  60 ms
```

The time it takes for the reverb to fall by 60 dB is the **RT60**, the standard measure of a
room's liveness: a recording studio 0.3 s, a living room 0.5 s, a concert hall 2 s, a cathedral
6–10 s. Four chapters of this book (46–49) are about synthesising that decay convincingly,
because reverb is the primary way an audio engine says *where you are*.

### Diffraction

Waves bend around obstacles whose size is comparable to or smaller than their wavelength. Low
frequencies bend around a human head, a pillar, a corner; high frequencies do not, and cast
shadows instead. This is the mechanism behind the "you hear the bass through the wall"
observation, and it is why turning your head changes the treble of a sound much more than the
bass.

### Refraction and wind

The speed of sound depends on temperature, so sound bends when it crosses temperature
gradients. On a cold night with warm air above, sound bends back down towards the ground and
you can hear a train from an implausible distance. On a hot day it bends upward and the same
train is inaudible. Wind gradients do something similar. This is mostly trivia for our
purposes, but it is the reason sound seems to "carry" at night, and it makes a nice detail for
outdoor scene design.

### Resonance

Any object has frequencies at which it vibrates most readily. Drive it at one of those and the
motion builds enormously for very little input. A wine glass, a guitar body, a tube, a room, an
ear canal, a car cabin, a human vocal tract — all resonators.

Resonance is the bridge between physics and synthesis. It is:

- Why instruments have bodies (a string alone is nearly inaudible; the body resonates and
  radiates).
- Why vowels are distinguishable — the vocal tract's resonances ("formants", Chapter 55) shape
  a single buzz into "ah" or "ee".
- The core of filters: a resonant filter is a mathematical resonator, and its "Q" is how
  sharply it resonates (Chapter 23).
- The core of modal synthesis: model an object as a bank of resonators and you can make it ring
  like metal, wood, or glass (Chapter 39).

### Doppler

If a source moves towards you, each successive cycle is emitted a little closer, so the cycles
arrive more frequently: the pitch rises. Moving away, it falls. The classic ambulance siren.

The shift is `f_observed = f_source × c / (c − v_source)` for a source approaching at speed `v`.
Chapter 77 implements it, and — importantly — explains why you should implement it as a
*varying delay line* rather than by changing frequency, which is the mistake that produces
clicks and chirps in amateur engines.

---

## 2.9 Noise: sound without periodicity

Everything so far has been periodic — a pattern that repeats. But listen to rain, applause,
wind, a hiss, a cymbal, the sea, a shovel in gravel. No repetition, no pitch, no harmonics.
This is **noise**: pressure variation that is essentially random.

Noise is not a failure of sound; it is half of all the sound you hear, and it is at least half
of all sound design. Its only real parameter is its **spectral tilt** — how energy is
distributed across frequency:

- **White noise**: equal energy per hertz. Sounds bright and hissy, like untuned analogue TV,
  because each octave upward contains twice the bandwidth and therefore twice the energy.
- **Pink noise**: equal energy per *octave*, falling at 3 dB per octave. Sounds balanced and
  natural — rainfall, a waterfall, a distant motorway. Pink noise is the reference signal for
  acoustic measurement because it matches how the ear divides up the spectrum.
- **Brown / red noise**: falls at 6 dB per octave. Deep rumble, thunder, heavy surf.

Chapter 15 generates all three, and Chapter 40 uses filtered, modulated noise to build wind,
rain, fire, and engines. It is remarkable how far you can get in sound design with noise and a
filter alone: most weather, most fluids, most impacts, and most of the "air" in a cinematic
mix start as noise.

Real sounds are almost always a mixture. A flute note is a strong periodic tone plus breath
noise, and the noise is what makes it sound like a real flute rather than a synthesiser. A
cymbal is nearly all noise with faint inharmonic resonances. Speech alternates between periodic
(vowels) and noisy (`s`, `f`, `sh`) many times per second. Chapter 55 splits and recombines
those two components on purpose.

---

## 2.10 Microphones and speakers: the two transducers

Two devices bracket the digital world, and both are the same idea run in opposite directions.

A **microphone** converts pressure variation into voltage variation. A thin diaphragm is pushed
by the sound; its movement is converted to electricity by one of several mechanisms (moving coil
in a magnetic field for dynamic mics; changing capacitance for condenser mics). The output is a
voltage that traces the pressure waveform.

A **loudspeaker** converts voltage variation into pressure variation. Current through a coil in
a magnetic field produces force, which moves a cone, which pushes air. The cone's excursion
follows the voltage, and the air near it is compressed and rarefied accordingly.

So the full chain from a musician to your brain is:

```
   Instrument  ->  air  ->  microphone  ->  voltage  ->  ADC  ->  numbers
                                                                    |
                                                          [ your program ]
                                                                    |
   Ear  <-  air  <-  speaker  <-  voltage  <-  DAC  <-  numbers  <--+
```

`ADC` and `DAC` are the analogue-to-digital and digital-to-analogue converters — the bridges
between the continuous physical world and the discrete numerical one. How that bridge works,
and what it costs, is the subject of Chapter 4, and it is the last piece of theory before we
start writing code.

Two things to take from this diagram:

**The waveform in your program is a scale model of speaker-cone position.** When you write
`buffer[i] = 0.5`, you are asking the cone to be halfway to its maximum outward excursion at
that instant. This is why DC offset is dangerous (you are asking the cone to sit pushed-out
indefinitely, heating the coil), and why a discontinuity in your buffer makes a click (you are
demanding an instantaneous jump, and the cone's sudden slam is a broadband impulse).

**Everything outside the middle box is linear and solved.** You do not need to model
microphones or speakers to do excellent audio programming. But knowing the chain exists tells
you where to look when something sounds wrong, and roughly two-thirds of "my code sounds wrong"
turns out to be somewhere in that chain rather than in the code.

---

## 2.11 Vocabulary consolidation

You now have the vocabulary. Here it is in one place, with the code name it will acquire later.
Come back to this table; it is load-bearing.

| Physical concept | Meaning | Becomes, in code |
|---|---|---|
| Pressure deviation | Instantaneous departure from atmospheric pressure | One **sample**: a `float` in −1.0 … +1.0 |
| Waveform | Pressure over time | An array or `std::vector<float>` of samples |
| Frequency | Cycles per second, Hz | `freq`, used to compute a phase increment |
| Period | Seconds per cycle, `1/f` | `1.0 / freq`, or a length in samples |
| Amplitude | Size of the swing | `amp`, `gain`, a multiplier |
| Phase | Position within the cycle | `phase`, a running accumulator in 0 … 2π |
| Wavelength | `c / f`, physical length of a cycle | Used in spatial and room calculations |
| Timbre | Character, from harmonic content | Result of waveform choice + filtering |
| Harmonic | Integer multiple of the fundamental | Bins in an FFT; partials in additive synthesis |
| Envelope | Amplitude over the sound's lifetime | An ADSR object multiplying the signal |
| Superposition | Sounds add | `out[i] = a[i] + b[i]` |
| Interference | Phase-dependent reinforcement/cancellation | Comb filters, flangers, mono-compatibility bugs |
| Beating | Slow pulsing from near-equal frequencies | Detuned oscillators; chorus |
| Noise | Aperiodic pressure variation | A random number generator, then a filter |
| Reflection | Sound bouncing off surfaces | Delay lines |
| Reverberation | Dense late reflections | Reverb algorithms (Chapters 46–49) |
| Absorption | Energy loss, worse at HF | A low-pass filter inside a feedback loop |
| Diffraction | Bending around obstacles | Occlusion modelled as a low-pass filter |
| Resonance | Preferred vibration frequency | Filter `Q`; modal synthesis |
| Doppler | Pitch shift from motion | A varying delay line |
| Inverse-square law | Level falls as `1/r` | `gain = 1 / distance` |
| Speed of sound | 343 m/s | Delay time `= distance / 343` |

---

## 2.12 Discussion: which of these does an audio programmer actually think about?

A fair question at this point is how much of this physics you carry around day to day. Honest
answer, from the shape of the work:

**Constantly, every day:** frequency, amplitude, phase, harmonics, envelope, superposition,
the time/frequency duality. These are your working units. You will think in them the way a
carpenter thinks in millimetres.

**Often:** wavelength (whenever space is involved), reflection and absorption (whenever reverb
is involved), noise and spectral tilt (whenever you design a sound), resonance (whenever you
touch a filter).

**Occasionally:** the inverse-square law, air absorption, Doppler — these come up as soon as you
do anything three-dimensional, and then they come up all the time.

**Almost never, but you should know it exists:** refraction, the actual pressure in pascals,
the molecular picture. You need this layer once a year, when something behaves surprisingly and
you have to reason from first principles. That is exactly when having it is worth a great deal.

The reason I have given you the whole picture rather than just the working units is that audio
is a field where the abstractions leak. A click is a physics problem. A filter that screams is a
physics problem. A 3D engine that sounds flat is a physics problem — specifically, a missing
air-absorption filter. When the abstraction leaks, the people who can reason about the layer
underneath fix the bug in an hour, and the people who cannot spend a week changing numbers at
random.

---

## 2.13 Exercises

No code yet, so these are thinking and listening exercises. Do them; they build intuition that
pays off in Part I.

**2.1** A sound has a period of 2.5 milliseconds. What is its frequency? What is its wavelength
in air at 20 °C?

**2.2** You are 10 m from a speaker. You move to 40 m. By what factor does the pressure
amplitude fall? (Use the inverse-square law in its pressure form.)

**2.3** Explain in your own words why a 50 Hz tone and a 5,000 Hz tone behave so differently
when there is a brick wall between you and the source. Use the word "wavelength".

**2.4** Two sine waves, both 300 Hz, amplitude 0.4 each, are 180° out of phase. What is the
amplitude of the sum? What if they were 90° out of phase? (For the second, you may need to
think in terms of adding two rotating arrows — Chapter 19 makes this precise, so an approximate
answer is fine now.)

**2.5** *Listening.* Find a quiet room. Sit still for two minutes and list every sound you can
hear, and for each one write down whether it is mostly periodic (pitched) or mostly noise. Then
guess its rough frequency range. This exercise is the single best training for sound design, and
professionals never stop doing it.

**2.6** *Listening.* Play music you know well through the smallest speaker you own (phone), then
through headphones. Write down what disappears. Now you know what your phone lies about.

**2.7** Why does a piano note played with the sustain pedal down sound different from one
without, even at the same pitch and loudness? Answer in terms of resonance and harmonics.

**2.8** A cinematic sound designer wants a helicopter to feel like it is 2 km away rather than
100 m. List three things they would change, using only concepts from this chapter. (There is
more than one good answer; aim for level, spectrum, and time.)

---

### Chapter summary

- Sound is a travelling pattern of pressure variation. The medium's molecules jiggle in place;
  the *pattern* travels, at 343 m/s in air.
- The variations are tiny — a billionth of atmospheric pressure at the threshold of hearing —
  and hearing spans a million-to-one range, which is why we need decibels.
- A waveform plots pressure deviation against time. Vertical is pressure, not loudness and not
  pitch.
- Five properties describe a simple sound: frequency, amplitude, phase, waveform shape,
  envelope.
- `λ = c / f` explains occlusion, localisation, room modes, and transducer sizes.
- Sounds **add**. Superposition makes phase audible: interference, beating, comb filtering, and
  mono-compatibility all follow from it.
- Timbre comes from harmonics. Fourier's theorem says all sound decomposes into sine waves, so
  time-domain and frequency-domain are two views of one object. That duality is the engine of
  Part II.
- Travelling sound is shaped by spreading loss, air absorption (a treble cut that reads as
  distance), reflection, reverberation, diffraction, resonance, and Doppler. These are the
  physics of every spatial and reverb chapter later in the book.
- Noise is aperiodic sound, characterised by spectral tilt, and it is half of all sound design.

**Next:** [Chapter 3 — How Hearing Works](03-how-hearing-works.md)
