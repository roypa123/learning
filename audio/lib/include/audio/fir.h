// libaudio - fir.h
// FIR filter design by the windowed-sinc method, plus the filter itself.
// See Chapter 22.

#pragma once

#include <audio/types.h>
#include <audio/processor.h>

#include <vector>
#include <complex>
#include <cmath>
#include <algorithm>

namespace audio {

enum class WindowType { Rectangular, Hann, Hamming, Blackman, BlackmanHarris, Kaiser };

// Zeroth-order modified Bessel function of the first kind, by series.
inline double besselI0(double x)
{
    double sum = 1.0, term = 1.0;
    for (int k = 1; k < 50; ++k)
    {
        const double t = x / (2.0 * k);
        term *= t * t;
        sum  += term;
        if (term < 1e-12 * sum) break;
    }
    return sum;
}

inline double windowValue(WindowType type, size_t n, size_t M, double kaiserBeta = 8.6)
{
    if (M == 0) return 1.0;

    const double r = static_cast<double>(n) / static_cast<double>(M);   // 0..1

    switch (type)
    {
        case WindowType::Rectangular:
            return 1.0;

        case WindowType::Hann:
            return 0.5 - 0.5 * std::cos(kTwoPi * r);

        case WindowType::Hamming:
            return 0.54 - 0.46 * std::cos(kTwoPi * r);

        case WindowType::Blackman:
            return 0.42 - 0.5 * std::cos(kTwoPi * r) + 0.08 * std::cos(2.0 * kTwoPi * r);

        case WindowType::BlackmanHarris:
            return 0.35875 - 0.48829 * std::cos(kTwoPi * r)
                 + 0.14128 * std::cos(2.0 * kTwoPi * r)
                 - 0.01168 * std::cos(3.0 * kTwoPi * r);

        case WindowType::Kaiser:
        {
            const double u = 2.0 * r - 1.0;                    // -1..1
            return besselI0(kaiserBeta * std::sqrt(std::max(0.0, 1.0 - u * u)))
                 / besselI0(kaiserBeta);
        }
    }
    return 1.0;
}

// --- Kaiser design from a specification ---------------------------------

inline double kaiserBeta(double attenuationDb)
{
    if (attenuationDb > 50.0)
        return 0.1102 * (attenuationDb - 8.7);
    if (attenuationDb >= 21.0)
        return 0.5842 * std::pow(attenuationDb - 21.0, 0.4)
             + 0.07886 * (attenuationDb - 21.0);
    return 0.0;
}

inline size_t kaiserTapCount(double attenuationDb, double transitionHz, double sampleRate)
{
    const double dw = kTwoPi * transitionHz / sampleRate;
    if (dw <= 0.0) return 1;
    size_t n = static_cast<size_t>(std::ceil((attenuationDb - 8.0) / (2.285 * dw))) + 1;
    if (n % 2 == 0) ++n;
    return n;
}

// --- design functions ----------------------------------------------------

inline std::vector<float> designLowPass(double cutoffHz, double sampleRate, size_t numTaps,
                                        WindowType w = WindowType::Hamming, double beta = 8.6)
{
    if (numTaps < 1) numTaps = 1;
    if (numTaps % 2 == 0) ++numTaps;          // odd: a true centre tap, exact symmetry

    std::vector<float> h(numTaps);

    const double fc     = cutoffHz / sampleRate;        // normalised, 0..0.5
    const size_t M      = numTaps - 1;
    const double centre = static_cast<double>(M) / 2.0;

    double sum = 0.0;

    for (size_t n = 0; n < numTaps; ++n)
    {
        const double t = static_cast<double>(n) - centre;

        // The ideal (infinite) low-pass impulse response: a sinc.
        // NOTE the t == 0 case: sin(0)/(pi*0) is 0/0 = NaN without it.
        double ideal;
        if (std::fabs(t) < 1e-9)
            ideal = 2.0 * fc;
        else
            ideal = std::sin(kTwoPi * fc * t) / (kPi * t);

        h[n] = static_cast<float>(ideal * windowValue(w, n, M, beta));
        sum += static_cast<double>(h[n]);
    }

    // Force the DC gain to exactly 1.0.
    if (std::fabs(sum) > 1e-12)
        for (auto& v : h) v = static_cast<float>(static_cast<double>(v) / sum);

    return h;
}

// highpass = delta - lowpass (spectral inversion)
inline std::vector<float> designHighPass(double cutoffHz, double sampleRate, size_t numTaps,
                                         WindowType w = WindowType::Hamming, double beta = 8.6)
{
    auto h = designLowPass(cutoffHz, sampleRate, numTaps, w, beta);
    for (auto& v : h) v = -v;
    h[h.size() / 2] += 1.0f;
    return h;
}

// bandpass = lowpass(high) - lowpass(low)
inline std::vector<float> designBandPass(double lowHz, double highHz, double sampleRate,
                                         size_t numTaps,
                                         WindowType w = WindowType::Hamming, double beta = 8.6)
{
    auto hi = designLowPass(highHz, sampleRate, numTaps, w, beta);
    auto lo = designLowPass(lowHz,  sampleRate, numTaps, w, beta);

    std::vector<float> h(hi.size());
    for (size_t i = 0; i < h.size(); ++i) h[i] = hi[i] - lo[i];
    return h;
}

// bandstop = delta - bandpass
inline std::vector<float> designBandStop(double lowHz, double highHz, double sampleRate,
                                         size_t numTaps,
                                         WindowType w = WindowType::Hamming, double beta = 8.6)
{
    auto h = designBandPass(lowHz, highHz, sampleRate, numTaps, w, beta);
    for (auto& v : h) v = -v;
    h[h.size() / 2] += 1.0f;
    return h;
}

// --- analysis ------------------------------------------------------------

// H(w) = sum over n of h[n] * e^(-j*w*n).  w is radians per sample.
inline std::complex<double> frequencyResponse(const std::vector<float>& h, double w)
{
    std::complex<double> H(0.0, 0.0);
    for (size_t n = 0; n < h.size(); ++n)
        H += static_cast<double>(h[n])
           * std::exp(std::complex<double>(0.0, -w * static_cast<double>(n)));
    return H;
}

inline double magnitudeDb(const std::vector<float>& h, double freqHz, double sampleRate)
{
    const double w = kTwoPi * freqHz / sampleRate;
    return gainToDb(std::abs(frequencyResponse(h, w)));
}

// --- the filter ----------------------------------------------------------

class FIRFilter : public Processor
{
public:
    void setCoefficients(std::vector<float> h)
    {
        h_ = std::move(h);
        reset();
    }

    const std::vector<float>& coefficients() const { return h_; }

    void prepare(double, int) override { reset(); }

    void process(AudioBuffer& buffer) override
    {
        ensureChannels(buffer.numChannels());
        for (int c = 0; c < buffer.numChannels(); ++c)
            for (float& s : buffer.channel(c))
                s = processSample(s, c);
    }

    void reset() override
    {
        for (auto& line : delay_) std::fill(line.begin(), line.end(), 0.0f);
        std::fill(writeIndex_.begin(), writeIndex_.end(), size_t{0});
    }

    const char* name() const override { return "FIRFilter"; }

    float processSample(float x, int channel = 0)
    {
        if (h_.empty()) return x;
        ensureChannels(channel + 1);

        auto&   line = delay_[static_cast<size_t>(channel)];
        size_t& wi   = writeIndex_[static_cast<size_t>(channel)];

        line[wi] = x;

        double acc = 0.0;
        size_t idx = wi;

        for (size_t k = 0; k < h_.size(); ++k)
        {
            acc += static_cast<double>(h_[k]) * static_cast<double>(line[idx]);
            idx = (idx == 0) ? line.size() - 1 : idx - 1;
        }

        wi = (wi + 1) % line.size();
        return static_cast<float>(acc);
    }

    // Latency for a symmetric (linear-phase) filter.
    int latencySamples() const
    {
        return h_.empty() ? 0 : static_cast<int>(h_.size() - 1) / 2;
    }

private:
    void ensureChannels(int n)
    {
        const size_t want = static_cast<size_t>(std::max(n, 1));
        if (delay_.size() < want)
        {
            delay_.resize(want, std::vector<float>(std::max<size_t>(h_.size(), 1), 0.0f));
            writeIndex_.resize(want, 0);
        }
        for (auto& line : delay_)
            if (line.size() != std::max<size_t>(h_.size(), 1))
                line.assign(std::max<size_t>(h_.size(), 1), 0.0f);
    }

    std::vector<float>              h_;
    std::vector<std::vector<float>> delay_;
    std::vector<size_t>             writeIndex_;
};

}   // namespace audio
