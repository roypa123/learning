// Chapter 10 - sine.cpp
// Your first sound. Generates sine waves with a phase accumulator and writes
// them to WAV files.
// Build:  g++ -std=c++17 -Wall -Wextra -O2 sine.cpp ../ch09/wavwriter.cpp -o sine.exe
// Run:    ./sine.exe        (TURN YOUR VOLUME DOWN FIRST)

#include "../ch09/wavwriter.h"

#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>

constexpr double kPi    = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;

// Generate a sine wave using a phase accumulator.
//   freq       : frequency in hertz
//   amplitude  : peak level, 0.0 .. 1.0
//   seconds    : duration
//   sampleRate : samples per second
//   phase0     : starting phase in radians
std::vector<float> generateSine(double freq,
                                double amplitude,
                                double seconds,
                                double sampleRate,
                                double phase0 = 0.0)
{
    const size_t numSamples = static_cast<size_t>(seconds * sampleRate);
    std::vector<float> buffer(numSamples);

    double phase = phase0;
    const double phaseIncrement = kTwoPi * freq / sampleRate;   // computed ONCE

    for (size_t n = 0; n < numSamples; ++n)
    {
        buffer[n] = static_cast<float>(amplitude * std::sin(phase));

        phase += phaseIncrement;
        if (phase >= kTwoPi)
            phase -= kTwoPi;
    }

    return buffer;
}

int main()
{
    const double sampleRate = 44100.0;
    const double seconds    = 2.0;

    // ---- 1. The canonical test tone: A4, 440 Hz ---------------------
    {
        auto tone = generateSine(440.0, 0.5, seconds, sampleRate);
        WavWriter::write("sine440.wav", tone, static_cast<int>(sampleRate), 1);
        std::cout << "Wrote sine440.wav\n";

        std::cout << "First 10 samples:\n";
        for (int i = 0; i < 10; ++i)
            std::cout << "  n=" << i << "  " << tone[static_cast<size_t>(i)] << "\n";
    }

    // ---- 2. Three amplitudes ----------------------------------------
    {
        std::vector<float> out;
        for (double amp : { 0.8, 0.25, 0.08 })
        {
            auto part = generateSine(440.0, amp, 0.7, sampleRate);
            out.insert(out.end(), part.begin(), part.end());
        }
        WavWriter::write("amplitudes.wav", out, static_cast<int>(sampleRate), 1);
        std::cout << "Wrote amplitudes.wav\n";
    }

    // ---- 3. An octave ladder ----------------------------------------
    {
        std::vector<float> out;
        for (double f : { 110.0, 220.0, 440.0, 880.0, 1760.0, 3520.0 })
        {
            auto part = generateSine(f, 0.4, 0.6, sampleRate);
            out.insert(out.end(), part.begin(), part.end());
        }
        WavWriter::write("octaves.wav", out, static_cast<int>(sampleRate), 1);
        std::cout << "Wrote octaves.wav\n";
    }

    // ---- 4. Beating: 440 Hz and 443 Hz together ---------------------
    {
        auto a = generateSine(440.0, 0.35, 5.0, sampleRate);
        auto b = generateSine(443.0, 0.35, 5.0, sampleRate);

        std::vector<float> mix(a.size());
        for (size_t i = 0; i < a.size(); ++i)
            mix[i] = a[i] + b[i];                 // superposition: just add

        WavWriter::write("beating.wav", mix, static_cast<int>(sampleRate), 1);
        std::cout << "Wrote beating.wav  (expect 3 pulses per second)\n";
    }

    // ---- 5. Cancellation --------------------------------------------
    {
        auto a = generateSine(440.0, 0.5, 2.0, sampleRate);
        auto b = generateSine(440.0, 0.5, 2.0, sampleRate, kPi);   // 180 degrees

        std::vector<float> mix(a.size());
        float maxAbs = 0.0f;
        for (size_t i = 0; i < a.size(); ++i)
        {
            mix[i] = a[i] + b[i];
            maxAbs = std::max(maxAbs, std::fabs(mix[i]));
        }

        WavWriter::write("cancellation.wav", mix, static_cast<int>(sampleRate), 1);
        std::cout << "Wrote cancellation.wav  peak = " << maxAbs << "  (should be ~0)\n";
    }

    // ---- 6. A frequency sweep, done correctly -----------------------
    {
        const size_t n = static_cast<size_t>(6.0 * sampleRate);
        std::vector<float> sweep(n);

        double phase = 0.0;
        for (size_t i = 0; i < n; ++i)
        {
            const double t = static_cast<double>(i) / static_cast<double>(n);
            const double f = 20.0 * std::pow(1000.0, t);   // 20 Hz -> 20 kHz, exponential

            sweep[i] = static_cast<float>(0.4 * std::sin(phase));

            phase += kTwoPi * f / sampleRate;              // increment changes, phase continues
            if (phase >= kTwoPi)
                phase -= kTwoPi;
        }

        WavWriter::write("sweep.wav", sweep, static_cast<int>(sampleRate), 1);
        std::cout << "Wrote sweep.wav  (20 Hz to 20 kHz over 6 s)\n";
    }

    // ---- 7. Detuned stereo: binaural beating ------------------------
    {
        auto left  = generateSine(220.0, 0.4, 4.0, sampleRate);
        auto right = generateSine(220.5, 0.4, 4.0, sampleRate);

        std::vector<std::vector<float>> stereo = { left, right };
        WavWriter::writePlanar("detuned_stereo.wav", stereo, static_cast<int>(sampleRate));
        std::cout << "Wrote detuned_stereo.wav  (headphones only)\n";
    }

    // ---- 8. Additive square wave ------------------------------------
    {
        const double f0 = 220.0;
        const int    numHarmonics = 16;         // try 1, 2, 4, 8, 16, 64
        const size_t n = static_cast<size_t>(2.0 * sampleRate);

        std::vector<float> out(n, 0.0f);

        for (int h = 1; h <= numHarmonics * 2; h += 2)      // odd harmonics: 1, 3, 5, 7...
        {
            const double freq = f0 * h;
            if (freq >= sampleRate / 2.0)                    // stay below Nyquist
                break;

            double phase = 0.0;
            const double inc = kTwoPi * freq / sampleRate;

            for (size_t i = 0; i < n; ++i)
            {
                out[i] += static_cast<float>(0.5 * (4.0 / kPi) * (1.0 / h) * std::sin(phase));
                phase += inc;
                if (phase >= kTwoPi) phase -= kTwoPi;
            }
        }

        WavWriter::write("additive_square.wav", out, static_cast<int>(sampleRate), 1);
        std::cout << "Wrote additive_square.wav  (" << numHarmonics << " harmonics)\n";
    }

    return 0;
}
