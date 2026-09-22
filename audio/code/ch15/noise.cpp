// Chapter 15 - noise.cpp
// Generates every colour of noise, measures its spectral tilt in octave
// bands, and builds a simple wind sound from pink noise.
// Build:  g++ -std=c++17 -Wall -Wextra -O2 noise.cpp ../ch09/wavwriter.cpp -o noise.exe
// Run:    ./noise.exe

#include "noisegen.h"
#include "../ch09/wavwriter.h"
#include "../ch11/decibels.h"

#include <iostream>
#include <iomanip>
#include <vector>
#include <cmath>
#include <string>

constexpr double kPi = 3.14159265358979323846;

// Measure RMS in octave bands using a simple two-pole resonator per band.
// Crude, but enough to see the tilt. A proper FFT arrives in Chapter 25.
void reportSpectralTilt(const std::string& name, const std::vector<float>& buf, double sr)
{
    const double centres[] = { 62.5, 125, 250, 500, 1000, 2000, 4000, 8000, 16000 };

    std::cout << std::setw(18) << std::left << name << std::right;

    for (double fc : centres)
    {
        const double w = 2.0 * kPi * fc / sr;
        const double r = 0.995;
        double y1 = 0.0, y2 = 0.0;
        double sumSq = 0.0;

        for (float x : buf)
        {
            const double y = (1.0 - r) * x + 2.0 * r * std::cos(w) * y1 - r * r * y2;
            y2 = y1;
            y1 = y;
            sumSq += y * y;
        }

        const double rms = std::sqrt(sumSq / static_cast<double>(buf.size()));
        std::cout << std::setw(8) << std::fixed << std::setprecision(1)
                  << db::gainToDb(rms);
    }
    std::cout << "\n";
}

int main()
{
    const double sr      = 44100.0;
    const double seconds = 3.0;
    const size_t n       = static_cast<size_t>(seconds * sr);

    FastRandom  rng(12345);
    PinkVoss    voss(999);
    PinkKellett kellett;
    Brown       brown;
    Blue        blue;

    std::vector<float> white(n), pinkV(n), pinkK(n), brownBuf(n), blueBuf(n);

    for (size_t i = 0; i < n; ++i)
    {
        const float w = rng.nextFloat();

        white[i]    = w * 0.3f;
        pinkV[i]    = voss.next() * 0.6f;
        pinkK[i]    = kellett.process(w) * 0.9f;
        brownBuf[i] = brown.process(w) * 0.3f;
        blueBuf[i]  = blue.process(w) * 0.3f;
    }

    WavWriter::write("noise_white.wav",        white,    static_cast<int>(sr), 1);
    WavWriter::write("noise_pink_voss.wav",    pinkV,    static_cast<int>(sr), 1);
    WavWriter::write("noise_pink_kellett.wav", pinkK,    static_cast<int>(sr), 1);
    WavWriter::write("noise_brown.wav",        brownBuf, static_cast<int>(sr), 1);
    WavWriter::write("noise_blue.wav",         blueBuf,  static_cast<int>(sr), 1);
    std::cout << "Wrote five noise files.\n\n";

    std::cout << "Octave-band RMS in dB (constant-Q bands, so WHITE rises +3 dB/oct):\n";
    std::cout << std::setw(18) << std::left << "" << std::right
              << std::setw(8) << "62Hz"  << std::setw(8) << "125Hz"
              << std::setw(8) << "250Hz" << std::setw(8) << "500Hz"
              << std::setw(8) << "1kHz"  << std::setw(8) << "2kHz"
              << std::setw(8) << "4kHz"  << std::setw(8) << "8kHz"
              << std::setw(8) << "16kHz" << "\n";

    reportSpectralTilt("white",          white,    sr);
    reportSpectralTilt("pink (voss)",    pinkV,    sr);
    reportSpectralTilt("pink (kellett)", pinkK,    sr);
    reportSpectralTilt("brown",          brownBuf, sr);
    reportSpectralTilt("blue",           blueBuf,  sr);

    std::cout << "\nCrest factors:\n" << std::setprecision(2);
    std::cout << "  white  " << db::crestFactorDb(white)    << " dB\n";
    std::cout << "  pink   " << db::crestFactorDb(pinkK)    << " dB\n";
    std::cout << "  brown  " << db::crestFactorDb(brownBuf) << " dB\n";

    // ---- wind: pink noise through a wandering band-pass ---------------
    {
        const size_t wn = static_cast<size_t>(20.0 * sr);
        std::vector<float> wind(wn);

        PinkKellett pink;
        FastRandom  wrng(777);

        double y1 = 0.0, y2 = 0.0;

        for (size_t i = 0; i < wn; ++i)
        {
            const double t = static_cast<double>(i) / sr;

            // Two incommensurate LFOs so the pattern never repeats exactly.
            const double gust = 0.35 + 0.65 * std::fabs(std::sin(2.0 * kPi * 0.07 * t)
                                                      * std::sin(2.0 * kPi * 0.031 * t));
            const double centre = 420.0 + 320.0 * std::sin(2.0 * kPi * 0.05 * t);

            const double w = 2.0 * kPi * centre / sr;
            const double r = 0.992;

            const double x = pink.process(wrng.nextFloat());
            const double y = (1.0 - r) * x + 2.0 * r * std::cos(w) * y1 - r * r * y2;
            y2 = y1;
            y1 = y;

            wind[i] = static_cast<float>(y * gust * 4.0);
        }

        WavWriter::write("wind.wav", wind, static_cast<int>(sr), 1);
        std::cout << "\nWrote wind.wav (20 s)   peak " << db::peakDb(wind) << " dBFS\n";
    }

    return 0;
}
