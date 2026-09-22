# Chapter 1 — What You Are About to Learn

> *"Sound is the only art form that happens inside your body."*

---

## 1.1 The promise of this book

By the end of this book you will be able to sit down at an empty text file and write, from
nothing, a program that produces sound. Not "play a sound file" — *produce* sound. You will
compute every single number that becomes the movement of a speaker cone, and you will know
why each number has the value it has.

That is a much bigger claim than it sounds like, so let me be concrete about the destination.
Here is a list of things you will have built by the last chapter:

1. A `.wav` file writer, written byte by byte, with no library.
2. A sine-wave generator, and an understanding of why the sine wave is the atom of all sound.
3. A set of classic synthesiser oscillators — saw, square, triangle — that do *not* produce
   the ugly digital whine that naïve implementations produce.
4. Filters: low-pass, high-pass, band-pass, shelving, peaking. Written from the underlying
   mathematics, not copy-pasted.
5. A Fast Fourier Transform, implemented by hand, and a spectrogram renderer that turns a
   sound into an image you can look at.
6. A polyphonic synthesiser with envelopes, LFOs, modulation routing, and voice management.
7. Delay, chorus, flanger, phaser.
8. Four different reverbs, including a convolution reverb that can put your sound inside a
   real cathedral using a recording of that cathedral.
9. A compressor, a limiter, a gate, a sidechain — the tools that make things sound "produced".
10. Real-time audio: sound coming out of your speakers as the program runs, with a callback,
    a lock-free queue, and no glitches.
11. MIDI input, so a keyboard can play your synthesiser.
12. Binaural 3D audio: sound that appears to come from *behind you*, over ordinary headphones.
13. Ambisonics, a surround renderer, and an object-based mixer of the kind that sits inside
    Dolby Atmos.
14. Cinematic sound design tools: risers, braams, sub-drops, impacts, whooshes — generated
    procedurally, in code.
15. A cinematic audio engine that takes a scene description and renders a finished mix.

Along the way you will learn C++ — properly, not as a set of incantations. Audio is an
unusually good subject for learning to program, and I want to explain why, because it will
change how you approach the exercises.

---

## 1.2 Why audio is a great way to learn to program

Most beginner programming courses have you print text to a screen. You write
`std::cout << "Hello"` and the word "Hello" appears. That is fine, but it teaches you almost
nothing about the two things that actually make programming hard: **state** and **time**.

Audio teaches both, immediately and viscerally.

**Audio is a firehose.** At the standard rate of 44,100 samples per second, a three-minute
stereo song is about 16 million numbers. You cannot inspect them by hand. You are forced,
from day one, to think in terms of *processes applied to streams* rather than individual
values. That is the single most important mental shift in programming, and audio hands it to
you for free.

**Audio has no hiding place.** If your loop is off by one, you hear a click. If your maths
overflows, you hear a horrible crunch. If your filter is unstable, your speakers scream. If
your timing is wrong by one part in a thousand, a musician will notice. Text output lets you
get away with sloppiness; audio does not. Bugs become *audible*, which means your ears become
a debugger — and ears are astonishingly good debuggers. A trained audio programmer can often
tell you what is wrong with a program by listening to its output for two seconds.

**Audio is immediately rewarding.** The gap between "I changed a number" and "the world sounds
different" is about one second. That tight feedback loop is the thing that keeps people
learning. Nobody has ever stayed up until 3 a.m. excited about a sorting algorithm. People do
that with synthesisers constantly.

**Audio is real engineering.** Real-time audio is one of the few areas of everyday software
where hard deadlines exist. Your code has, say, 5.8 milliseconds to produce 256 samples. Not
"usually 5.8 milliseconds" — every single time, forever, or the user hears a glitch. This
forces you to learn about memory allocation, locks, cache behaviour, and the actual cost of
operations. Audio programmers are famously good systems programmers, and this is why.

---

## 1.3 The two halves of this book: "normal" sound and "cinematic" sound

You asked for normal sound first, then cinematic sound. That split is exactly right, and it is
worth spelling out what it means, because it is not a difference of quality — it is a
difference of *purpose*.

### Normal sound: correctness

The first two-thirds of this book is about making sound that is **correct**. A 440 Hz sine
wave that is actually 440 Hz. A low-pass filter whose cutoff is really where you said it was.
A delay of exactly 250 milliseconds. A mix that does not clip. This is engineering: there are
right answers, and you can measure whether you got them.

Correctness is not glamorous but it is non-negotiable. Every impressive cinematic effect in
Part VIII is built out of the correct primitives from Parts I–IV. A braam is a stack of
detuned saw oscillators through a filter through a distortion through a reverb. If your saw
oscillator aliases, your braam sounds cheap, and no amount of artistry fixes it. The craft
sits on top of the engineering, always.

### Cinematic sound: meaning

The last third is about making sound that **means something**. This is a different skill, and
it obeys different rules. Consider a concrete example that I will return to many times:

> A door closes in a film. The sound designer does not use the sound of a door closing. They
> use a layered composite: the *click* of a rifle bolt for the latch, the *thud* of a
> weighted studio door for the body, and a long, almost inaudible sub-bass tail for the sense
> of finality. The result is not what a door sounds like. It is what a door closing *feels
> like* when the scene is about someone being shut out.

This is the central idea of cinematic sound. Realism is not the goal; **perceived meaning** is
the goal. A punch in a film sounds nothing like a punch. A spaceship makes no sound at all in
reality. A sword being drawn from a scabbard is mostly silent; in film it is a bright metallic
*shring* because that sound says "danger, and it is sharp".

So Part VIII is about three things at once:

- **Psychoacoustics** — the measurable facts about how human hearing produces feelings.
  Low frequencies below about 60 Hz are felt in the chest more than heard, which is why
  dread and power live down there. Frequencies around 2–4 kHz are where the ear is most
  sensitive, which is why that band means "urgent, alarming, close".
- **Craft** — the accumulated techniques of the profession: layering, pre-lapping, the use of
  silence, frequency-slotting so dialogue survives an explosion.
- **Code** — building the tools that make it possible: object-based renderers, convolution
  reverbs, loudness meters, procedural generators.

You cannot fake your way into the last third. But you also cannot get the last third out of
the first two-thirds alone: knowing how to implement a reverb does not tell you that a
conversation in a cathedral should have its reverb *ducked* under the dialogue so the words
stay intelligible while the space stays big. Both halves are necessary.

---

## 1.4 What "audio programming" actually consists of

It helps to have a map of the territory before you walk into it. Audio programming divides
into roughly six activities. Every chapter in this book belongs to one of them.

**1. Generation (synthesis).** Producing sound from nothing but numbers. Oscillators,
noise generators, physical models. The input is a description ("a 440 Hz saw wave, loud, for
two seconds"); the output is samples. Part III.

**2. Transformation (effects / DSP).** Taking existing samples and changing them. Filters,
delays, reverbs, compressors, distortion, pitch-shifters. The input is samples; the output is
different samples. Parts II and IV.

**3. Analysis.** Taking samples and producing *facts* about them. What is the pitch? How loud
is it? What frequencies are present? Is there a beat, and where? Analysis powers everything
from auto-tune to loudness compliance to visualisers. Mostly Part II, with applications
scattered throughout.

**4. Transport and I/O.** Getting samples to and from the outside world: files, sound cards,
network streams, disk streaming, format conversion. Parts I and V.

**5. Timing and scheduling.** Deciding *when* things happen, to sample accuracy. Sequencers,
tempo, MIDI, synchronisation to picture. Part VI.

**6. Spatialisation.** Deciding *where* things appear to come from. Panning, binaural
rendering, ambisonics, distance, occlusion, room simulation. Part VII.

Notice that "mixing" is not on the list. Mixing is not a seventh activity; it is the
combination of transformation, spatialisation, and timing applied with taste. We will treat it
that way.

---

## 1.5 A minimal mental model to carry with you

Before any code, install this picture in your head. Nearly everything in the book is a
variation on it.

```
   Something          Something            Something
   vibrates    -->    carries it    -->    detects it    -->    Something
   (source)           (medium)             (transducer)         understands it
                                                                 (brain / program)
```

In the physical world:

```
   Guitar string  -->  Air  -->  Microphone  -->  Your ears
```

Inside a computer, the same chain exists, but the "carrier" is a list of numbers:

```
   Your code       -->   An array of        -->   Digital-to-analogue  -->  Speaker  -->  Air  -->  Ear
   (the source)          floating-point           converter (DAC)
                         numbers
```

The entire craft of audio programming is the middle box: **producing and manipulating that
array of numbers**. Everything else — the DAC, the amplifier, the speaker, the air, the
eardrum — is somebody else's problem, and mercifully it is a solved problem.

So here is the single sentence definition you should memorise:

> **Audio programming is the art of computing a list of numbers that, when converted to
> voltage and used to push a speaker cone back and forth, makes a human being feel
> something.**

Every chapter is a technique for computing that list of numbers with more control and more
intent than the chapter before it.

---

## 1.6 How to read this book (this section matters — please read it)

I have watched a lot of people try to learn this material. The ones who succeed do four
things. The ones who fail skip them.

### 1. Type the code. Do not copy-paste it.

This feels like superstition and it is not. Typing forces your eye across every character.
You will notice the `<` that should be `<=`. You will make typos, get compiler errors, and
learn to read compiler errors — which is a genuine skill that takes maybe forty errors to
acquire and pays off for the rest of your life. Copy-pasting a working program teaches you
approximately nothing.

Every program in this book is also stored in `code/`, so you can check yours against mine.
Check *after* you have typed it and tried to run it, not before.

### 2. Break things deliberately.

Each chapter has **Experiment** boxes. They look optional. They are not. When a box says
"change `0.5` to `5.0` and predict what happens", the important part is not the change — it is
the *prediction*. Write your prediction down. If you were right, you understand the system. If
you were wrong, you have just found a gap in your mental model at the exact moment you are
best placed to fix it.

The deepest understanding in audio comes from deliberate breakage. Make a filter unstable on
purpose so that you recognise the sound of instability forever. Make a buffer overflow so you
know what a click is. Set a delay feedback above 1.0 once, at low volume, so you know what
runaway feedback feels like.

### 3. Listen on something honest, and protect your ears.

You do not need expensive equipment. You do need to know what your equipment lies about.
Laptop speakers produce essentially nothing below about 200 Hz, so you will not hear the
sub-bass work in Part VIII on them at all. Cheap earbuds usually exaggerate bass and treble.
Pick one pair of headphones and use them for the whole book so that your reference stays
constant, and learn its personality.

And now the serious warning, which I will repeat in Chapter 10:

> **Turn your volume down before running any program you have just written.**
>
> A single sign error can produce full-scale digital noise. Full-scale digital noise through
> headphones at a normal listening volume is loud enough to cause permanent hearing damage.
> Not "uncomfortable" — permanent. Hearing loss does not heal.
>
> Habit to build now: volume to near-zero, run the program, raise the volume until you can
> hear it. Every time. Professionals do this instinctively; it is the audio equivalent of
> checking the chamber of a firearm.

### 4. Do not skip the theory to get to the code.

This is the most common failure mode, and it is seductive, because code *runs* and theory does
not. But audio is a field where the code is short and the understanding is long. A biquad
filter is eleven lines of C++. Those eleven lines took the field about forty years to arrive
at, and if you do not understand what they mean you cannot debug them, tune them, or extend
them — you can only copy them.

When you hit a theory section that feels heavy, the correct move is to keep reading at a slower
pace, not to jump ahead. I have tried very hard to make every piece of theory in this book
arrive *just before* the code that needs it, and to justify why it is there.

---

## 1.7 What you need

**Hardware.** Any computer from the last fifteen years. Audio DSP is computationally cheap by
modern standards; the machine you have is fine.

**Software.** A C++ compiler and a text editor. Chapter 5 walks through installing a compiler
on Windows step by step, with screenshots of what the output should look like. You are already
in VS Code, which is a perfect editor for this.

**Headphones or speakers.** Discussed above.

**Mathematics.** Here is the honest answer, because people are often scared off at this point.

You need, and must be comfortable with:

- Arithmetic and fractions.
- What a function is: you put a number in, you get a number out.
- Basic algebra: rearranging `y = 2x + 3` to get `x`.

You need to *meet*, and will be taught here from scratch:

- Sine and cosine, as circular motion rather than as triangles. Chapter 10 and Appendix A.
- What a logarithm is, and why decibels use one. Chapter 11.
- Complex numbers, taught as "rotations" rather than as "imaginary". Chapter 19.
- The idea of a sum over a sequence (the Σ notation). Chapter 18.

You do **not** need:

- Calculus. There are perhaps five places where a derivative or integral clarifies something,
  and in each case I give the plain-language version too.
- Linear algebra, beyond "a matrix is a grid of numbers that transforms a list of numbers".
  Used only in Chapters 47 and 79.
- Any university mathematics.

The reputation of audio DSP as mathematically forbidding comes from textbooks written for
electrical engineering graduates. The *ideas* are genuinely accessible; it is the notation
that is hostile. I will introduce notation only when it earns its place, and I will always
give you the plain-English sentence first.

---

## 1.8 The shape of the journey

Here is the emotional arc, so you know where you are when you are in it.

**Part 0 (Chapters 1–7): Ground floor.** Sound physics, hearing, and enough C++ to compile a
program. This part has the least audio in it and is the most necessary. If you already
program, you can skim Chapters 6 and 7 — but read Chapters 2, 3, and 4 carefully even if you
think you know them, because the way this book frames sampling is the foundation for
everything.

**Part I (8–17): First light.** You write bytes to a file and sound comes out of a media
player. The moment your first sine wave plays is the moment the whole subject becomes real.
This part is enormously satisfying.

**Part II (18–29): The hard climb.** Filters, convolution, the Fourier transform. This is the
steepest part of the book and there is no way around it. It is also the part that separates
people who can use audio tools from people who can build them. Take it slowly. Expect a
chapter to take a couple of days. When you come out the other side you will be able to read
almost any DSP paper.

**Part III (30–41): Play.** Synthesis. This is the fun part and it is where the tools from Part
II start paying dividends. Prepare to lose entire evenings to twisting parameters.

**Part IV (42–55): Craft.** Effects. Each chapter here is a self-contained, genuinely useful
processor. This part also teaches most of what you need to know about *taste*, because effects
are where taste shows.

**Part V (56–65): Engineering.** Real-time. This is where you become a systems programmer
whether you meant to or not. Mentally the hardest part after Part II, but hard in a different
way — less mathematics, more discipline.

**Part VI (66–71): Time.** Music, sequencing, and structure. A breather after Part V.

**Part VII (72–79): Space.** Psychoacoustics and 3D audio. Full of genuinely surprising
material about your own perception. Many people find this the most interesting part of the
book.

**Part VIII (80–91): Meaning.** Cinematic sound. Theory-heavy, craft-heavy, and the reason you
picked this up. It will feel like a different discipline, because it is.

**Part IX (92–95): Proof.** Three capstone projects that combine everything.

---

## 1.9 One warning about the culture you are entering

Audio programming has a slightly odd relationship with rigour. On one side you have DSP
researchers who care about provable stability and numerical precision. On the other you have
sound designers who will happily tell you that a particular reverb "sounds warmer" when run
at 48 kHz. Both communities are correct about different things, and both occasionally talk
nonsense about the other's domain.

My approach in this book: **measure what can be measured, and be honest about what cannot.**
When I say a filter is stable, I will show you why. When I say a sound feels more powerful, I
will tell you that it is a perceptual claim, give you the psychoacoustic reason if there is
one, and let you judge with your own ears. You should get in the habit of asking which kind of
claim you are being offered — in this book and everywhere else.

You will also meet a lot of audio folklore. Some of it is true and poorly explained, some is
true only for analogue gear, and some is nonsense. By Part IV you will have the tools to test
folklore yourself, which is one of the more satisfying skills this subject gives you.

---

## 1.10 What to do next

Read Chapter 2. It contains no code. It explains what sound is with more precision than you
probably currently have, and every idea in it becomes a variable name later in the book.

Then read Chapter 3, on hearing, which is where the *cinematic* half of the book secretly
begins — because cinematic sound is applied psychoacoustics, and psychoacoustics starts with
the anatomy of the ear.

One last thing. This is a long book, and long books get abandoned. The way to not abandon it
is to keep the loop tight: read a section, type the code, hear the result. As long as you are
hearing things, you will keep going. If you find yourself three chapters deep in reading
without having compiled anything, stop and go build something, even something silly. The
subject rewards curiosity enormously — perhaps more than any other area of programming,
because at the end of every experiment there is a sound, and sounds are interesting in a way
that print statements never are.

Let us begin with air.

---

### Chapter summary

- Audio programming is computing a list of numbers that becomes speaker movement.
- The first two-thirds of this book is about **correctness**; the last third is about
  **meaning**. Cinematic sound is built out of correct primitives, so the order matters.
- Six activities: generation, transformation, analysis, I/O, timing, spatialisation.
- Type the code, break things on purpose, listen on consistent gear, turn the volume down
  *before* running anything, and do not skip the theory.
- The mathematics you need is small and will be taught here. The notation is the scary part,
  not the ideas.

**Next:** [Chapter 2 — What Sound Actually Is](02-what-sound-actually-is.md)
