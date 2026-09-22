// Chapter 13 - envelopes.cpp
// Clicks vs fades, an ADSR preset gallery, and linear vs exponential decay.
// Build:  g++ -std=c++17 -Wall -Wextra -O2 envelopes.cpp ../ch09/wavwriter.cpp -o envelopes.exe
// Run:    ./envelopes.exe

#include "adsr.h"
#include "../ch09/wavwriter.h"
#include "../ch11/decibels.h"

#include <iostream>
#include <vector>
#include <cmath>

constexpr double kTwoPi = 6.283185307179586;

// Render one note: an oscillator through an ADSR.
std::vector<float> renderNote(double freq, double amp,
                              double heldSeconds, double tailSeconds,
                              double a, double d, double s, double r,
                              double sr)
{
    const size_t heldSamples  = static_cast<size_t>(heldSeconds * sr);
    const size_t totalSamples = static_cast<size_t>((heldSeconds + tailSeconds) * sr);

    std::vector<float> out(totalSamples, 0.0f);

    ADSR env;
    env.setSampleRate(sr);
    env.setParameters(a, d, s, r);
    env.noteOn();

    double phase = 0.0;
    const double inc = kTwoPi * freq / sr;

    for (size_t i = 0; i < totalSamples; ++i)
    {
        if (i == heldSamples)
            env.noteOff();

        const double osc = std::sin(phase);
        out[i] = static_cast<float>(amp * osc * env.nextSample());

        phase += inc;
        if (phase >= kTwoPi) phase -= kTwoPi;
    }

    return out;
}

int main()
{
    const double sr = 44100.0;

    // ---- 1. The click, then the fix ---------------------------------
    {
        std::vector<float> out;

        // (a) No envelope at all: clicks at both ends.
        {
            std::vector<float> raw(static_cast<size_t>(sr));
            double phase = 0.0;
            const double inc = kTwoPi * 440.0 / sr;
            for (auto& v : raw)
            {
                v = static_cast<float>(0.4 * std::sin(phase));
                phase += inc;
                if (phase >= kTwoPi) phase -= kTwoPi;
            }
            out.insert(out.end(), raw.begin(), raw.end());
        }

        out.insert(out.end(), static_cast<size_t>(0.3 * sr), 0.0f);

        // (b) 5 ms fades: clean.
        {
            auto clean = renderNote(440.0, 0.4, 0.995, 0.005,
                                    0.005, 0.0, 1.0, 0.005, sr);
            out.insert(out.end(), clean.begin(), clean.end());
        }

        WavWriter::write("click_vs_fade.wav", out, static_cast<int>(sr), 1);
        std::cout << "Wrote click_vs_fade.wav  (clicky, then clean)\n";
    }

    // ---- 2. The preset gallery ---------------------------------------
    {
        struct Preset { const char* name; double a, d, s, r, held, tail; };
        const Preset presets[] = {
            { "organ",      0.002, 0.000, 1.00, 0.005, 1.0, 0.2 },
            { "piano",      0.002, 1.500, 0.00, 0.300, 1.0, 1.5 },
            { "pluck",      0.001, 0.400, 0.00, 0.100, 0.5, 0.6 },
            { "pad",        0.400, 0.200, 0.80, 0.800, 1.5, 1.2 },
            { "brass",      0.060, 0.100, 0.85, 0.150, 1.0, 0.4 },
            { "percussion", 0.000, 0.150, 0.00, 0.010, 0.2, 0.3 },
            { "reverse",    2.000, 0.010, 0.00, 0.010, 2.0, 0.1 },
        };

        std::cout << "adsr_presets.wav order:\n";
        std::vector<float> out;
        for (const auto& p : presets)
        {
            std::cout << "  " << p.name << "\n";
            auto note = renderNote(220.0, 0.5, p.held, p.tail, p.a, p.d, p.s, p.r, sr);
            out.insert(out.end(), note.begin(), note.end());
            out.insert(out.end(), static_cast<size_t>(0.3 * sr), 0.0f);
        }
        WavWriter::write("adsr_presets.wav", out, static_cast<int>(sr), 1);
    }

    // ---- 3. Linear vs exponential decay -----------------------------
    {
        const size_t n = static_cast<size_t>(3.0 * sr);
        std::vector<float> lin(n), expo(n);

        double phase = 0.0;
        const double inc = kTwoPi * 330.0 / sr;
        const double decayPerSample = std::pow(10.0, -60.0 / (20.0 * 3.0 * sr));
        double g = 1.0;

        for (size_t i = 0; i < n; ++i)
        {
            const double osc = std::sin(phase);
            const double t   = static_cast<double>(i) / static_cast<double>(n);

            lin[i]  = static_cast<float>(0.5 * osc * (1.0 - t));
            expo[i] = static_cast<float>(0.5 * osc * g);

            g *= decayPerSample;
            if (g < 1e-8) g = 0.0;          // avoid denormals

            phase += inc;
            if (phase >= kTwoPi) phase -= kTwoPi;
        }

        std::vector<float> both;
        both.insert(both.end(), lin.begin(),  lin.end());
        both.insert(both.end(), expo.begin(), expo.end());
        WavWriter::write("linear_vs_exponential.wav", both, static_cast<int>(sr), 1);
        std::cout << "Wrote linear_vs_exponential.wav  (linear 3 s, then exponential 3 s)\n";
    }

    // ---- 4. A kick drum: pitch envelope + amplitude envelope --------
    {
        const size_t n = static_cast<size_t>(1.0 * sr);
        std::vector<float> out(n);

        ADSR amp;
        amp.setSampleRate(sr);
        amp.setParameters(0.0005, 0.300, 0.0, 0.010);
        amp.noteOn();

        double phase = 0.0;
        double pitch = 150.0;
        const double pitchDecayPerSample = std::exp(-1.0 / (0.040 * sr));

        for (size_t i = 0; i < n; ++i)
        {
            const double f = 50.0 + (pitch - 50.0);          // current pitch
            out[i] = static_cast<float>(0.8 * std::sin(phase) * amp.nextSample());

            phase += kTwoPi * f / sr;
            if (phase >= kTwoPi) phase -= kTwoPi;

            pitch = 50.0 + (pitch - 50.0) * pitchDecayPerSample;   // 150 -> 50 Hz
        }

        WavWriter::write("kick.wav", out, static_cast<int>(sr), 1);
        std::cout << "Wrote kick.wav   peak " << db::peakDb(out) << " dBFS\n";
    }

    return 0;
}
