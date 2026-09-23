# Chapter 27 — The STFT and Spectrograms

> A single FFT tells you what frequencies are in a block of audio. It does not tell you *when*.
> Music, speech and sound effects are entirely about *when*. The Short-Time Fourier Transform
> fixes that, and it is the foundation of every spectral effect in Parts IV and VIII.

---

## 27.1 The problem with one big FFT

Take a three-minute song and FFT the whole thing. You get 8 million bins with superb frequency
resolution — 0.005 Hz — and **no time information whatsoever**. The result tells you which
frequencies appeared at some point during the song. It cannot distinguish a song from the same
song played backwards.

The fix is obvious: chop the signal into short frames and transform each one.

```
   signal:   [================================================]

   frames:   [--f0--]
                  [--f1--]
                        [--f2--]
                              [--f3--]         (overlapping)
                                    [--f4--]

   result:   a spectrum for each frame -> a 2D map of frequency vs time
```

That is the **Short-Time Fourier Transform (STFT)**, and the 2D map is a **spectrogram**.

---

## 27.2 The three parameters

**Frame size `N`.** How many samples per FFT. Sets frequency resolution (`fs/N`) and time
resolution (`N/fs`), and Chapter 26 told you these fight each other.

**Hop size `H`.** How far you advance between frames. The **overlap** is `1 - H/N`.

| Hop | Overlap | Frames/sec at N=1024, 44.1k | Notes |
|---|---|---|---|
| `N` | 0% | 43 | Cheapest. Windows do not sum flat — resynthesis fails. |
| `N/2` | 50% | 86 | Hann sums to exactly 1.0. The standard for analysis. |
| `N/4` | 75% | 172 | Hann sums to 2.0. Standard for resynthesis and pitch shifting. |
| `N/8` | 87.5% | 344 | Very smooth; 8× the CPU. Used for high-quality time stretching. |

**Window type.** Chapter 26. For anything you intend to reconstruct, **Hann** — because of COLA.

### The COLA condition

If you are going to modify the spectrum and transform back, the overlapping windows must sum to a
constant:

```
     Σ  w[n - mH]  =  constant,  for all n
     m
```

Otherwise the output has periodic amplitude ripple at the frame rate — an audible buzz at
`fs/H` Hz.

Hann satisfies this exactly at 50% and 75% overlap. That single fact is why Hann is the default
window in every phase vocoder, spectral gate and pitch shifter ever written.

> **A subtlety for resynthesis:** if you window on *both* analysis and synthesis (which is
> standard, because it suppresses artefacts introduced by modification), the *squared* window
> must satisfy COLA. Hann² at 75% overlap sums to 1.5, which is constant — fine. Hann² at 50%
> overlap does **not** sum to a constant. This is the practical reason phase vocoders use 75%
> overlap rather than 50%.

---

## 27.3 The implementation

**Code — `lib/include/audio/stft.h`** (core)

```cpp
namespace audio {

struct STFTFrame
{
    std::vector<Complex> bins;      // fftSize bins (only 0..N/2 are unique)
    size_t startSample = 0;
};

class STFT
{
public:
    void prepare(size_t fftSize, size_t hopSize,
                 AnalysisWindow win = AnalysisWindow::Hann)
    {
        fftSize_ = nextPowerOfTwo(fftSize);
        hopSize_ = hopSize;
        window_  = makeWindow(win, fftSize_);
        gain_    = coherentGain(window_);
    }

    // Analyse a whole signal into frames.
    std::vector<STFTFrame> analyse(const std::vector<float>& x) const
    {
        std::vector<STFTFrame> frames;
        if (x.empty() || fftSize_ == 0) return frames;

        for (size_t start = 0; start + fftSize_ <= x.size(); start += hopSize_)
        {
            std::vector<Complex> buf(fftSize_);
            for (size_t n = 0; n < fftSize_; ++n)
                buf[n] = Complex(static_cast<double>(x[start + n]) * window_[n], 0.0);

            fft(buf, false);
            frames.push_back({ std::move(buf), start });
        }
        return frames;
    }

    // Reconstruct by overlap-add. With a Hann window at 50% or 75% overlap
    // this is near-perfect (error ~1e-6).
    std::vector<float> synthesise(const std::vector<STFTFrame>& frames,
                                  size_t outputLength) const
    {
        std::vector<float>  out(outputLength, 0.0f);
        std::vector<double> norm(outputLength, 0.0);   // running window sum

        for (const auto& f : frames)
        {
            auto buf = f.bins;
            fft(buf, true);                             // inverse

            for (size_t n = 0; n < fftSize_; ++n)
            {
                const size_t idx = f.startSample + n;
                if (idx >= outputLength) break;

                // Window again on synthesis, then divide by the summed
                // squared window. This is weighted overlap-add (WOLA).
                out[idx]  += static_cast<float>(buf[n].real() * window_[n]);
                norm[idx] += window_[n] * window_[n];
            }
        }

        // Normalise by the actual window sum: exact, and it handles the
        // partial overlap at the very start and end correctly.
        for (size_t n = 0; n < outputLength; ++n)
            if (norm[n] > 1e-9)
                out[n] = static_cast<float>(out[n] / norm[n]);

        return out;
    }

private:
    size_t              fftSize_ = 1024;
    size_t              hopSize_ = 256;
    std::vector<double> window_;
    double              gain_ = 1.0;
};

}   // namespace audio
```

### Walkthrough

**The dividing-by-`norm` step is the important one.** Rather than relying on the window summing
to a known constant, we accumulate the actual sum of squared windows and divide by it. This:

- Works for any window and any hop size, not just COLA-satisfying combinations.
- Handles the first and last few frames correctly, where fewer windows overlap. Without it,
  reconstructed audio fades in and out at the edges — a classic STFT artefact.
- Costs one extra buffer and one division per sample. Worth it.

**Perfect reconstruction** is the test to run first. Analyse, do nothing, synthesise, and compare
with the input. The error should be around 1e-6. If it is larger, or if the output has periodic
amplitude ripple, the window/hop combination is wrong.

---

## 27.4 Drawing a spectrogram

We have no plotting library, so we write an image file directly. **PGM** (Portable GrayMap) is
the simplest image format in existence — a text header and then the pixel bytes:

```
P5
width height
255
<binary pixel data>
```

Every image viewer and web browser handles it, and GIMP, IrfanView and ImageMagick all open it.

```cpp
bool writePGM(const std::string& path, const std::vector<uint8_t>& pixels,
              size_t width, size_t height)
{
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;

    f << "P5\n" << width << " " << height << "\n255\n";
    f.write(reinterpret_cast<const char*>(pixels.data()),
            static_cast<std::streamsize>(pixels.size()));
    return f.good();
}
```

And the spectrogram itself:

```cpp
void writeSpectrogram(const std::string& path,
                      const std::vector<float>& signal,
                      double sampleRate,
                      size_t fftSize = 1024,
                      size_t hopSize = 256,
                      double floorDb = -90.0,
                      bool logFrequency = true)
{
    STFT stft;
    stft.prepare(fftSize, hopSize);
    auto frames = stft.analyse(signal);
    if (frames.empty()) return;

    const size_t width  = frames.size();
    const size_t height = 512;
    std::vector<uint8_t> pixels(width * height, 0);

    const size_t numBins = fftSize / 2 + 1;

    for (size_t x = 0; x < width; ++x)
    {
        const auto mag = magnitudeSpectrumDb(frames[x].bins);

        for (size_t y = 0; y < height; ++y)
        {
            // y = 0 is the TOP of the image, so invert: high frequency up.
            const double v = 1.0 - static_cast<double>(y) / (height - 1);

            size_t bin;
            if (logFrequency)
            {
                // Map the image vertically over 20 Hz .. Nyquist, log spaced,
                // because hearing is logarithmic (Chapter 3).
                const double fMin = 20.0, fMax = sampleRate / 2.0;
                const double f = fMin * std::pow(fMax / fMin, v);
                bin = static_cast<size_t>(f * fftSize / sampleRate);
            }
            else
            {
                bin = static_cast<size_t>(v * (numBins - 1));
            }

            if (bin >= numBins) bin = numBins - 1;

            // Map dB to 0..255
            const double db = std::max(mag[bin], floorDb);
            const double t  = (db - floorDb) / (0.0 - floorDb);
            pixels[y * width + x] =
                static_cast<uint8_t>(std::clamp(t, 0.0, 1.0) * 255.0);
        }
    }

    writePGM(path, pixels, width, height);
}
```

**Two decisions worth noting.**

**Logarithmic frequency axis.** Chapter 3 established that hearing is logarithmic. A linear
frequency axis squeezes the bottom five octaves into the bottom 5% of the image — where nearly
all musical information lives. Every useful spectrogram uses a log axis. This is why the default
is `logFrequency = true`.

**dB magnitude with a floor.** Linear magnitude makes everything below the loudest peak look
black. A −90 dB floor mapped across the full grey range shows reverb tails, noise floors and
quiet detail.

---

## 27.5 Reading a spectrogram

This is a skill worth developing — you can diagnose an enormous amount by eye.

```
  freq
   |                                             LEGEND
   |  ~~~~~~~~~~~~~~~~~~~~   broadband noise
   |  ||||||||||||||||||||   harmonic stack (a pitched note)
   |  |                      vertical line = transient/click
   |   \_____                descending line = a sweep down
   |  ####                   solid block = loud broadband (an impact)
   |  ....''''....           wavy line = vibrato
   +----------------------> time
```

| What you see | What it means |
|---|---|
| Evenly spaced horizontal lines | A pitched, harmonic sound. The spacing *is* the fundamental. |
| Lines not evenly spaced | Inharmonic — metal, bells, noise-like percussion (Ch 39) |
| A full-height vertical line | A transient: a click, a drum hit, or an edit point |
| A smooth diagonal | A frequency sweep — a riser (Ch 84) or a Doppler pass (Ch 77) |
| Wavy horizontal lines | Vibrato or chorus modulation |
| Energy fading gradually after an event | Reverb tail. The *slope* is the RT60. |
| A hard horizontal cutoff near the top | Lossy codec (MP3 cuts at 16–20 kHz) or a low-pass |
| Regular vertical stripes | Amplitude modulation, or a buffer-boundary bug |
| A haze filling the gaps | Noise floor, or excessive reverb/compression |
| Bright band at 2–5 kHz during speech | Consonants — the intelligibility band (Ch 89) |

**This is genuinely diagnostic.** A spectrogram will show you:

- Whether a "hiss" problem is broadband noise or a specific resonance.
- Whether a file has been through a lossy codec (the hard shelf at the top).
- Exactly where a click is, to the frame.
- Whether a synth is aliasing (Chapter 12's descending ghosts are visible as diagonals running
  the *wrong* way).

That last one is worth doing: render Chapter 12's `alias_naive_saw.wav`, make a spectrogram, and
watch the aliased partials descend while the fundamental ascends. Seeing it makes the arithmetic
of Chapter 4 permanent.

---

## 27.6 The time-frequency trade-off, visually

`examples/ch27_spectrogram.cpp` renders the same signal — a mix of a sustained chord and sharp
percussion — at four FFT sizes:

| `N` | Frequency resolution | Time resolution | The picture |
|---|---|---|---|
| 128 | 345 Hz | 2.9 ms | Drums are razor-sharp vertical lines; the chord is an unreadable smear |
| 512 | 86 Hz | 11.6 ms | Both visible, both imperfect |
| 2048 | 21.5 Hz | 46 ms | Chord harmonics are crisp lines; drums are smeared horizontal blobs |
| 8192 | 5.4 Hz | 186 ms | Individual harmonics beautifully resolved; drums have vanished entirely |

**You cannot have both, and the images make it visceral in a way no equation does.** This is why
professional analysers offer a resolution control, and why some use **multi-resolution** analysis
— a short window for the high frequencies where time matters, a long one for the low frequencies
where pitch matters. That is essentially what the wavelet transform and the constant-Q transform
do, and it is also, not coincidentally, what your cochlea does (Chapter 3: the basilar membrane's
filters are wider at high frequencies).

---

## 27.7 What the STFT unlocks

The STFT is not just a display. It is the working representation for a large family of effects,
all of which follow the same pattern: **analyse → modify the bins → synthesise**.

| Effect | What you do to the bins | Chapter |
|---|---|---|
| **Spectral gate / de-noise** | Zero bins below a threshold | Exercise 27.7 |
| **Spectral EQ** | Scale bins by a curve | 53 |
| **Time stretch** | Synthesise with a different hop than you analysed | 54 |
| **Pitch shift** | Time stretch, then resample | 54 |
| **Vocoder** | Impose one signal's spectral envelope on another | 55 |
| **Formant shift** | Move the spectral envelope without moving the partials | 55 |
| **Spectral freeze** | Repeat one frame's magnitudes indefinitely | 84 |
| **Spectral blur / smear** | Average bins across time | 84 |
| **Convolution** | Multiply by another signal's bins | 48 |

**The phase problem.** Almost all of these are harder than they look, for one reason: if you
change magnitudes and keep the original phases, the frames no longer line up coherently when they
overlap, and you get a characteristic hollow, metallic "phasiness". Solving this properly is the
**phase vocoder**, which tracks each bin's instantaneous frequency and advances its phase
accordingly. Chapter 54 builds it.

For now, note the symptom: **if a spectral effect sounds metallic and smeared, it is a phase
problem, not a magnitude problem.**

---

## 27.8 Exercises

**27.1** Implement the STFT and verify perfect reconstruction: analyse and synthesise with no
modification, and measure the error. Try Hann at 50% and 75% overlap, and Hamming at 50%.

**27.2** *Deliberate breakage.* Remove the `norm` division from `synthesise` and rely on the
window summing to a constant. What happens at the very start and end of the file?

**27.3** Write spectrograms of: a sine, a sawtooth, white noise, a chord, a drum loop, speech.
Look at each and confirm you can identify it from the picture alone.

**27.4** Make a spectrogram of Chapter 12's `alias_naive_saw.wav`. Find the descending aliased
partials. Then do the same for `alias_bandlimited_saw.wav` and compare.

**27.5** Render the same file at `N` = 128, 512, 2048 and 8192 and view all four. Write a
paragraph on what each is good for.

**27.6** Add a colour version: write PPM instead of PGM and map dB to a colour ramp (black →
blue → red → yellow → white). Does it reveal more than greyscale?

**27.7** Build a **spectral gate**: analyse, zero every bin whose magnitude is below a threshold,
synthesise. Apply it to a noisy recording. What does the artefact sound like when the threshold is
too high? (That warbling is called "musical noise" and it is a well-known problem.)

**27.8** Implement **spectral freeze**: capture one frame's magnitudes and repeat them
indefinitely with randomised phases. This is a standard cinematic drone generator (Chapter 84).

**27.9** Measure the RT60 of a reverb from its spectrogram: find the slope of the decay at
several frequencies. Does the tail decay faster at high frequencies? (It should — Chapter 2's air
absorption.)

**27.10** Write a function that detects transients by comparing each frame's total energy with
the previous frame's, and flags increases above a threshold. Test it on a drum loop. This is the
core of onset detection (Chapter 67).

---

### Chapter summary

- A single FFT has no time information. The **STFT** chops the signal into overlapping frames and
  transforms each, giving a 2D map of frequency versus time — a **spectrogram**.
- Three parameters: **frame size `N`** (resolution trade-off), **hop size `H`** (overlap), and
  **window type**.
- For anything you will reconstruct, use **Hann**, because it satisfies **COLA**. With windowing
  on both analysis and synthesis, use **75% overlap** — Hann² does not sum flat at 50%.
- **Divide by the accumulated squared-window sum** on synthesis rather than assuming a constant.
  It is exact, handles the edges correctly, and works for any window/hop combination.
- Draw spectrograms with a **logarithmic frequency axis** and a **dB magnitude scale with a
  floor** — anything else misrepresents what you hear.
- Learn to read them: harmonic stacks, transients as vertical lines, sweeps as diagonals, codec
  cutoffs as a hard shelf, reverb as a fading haze, **aliasing as diagonals running the wrong
  way**.
- The frame size trade-off is visible: short frames show drums and smear chords; long frames do
  the reverse. Multi-resolution analysis is the fix, and it is what your cochlea does.
- The STFT is the working representation for de-noising, time stretching, pitch shifting,
  vocoding, spectral freezing and convolution — all of which are **analyse, modify, synthesise**.
- **Metallic, smeared spectral effects are a phase problem**, solved by the phase vocoder in
  Chapter 54.

**Next:** [Chapter 28 — Resampling and Interpolation](28-resampling.md)
