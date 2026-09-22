// libaudio - db.h
// Decibel conversions and buffer measurements. See Chapter 11.

#pragma once

#include <cmath>
#include <vector>
#include <algorithm>

namespace audio {

// Below this amplitude we report a large negative number instead of
// -infinity, so that arithmetic downstream stays finite.
constexpr double kMinusInfinityDb = -200.0;
constexpr double kTinyGain        = 1e-10;

// Amplitude ratio -> decibels. 20*log10 because power is proportional to
// amplitude squared. See Chapter 11, section 11.3.
inline double gainToDb(double gain)
{
    const double a = std::fabs(gain);
    return (a < kTinyGain) ? kMinusInfinityDb : 20.0 * std::log10(a);
}

inline double dbToGain(double decibels)
{
    return (decibels <= kMinusInfinityDb) ? 0.0 : std::pow(10.0, decibels / 20.0);
}

// Power ratio -> decibels. 10*log10.
inline double powerToDb(double power)
{
    return (power < kTinyGain * kTinyGain) ? kMinusInfinityDb : 10.0 * std::log10(power);
}

// --- measurements over a plain vector -----------------------------------

inline double peak(const std::vector<float>& buf)
{
    double p = 0.0;
    for (float s : buf)
        p = std::max(p, static_cast<double>(std::fabs(s)));
    return p;
}

inline double rms(const std::vector<float>& buf)
{
    if (buf.empty()) return 0.0;

    double sumSquares = 0.0;              // accumulate in double
    for (float s : buf)
        sumSquares += static_cast<double>(s) * static_cast<double>(s);

    return std::sqrt(sumSquares / static_cast<double>(buf.size()));
}

inline double peakDb(const std::vector<float>& buf) { return gainToDb(peak(buf)); }
inline double rmsDb (const std::vector<float>& buf) { return gainToDb(rms(buf));  }

// peak - RMS. A sine is 3.01 dB, a square 0 dB, a film mix 25-35 dB.
inline double crestFactorDb(const std::vector<float>& buf)
{
    return peakDb(buf) - rmsDb(buf);
}

// Mean sample value. Should be ~0 for any healthy audio signal.
inline double dcOffset(const std::vector<float>& buf)
{
    if (buf.empty()) return 0.0;

    double sum = 0.0;
    for (float s : buf)
        sum += static_cast<double>(s);

    return sum / static_cast<double>(buf.size());
}

// --- safety checks -------------------------------------------------------

inline bool hasNaN(const std::vector<float>& buf)
{
    for (float s : buf)
        if (std::isnan(s)) return true;
    return false;
}

inline bool hasClipping(const std::vector<float>& buf, float ceiling = 1.0f)
{
    for (float s : buf)
        if (std::fabs(s) > ceiling) return true;
    return false;
}

// --- fader law -----------------------------------------------------------
// position 0.0 (silent) .. 1.0 (unity), linear in dB over `rangeDb`.
inline double faderToGain(double position, double rangeDb = 60.0)
{
    if (position <= 0.0) return 0.0;
    return dbToGain(-rangeDb * (1.0 - position));
}

}   // namespace audio
