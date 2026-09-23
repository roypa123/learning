// libaudio - oscillator.h
// A precise, modulatable oscillator with PolyBLEP band-limiting.
// See Chapters 30 and 31.

#pragma once

#include <audio/types.h>
#include <audio/osc.h>          // for Waveform

#include <cmath>
#include <algorithm>

namespace audio {

// PolyBLEP correction: the residual between an ideal step and a band-limited
// one, approximated by a polynomial over the two samples adjacent to a
// discontinuity. t is normalised phase (0..1), dt the phase increment.
inline double polyBlep(double t, double dt)
{
    if (dt <= 0.0) return 0.0;

    if (t < dt)                       // just after the discontinuity
    {
        t /= dt;
        return t + t - t * t - 1.0;
    }
    if (t > 1.0 - dt)                 // just before it
    {
        t = (t - 1.0) / dt;
        return t * t + t + t + 1.0;
    }
    return 0.0;                        // the common case: no correction needed
}

class Osc
{
public:
    void setSampleRate(double sr)
    {
        sampleRate_    = sr;
        invSampleRate_ = 1.0 / sr;      // cached: division is 10-20x a multiply
        updateIncrement();
    }

    void setFrequency(double hz)  { frequency_ = hz; updateIncrement(); }

    void setDetuneCents(double cents)
    {
        detuneRatio_ = std::pow(2.0, cents / 1200.0);
        updateIncrement();
    }

    void setWaveform(Waveform w)   { waveform_ = w; }
    void setPulseWidth(double w)   { pulseWidth_ = std::clamp(w, 0.001, 0.999); }
    void setBandLimited(bool b)    { bandLimited_ = b; }

    void   setPhase(double p) { phase_ = wrap01(p); }
    double phase() const      { return phase_; }
    double increment() const  { return increment_; }
    double frequency() const  { return frequency_ * detuneRatio_; }

    void reset() { phase_ = 0.0; triState_ = 0.0; wrapped_ = false; }

    bool justWrapped() const { return wrapped_; }    // for hard sync

    // pmAmount: phase modulation in cycles (Chapter 35).
    // freqRatio: per-sample frequency modulation -- vibrato, glide, Doppler.
    float nextSample(double pmAmount = 0.0, double freqRatio = 1.0)
    {
        const double readPhase = wrap01(phase_ + pmAmount);
        const double dt        = increment_ * freqRatio;
        const float  out       = shape(readPhase, dt);

        phase_  += dt;
        wrapped_ = false;
        while (phase_ >= 1.0) { phase_ -= 1.0; wrapped_ = true; }
        while (phase_ <  0.0) { phase_ += 1.0; wrapped_ = true; }

        return out;
    }

private:
    static double wrap01(double p) { return p - std::floor(p); }

    void updateIncrement()
    {
        increment_ = frequency_ * detuneRatio_ * invSampleRate_;
    }

    float shape(double p, double dt)
    {
        switch (waveform_)
        {
            case Waveform::Sine:
                return static_cast<float>(std::sin(kTwoPi * p));

            case Waveform::Saw:
            {
                double v = 2.0 * p - 1.0;
                if (bandLimited_) v -= polyBlep(p, dt);
                return static_cast<float>(v);
            }

            case Waveform::Square:
            case Waveform::Pulse:
            {
                const double w = (waveform_ == Waveform::Square) ? 0.5 : pulseWidth_;
                double v = (p < w) ? 1.0 : -1.0;

                if (bandLimited_)
                {
                    v += polyBlep(p, dt);              // the rising edge at 0
                    double t2 = p - w;                  // the falling edge at w
                    if (t2 < 0.0) t2 += 1.0;
                    v -= polyBlep(t2, dt);
                }
                return static_cast<float>(v);
            }

            case Waveform::Triangle:
            {
                if (!bandLimited_)
                {
                    const double t = (p < 0.5) ? (p * 2.0) : (2.0 - p * 2.0);
                    return static_cast<float>(2.0 * t - 1.0);
                }

                // Integrate a PolyBLEP square. The leak is essential: a pure
                // integrator accumulates DC without bound.
                double sq = (p < 0.5) ? 1.0 : -1.0;
                sq += polyBlep(p, dt);
                double t2 = p - 0.5;
                if (t2 < 0.0) t2 += 1.0;
                sq -= polyBlep(t2, dt);

                triState_ = 4.0 * dt * sq + (1.0 - 4.0 * dt) * triState_;
                return static_cast<float>(triState_);
            }
        }
        return 0.0f;
    }

    double   sampleRate_    = kDefaultRate;
    double   invSampleRate_ = 1.0 / kDefaultRate;
    double   frequency_     = 440.0;
    double   detuneRatio_   = 1.0;
    double   increment_     = 440.0 / kDefaultRate;
    double   phase_         = 0.0;      // double: float drifts ~4 cents/hour
    double   pulseWidth_    = 0.5;
    double   triState_      = 0.0;
    bool     wrapped_       = false;
    bool     bandLimited_   = true;
    Waveform waveform_      = Waveform::Sine;
};

inline double centsToRatio(double cents) { return std::pow(2.0, cents / 1200.0); }
inline double ratioToCents(double ratio) { return 1200.0 * std::log2(ratio); }

}   // namespace audio
