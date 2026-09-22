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
#include <audio/processor.h>   // Processor interface, Gain, Chain          (Ch 17)
