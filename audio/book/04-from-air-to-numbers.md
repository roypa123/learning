# Chapter 4 — From Air to Numbers: Sampling and Quantization

> The last theory chapter before we install a compiler. This one is the bridge: it explains
> exactly what the numbers in your arrays *are*, and it contains the two ideas — Nyquist and
> quantization — that every remaining chapter depends on.

---

## 4.1 The problem

A pressure wave in air is **continuous** in two separate ways:

1. **Continuous in time.** At every instant — and there are infinitely many instants in any
   interval — there is a pressure value. Between 1.0 s and 1.1 s there is a value at 1.05 s,
   at 1.0500001 s, at 1.05000000001 s.
2. **Continuous in value.** The pressure can be 0.5 Pa, or 0.5000001 Pa, or any real number in
   between, with infinite precision.

A computer can store neither infinity. It has finite memory and finite precision. So we must
make two separate approximations, and they are genuinely separate — different mechanisms,
different failure modes, different names:

| Approximation | Name | Makes discrete... | Governed by |
|---|---|---|---|
| Measure only at certain instants | **Sampling** | time | **sample rate** |
| Round each measurement to one of a finite set of values | **Quantization** | amplitude | **bit depth** |

The astonishing result, and the reason digital audio works at all, is that the first
approximation can be made **perfect** — not "good enough", but provably lossless — as long as
one condition is met. The second can be made inaudible. Let us take them in turn.

---

## 4.2 Sampling: slicing time

**Sampling** means measuring the waveform's value at regular intervals and storing only those
measurements.

```
   Continuous waveform                 Sampled: only the dots are stored
        .-''-.                                 .  .
      .'      '.                            .        .
   --'----------'--------           ---- .-------------.-------
                  '.        .'                          .     .
                    '-....-'                                .  .
                                        |  |  |  |  |  |  |  |  |
                                        equally spaced instants
```

The interval between samples is the **sample period**, `Ts`. Its reciprocal is the **sample
rate** (or sampling frequency), `fs`, measured in hertz — samples per second.

```
   fs = 1 / Ts          Ts = 1 / fs
```

At `fs = 44,100 Hz`, we take 44,100 measurements per second, one every 22.68 microseconds.

Every stored number is one **sample**. That word is overloaded in the audio world and you must
keep the meanings apart:

- **Sample** (this book's default meaning): one numeric measurement of the waveform at one
  instant.
- **Sample** (musician's meaning): a recorded sound file, as in "a drum sample". Chapter 69
  uses this sense and I will say "sample file" when it could be ambiguous.

### Standard sample rates and why they are what they are

| Rate | Where it is used | Why |
|---|---|---|
| 8,000 Hz | Telephone | Speech intelligibility needs ~3.4 kHz of bandwidth; minimal data |
| 22,050 Hz | Old games, low-quality web audio | Half of 44.1; cheap |
| **44,100 Hz** | CD, most music, consumer audio | See below |
| **48,000 Hz** | Film, video, broadcast, most pro gear, game engines | Divides evenly into video frame rates |
| 88,200 / 96,000 Hz | High-resolution recording, mastering | Headroom for processing; easier anti-alias filters |
| 176,400 / 192,000 Hz | Archival, some plugins internally | Diminishing returns; large files |
| 2.8224 MHz | DSD / SACD | 1-bit format, entirely different scheme |

The odd one is 44,100. Why such an ugly number? Because early digital audio masters were stored
on video tape (a PCM adaptor wrote audio as a fake video signal), and 44,100 is the number that
fits both NTSC and PAL video timing: 3 samples × 245 active lines × 60 fields = 44,100, and
3 × 294 × 50 = 44,100. It is a historical accident that we are stuck with forever.

48,000 Hz is the professional choice because it divides evenly by common frame rates
(24, 25, 30, 48, 50, 60), so one video frame contains a whole number of samples — 2,000 samples
at 24 fps. At 44.1 kHz, 24 fps gives 1,837.5 samples per frame, and the resulting half-sample
drift is a genuine nuisance when syncing to picture.

**For this book we use 44,100 Hz by default** because it is the most universally supported rate
for files, and 48,000 Hz from Part VIII onward, where we work to picture. Both appear as a
constant you can change in one place.

### What "CD quality" costs

A useful number to have: one second of CD-quality stereo audio is

```
   44,100 samples/sec × 2 bytes/sample × 2 channels = 176,400 bytes/sec
```

That is about 10.6 MB per minute, 635 MB per hour — which is why a CD holds about 74 minutes.
Memorise the 176,400 figure; you will use it constantly for sanity checks on file sizes.

---

## 4.3 Aliasing: what goes wrong, and when

Sampling throws away everything between the sample instants. Sometimes that loses nothing.
Sometimes it is catastrophic. Here is the catastrophe.

### The wagon wheel

You have seen this in films: a car accelerates, its wheels spin faster, and at some point the
spokes appear to slow, stop, then rotate *backwards*. The camera samples 24 times a second. If
the wheel rotates slightly less than one full spoke-spacing between frames, each frame shows the
spoke a little *behind* where it was — so it looks like it is going backwards. The camera did
not malfunction. The sampling rate was too low for the motion, and the reconstructed motion is a
plausible but **wrong** interpretation of the samples.

Audio does exactly the same thing, and the wrong interpretation is a wrong *frequency*.

### The audio version

Sample a sine wave at `fs = 1000 Hz`.

A 100 Hz sine gets 10 samples per cycle. Plenty — the shape is obvious:

```
   .  .                .  .
  .    .              .    .
 .      .            .      .
--.------.----------.-------.-----
          .      .
           .    .
             ..
```

A 400 Hz sine gets 2.5 samples per cycle. Sparse, but as we will see, still enough.

A 900 Hz sine gets 1.11 samples per cycle. Now watch what happens. The samples we take happen
to lie exactly on a **100 Hz** sine wave as well. Both signals pass through every single sample
point. Once sampled, there is no information left that distinguishes them — they are the same
data.

```
   900 Hz signal (dense wiggle)    sampled at 1000 Hz
   ...but the samples trace out a slow 100 Hz wave
```

When we play those samples back, we hear **100 Hz**. The 900 Hz tone has been "folded" down. It
has taken on an **alias** — a false identity.

### The rule

A frequency `f` sampled at `fs`, where `f > fs/2`, appears after sampling at

```
   f_alias = | f − fs × round(f / fs) |
```

and for the common case `fs/2 < f < fs`, that simplifies to:

```
   f_alias = fs − f
```

Check: `1000 − 900 = 100`. ✓

The critical frequency `fs/2` is the **Nyquist frequency**. It is the highest frequency that can
be represented at a given sample rate:

| Sample rate | Nyquist frequency |
|---|---|
| 8,000 Hz | 4,000 Hz |
| 22,050 Hz | 11,025 Hz |
| 44,100 Hz | 22,050 Hz |
| 48,000 Hz | 24,000 Hz |
| 96,000 Hz | 48,000 Hz |

Notice that 44.1 kHz gives a Nyquist of 22,050 Hz, comfortably above the ~20 kHz limit of human
hearing. That is not a coincidence — it is the design requirement that set the rate.

### The Sampling Theorem

Now the good news, and it is very good news. Nyquist and Shannon proved:

> **A signal containing no frequencies at or above `fs/2` can be reconstructed exactly and
> completely from its samples taken at rate `fs`.**

Read that again, because it is stronger than people expect. Not "approximately". Not "well
enough". **Exactly.** If the signal is band-limited below Nyquist, the samples contain 100% of
the information in the original continuous waveform. Nothing whatsoever is lost by sampling.

The intuition: a band-limited signal cannot wiggle arbitrarily fast. Between two samples it is
constrained — there is only one band-limited curve that passes through all the sample points.
Sampling does not discard information because the "missing" values were never free to vary
independently; they were already determined.

This theorem is why digital audio is not a compromise. The two frequent objections are both
answered by it:

- *"Surely you lose what happens between samples?"* No — for a band-limited signal, what happens
  between samples is uniquely determined by the samples.
- *"Surely 2 samples per cycle is not enough to draw a sine?"* It is not enough to *draw* one
  with straight lines, but it is enough to *determine* one, which is a different and stronger
  statement.

### Anti-aliasing filters

The theorem has a condition — no content at or above Nyquist — and the real world does not obey
conditions. So every analogue-to-digital converter puts a steep **low-pass filter** in front of
the sampler, removing everything above Nyquist *before* it can alias.

This matters enormously because **aliasing is irreversible**. Once a 21 kHz tone has folded down
to 1 kHz in your data, it is simply a 1 kHz tone; nothing downstream can identify or remove it.
Filtering afterwards removes the legitimate 1 kHz content along with the alias. This is the
defining property of aliasing and the reason it is taken so seriously:

> **Aliasing must be prevented. It cannot be fixed.**

And here is why this concerns *you*, not just converter designers: **when you generate sound in
code, you are the converter.** If you write a naïve sawtooth oscillator at 5 kHz, it contains
harmonics at 10, 15, 20, 25, 30 kHz... and everything above 22,050 folds back down to
frequencies that are *not harmonically related* to the note. The result is a metallic,
dissonant shimmer that changes character as you play up the keyboard, and it is the number one
reason amateur synthesisers sound cheap.

Chapter 12 will let you hear it deliberately. Chapter 31 fixes it properly with band-limited
oscillators, and Chapter 29 covers oversampling, the general solution for nonlinear processes
like distortion, which generate new harmonics of their own.

### Why 96 kHz, then?

If 44.1 kHz already covers all of hearing, why do professionals record at 96 kHz?

Three honest reasons and one dubious one:

1. **Gentler anti-alias filters.** At 44.1 kHz the filter must go from passing 20 kHz to
   stopping 22.05 kHz — extremely steep, and steep filters have side effects (phase distortion,
   ringing). At 96 kHz there is a whole octave of transition room.
2. **Headroom for nonlinear processing.** Distortion, compression, saturation and pitch-shifting
   all create new high frequencies. At a higher rate there is more room before they fold.
   (Note: you can get the same benefit by oversampling only inside the plugin, which is what
   well-written plugins do — Chapter 29.)
3. **Time resolution for editing and spatial work.** Finer sample granularity helps with
   sample-accurate alignment and inter-channel delays.
4. *(Dubious)* "You can hear above 20 kHz." The evidence for audible benefit from ultrasonic
   content in the final playback chain is weak at best, and some equipment performs *worse* with
   ultrasonic content present because it intermodulates down into the audible band.

The practical position most engineers settle on: record and process at 48 or 96 kHz, deliver at
48 or 44.1 kHz. In this book we work at 44.1 kHz and oversample locally where it matters, which
is the approach with the best cost/benefit ratio.

---

## 4.4 Reconstruction, and the "stairstep" myth

Ask most people to draw digital audio and they draw a staircase:

```
       ___
      |   |___
   ___|       |
              |___
```

**This is wrong, and it causes real misunderstandings, so let us kill it properly.**

Samples are not little rectangles of held voltage. They are **instantaneous point values** —
infinitely thin, zero-width measurements. The staircase is an artefact of how we draw them, and
of one particular (crude) way to convert them back.

To reconstruct the continuous waveform, a DAC does not hold each value and jump. It passes the
impulse train through a **reconstruction filter** — a low-pass filter at Nyquist. Mathematically
this is equivalent to placing a `sinc` function at each sample, scaled by that sample's value,
and adding them all up:

```
   sinc(x) = sin(πx) / (πx)
```

The `sinc` function is 1 at its centre and **exactly zero at every other integer** — which is
precisely the property needed: each sample's contribution is exactly its own value at its own
instant, and contributes zero at every other sample instant, while smoothly filling the gaps
between. Sum them and you get back a perfectly smooth curve — the *unique* band-limited curve
through those points.

```
   Sample values:      .       .       .       .
                       |       |       |       |
   Each becomes a sinc: gentle ripple centred on its sample
   Sum of all sincs:   .-'''-.__.-'''-.  smooth, no stairs
```

So the correct mental picture of digital audio is:

> A list of exact point measurements, which together uniquely specify one smooth,
> band-limited curve.

Not a staircase. Practical consequences of getting this right:

- **Samples can exceed 0 dBFS between sample points.** Two adjacent samples at 0.99 can lie on a
  reconstructed curve that peaks at 1.05 in between them. This is **inter-sample peaking** and
  it makes downstream converters and MP3 encoders clip even though no stored sample exceeds
  full scale. Broadcast standards therefore specify **true peak** limits (−1 dBTP), measured by
  oversampling to find the real peaks. Chapter 51 builds a true-peak limiter, and this
  paragraph is why it exists.
- **"Digital sounds harsh because of the stairsteps"** is not a thing. Whatever harshness people
  hear comes from aliasing, from poor converters, from clipping, or from excessive processing —
  all real, all explicable, none of them staircases.
- **Zooming into a waveform editor and seeing steps** is your editor's drawing choice. Good
  editors offer a "show reconstructed curve" mode that draws the sinc interpolation, and it
  visibly overshoots at transients — which is the truth.

---

## 4.5 Quantization: slicing amplitude

The second approximation. Having decided *when* to measure, we must store *what* we measured,
and we have a finite number of bits to do it with.

With `N` bits, we have `2^N` distinct values available:

| Bit depth | Distinct values | Typical use |
|---|---|---|
| 8-bit | 256 | Retro games, old samplers; audibly gritty |
| 12-bit | 4,096 | Early samplers (that characteristic sound) |
| **16-bit** | 65,536 | CD, most distributed audio |
| **24-bit** | 16,777,216 | Professional recording standard |
| 32-bit float | ~2^24 precision, huge range | DSP, DAWs, this book's internal format |
| 64-bit float | Enormous | Some plugin internals, filter coefficients |

**Quantization** is rounding each measured value to the nearest available level.

```
   level
   +3 ---------------------------------
   +2 --------  o  ---------------------      o = actual value
   +1 ------ o -- X ------------------        X = quantized (rounded)
    0 ---- X ---------- o ---------------
   -1 --------------- X ------------------
   -2 ---------------------------------
```

The difference between the true value and the stored one is the **quantization error**. It is at
most half a step, and for complicated signals it is essentially random from sample to sample. A
random error added to a signal is, by definition, **noise**.

So: *quantization adds a noise floor.* That is the entire cost of finite bit depth, and it is a
much friendlier cost than aliasing, because it is additive and bounded rather than confusing and
irreversible.

### The 6 dB per bit rule

Each additional bit doubles the number of levels, which halves the step size, which halves the
error amplitude — a factor of 2, which is 6.02 dB. Hence the most quoted rule in digital audio:

```
   Dynamic range ≈ 6.02 × N + 1.76  dB     (for N bits, full-scale sine)
```

In practice people use the simpler approximation:

| Bit depth | Approximate dynamic range |
|---|---|
| 8-bit | 48 dB |
| 12-bit | 72 dB |
| 16-bit | **96 dB** |
| 20-bit | 120 dB |
| 24-bit | **144 dB** |
| 32-bit float | ~1,500 dB of range (see below) |

Now put those against the physical world:

- Quietest audible sound to threshold of pain: about **120 dB**.
- A very quiet recording studio's noise floor: about 20 dB SPL.
- A loud orchestra: about 110 dB SPL. So the usable range of a live recording: ~90 dB.
- The self-noise of the best microphones: around 5–10 dB SPL equivalent.

**16-bit's 96 dB is enough for finished, distributed audio.** The noise floor sits below the
noise floor of the room and the microphone. **24-bit's 144 dB exceeds anything physically
achievable**, which is precisely why professionals record in 24-bit: not because they need 144
dB, but because it means you can record at a conservative level, leaving 20 dB of safety
headroom for an unexpected loud moment, and still have 120 dB of range left. Bit depth buys you
*carelessness*, and carelessness is worth a great deal when a take cannot be repeated.

### Why quantization noise is not simply "hiss"

At high bit depths, quantization error is random and sounds like gentle hiss. At low bit depths,
or for very quiet signals, the error becomes **correlated with the signal** — it stops being
noise and becomes distortion, which is far more objectionable because it tracks the music.

Picture a sine wave so quiet that it only crosses two or three quantization levels. Instead of a
smooth curve you get a crude staircase whose steps happen at points determined by the signal
itself. What you hear is not hiss; it is a gritty, buzzy distortion that appears and disappears
with the signal, and which is especially ugly on fade-outs and reverb tails. This is
**quantization distortion**, and it is the reason for the next section.

---

## 4.6 Dither: adding noise on purpose

The fix is beautifully counter-intuitive: **add a small amount of random noise before
quantizing.**

This is **dither**, about one quantization step of noise (peak-to-peak). It seems insane — you
are deliberately degrading the signal — but it works, and here is why.

Without dither, a signal at one-third of a step always rounds to the same value: information
below the step size is completely lost, deterministically. With dither, that signal rounds *up*
about a third of the time and *down* two-thirds of the time, randomly. The instantaneous value
is wrong every time, but the **average over many samples is correct**.

Because your auditory system integrates over time (Chapter 3: ~100–200 ms, which is thousands of
samples), it perceives that average. The result: you can hear signals **below** the level of one
quantization step. Dither trades a small, constant, benign noise floor for the elimination of
signal-correlated distortion, and it recovers resolution below the least significant bit.

```
   Without dither:   signal below 1 LSB  ->  silence, then abrupt steps, gritty distortion
   With dither:      signal below 1 LSB  ->  audible through the noise, smooth, no distortion
```

This is a genuinely remarkable result and it generalises well beyond audio (it is why image
dithering works, and it is the core idea of stochastic rounding).

Practical rules for dither, which will matter from Chapter 9 onward when you write 16-bit files:

- Dither only when **reducing** bit depth — e.g. 24-bit or float down to 16-bit for delivery.
- Dither **once**, at the very last step. Repeated dithering accumulates noise.
- **Noise-shaped dither** pushes the added noise into frequency regions where the ear is less
  sensitive (up around 15–18 kHz), which can make it perceptually 10–15 dB quieter than flat
  dither at the same true level. Any mastering tool offers several shapes.
- For loud, dense material it is nearly inaudible either way. For quiet classical recordings,
  long reverb tails and fade-outs, it is clearly worth it.

---

## 4.7 The formats your samples can be stored in

This is a practical section; you will implement all of these in Chapters 9 and 16.

### Integer PCM

**PCM** (Pulse Code Modulation) is just the plain name for "store the sample values directly".

**16-bit signed integer** — by far the most common file format. Range −32,768 to +32,767.
Conventionally, −32,768 maps to −1.0 and +32,767 maps to +1.0.

Note the asymmetry: there is one more negative value than positive. This is a property of
two's-complement representation, and it creates a small nuisance when converting. The safe
conversion, which we will use throughout:

```
   float -> int16:   i = round(clamp(f, -1.0, 1.0) * 32767.0)
   int16 -> float:   f = i / 32768.0
```

Using 32767 when going *out* and 32768 when coming *in* guarantees you never overflow in either
direction. Using 32768 on the way out lets a sample at exactly +1.0 wrap around to a large
negative number — a very loud click, and a classic beginner bug.

**24-bit signed integer** — range ±8,388,608. Awkward in code because there is no 24-bit type;
you handle three bytes manually. Chapter 16 shows how.

**8-bit** — in WAV files, 8-bit is *unsigned* (0–255, with 128 as silence), for historical
reasons, while 16- and 24-bit are signed. This inconsistency has caused an enormous amount of
confusion and at least one generation of broken loaders. Be careful.

### Floating point

**32-bit float** stores values as a sign, an 8-bit exponent, and 23 bits of mantissa. The
consequences for audio are excellent:

- The usable range is astronomically large (roughly 10^−38 to 10^38), so **you cannot clip
  internally**. A signal that hits 50.0 is not damaged; turn it down later and it is intact.
  With integers, exceeding full scale destroys the sample permanently.
- Precision is *relative*: about 7 significant decimal digits at any magnitude. Quiet signals
  get the same relative precision as loud ones, which matches how hearing works.
- Effective dynamic range within any small region is about 144 dB (24 bits of mantissa
  including the implicit leading bit) — equal to 24-bit integer — but the range *slides* with
  the exponent.

**This is why every serious audio application processes in 32-bit float internally**, converting
to integer only at the file or hardware boundary. We do the same: `float` everywhere inside,
integers only in the WAV writer.

The convention: **−1.0 to +1.0 is full scale.** Exceeding it internally is harmless; exceeding
it *at the output* clips.

**64-bit double** is used for filter coefficients and for anything involving long feedback
chains or accumulated phase, where small errors compound. We use `double` for phase accumulators
and coefficient computation, `float` for sample data — a standard and sensible split, and
Chapter 30 explains exactly why phase accumulators in particular need the extra precision.

### Channels and interleaving

A stereo file contains two streams. There are two ways to lay them out:

**Interleaved** — alternating, `L R L R L R ...`. This is what WAV files use and what most audio
hardware APIs expect, because it keeps the samples that are played at the same instant next to
each other in memory.

```
   [L0][R0][L1][R1][L2][R2]...
```

**Planar (de-interleaved)** — each channel in its own contiguous block:

```
   L: [L0][L1][L2]...      R: [R0][R1][R2]...
```

Planar is much more convenient for DSP (you can process a channel with a simple loop, and it
vectorises well with SIMD — Chapter 63), so most engines de-interleave on input and re-interleave
on output. We will do exactly that, and Chapter 17 puts the conversion in `libaudio`.

---

## 4.8 Frames, blocks, and buffers — vocabulary that trips people up

Three words that beginners routinely conflate, and that cause real bugs when confused:

- **Sample**: one value, one channel. A single number.
- **Frame**: one sample for *every* channel, at one instant. A stereo frame is 2 samples.
- **Buffer** / **block**: a group of frames processed together, typically 64 to 2048 frames.

So a stereo buffer of 512 frames contains **1,024 samples** and occupies 4,096 bytes as 32-bit
float.

Getting this wrong is the source of a famous class of bugs where everything plays at double
speed, half speed, or with the channels swapped. When a library's documentation says "size",
always establish whether it means samples, frames, or bytes. Some APIs use all three in
different functions. The convention in this book: variables are named `numFrames`,
`numSamples`, or `numBytes`, never just `size`.

**Buffer size determines latency**, which is the other reason it matters:

```
   latency (seconds) = buffer frames / sample rate
```

| Buffer size | Latency at 44.1 kHz | Feel |
|---|---|---|
| 64 frames | 1.45 ms | Imperceptible; demanding on the CPU |
| 128 frames | 2.90 ms | Excellent for live playing |
| 256 frames | 5.80 ms | Comfortable; a common default |
| 512 frames | 11.6 ms | Noticeable when playing an instrument |
| 1024 frames | 23.2 ms | Clearly laggy for performance; fine for playback |
| 2048 frames | 46.4 ms | Playback only |

Smaller buffers mean lower latency but more callbacks per second, each with fixed overhead, and
less slack to absorb a momentarily slow computation. This trade-off is the central tension of
Part V, and Chapter 56 examines it properly.

---

## 4.9 Putting the whole chain together

The complete path, with every transformation named:

```
   ACOUSTIC        Pressure wave in air
       |
       v
   TRANSDUCTION    Microphone: pressure -> voltage      (continuous, continuous)
       |
       v
   ANTI-ALIAS      Low-pass filter below fs/2           (continuous, continuous)
       |
       v
   SAMPLING        Measure every Ts seconds             (DISCRETE time, continuous value)
       |
       v
   QUANTIZATION    Round to nearest of 2^N levels       (DISCRETE time, DISCRETE value)
       |
       v
   STORAGE         Bytes in a file or array             <-- YOUR PROGRAM LIVES HERE
       |
       v
   QUANTIZATION    (Convert back to analogue levels)
       |
       v
   RECONSTRUCTION  Low-pass filter at fs/2 (sinc)       (continuous again)
       |
       v
   AMPLIFICATION   Voltage -> more voltage
       |
       v
   TRANSDUCTION    Speaker: voltage -> pressure
       |
       v
   ACOUSTIC        Pressure wave in air -> your ear
```

Between the two filters, everything is numbers. That middle section is the entire subject of
this book, and the two filters at its boundaries are what make the numbers meaningful.

---

## 4.10 Discussion: what digital audio actually costs

It is worth being precise about the losses, because this territory is full of confident
misinformation in both directions.

**What sampling costs:** nothing, provided the signal is band-limited below Nyquist. This is a
theorem, not an opinion. The practical caveats are in the *filters* at either end — real
anti-alias and reconstruction filters are not ideal, and their imperfections (phase response,
ripple, ringing) are the genuine source of converter quality differences.

**What quantization costs:** a noise floor at a known level, plus signal-correlated distortion
if you fail to dither. At 16 bits with proper dither, that floor is around −96 dBFS, well below
the noise floor of any real recording environment.

**What aliasing costs:** everything, if you let it happen. It is the only irreversible failure
in the chain, and — importantly for us — it is the one most likely to be *your* fault, because
synthesis and nonlinear processing generate it internally where no converter can protect you.

**What clipping costs:** harsh odd-order distortion and, in integers, permanent damage to the
sample. The float pipeline in this book means internal clipping is impossible, but the *output*
stage is still a hard boundary. Chapter 14 covers the discipline of gain staging.

So the honest summary: digital audio is not a lossy approximation of analogue. It is an exact
representation of a band-limited signal, with a known noise floor. The failure modes are real
but they are all *engineering* failures — bad filters, aliasing, clipping, wrong gain — rather
than intrinsic to the medium. That is a good position to be in, because engineering failures can
be understood, measured, and fixed, and over the next ninety chapters you will do all three.

---

## 4.11 Exercises

**4.1** A signal is sampled at 48 kHz. What is the Nyquist frequency? A 26 kHz tone is present
(someone forgot the anti-alias filter). What frequency will it appear as after sampling?

**4.2** How many bytes does a 3-minute stereo 24-bit 48 kHz file occupy? Show your working.

**4.3** You record at 16-bit and, being cautious, set your levels so the peak reaches only
−30 dBFS. What effective dynamic range are you left with? Repeat for 24-bit. This exercise is
the entire argument for 24-bit recording.

**4.4** Explain to a sceptical friend, in three sentences, why the "stairstep" picture of digital
audio is wrong. Then explain what inter-sample peaks are and why they follow from your answer.

**4.5** A naïve sawtooth oscillator at 3 kHz is generated at 44.1 kHz with harmonics at every
integer multiple. List the first five harmonics that alias, and give the (incorrect) frequency
each will appear at. Are any of them harmonically related to 3 kHz?

**4.6** Why is dither applied *before* quantization rather than after? What would adding noise
after quantization achieve?

**4.7** A buffer of 256 frames at 48 kHz. What is the latency in milliseconds? How many times per
second is the audio callback invoked? If your processing takes 4 ms per callback, are you safe?

**4.8** Convert these float sample values to 16-bit integers using the rule in §4.7:
`0.0`, `0.5`, `-0.5`, `1.0`, `-1.0`, `1.2`. What must happen to the last one, and what would you
hear if it were not handled?

---

### Chapter summary

- Two independent approximations turn sound into numbers: **sampling** (discrete time, governed
  by sample rate) and **quantization** (discrete amplitude, governed by bit depth).
- The **Nyquist frequency** is `fs/2`. The **Sampling Theorem** guarantees *exact* reconstruction
  of any signal band-limited below it — sampling itself loses nothing.
- Content above Nyquist **aliases**: it folds down to `fs − f` and becomes indistinguishable from
  a legitimate lower frequency. Aliasing is irreversible and must be prevented, including in your
  own synthesis code.
- Reconstruction is sinc interpolation, not a staircase. This is why inter-sample peaks exist and
  why true-peak metering is necessary.
- Quantization adds a noise floor of about `6.02N + 1.76` dB of dynamic range. 16-bit gives 96 dB
  (enough for delivery); 24-bit gives 144 dB (enough to be careless while recording).
- **Dither** — adding ~1 LSB of noise before quantizing — converts ugly signal-correlated
  distortion into benign noise and recovers detail below the least significant bit.
- Process internally in 32-bit **float** with ±1.0 as full scale; convert to integers only at the
  boundaries. Use `double` for phase and coefficients.
- Know the difference between **sample**, **frame**, and **buffer**. Buffer size divided by
  sample rate is your latency.

**Next:** [Chapter 5 — Setting Up Your C++ Toolchain](05-setting-up-your-toolchain.md)
