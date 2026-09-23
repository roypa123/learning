// libaudio - biquad.h
// The biquad: second-order IIR filter, with RBJ Audio EQ Cookbook
// coefficients for all eight standard shapes. See Chapter 23.

#pragma once

#include <audio/types.h>
#include <audio/db.h>
#include <audio/processor.h>

#include <vector>
#include <complex>
#include <cmath>
#include <algorithm>

namespace audio {

enum class FilterType {
    LowPass, HighPass, BandPass, Notch, AllPass,
    Peaking, LowShelf, HighShelf
};

struct BiquadCoeffs
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0;
    double a1 = 0.0, a2 = 0.0;

    static BiquadCoeffs design(FilterType type, double freqHz, double sampleRate,
                               double Q = 0.70710678118, double gainDb = 0.0)
    {
        BiquadCoeffs c;

        // A cutoff at or above Nyquist produces garbage; at zero it divides
        // by zero. Clamp well inside the valid range.
        freqHz = std::clamp(freqHz, 1.0, sampleRate * 0.495);
        Q      = std::max(Q, 0.001);

        const double w0    = kTwoPi * freqHz / sampleRate;
        const double cos_w = std::cos(w0);
        const double sin_w = std::sin(w0);
        const double alpha = sin_w / (2.0 * Q);
        const double A     = std::pow(10.0, gainDb / 40.0);   // 40, not 20
        const double sqrtA = std::sqrt(A);

        double b0 = 1, b1 = 0, b2 = 0, a0 = 1, a1 = 0, a2 = 0;

        switch (type)
        {
            case FilterType::LowPass:
                b0 = (1.0 - cos_w) * 0.5;
                b1 =  1.0 - cos_w;
                b2 = (1.0 - cos_w) * 0.5;
                a0 =  1.0 + alpha;
                a1 = -2.0 * cos_w;
                a2 =  1.0 - alpha;
                break;

            case FilterType::HighPass:
                b0 =  (1.0 + cos_w) * 0.5;
                b1 = -(1.0 + cos_w);
                b2 =  (1.0 + cos_w) * 0.5;
                a0 =   1.0 + alpha;
                a1 =  -2.0 * cos_w;
                a2 =   1.0 - alpha;
                break;

            case FilterType::BandPass:        // constant peak gain of 1
                b0 =  alpha;
                b1 =  0.0;
                b2 = -alpha;
                a0 =  1.0 + alpha;
                a1 = -2.0 * cos_w;
                a2 =  1.0 - alpha;
                break;

            case FilterType::Notch:
                b0 =  1.0;
                b1 = -2.0 * cos_w;
                b2 =  1.0;
                a0 =  1.0 + alpha;
                a1 = -2.0 * cos_w;
                a2 =  1.0 - alpha;
                break;

            case FilterType::AllPass:
                b0 =  1.0 - alpha;
                b1 = -2.0 * cos_w;
                b2 =  1.0 + alpha;
                a0 =  1.0 + alpha;
                a1 = -2.0 * cos_w;
                a2 =  1.0 - alpha;
                break;

            case FilterType::Peaking:
                b0 =  1.0 + alpha * A;
                b1 = -2.0 * cos_w;
                b2 =  1.0 - alpha * A;
                a0 =  1.0 + alpha / A;
                a1 = -2.0 * cos_w;
                a2 =  1.0 - alpha / A;
                break;

            case FilterType::LowShelf:
                b0 =       A * ((A + 1.0) - (A - 1.0) * cos_w + 2.0 * sqrtA * alpha);
                b1 = 2.0 * A * ((A - 1.0) - (A + 1.0) * cos_w);
                b2 =       A * ((A + 1.0) - (A - 1.0) * cos_w - 2.0 * sqrtA * alpha);
                a0 =            (A + 1.0) + (A - 1.0) * cos_w + 2.0 * sqrtA * alpha;
                a1 =     -2.0 * ((A - 1.0) + (A + 1.0) * cos_w);
                a2 =            (A + 1.0) + (A - 1.0) * cos_w - 2.0 * sqrtA * alpha;
                break;

            case FilterType::HighShelf:
                b0 =        A * ((A + 1.0) + (A - 1.0) * cos_w + 2.0 * sqrtA * alpha);
                b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cos_w);
                b2 =        A * ((A + 1.0) + (A - 1.0) * cos_w - 2.0 * sqrtA * alpha);
                a0 =             (A + 1.0) - (A - 1.0) * cos_w + 2.0 * sqrtA * alpha;
                a1 =       2.0 * ((A - 1.0) - (A + 1.0) * cos_w);
                a2 =             (A + 1.0) - (A - 1.0) * cos_w - 2.0 * sqrtA * alpha;
                break;
        }

        // THE STEP EVERYONE FORGETS: normalise by a0.
        c.b0 = b0 / a0;  c.b1 = b1 / a0;  c.b2 = b2 / a0;
        c.a1 = a1 / a0;  c.a2 = a2 / a0;

        return c;
    }

    // |H(w)| in dB. Evaluates the transfer function on the unit circle.
    double magnitudeDb(double freqHz, double sampleRate) const
    {
        const double w = kTwoPi * freqHz / sampleRate;
        const std::complex<double> z1 = std::exp(std::complex<double>(0.0, -w));
        const std::complex<double> z2 = z1 * z1;

        const std::complex<double> num = b0 + b1 * z1 + b2 * z2;
        const std::complex<double> den = 1.0 + a1 * z1 + a2 * z2;

        return gainToDb(std::abs(num / den));
    }

    double phaseRadians(double freqHz, double sampleRate) const
    {
        const double w = kTwoPi * freqHz / sampleRate;
        const std::complex<double> z1 = std::exp(std::complex<double>(0.0, -w));
        const std::complex<double> z2 = z1 * z1;
        return std::arg((b0 + b1 * z1 + b2 * z2) / (1.0 + a1 * z1 + a2 * z2));
    }

    // Both poles inside the unit circle. See Chapter 24 for the derivation.
    bool isStable() const
    {
        return std::fabs(a2) < 1.0 && std::fabs(a1) < 1.0 + a2;
    }
};

// ---------------------------------------------------------------- Biquad

class Biquad : public Processor
{
public:
    void setCoefficients(const BiquadCoeffs& c) { coeffs_ = c; }
    const BiquadCoeffs& coefficients() const    { return coeffs_; }

    void setSampleRate(double sr) { sampleRate_ = sr; }

    void setFilter(FilterType type, double freqHz, double Q = 0.70710678118,
                   double gainDb = 0.0)
    {
        coeffs_ = BiquadCoeffs::design(type, freqHz, sampleRate_, Q, gainDb);
    }

    void prepare(double sampleRate, int) override
    {
        sampleRate_ = sampleRate;
        reset();
    }

    void process(AudioBuffer& buffer) override
    {
        ensureChannels(buffer.numChannels());
        for (int c = 0; c < buffer.numChannels(); ++c)
            for (float& s : buffer.channel(c))
                s = processSample(s, c);
    }

    void reset() override
    {
        for (auto& s : state_) { s.s1 = 0.0; s.s2 = 0.0; }
    }

    const char* name() const override { return "Biquad"; }

    // Transposed Direct Form II: two state variables, good numerics.
    float processSample(float x, int channel = 0)
    {
        ensureChannels(channel + 1);

        auto& s = state_[static_cast<size_t>(channel)];
        const double xd = static_cast<double>(x);

        const double y = coeffs_.b0 * xd + s.s1;
        s.s1 = coeffs_.b1 * xd - coeffs_.a1 * y + s.s2;
        s.s2 = coeffs_.b2 * xd - coeffs_.a2 * y;

        if (!std::isfinite(y)) { reset(); return 0.0f; }

        return static_cast<float>(y);
    }

private:
    struct State { double s1 = 0.0, s2 = 0.0; };

    void ensureChannels(int n)
    {
        const size_t want = static_cast<size_t>(std::max(n, 1));
        if (state_.size() < want) state_.resize(want);
    }

    BiquadCoeffs       coeffs_;
    std::vector<State> state_{ 2 };
    double             sampleRate_ = kDefaultRate;
};

// ------------------------------------------------------- BiquadCascade

// Per-stage Q values for a Butterworth cascade of the given even order.
inline std::vector<double> butterworthQ(int order)
{
    std::vector<double> qs;
    const int pairs = order / 2;
    for (int k = 0; k < pairs; ++k)
    {
        const double theta = kPi * (2.0 * k + 1.0) / (2.0 * order);
        qs.push_back(1.0 / (2.0 * std::cos(theta)));
    }
    return qs;
}

class BiquadCascade : public Processor
{
public:
    void setSampleRate(double sr)
    {
        sampleRate_ = sr;
        for (auto& s : stages_) s.setSampleRate(sr);
    }

    // order is rounded up to the next even number.
    void setButterworth(FilterType type, double freqHz, int order)
    {
        if (order < 2) order = 2;
        if (order % 2) ++order;

        const auto qs = butterworthQ(order);
        stages_.assign(qs.size(), Biquad{});

        for (size_t i = 0; i < qs.size(); ++i)
        {
            stages_[i].setSampleRate(sampleRate_);
            stages_[i].setFilter(type, freqHz, qs[i]);
        }
    }

    void prepare(double sampleRate, int blockSize) override
    {
        sampleRate_ = sampleRate;
        for (auto& s : stages_) s.prepare(sampleRate, blockSize);
    }

    void process(AudioBuffer& buffer) override
    {
        for (auto& s : stages_) s.process(buffer);
    }

    void reset() override { for (auto& s : stages_) s.reset(); }

    const char* name() const override { return "BiquadCascade"; }

    float processSample(float x, int channel = 0)
    {
        for (auto& s : stages_) x = s.processSample(x, channel);
        return x;
    }

    double magnitudeDb(double freqHz) const
    {
        double total = 0.0;
        for (const auto& s : stages_)
            total += s.coefficients().magnitudeDb(freqHz, sampleRate_);
        return total;
    }

    size_t numStages() const { return stages_.size(); }

private:
    std::vector<Biquad> stages_;
    double              sampleRate_ = kDefaultRate;
};

}   // namespace audio
