// libaudio - osc.cpp
// Band-limited additive oscillators. Exact, expensive; use as a reference.
// See Chapter 12.

#include <audio/osc.h>

#include <cmath>

namespace audio {

std::vector<float> additiveSaw(double freq, double amp, double seconds, double sampleRate)
{
    const size_t n = static_cast<size_t>(seconds * sampleRate);
    std::vector<float> out(n, 0.0f);

    for (int h = 1; ; ++h)
    {
        const double f = freq * h;
        if (f >= sampleRate / 2.0)          // never generate above Nyquist
            break;

        const double a = (2.0 / kPi) * ((h % 2 == 1) ? 1.0 : -1.0) / h;

        double phase = 0.0;
        const double inc = kTwoPi * f / sampleRate;

        for (size_t i = 0; i < n; ++i)
        {
            out[i] += static_cast<float>(amp * a * std::sin(phase));
            phase += inc;
            if (phase >= kTwoPi) phase -= kTwoPi;
        }
    }
    return out;
}

std::vector<float> additiveSquare(double freq, double amp, double seconds, double sampleRate)
{
    const size_t n = static_cast<size_t>(seconds * sampleRate);
    std::vector<float> out(n, 0.0f);

    for (int h = 1; ; h += 2)               // odd harmonics only
    {
        const double f = freq * h;
        if (f >= sampleRate / 2.0)
            break;

        const double a = (4.0 / kPi) / h;

        double phase = 0.0;
        const double inc = kTwoPi * f / sampleRate;

        for (size_t i = 0; i < n; ++i)
        {
            out[i] += static_cast<float>(amp * a * std::sin(phase));
            phase += inc;
            if (phase >= kTwoPi) phase -= kTwoPi;
        }
    }
    return out;
}

}   // namespace audio
