// libaudio - resample.h
// Interpolation (nearest / linear / Hermite / windowed sinc) and sample-rate
// conversion. See Chapter 28.

#pragma once

#include <audio/types.h>
#include <audio/fir.h>
#include <audio/fft.h>

#include <vector>
#include <cmath>
#include <algorithm>

namespace audio {
namespace interp {

// Lo-fi only: up to half a sample of time jitter, which is broadband noise.
inline float nearest(const std::vector<float>& x, double pos)
{
    const long i = static_cast<long>(pos + 0.5);
    return (i >= 0 && i < static_cast<long>(x.size()))
         ? x[static_cast<size_t>(i)] : 0.0f;
}

// The workhorse. Fine below fs/10; poor near Nyquist.
inline float linear(const std::vector<float>& x, double pos)
{
    if (pos < 0.0) return 0.0f;

    const size_t i    = static_cast<size_t>(pos);
    const double frac = pos - static_cast<double>(i);

    if (i + 1 >= x.size()) return (i < x.size()) ? x[i] : 0.0f;

    return static_cast<float>(static_cast<double>(x[i]) * (1.0 - frac)
                            + static_cast<double>(x[i + 1]) * frac);
}

// Catmull-Rom cubic. ~25 dB better than linear for 4x the cost.
inline float hermite(const std::vector<float>& x, double pos)
{
    const long   i    = static_cast<long>(std::floor(pos));
    const double frac = pos - static_cast<double>(i);

    auto at = [&](long k) -> double {
        return (k >= 0 && k < static_cast<long>(x.size()))
             ? static_cast<double>(x[static_cast<size_t>(k)]) : 0.0;
    };

    const double xm1 = at(i - 1), x0 = at(i), x1 = at(i + 1), x2 = at(i + 2);

    const double c0 = x0;
    const double c1 = 0.5 * (x1 - xm1);
    const double c2 = xm1 - 2.5 * x0 + 2.0 * x1 - 0.5 * x2;
    const double c3 = 0.5 * (x2 - xm1) + 1.5 * (x0 - x1);

    return static_cast<float>(((c3 * frac + c2) * frac + c1) * frac + c0);
}

// Essentially exact (below -100 dB at halfWidth=16), but slow as written.
// Production code tables the sinc values by fractional phase.
inline float sincInterp(const std::vector<float>& x, double pos, int halfWidth = 16)
{
    const long   i    = static_cast<long>(std::floor(pos));
    const double frac = pos - static_cast<double>(i);

    double acc = 0.0;

    for (int k = -halfWidth + 1; k <= halfWidth; ++k)
    {
        const long idx = i + k;
        if (idx < 0 || idx >= static_cast<long>(x.size())) continue;

        const double t = frac - static_cast<double>(k);

        // sinc(0) is 0/0 without this. Same trap as Chapter 22.
        const double s = (std::fabs(t) < 1e-9) ? 1.0
                                               : std::sin(kPi * t) / (kPi * t);

        // Window it, or truncation ripples.
        const double w = 0.5 + 0.5 * std::cos(kPi * t / static_cast<double>(halfWidth));

        acc += static_cast<double>(x[static_cast<size_t>(idx)]) * s * w;
    }

    return static_cast<float>(acc);
}

}   // namespace interp

// ---------------------------------------------------------------- resampling

enum class ResampleQuality { Nearest, Linear, Hermite, Sinc };

// Sample-rate conversion.
// When DOWNsampling, band-limits first: filtering afterwards cannot remove
// an alias. See Chapter 28 section 28.3.
inline std::vector<float> resample(const std::vector<float>& x,
                                   double fromRate, double toRate,
                                   ResampleQuality quality = ResampleQuality::Sinc)
{
    if (x.empty() || fromRate <= 0.0 || toRate <= 0.0) return {};
    if (std::fabs(fromRate - toRate) < 1e-9) return x;

    const double ratio = toRate / fromRate;
    std::vector<float> source = x;

    if (ratio < 1.0)                         // downsampling: filter FIRST
    {
        const double newNyquist = toRate * 0.5 * 0.92;
        const size_t taps = std::min<size_t>(
            kaiserTapCount(80.0, toRate * 0.04, fromRate), 2049);

        auto lp = designLowPass(newNyquist, fromRate, taps,
                                WindowType::Kaiser, kaiserBeta(80.0));
        source = fastConvolve(source, lp);

        const size_t latency = (lp.size() - 1) / 2;
        if (latency < source.size())
            source.erase(source.begin(), source.begin() + static_cast<long>(latency));
    }

    const size_t outLength = static_cast<size_t>(
        static_cast<double>(x.size()) * ratio);

    std::vector<float> out(outLength);

    for (size_t n = 0; n < outLength; ++n)
    {
        // Compute the position FROM THE INDEX. Accumulating `pos += step`
        // drifts audibly over long files.
        const double pos = static_cast<double>(n) / ratio;

        switch (quality)
        {
            case ResampleQuality::Nearest: out[n] = interp::nearest(source, pos);    break;
            case ResampleQuality::Linear:  out[n] = interp::linear(source, pos);     break;
            case ResampleQuality::Hermite: out[n] = interp::hermite(source, pos);    break;
            case ResampleQuality::Sinc:    out[n] = interp::sincInterp(source, pos); break;
        }
    }

    return out;
}

// Read a buffer at a continuously varying rate -- the core of a sampler,
// a tape-stop effect, and Doppler.
class VariableRateReader
{
public:
    void setSource(const std::vector<float>* src) { source_ = src; position_ = 0.0; }
    void setRate(double r)     { rate_ = r; }
    void setPosition(double p) { position_ = p; }
    double position() const    { return position_; }

    bool finished() const
    {
        return source_ == nullptr
            || position_ >= static_cast<double>(source_->size());
    }

    float nextSample(ResampleQuality q = ResampleQuality::Hermite)
    {
        if (finished()) return 0.0f;

        float out = 0.0f;
        switch (q)
        {
            case ResampleQuality::Nearest: out = interp::nearest(*source_, position_);    break;
            case ResampleQuality::Linear:  out = interp::linear(*source_, position_);     break;
            case ResampleQuality::Hermite: out = interp::hermite(*source_, position_);    break;
            case ResampleQuality::Sinc:    out = interp::sincInterp(*source_, position_); break;
        }

        position_ += rate_;
        return out;
    }

private:
    const std::vector<float>* source_ = nullptr;
    double position_ = 0.0;
    double rate_     = 1.0;
};

}   // namespace audio
