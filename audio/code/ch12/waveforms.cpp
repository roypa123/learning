// Chapter 12 - waveforms.cpp
// The four classic waveforms, naive vs band-limited, and an audible
// demonstration of aliasing.
// Build:  g++ -std=c++17 -Wall -Wextra -O2 waveforms.cpp ../ch09/wavwriter.cpp -o waveforms.exe
// Run:    ./waveforms.exe        (VOLUME DOWN FIRST)

#include "../ch09/wavwriter.h"
#include "../ch11/decibels.h"

#include <iostream>
#include <iomanip>
#include <vector>
#include <cmath>
#include <string>
#include <functional>
#include <utility>

constexpr double kPi = 3.14159265358979323846;

// ---------------------------------------------------------------- shapes
// All take normalised phase in 0..1 and return -1..+1.

float shapeSine(double p)   { return static_cast<float>(std::sin(2.0 * kPi * p)); }
float shapeSaw(double p)    { return static_cast<float>(2.0 * p - 1.0); }
float shapeSquare(double p) { return (p < 0.5) ? 1.0f : -1.0f; }

float shapeTriangle(double p)
{
    const double t = (p < 0.5) ? (p * 2.0) : (2.0 - p * 2.0);
    return static_cast<float>(2.0 * t - 1.0);
}

float shapePulse(double p, double width)
{
    return (p < width) ? 1.0f : -1.0f;
}

// ---------------------------------------------------------------- render

std::vector<float> render(const std::function<float(double)>& shape,
                          double freq, double amplitude,
                          double seconds, double sampleRate)
{
    const size_t n = static_cast<size_t>(seconds * sampleRate);
    std::vector<float> out(n);

    double phase = 0.0;
    const double inc = freq / sampleRate;       // normalised: 1.0 = one full cycle

    for (size_t i = 0; i < n; ++i)
    {
        out[i] = static_cast<float>(amplitude * shape(phase));
        phase += inc;
        if (phase >= 1.0) phase -= 1.0;
    }
    return out;
}

// ------------------------------------------------- band-limited (additive)

std::vector<float> bandlimitedSaw(double freq, double amp, double seconds, double sr)
{
    const size_t n = static_cast<size_t>(seconds * sr);
    std::vector<float> out(n, 0.0f);

    for (int h = 1; ; ++h)
    {
        const double f = freq * h;
        if (f >= sr / 2.0) break;

        const double a = (2.0 / kPi) * ((h % 2 == 1) ? 1.0 : -1.0) / h;

        double phase = 0.0;
        const double inc = 2.0 * kPi * f / sr;

        for (size_t i = 0; i < n; ++i)
        {
            out[i] += static_cast<float>(amp * a * std::sin(phase));
            phase += inc;
            if (phase >= 2.0 * kPi) phase -= 2.0 * kPi;
        }
    }
    return out;
}

std::vector<float> bandlimitedSquare(double freq, double amp, double seconds, double sr)
{
    const size_t n = static_cast<size_t>(seconds * sr);
    std::vector<float> out(n, 0.0f);

    for (int h = 1; ; h += 2)                   // odd harmonics only
    {
        const double f = freq * h;
        if (f >= sr / 2.0) break;

        const double a = (4.0 / kPi) / h;

        double phase = 0.0;
        const double inc = 2.0 * kPi * f / sr;

        for (size_t i = 0; i < n; ++i)
        {
            out[i] += static_cast<float>(amp * a * std::sin(phase));
            phase += inc;
            if (phase >= 2.0 * kPi) phase -= 2.0 * kPi;
        }
    }
    return out;
}

// ---------------------------------------------------------------- main

int main()
{
    const double sr = 44100.0;

    // ---- 1. The four shapes at a low frequency ---------------------
    {
        std::vector<float> out;
        const std::pair<const char*, std::function<float(double)>> shapes[] = {
            { "sine",     shapeSine     },
            { "triangle", shapeTriangle },
            { "square",   shapeSquare   },
            { "saw",      shapeSaw      },
        };

        std::cout << "shapes_110.wav order:\n";
        for (const auto& s : shapes)
        {
            std::cout << "  " << s.first << "\n";
            auto part = render(s.second, 110.0, 0.3, 1.0, sr);
            out.insert(out.end(), part.begin(), part.end());
        }
        WavWriter::write("shapes_110.wav", out, static_cast<int>(sr), 1);
    }

    // ---- 2. Aliasing: a naive saw swept upward ----------------------
    {
        const size_t n = static_cast<size_t>(8.0 * sr);
        std::vector<float> out(n);

        double phase = 0.0;
        for (size_t i = 0; i < n; ++i)
        {
            const double t = static_cast<double>(i) / static_cast<double>(n);
            const double f = 100.0 * std::pow(40.0, t);     // 100 Hz -> 4 kHz

            out[i] = static_cast<float>(0.3 * shapeSaw(phase));

            phase += f / sr;
            if (phase >= 1.0) phase -= 1.0;
        }
        WavWriter::write("alias_naive_saw.wav", out, static_cast<int>(sr), 1);
        std::cout << "Wrote alias_naive_saw.wav        (listen for DESCENDING ghosts)\n";
    }

    // ---- 3. The same sweep, band-limited ----------------------------
    {
        const size_t n = static_cast<size_t>(8.0 * sr);
        std::vector<float> out(n, 0.0f);
        std::vector<double> phases(200, 0.0);

        for (size_t i = 0; i < n; ++i)
        {
            const double t  = static_cast<double>(i) / static_cast<double>(n);
            const double f0 = 100.0 * std::pow(40.0, t);

            double sum = 0.0;
            for (int h = 1; h < 200; ++h)
            {
                const double f = f0 * h;
                if (f >= sr / 2.0) break;

                const double a = (2.0 / kPi) * ((h % 2 == 1) ? 1.0 : -1.0) / h;
                sum += a * std::sin(phases[static_cast<size_t>(h)]);

                phases[static_cast<size_t>(h)] += 2.0 * kPi * f / sr;
                if (phases[static_cast<size_t>(h)] >= 2.0 * kPi)
                    phases[static_cast<size_t>(h)] -= 2.0 * kPi;
            }
            out[i] = static_cast<float>(0.3 * sum);
        }
        WavWriter::write("alias_bandlimited_saw.wav", out, static_cast<int>(sr), 1);
        std::cout << "Wrote alias_bandlimited_saw.wav  (clean)\n";
    }

    // ---- 4. Naive vs band-limited at 1k, 2k, 4k ---------------------
    {
        std::vector<float> out;
        for (double f : { 1000.0, 2000.0, 4000.0 })
        {
            auto naive = render(shapeSaw, f, 0.3, 0.8, sr);
            auto bl    = bandlimitedSaw(f, 0.3, 0.8, sr);
            out.insert(out.end(), naive.begin(), naive.end());
            out.insert(out.end(), bl.begin(), bl.end());
        }
        WavWriter::write("naive_vs_bandlimited.wav", out, static_cast<int>(sr), 1);
        std::cout << "Wrote naive_vs_bandlimited.wav   (naive, clean, x3)\n";
    }

    // ---- 5. Pulse-width modulation ----------------------------------
    {
        const size_t n = static_cast<size_t>(6.0 * sr);
        std::vector<float> out(n);

        double phase = 0.0, lfoPhase = 0.0;
        const double inc    = 110.0 / sr;
        const double lfoInc = 0.5   / sr;         // 0.5 Hz sweep of the width

        for (size_t i = 0; i < n; ++i)
        {
            const double width = 0.5 + 0.45 * std::sin(2.0 * kPi * lfoPhase);
            out[i] = static_cast<float>(0.3 * shapePulse(phase, width));

            phase    += inc;    if (phase    >= 1.0) phase    -= 1.0;
            lfoPhase += lfoInc; if (lfoPhase >= 1.0) lfoPhase -= 1.0;
        }
        WavWriter::write("pwm.wav", out, static_cast<int>(sr), 1);
        std::cout << "Wrote pwm.wav   DC offset = " << db::dcOffset(out) << "\n";
    }

    // ---- 6. Measurements --------------------------------------------
    std::cout << "\nCrest factors at amplitude 1.0:\n" << std::fixed << std::setprecision(2);
    {
        const std::pair<const char*, std::function<float(double)>> shapes[] = {
            { "sine",     shapeSine     },
            { "triangle", shapeTriangle },
            { "square",   shapeSquare   },
            { "saw",      shapeSaw      },
        };
        for (const auto& s : shapes)
        {
            auto buf = render(s.second, 110.0, 1.0, 1.0, sr);
            std::cout << "  " << std::setw(10) << std::left << s.first << std::right
                      << "  RMS " << std::setw(7) << db::rmsDb(buf) << " dBFS"
                      << "  crest " << std::setw(6) << db::crestFactorDb(buf) << " dB\n";
        }
    }

    return 0;
}
