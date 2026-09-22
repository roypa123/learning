// libaudio - osc.h
// Oscillators built on a normalised phase accumulator. See Chapters 10 and 12.
//
// NOTE: these are NAIVE waveforms. They alias above the bass register.
// Band-limited versions (PolyBLEP, wavetable) arrive in Chapters 31-32.

#pragma once

#include <audio/types.h>
#include <cmath>

namespace audio {

enum class Waveform { Sine, Triangle, Square, Saw, Pulse };

class Oscillator
{
public:
    void setSampleRate(double sr)
    {
        sampleRate_ = sr;
        updateIncrement();
    }

    void setFrequency(double hz)
    {
        frequency_ = hz;
        updateIncrement();
    }

    void setWaveform(Waveform w)   { waveform_ = w; }
    void setPulseWidth(double w)   { pulseWidth_ = w < 0.001 ? 0.001 : (w > 0.999 ? 0.999 : w); }

    // Normalised phase, 0..1.
    void setPhase(double normalised)
    {
        phase_ = normalised - std::floor(normalised);
    }

    double phase() const     { return phase_; }
    double frequency() const { return frequency_; }

    void reset() { phase_ = 0.0; }

    float nextSample()
    {
        const float out = shape(phase_);

        phase_ += increment_;
        // while, not if: at extreme frequencies the phase can advance past
        // 1.0 more than once per sample.
        while (phase_ >= 1.0)
            phase_ -= 1.0;

        return out;
    }

private:
    float shape(double p) const
    {
        switch (waveform_)
        {
            case Waveform::Sine:
                return static_cast<float>(std::sin(kTwoPi * p));

            case Waveform::Triangle:
            {
                const double t = (p < 0.5) ? (p * 2.0) : (2.0 - p * 2.0);
                return static_cast<float>(2.0 * t - 1.0);
            }

            case Waveform::Square:
                return (p < 0.5) ? 1.0f : -1.0f;

            case Waveform::Saw:
                return static_cast<float>(2.0 * p - 1.0);

            case Waveform::Pulse:
                return (p < pulseWidth_) ? 1.0f : -1.0f;
        }
        return 0.0f;
    }

    void updateIncrement()
    {
        increment_ = (sampleRate_ > 0.0) ? frequency_ / sampleRate_ : 0.0;
    }

    double   sampleRate_ = kDefaultRate;
    double   frequency_  = 440.0;
    double   increment_  = 440.0 / kDefaultRate;
    double   phase_      = 0.0;      // double: phase accumulates, see Chapter 6
    double   pulseWidth_ = 0.5;
    Waveform waveform_   = Waveform::Sine;
};

// Band-limited additive oscillators. Exact, but expensive -- use as a
// reference to compare naive oscillators against. See Chapter 12.
std::vector<float> additiveSaw   (double freq, double amp, double seconds, double sampleRate);
std::vector<float> additiveSquare(double freq, double amp, double seconds, double sampleRate);

}   // namespace audio
