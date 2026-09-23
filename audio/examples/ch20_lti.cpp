// Example - ch20_lti.cpp
// Tests five systems for linearity and time-invariance, measures their
// impulse responses, and checks whether convolution with h[n] reproduces
// the system exactly. See Chapter 20.
//
//     cmake --build build && ./build/bin/ch20_lti

#include <audio/audio.h>
#include <audio/signal.h>

#include <iostream>
#include <iomanip>
#include <vector>
#include <cmath>
#include <memory>

using namespace audio;

// ---------------------------------------------------------------- systems

struct System
{
    virtual ~System() = default;
    virtual float process(float x) = 0;
    virtual void  reset() = 0;
    virtual const char* name() const = 0;
};

struct GainSys : System
{
    float g;
    explicit GainSys(float gain) : g(gain) {}
    float process(float x) override { return g * x; }
    void  reset() override {}
    const char* name() const override { return "gain 0.5"; }
};

struct AveragerSys : System
{
    float x1 = 0.0f;
    float process(float x) override { const float y = 0.5f * x + 0.5f * x1; x1 = x; return y; }
    void  reset() override { x1 = 0.0f; }
    const char* name() const override { return "2-point average"; }
};

struct OnePoleSys : System
{
    double y1 = 0.0;
    double a  = 0.9;
    float process(float x) override
    {
        y1 = (1.0 - a) * static_cast<double>(x) + a * y1;
        return static_cast<float>(y1);
    }
    void  reset() override { y1 = 0.0; }
    const char* name() const override { return "one-pole LP (a=0.9)"; }
};

struct SaturatorSys : System              // linear? NO
{
    float process(float x) override { return std::tanh(3.0f * x); }
    void  reset() override {}
    const char* name() const override { return "tanh saturator"; }
};

struct TimeVaryingSys : System            // time-invariant? NO
{
    long n = 0;
    float process(float x) override
    {
        const float g = 0.5f + 0.5f * std::sin(static_cast<float>(n) * 0.001f);
        ++n;
        return g * x;
    }
    void  reset() override { n = 0; }
    const char* name() const override { return "tremolo (time-varying)"; }
};

// ---------------------------------------------------------------- helpers

std::vector<float> run(System& sys, const std::vector<float>& x)
{
    std::vector<float> y(x.size());
    for (size_t n = 0; n < x.size(); ++n)
        y[n] = sys.process(x[n]);
    return y;
}

std::vector<float> impulseResponse(System& sys, size_t length)
{
    sys.reset();                                  // essential -- see Exercise 20.8
    std::vector<float> h(length);
    for (size_t n = 0; n < length; ++n)
        h[n] = sys.process(n == 0 ? 1.0f : 0.0f);
    return h;
}

bool testLinearity(System& sys)
{
    auto x1 = sig::sine(2000, 440.0, 44100.0, 0.3);
    auto x2 = sig::sine(2000, 660.0, 44100.0, 0.2);

    sys.reset(); auto y1 = run(sys, x1);
    sys.reset(); auto y2 = run(sys, x2);
    sys.reset(); auto ys = run(sys, sig::add(x1, x2));

    return sig::maxDifference(ys, sig::add(y1, y2)) < 1e-5;
}

bool testTimeInvariance(System& sys)
{
    auto x = sig::sine(2000, 440.0, 44100.0, 0.3);
    const int k = 100;

    sys.reset(); auto y        = run(sys, x);
    sys.reset(); auto yShifted = run(sys, sig::shift(x, k));

    auto expected = sig::shift(y, k);

    double worst = 0.0;
    for (size_t n = static_cast<size_t>(k) + 200; n < y.size(); ++n)
        worst = std::max(worst,
            std::fabs(static_cast<double>(yShifted[n]) - static_cast<double>(expected[n])));

    return worst < 1e-5;
}

// THE key test: does convolution with h[n] reproduce the system exactly?
bool testConvolutionEquivalence(System& sys, size_t hLength)
{
    auto h = impulseResponse(sys, hLength);
    auto x = sig::sine(2000, 300.0, 44100.0, 0.4);

    sys.reset();
    auto direct = run(sys, x);

    std::vector<float> viaConv(x.size(), 0.0f);
    for (size_t n = 0; n < x.size(); ++n)
    {
        double acc = 0.0;
        for (size_t k = 0; k <= n; ++k)
        {
            const size_t idx = n - k;
            if (idx < h.size())
                acc += static_cast<double>(x[k]) * static_cast<double>(h[idx]);
        }
        viaConv[n] = static_cast<float>(acc);
    }

    return sig::maxDifference(direct, viaConv) < 1e-4;
}

// ---------------------------------------------------------------- main

int main()
{
    std::vector<std::unique_ptr<System>> systems;
    systems.push_back(std::make_unique<GainSys>(0.5f));
    systems.push_back(std::make_unique<AveragerSys>());
    systems.push_back(std::make_unique<OnePoleSys>());
    systems.push_back(std::make_unique<SaturatorSys>());
    systems.push_back(std::make_unique<TimeVaryingSys>());

    std::cout << std::left << std::setw(26) << "system"
              << std::setw(9)  << "linear"
              << std::setw(11) << "time-inv"
              << std::setw(15) << "conv==direct"
              << "h[0..4]\n";
    std::cout << std::string(86, '-') << "\n";

    for (auto& s : systems)
    {
        const bool lin  = testLinearity(*s);
        const bool ti   = testTimeInvariance(*s);
        const bool conv = testConvolutionEquivalence(*s, 2000);
        const auto h    = impulseResponse(*s, 5);

        std::cout << std::left << std::setw(26) << s->name()
                  << std::setw(9)  << (lin  ? "yes" : " NO")
                  << std::setw(11) << (ti   ? "yes" : " NO")
                  << std::setw(15) << (conv ? "yes" : " NO");

        std::cout << std::fixed << std::setprecision(3) << std::right;
        for (float v : h) std::cout << std::setw(7) << v;
        std::cout << " ...\n" << std::left;
    }

    std::cout << "\nThe saturator is time-invariant but NOT linear.\n"
              << "The tremolo is linear but NOT time-invariant.\n"
              << "Neither is reproducible by convolution: the impulse response\n"
              << "of a non-LTI system is meaningless.\n";

    // ---- order of cascade does not matter for LTI systems ------------
    {
        std::cout << "\n--- cascade order (LTI) ---\n";
        auto x = sig::sine(4000, 300.0, 44100.0, 0.4);

        AveragerSys a1; OnePoleSys b1;
        a1.reset(); b1.reset();
        auto ab = run(b1, run(a1, x));            // A then B

        AveragerSys a2; OnePoleSys b2;
        a2.reset(); b2.reset();
        auto ba = run(a2, run(b2, x));            // B then A

        std::cout << "  max difference A->B vs B->A: "
                  << std::scientific << sig::maxDifference(ab, ba) << "\n";

        SaturatorSys s1; OnePoleSys c1;
        auto sc = run(c1, run(s1, x));
        SaturatorSys s2; OnePoleSys c2;
        auto cs = run(s2, run(c2, x));
        std::cout << "  max difference sat->LP vs LP->sat: "
                  << sig::maxDifference(sc, cs) << std::fixed << "\n";
        std::cout << "  (LTI order is irrelevant; non-linear order is not)\n";
    }

    // ---- write an impulse response out so you can look at it ---------
    {
        OnePoleSys lp;
        auto h = impulseResponse(lp, 4410);
        writeWav("impulse_response_onepole.wav", h, 44100, 1);
        std::cout << "\nWrote impulse_response_onepole.wav\n";
        std::cout << "  energy " << sig::energy(h)
                  << "   peak " << peakDb(h) << " dBFS\n";
    }

    return 0;
}
