// Chapter 11 - decibels.h
// Decibel conversions and buffer measurements (peak, RMS, crest factor, DC).

#pragma once

#include <cmath>
#include <vector>
#include <algorithm>

namespace db
{
    // Below this amplitude we report a large negative number instead of
    // -infinity. -200 dB is around 1e-10, far below anything representable.
    constexpr double kMinusInfinityDb = -200.0;
    constexpr double kTinyGain        = 1e-10;

    // Amplitude ratio -> decibels. Uses 20*log10 (see Chapter 11, section 11.3).
    inline double gainToDb(double gain)
    {
        const double a = std::fabs(gain);
        return (a < kTinyGain) ? kMinusInfinityDb : 20.0 * std::log10(a);
    }

    // Decibels -> amplitude ratio.
    inline double dbToGain(double decibels)
    {
        return (decibels <= kMinusInfinityDb) ? 0.0
                                              : std::pow(10.0, decibels / 20.0);
    }

    // Power ratio -> decibels. Uses 10*log10.
    inline double powerToDb(double power)
    {
        return (power < kTinyGain * kTinyGain) ? kMinusInfinityDb
                                               : 10.0 * std::log10(power);
    }

    // --- measurements over a buffer ---------------------------------

    inline double peak(const std::vector<float>& buf)
    {
        double p = 0.0;
        for (float s : buf)
            p = std::max(p, static_cast<double>(std::fabs(s)));
        return p;
    }

    inline double rms(const std::vector<float>& buf)
    {
        if (buf.empty())
            return 0.0;

        double sumSquares = 0.0;            // accumulate in double
        for (float s : buf)
            sumSquares += static_cast<double>(s) * static_cast<double>(s);

        return std::sqrt(sumSquares / static_cast<double>(buf.size()));
    }

    inline double peakDb(const std::vector<float>& buf) { return gainToDb(peak(buf)); }
    inline double rmsDb (const std::vector<float>& buf) { return gainToDb(rms(buf));  }

    inline double crestFactorDb(const std::vector<float>& buf)
    {
        return peakDb(buf) - rmsDb(buf);
    }

    // Mean sample value. Should be ~0 for any healthy audio signal.
    inline double dcOffset(const std::vector<float>& buf)
    {
        if (buf.empty())
            return 0.0;

        double sum = 0.0;
        for (float s : buf)
            sum += static_cast<double>(s);

        return sum / static_cast<double>(buf.size());
    }
}
