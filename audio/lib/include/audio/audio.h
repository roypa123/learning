// libaudio - audio.h
//
// Audio Programming in C++ -- companion library.
//
//     #include <audio/audio.h>
//     using namespace audio;
//
// Everything in this header is explained in Chapters 1-17.

#pragma once

#include <audio/types.h>       // constants, AudioBuffer, midiToFrequency   (Ch 6, 7)
#include <audio/db.h>          // decibels, peak/RMS/crest/DC, fader law    (Ch 11)
#include <audio/wav.h>         // WAV read and write                        (Ch 9, 16)
#include <audio/osc.h>         // oscillators, additive references          (Ch 10, 12)
#include <audio/envelope.h>    // ADSR                                      (Ch 13)
#include <audio/noise.h>       // white / pink / brown / blue               (Ch 15)
#include <audio/filter.h>      // DC blocker (biquads arrive in Ch 23)      (Ch 14)
#include <audio/signal.h>      // building-block signals and operations   (Ch 18)
#include <audio/convolve.h>    // convolution, correlation, overlap-add   (Ch 21)
#include <audio/fir.h>         // FIR design (windowed sinc) and filter      (Ch 22)
#include <audio/biquad.h>      // biquad, RBJ cookbook, Butterworth cascade (Ch 23)
#include <audio/fft.h>         // DFT, FFT, fast convolution                (Ch 25)
#include <audio/window.h>      // analysis windows, coherent/power gain      (Ch 26)
#include <audio/stft.h>        // STFT, spectrograms, PGM/PPM output         (Ch 27)
#include <audio/resample.h>    // interpolation and sample-rate conversion   (Ch 28)
#include <audio/oversample.h>  // oversampling, waveshapers, alias measure   (Ch 29)
#include <audio/oscillator.h> // PolyBLEP oscillator, cents helpers          (Ch 30, 31)
#include <audio/wavetable.h>   // mipmapped + morphing wavetables            (Ch 32)
#include <audio/processor.h>   // Processor interface, Gain, Chain          (Ch 17)
