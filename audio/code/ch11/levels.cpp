// Chapter 11 - levels.cpp
// Demonstrates dB conversion and measures peak / RMS / crest factor / DC
// for a range of test signals.
// Build:  g++ -std=c++17 -Wall -Wextra -O2 levels.cpp ../ch09/wavwriter.cpp -o levels.exe
// Run:    ./levels.exe

#include "decibels.h"
#include "../ch09/wavwriter.h"

#include <iostream>
#include <iomanip>
#include <vector>
#include <cmath>
#include <string>

constexpr double kPi    = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;

std::vector<float> makeSine(double freq, double amp, double seconds, double sr)
{
    std::vector<float> out(static_cast<size_t>(seconds * sr));
    double phase = 0.0;
    const double inc = kTwoPi * freq / sr;
    for (auto& s : out)
    {
        s = static_cast<float>(amp * std::sin(phase));
        phase += inc;
        if (phase >= kTwoPi) phase -= kTwoPi;
    }
    return out;
}

std::vector<float> makeSquare(double freq, double amp, double seconds, double sr)
{
    std::vector<float> out(static_cast<size_t>(seconds * sr));
    double phase = 0.0;
    const double inc = kTwoPi * freq / sr;
    for (auto& s : out)
    {
        s = static_cast<float>(phase < kPi ? amp : -amp);
        phase += inc;
        if (phase >= kTwoPi) phase -= kTwoPi;
    }
    return out;
}

void report(const std::string& name, const std::vector<float>& buf)
{
    std::cout << std::fixed << std::setprecision(2)
              << std::setw(22) << std::left  << name << std::right
              << "  peak " << std::setw(7) << db::peakDb(buf) << " dBFS"
              << "   RMS " << std::setw(7) << db::rmsDb(buf)  << " dBFS"
              << "   crest " << std::setw(6) << db::crestFactorDb(buf) << " dB"
              << "   DC " << std::setw(8) << std::setprecision(5)
              << db::dcOffset(buf) << "\n"
              << std::setprecision(2);
}

int main()
{
    const double sr = 44100.0;

    std::cout << "--- dB conversion table ---\n";
    std::cout << std::fixed << std::setprecision(4);
    for (double g : { 1.0, 0.707, 0.5, 0.25, 0.1, 0.01, 0.001 })
        std::cout << "  gain " << std::setw(7) << g
                  << "  ->  " << std::setw(8) << std::setprecision(2)
                  << db::gainToDb(g) << " dB\n" << std::setprecision(4);

    std::cout << "\n";
    for (double d : { 0.0, -1.0, -3.0, -6.0, -10.0, -20.0, -40.0, -60.0 })
        std::cout << "  " << std::setw(6) << std::setprecision(1) << d
                  << " dB  ->  gain " << std::setprecision(6)
                  << db::dbToGain(d) << "\n";

    std::cout << "\n--- signal measurements ---\n";

    report("sine 1.0",   makeSine  (440.0, 1.0,  1.0, sr));
    report("sine 0.5",   makeSine  (440.0, 0.5,  1.0, sr));
    report("sine 0.25",  makeSine  (440.0, 0.25, 1.0, sr));
    report("square 1.0", makeSquare(440.0, 1.0,  1.0, sr));
    report("square 0.5", makeSquare(440.0, 0.5,  1.0, sr));

    {
        std::vector<float> impulse(static_cast<size_t>(sr), 0.0f);
        impulse[22050] = 1.0f;
        report("impulse", impulse);
    }

    {
        auto buf = makeSine(440.0, 0.4, 1.0, sr);
        for (auto& s : buf) s += 0.2f;
        report("sine 0.4 + DC 0.2", buf);
    }

    std::cout << "\n--- the -6 dB ladder (listen) ---\n";
    {
        std::vector<float> out;
        for (int step = 0; step < 8; ++step)
        {
            const double gain = db::dbToGain(-6.0 * step);
            std::cout << "  step " << step << ": " << std::setprecision(1)
                      << -6.0 * step << " dB  gain " << std::setprecision(5)
                      << gain << "\n";

            auto part = makeSine(440.0, gain, 0.5, sr);
            out.insert(out.end(), part.begin(), part.end());
        }
        WavWriter::write("db_ladder.wav", out, static_cast<int>(sr), 1);
        std::cout << "  -> wrote db_ladder.wav\n";
    }

    return 0;
}
