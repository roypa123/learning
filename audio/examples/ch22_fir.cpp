// Example - ch22_fir.cpp
// Windowed-sinc FIR design: window comparison, tap-count comparison,
// measured frequency response, and a filter sweep. See Chapter 22.

#include <audio/audio.h>

#include <iostream>
#include <iomanip>
#include <vector>
#include <string>

using namespace audio;

void printResponse(const std::string& label, const std::vector<float>& h, double sr)
{
    std::cout << "\n  " << label << "  (" << h.size() << " taps, latency "
              << std::fixed << std::setprecision(2)
              << (h.size() - 1) / 2.0 / sr * 1000.0 << " ms)\n";

    for (double f : { 50.0, 200.0, 500.0, 900.0, 1000.0, 1100.0, 1500.0,
                      2000.0, 5000.0, 10000.0 })
        std::cout << "      " << std::setw(6) << static_cast<int>(f) << " Hz: "
                  << std::setw(8) << std::setprecision(2)
                  << magnitudeDb(h, f, sr) << " dB\n";
}

int main()
{
    const double sr = 44100.0;

    // ---- 1. window comparison ---------------------------------------
    std::cout << "--- window comparison, 1 kHz low-pass, 101 taps ---\n";
    std::cout << std::setw(18) << std::left << "window" << std::right
              << std::setw(12) << "@900 Hz"
              << std::setw(12) << "@1000 Hz"
              << std::setw(12) << "@1500 Hz"
              << std::setw(12) << "@4000 Hz" << "\n";

    struct W { const char* name; WindowType t; };
    const W windows[] = {
        { "Rectangular",    WindowType::Rectangular    },
        { "Hann",           WindowType::Hann           },
        { "Hamming",        WindowType::Hamming        },
        { "Blackman",       WindowType::Blackman       },
        { "BlackmanHarris", WindowType::BlackmanHarris },
        { "Kaiser(8.6)",    WindowType::Kaiser         },
    };

    std::vector<float> noiseBed(static_cast<size_t>(sr * 1.0));
    {
        FastRandom rng(11);
        PinkKellett pink;
        for (auto& s : noiseBed) s = pink.process(rng.nextFloat()) * 0.6f;
    }

    std::vector<float> windowDemo;

    for (const auto& w : windows)
    {
        auto h = designLowPass(1000.0, sr, 101, w.t);

        std::cout << std::setw(18) << std::left << w.name << std::right
                  << std::fixed << std::setprecision(2)
                  << std::setw(12) << magnitudeDb(h, 900.0,  sr)
                  << std::setw(12) << magnitudeDb(h, 1000.0, sr)
                  << std::setw(12) << magnitudeDb(h, 1500.0, sr)
                  << std::setw(12) << magnitudeDb(h, 4000.0, sr) << "\n";

        FIRFilter f;
        f.setCoefficients(h);
        auto seg = noiseBed;
        for (auto& s : seg) s = f.processSample(s, 0);
        windowDemo.insert(windowDemo.end(), seg.begin(), seg.end());
    }

    writeWav("fir_windows.wav", windowDemo, static_cast<int>(sr), 1, true);

    // ---- 2. tap count -----------------------------------------------
    std::cout << "\n--- tap count, 1 kHz low-pass, Hamming ---\n";
    std::cout << std::setw(10) << std::left << "taps" << std::right
              << std::setw(14) << "latency (ms)"
              << std::setw(14) << "@1100 Hz"
              << std::setw(14) << "@2000 Hz" << "\n";

    std::vector<float> tapDemo;
    for (size_t taps : { size_t{11}, size_t{31}, size_t{101}, size_t{501}, size_t{2001} })
    {
        auto h = designLowPass(1000.0, sr, taps);

        std::cout << std::setw(10) << std::left << taps << std::right
                  << std::fixed << std::setprecision(2)
                  << std::setw(14) << (taps - 1) / 2.0 / sr * 1000.0
                  << std::setw(14) << magnitudeDb(h, 1100.0, sr)
                  << std::setw(14) << magnitudeDb(h, 2000.0, sr) << "\n";

        FIRFilter f;
        f.setCoefficients(h);
        auto seg = noiseBed;
        for (auto& s : seg) s = f.processSample(s, 0);
        tapDemo.insert(tapDemo.end(), seg.begin(), seg.end());
    }
    writeWav("fir_tapcount.wav", tapDemo, static_cast<int>(sr), 1, true);

    // ---- 3. detailed response ---------------------------------------
    printResponse("1 kHz low-pass, 101 taps, Hamming",
                  designLowPass(1000.0, sr, 101), sr);

    // ---- 4. Kaiser from a specification ------------------------------
    {
        const double atten = 80.0, transition = 500.0;
        const size_t taps  = kaiserTapCount(atten, transition, sr);
        const double beta  = kaiserBeta(atten);

        std::cout << "\n--- Kaiser design from specification ---\n";
        std::cout << "  " << atten << " dB rejection, " << transition
                  << " Hz transition  ->  beta " << std::setprecision(3) << beta
                  << ", " << taps << " taps\n";

        auto h = designLowPass(1000.0, sr, taps, WindowType::Kaiser, beta);
        std::cout << "  measured at 1500 Hz: " << std::setprecision(1)
                  << magnitudeDb(h, 1500.0, sr) << " dB\n";
    }

    // ---- 5. spectral inversion check ---------------------------------
    {
        auto lp = designLowPass (1000.0, sr, 201);
        auto hp = designHighPass(1000.0, sr, 201);

        double worst = 0.0;
        for (double f = 20.0; f < 20000.0; f *= 1.05)
        {
            const auto sum = frequencyResponse(lp, kTwoPi * f / sr)
                           + frequencyResponse(hp, kTwoPi * f / sr);
            worst = std::max(worst, std::fabs(std::abs(sum) - 1.0));
        }
        std::cout << "\n--- spectral inversion ---\n";
        std::cout << "  max |LP + HP| deviation from 1.0: "
                  << std::scientific << worst << std::fixed << "\n";
        std::cout << "  (they are exact complements, so they sum flat)\n";
    }

    // ---- 6. a filter sweep -------------------------------------------
    {
        Oscillator osc;
        osc.setSampleRate(sr);
        osc.setFrequency(110.0);
        osc.setWaveform(Waveform::Saw);

        const size_t n = static_cast<size_t>(8.0 * sr);
        std::vector<float> out(n);

        FIRFilter f;
        const size_t blockSize = 256;
        size_t block = 0;

        for (size_t i = 0; i < n; ++i)
        {
            if (i % blockSize == 0)
            {
                const double t  = static_cast<double>(i) / static_cast<double>(n);
                const double fc = 18000.0 * std::pow(200.0 / 18000.0, t);
                f.setCoefficients(designLowPass(fc, sr, 301));
                ++block;
            }
            out[i] = osc.nextSample() * 0.4f;
            out[i] = f.processSample(out[i], 0);
        }

        writeWav("fir_sweep.wav", out, static_cast<int>(sr), 1, true);
        std::cout << "\nWrote fir_sweep.wav (" << block
                  << " filter redesigns -- expensive, which is why synths use IIR)\n";
    }

    std::cout << "\nWrote fir_windows.wav, fir_tapcount.wav, fir_sweep.wav\n";
    return 0;
}
