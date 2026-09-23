// libaudio - oversample.h
// Oversampling around a nonlinearity, plus the standard waveshapers.
// See Chapter 29.

#pragma once

#include <audio/types.h>
#include <audio/fir.h>

#include <vector>
#include <cmath>
#include <algorithm>

namespace audio {

// ---------------------------------------------------------------- shapers

namespace shape {

inline float hardClip(float x)          { return std::clamp(x, -1.0f, 1.0f); }
inline float tanhShape(float x)         { return std::tanh(x); }
inline float atanShape(float x)         { return static_cast<float>((2.0 / kPi) * std::atan(x)); }

// Exactly linear below 1/3, so it is transparent until driven.
inline float cubicClip(float x)
{
    if (x <= -1.0f) return -2.0f / 3.0f;
    if (x >=  1.0f) return  2.0f / 3.0f;
    return x - (x * x * x) / 3.0f;
}

inline float bitCrush(float x, int bits)
{
    const float levels = static_cast<float>(1 << std::max(1, bits));
    return std::round(x * levels) / levels;
}

}   // namespace shape

// ---------------------------------------------------------------- oversampler

// Upsample by L, apply a per-sample function at the high rate, downsample.
// The downsampling filter removes the harmonics that would otherwise alias.
class Oversampler
{
public:
    void prepare(int factor, double sampleRate, int qualityPerFactor = 64)
    {
        factor_ = std::max(1, factor);
        if (factor_ == 1) { coeffs_.clear(); return; }

        // One design serves both directions: cutoff just below the ORIGINAL
        // Nyquist, evaluated at the high rate.
        const double highRate = sampleRate * factor_;
        const double cutoff   = sampleRate * 0.5 * 0.90;

        const size_t taps = static_cast<size_t>(qualityPerFactor * factor_) | 1u;

        coeffs_ = designLowPass(cutoff, highRate, taps,
                                WindowType::Kaiser, kaiserBeta(90.0));

        up_.setCoefficients(coeffs_);
        down_.setCoefficients(coeffs_);
        reset();
    }

    void reset() { up_.reset(); down_.reset(); }

    int factor() const { return factor_; }

    // fn is applied at the high rate.
    template <typename Fn>
    float processSample(float x, Fn&& fn)
    {
        if (factor_ == 1)
            return fn(x);

        float result = 0.0f;

        for (int i = 0; i < factor_; ++i)
        {
            // Zero-stuff. The x*factor_ restores the energy the zeros remove;
            // omitting it makes the output 20*log10(L) dB quiet.
            const float in = (i == 0) ? x * static_cast<float>(factor_) : 0.0f;

            const float upsampled = up_.processSample(in, 0);
            const float shaped    = fn(upsampled);
            const float filtered  = down_.processSample(shaped, 0);

            if (i == 0) result = filtered;    // keep 1 of every L
        }

        return result;
    }

    // Two FIR passes at the high rate. Report this and compensate, or a
    // parallel dry path will comb-filter.
    int latencySamples() const
    {
        return (factor_ == 1 || coeffs_.empty())
             ? 0
             : static_cast<int>(coeffs_.size() - 1) / factor_;
    }

private:
    int                factor_ = 1;
    std::vector<float> coeffs_;
    FIRFilter          up_, down_;
};

// ------------------------------------------------- aliasing measurement

// Sum the energy in bins that are NOT harmonics of `fundamental`, relative
// to the total. The standard way to quantify a nonlinearity's aliasing.
double aliasingEnergyDb(const std::vector<float>& signal,
                        double fundamental,
                        double sampleRate,
                        double toleranceHz = 15.0);

}   // namespace audio
