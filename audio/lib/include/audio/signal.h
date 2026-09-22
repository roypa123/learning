// libaudio - signal.h
// Building-block signals and the basic operations on them. See Chapter 18.

#pragma once

#include <audio/types.h>

#include <vector>
#include <algorithm>
#include <cmath>

namespace audio {
namespace sig {

// ---------------------------------------------------------------- generators

// The unit impulse: one sample of 1.0, silence elsewhere.
// The single most important test signal in DSP (Chapter 20).
inline std::vector<float> impulse(size_t length, size_t position = 0)
{
    std::vector<float> x(length, 0.0f);
    if (position < length) x[position] = 1.0f;
    return x;
}

// The unit step: silence, then a constant 1.0.
inline std::vector<float> step(size_t length, size_t position = 0)
{
    std::vector<float> x(length, 0.0f);
    for (size_t n = position; n < length; ++n) x[n] = 1.0f;
    return x;
}

// a^n. Stable (decaying) iff |a| < 1.
inline std::vector<float> exponential(size_t length, double a)
{
    std::vector<float> x(length);
    double v = 1.0;
    for (size_t n = 0; n < length; ++n)
    {
        x[n] = static_cast<float>(v);
        v *= a;
    }
    return x;
}

// A sinusoid, using a phase accumulator (Chapter 10).
inline std::vector<float> sine(size_t length, double freqHz, double sampleRate,
                               double amplitude = 1.0, double phase0 = 0.0)
{
    std::vector<float> x(length);
    double phase = phase0;
    const double inc = kTwoPi * freqHz / sampleRate;
    for (size_t n = 0; n < length; ++n)
    {
        x[n] = static_cast<float>(amplitude * std::sin(phase));
        phase += inc;
        if (phase >= kTwoPi) phase -= kTwoPi;
    }
    return x;
}

// ---------------------------------------------------------------- operations

// y[n] = x[n - k].  Positive k DELAYS; negative k advances.
inline std::vector<float> shift(const std::vector<float>& x, int k)
{
    std::vector<float> y(x.size(), 0.0f);
    for (size_t n = 0; n < x.size(); ++n)
    {
        const long src = static_cast<long>(n) - k;
        if (src >= 0 && src < static_cast<long>(x.size()))
            y[n] = x[static_cast<size_t>(src)];
    }
    return y;
}

inline std::vector<float> scale(const std::vector<float>& x, float a)
{
    std::vector<float> y(x.size());
    for (size_t n = 0; n < x.size(); ++n) y[n] = a * x[n];
    return y;
}

// Differing lengths: the shorter is treated as zero-padded.
inline std::vector<float> add(const std::vector<float>& a, const std::vector<float>& b)
{
    std::vector<float> y(std::max(a.size(), b.size()), 0.0f);
    for (size_t n = 0; n < a.size(); ++n) y[n] += a[n];
    for (size_t n = 0; n < b.size(); ++n) y[n] += b[n];
    return y;
}

// Differing lengths: truncates to the shorter.
inline std::vector<float> multiply(const std::vector<float>& a, const std::vector<float>& b)
{
    std::vector<float> y(std::min(a.size(), b.size()));
    for (size_t n = 0; n < y.size(); ++n) y[n] = a[n] * b[n];
    return y;
}

inline std::vector<float> reverse(std::vector<float> x)
{
    std::reverse(x.begin(), x.end());
    return x;
}

inline std::vector<float> negate(const std::vector<float>& x)
{
    return scale(x, -1.0f);
}

// ---------------------------------------------------------------- measures

inline double energy(const std::vector<float>& x)
{
    double e = 0.0;
    for (float s : x) e += static_cast<double>(s) * static_cast<double>(s);
    return e;
}

// Largest absolute difference between two signals, in dB. Returns
// kMinusInfinityDb if identical. The workhorse of DSP regression testing.
inline double maxDifference(const std::vector<float>& a, const std::vector<float>& b)
{
    const size_t n = std::min(a.size(), b.size());
    double d = 0.0;
    for (size_t i = 0; i < n; ++i)
        d = std::max(d, std::fabs(static_cast<double>(a[i]) - static_cast<double>(b[i])));
    return d;
}

}}   // namespace audio::sig
