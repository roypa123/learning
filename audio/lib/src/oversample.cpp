// libaudio - oversample.cpp

#include <audio/oversample.h>
#include <audio/fft.h>
#include <audio/window.h>
#include <audio/db.h>

#include <cmath>

namespace audio {

double aliasingEnergyDb(const std::vector<float>& signal,
                        double fundamental,
                        double sampleRate,
                        double toleranceHz)
{
    if (signal.size() < 4096 || fundamental <= 0.0)
        return kMinusInfinityDb;

    const size_t N = 16384;

    // Take a window from the middle, away from any start/end transient.
    const size_t start = (signal.size() > N) ? (signal.size() - N) / 2 : 0;

    std::vector<float> frame(N, 0.0f);
    for (size_t i = 0; i < N && start + i < signal.size(); ++i)
        frame[i] = signal[start + i];

    // Blackman-Harris: we need to see components far below the fundamental,
    // so we need very low sidelobes (Chapter 26).
    auto w = makeWindow(AnalysisWindow::BlackmanHarris, N);
    applyWindow(frame, w);

    auto X   = fftReal(frame, N);
    auto mag = magnitudeSpectrum(X);

    double harmonicEnergy = 0.0;
    double aliasEnergy    = 0.0;

    const double nyquist = sampleRate * 0.5;

    for (size_t k = 1; k < mag.size(); ++k)
    {
        const double f = binFrequency(k, N, sampleRate);
        if (f >= nyquist) break;

        // Is this bin within tolerance of any harmonic of the fundamental?
        const double ratio  = f / fundamental;
        const double nearest = std::round(ratio);
        const bool isHarmonic = nearest >= 1.0
                             && std::fabs(f - nearest * fundamental) < toleranceHz;

        const double e = mag[k] * mag[k];
        if (isHarmonic) harmonicEnergy += e;
        else            aliasEnergy    += e;
    }

    if (harmonicEnergy <= 0.0)
        return kMinusInfinityDb;

    return 10.0 * std::log10(aliasEnergy / harmonicEnergy);
}

}   // namespace audio
