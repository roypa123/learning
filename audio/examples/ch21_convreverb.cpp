// Example - ch21_convreverb.cpp
// Builds synthetic room impulse responses and convolves a dry source with
// them. A real reverb, from a double loop. See Chapter 21.
//
//     cmake --build build && ./build/bin/ch21_convreverb
//
// NOTE: the cathedral takes several seconds. That slowness is the point --
// it is why Chapter 48 exists.

#include <audio/audio.h>
#include <audio/signal.h>
#include <audio/convolve.h>

#include <iostream>
#include <iomanip>
#include <vector>
#include <cmath>
#include <chrono>
#include <string>
#include <algorithm>

using namespace audio;

// ---------------------------------------------------------- synthetic IR

std::vector<float> makeRoomIR(double rt60Seconds,
                              double preDelayMs,
                              double sampleRate,
                              double dampingCoeff = 0.35,
                              uint32_t seed = 4242)
{
    const size_t length = static_cast<size_t>(rt60Seconds * 1.3 * sampleRate);
    std::vector<float> h(length, 0.0f);

    FastRandom rng(seed);

    // --- 1. direct sound ------------------------------------------
    const size_t preDelay = static_cast<size_t>(preDelayMs * 0.001 * sampleRate);
    h[0] = 1.0f;

    // --- 2. early reflections: sparse and IRREGULAR ---------------
    // Regularly spaced reflections comb-filter and sound metallic.
    const double reflectionTimesMs[] = { 11, 17, 23, 29, 37, 41, 53, 61, 73, 89 };
    for (double ms : reflectionTimesMs)
    {
        const size_t pos = preDelay + static_cast<size_t>(ms * 0.001 * sampleRate);
        if (pos < length)
            h[pos] += static_cast<float>(0.7 * std::exp(-ms / 45.0)
                                         * (rng.nextFloat() > 0.0f ? 1.0 : -1.0));
    }

    // --- 3. late reverb: exponentially decaying, low-passed noise -
    const double decay = std::pow(10.0, -60.0 / (20.0 * rt60Seconds * sampleRate));
    double gain    = 1.0;
    double lpState = 0.0;

    const size_t lateStart = preDelay + static_cast<size_t>(0.02 * sampleRate);

    for (size_t n = lateStart; n < length; ++n)
    {
        const double white = rng.nextFloat();
        lpState = lpState * dampingCoeff + white * (1.0 - dampingCoeff);

        h[n] += static_cast<float>(lpState * gain * 0.35);
        gain *= decay;
        if (gain < 1e-9) break;
    }

    // --- 4. fade the end so convolved sounds do not click ----------
    const size_t fade = std::min(length / 10, static_cast<size_t>(0.05 * sampleRate));
    for (size_t i = 0; i < fade; ++i)
        h[length - 1 - i] *= static_cast<float>(i) / static_cast<float>(fade);

    return h;
}

// ---------------------------------------------------------- dry source

std::vector<float> makeDrySource(double sr)
{
    const size_t n = static_cast<size_t>(0.4 * sr);
    std::vector<float> out(n, 0.0f);

    Oscillator osc;
    osc.setSampleRate(sr);
    osc.setFrequency(midiToFrequency(50));      // D3
    osc.setWaveform(Waveform::Saw);

    ADSR env;
    env.setSampleRate(sr);
    env.setParameters(0.002, 0.15, 0.0, 0.02);
    env.noteOn();

    for (size_t i = 0; i < n; ++i)
        out[i] = osc.nextSample() * env.nextSample() * 0.6f;

    return out;
}

// ---------------------------------------------------------- RT60 measure

double measureRT60(const std::vector<float>& h, double sr)
{
    const double p = peak(h);
    if (p <= 0.0) return 0.0;

    const double threshold = p * dbToGain(-60.0);
    for (size_t n = h.size(); n-- > 0; )
        if (std::fabs(h[n]) > threshold)
            return static_cast<double>(n) / sr;
    return 0.0;
}

// ---------------------------------------------------------------- main

int main()
{
    const double sr = 44100.0;

    auto dry = makeDrySource(sr);
    writeWav("reverb_dry.wav", dry, static_cast<int>(sr), 1, true);
    std::cout << "Wrote reverb_dry.wav (" << dry.size() << " samples)\n\n";

    struct Space { const char* name; double rt60; double preDelay; double damping; };
    const Space spaces[] = {
        { "small_room", 0.4,  8.0, 0.55 },
        { "hall",       2.2, 25.0, 0.40 },
        { "cathedral",  6.5, 45.0, 0.30 },
        { "plate",      3.0,  0.0, 0.15 },      // bright, no pre-delay
    };

    std::cout << std::left << std::setw(13) << "space"
              << std::right << std::setw(9) << "taps"
              << std::setw(9) << "len(s)"
              << std::setw(10) << "RT60(s)"
              << std::setw(11) << "time(s)"
              << std::setw(11) << "GMAC/s" << "\n";
    std::cout << std::string(63, '-') << "\n";

    for (const auto& s : spaces)
    {
        auto h = makeRoomIR(s.rt60, s.preDelay, sr, s.damping);

        const auto t0 = std::chrono::steady_clock::now();
        auto wet = convolve(dry, h);
        const auto t1 = std::chrono::steady_clock::now();

        // Mix: the wet signal is much longer, so start from it.
        std::vector<float> out = wet;
        for (auto& v : out) v *= 0.35f;
        for (size_t i = 0; i < dry.size(); ++i)
            out[i] += dry[i] * 0.8f;

        const double p = peak(out);
        if (p > 0.0)
        {
            const float g = static_cast<float>(0.9 / p);
            for (auto& v : out) v *= g;
        }

        writeWav(std::string("reverb_") + s.name + ".wav", out,
                 static_cast<int>(sr), 1, true);

        const double seconds = std::chrono::duration<double>(t1 - t0).count();
        const double macs    = static_cast<double>(dry.size()) * static_cast<double>(h.size());

        std::cout << std::left << std::setw(13) << s.name << std::right
                  << std::setw(9) << h.size()
                  << std::setw(9) << std::fixed << std::setprecision(2)
                  << h.size() / sr
                  << std::setw(10) << measureRT60(h, sr)
                  << std::setw(11) << std::setprecision(3) << seconds
                  << std::setw(11) << std::setprecision(2) << macs / seconds / 1e9
                  << "\n";
    }

    // ---- verify the algebraic properties ----------------------------
    std::cout << "\n--- convolution properties ---\n";
    {
        FastRandom rng(7);
        std::vector<float> a(200), b(50), c(30);
        for (auto& v : a) v = rng.nextFloat();
        for (auto& v : b) v = rng.nextFloat();
        for (auto& v : c) v = rng.nextFloat();

        std::cout << std::scientific << std::setprecision(3);

        std::cout << "  commutative  a*b vs b*a          : "
                  << sig::maxDifference(convolve(a, b), convolve(b, a)) << "\n";

        std::cout << "  associative  (a*b)*c vs a*(b*c)  : "
                  << sig::maxDifference(convolve(convolve(a, b), c),
                                        convolve(a, convolve(b, c))) << "\n";

        std::cout << "  distributive a*(b+c) vs a*b + a*c: "
                  << sig::maxDifference(convolve(a, sig::add(b, c)),
                                        sig::add(convolve(a, b), convolve(a, c))) << "\n";

        auto delta = sig::impulse(1);
        std::cout << "  identity     a*delta vs a        : "
                  << sig::maxDifference(convolve(a, delta), a) << "\n";

        std::cout << "  scatter vs gather form           : "
                  << sig::maxDifference(convolve(a, b), convolveGather(a, b))
                  << std::fixed << "\n";
    }

    // ---- autocorrelation finds the period ---------------------------
    std::cout << "\n--- autocorrelation pitch detection ---\n";
    {
        auto tone = sig::sine(8192, 220.0, sr, 0.5);
        auto r    = autocorrelate(tone, 1000);

        size_t bestLag = 0;
        float  best    = 0.0f;
        for (size_t lag = 20; lag < r.size(); ++lag)
            if (r[lag] > best) { best = r[lag]; bestLag = lag; }

        std::cout << "  peak at lag " << bestLag << " samples"
                  << "  -> " << std::setprecision(2) << sr / bestLag << " Hz"
                  << "  (expected 220.00 Hz at lag "
                  << std::setprecision(2) << sr / 220.0 << ")\n";
    }

    return 0;
}
