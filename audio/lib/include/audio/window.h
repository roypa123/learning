// libaudio - window.h
// Analysis windows, in PERIODIC form (divide by N, not N-1) for FFT use.
// Chapter 22's FIR design uses the symmetric form; see Chapter 26 s26.5.

#pragma once

#include <audio/types.h>
#include <audio/fir.h>       // for besselI0 and WindowType

#include <vector>
#include <cmath>

namespace audio {

enum class AnalysisWindow {
    Rectangular, Hann, Hamming, Blackman, BlackmanHarris, Kaiser, FlatTop
};

// Periodic window value at index n of N.
inline double analysisWindowValue(AnalysisWindow type, size_t n, size_t N,
                                  double kaiserBeta = 8.6)
{
    if (N == 0) return 1.0;

    const double r = static_cast<double>(n) / static_cast<double>(N);   // PERIODIC: /N

    switch (type)
    {
        case AnalysisWindow::Rectangular:
            return 1.0;

        case AnalysisWindow::Hann:
            return 0.5 - 0.5 * std::cos(kTwoPi * r);

        case AnalysisWindow::Hamming:
            return 0.54 - 0.46 * std::cos(kTwoPi * r);

        case AnalysisWindow::Blackman:
            return 0.42 - 0.5 * std::cos(kTwoPi * r) + 0.08 * std::cos(2.0 * kTwoPi * r);

        case AnalysisWindow::BlackmanHarris:
            return 0.35875 - 0.48829 * std::cos(kTwoPi * r)
                 + 0.14128 * std::cos(2.0 * kTwoPi * r)
                 - 0.01168 * std::cos(3.0 * kTwoPi * r);

        case AnalysisWindow::Kaiser:
        {
            const double u = 2.0 * r - 1.0;
            return besselI0(kaiserBeta * std::sqrt(std::max(0.0, 1.0 - u * u)))
                 / besselI0(kaiserBeta);
        }

        case AnalysisWindow::FlatTop:
            // Accurate amplitude (0.01 dB scalloping loss), terrible resolution.
            return 0.21557895
                 - 0.41663158 * std::cos(kTwoPi * r)
                 + 0.277263158 * std::cos(2.0 * kTwoPi * r)
                 - 0.083578947 * std::cos(3.0 * kTwoPi * r)
                 + 0.006947368 * std::cos(4.0 * kTwoPi * r);
    }
    return 1.0;
}

inline std::vector<double> makeWindow(AnalysisWindow type, size_t N,
                                      double kaiserBeta = 8.6)
{
    std::vector<double> w(N);
    for (size_t n = 0; n < N; ++n)
        w[n] = analysisWindowValue(type, n, N, kaiserBeta);
    return w;
}

// Mean of the window. Divide a measured SINE amplitude by this.
inline double coherentGain(const std::vector<double>& w)
{
    if (w.empty()) return 1.0;
    double s = 0.0;
    for (double v : w) s += v;
    return s / static_cast<double>(w.size());
}

// RMS of the window. Divide a measured NOISE level by this.
inline double powerGain(const std::vector<double>& w)
{
    if (w.empty()) return 1.0;
    double s = 0.0;
    for (double v : w) s += v * v;
    return std::sqrt(s / static_cast<double>(w.size()));
}

inline void applyWindow(std::vector<float>& x, const std::vector<double>& w)
{
    const size_t n = std::min(x.size(), w.size());
    for (size_t i = 0; i < n; ++i)
        x[i] = static_cast<float>(static_cast<double>(x[i]) * w[i]);
}

inline const char* windowName(AnalysisWindow t)
{
    switch (t)
    {
        case AnalysisWindow::Rectangular:    return "Rectangular";
        case AnalysisWindow::Hann:           return "Hann";
        case AnalysisWindow::Hamming:        return "Hamming";
        case AnalysisWindow::Blackman:       return "Blackman";
        case AnalysisWindow::BlackmanHarris: return "BlackmanHarris";
        case AnalysisWindow::Kaiser:         return "Kaiser";
        case AnalysisWindow::FlatTop:        return "FlatTop";
    }
    return "?";
}

}   // namespace audio
