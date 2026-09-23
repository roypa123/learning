// Example - ch23_biquad.cpp
// The eight biquad types, Q and resonance, Butterworth cascades, and the
// classic filter sweep. See Chapter 23.

#include <audio/audio.h>

#include <iostream>
#include <iomanip>
#include <vector>
#include <string>

using namespace audio;

std::vector<float> makeSaw(double freq, double seconds, double sr)
{
    Oscillator osc;
    osc.setSampleRate(sr);
    osc.setFrequency(freq);
    osc.setWaveform(Waveform::Saw);

    std::vector<float> out(static_cast<size_t>(seconds * sr));
    for (auto& s : out) s = osc.nextSample() * 0.4f;
    return out;
}

std::vector<float> makePink(double seconds, double sr)
{
    FastRandom  rng(23);
    PinkKellett pink;
    std::vector<float> out(static_cast<size_t>(seconds * sr));
    for (auto& s : out) s = pink.process(rng.nextFloat()) * 0.6f;
    return out;
}

int main()
{
    const double sr = 44100.0;

    // ---- 1. the eight filter types ----------------------------------
    {
        struct T { const char* name; FilterType type; double gain; };
        const T types[] = {
            { "low-pass",   FilterType::LowPass,    0.0 },
            { "high-pass",  FilterType::HighPass,   0.0 },
            { "band-pass",  FilterType::BandPass,   0.0 },
            { "notch",      FilterType::Notch,      0.0 },
            { "all-pass",   FilterType::AllPass,    0.0 },
            { "peaking+12", FilterType::Peaking,   12.0 },
            { "low shelf",  FilterType::LowShelf,  12.0 },
            { "high shelf", FilterType::HighShelf, 12.0 },
        };

        std::cout << "--- the eight types at 1 kHz, Q=0.707 ---\n";
        std::cout << std::setw(12) << std::left << "type" << std::right
                  << std::setw(11) << "@100 Hz" << std::setw(11) << "@1 kHz"
                  << std::setw(11) << "@10 kHz" << std::setw(10) << "stable" << "\n";

        auto pink = makePink(1.0, sr);
        std::vector<float> demo;

        for (const auto& t : types)
        {
            const auto c = BiquadCoeffs::design(t.type, 1000.0, sr, 0.707, t.gain);

            std::cout << std::setw(12) << std::left << t.name << std::right
                      << std::fixed << std::setprecision(2)
                      << std::setw(11) << c.magnitudeDb(100.0,   sr)
                      << std::setw(11) << c.magnitudeDb(1000.0,  sr)
                      << std::setw(11) << c.magnitudeDb(10000.0, sr)
                      << std::setw(10) << (c.isStable() ? "yes" : "NO") << "\n";

            Biquad f;
            f.setSampleRate(sr);
            f.setCoefficients(c);
            auto seg = pink;
            for (auto& s : seg) s = f.processSample(s, 0);
            demo.insert(demo.end(), seg.begin(), seg.end());
        }
        writeWav("biquad_types.wav", demo, static_cast<int>(sr), 1, true);
    }

    // ---- 2. Q and resonance -----------------------------------------
    {
        std::cout << "\n--- Q / resonance, 1 kHz low-pass ---\n";
        std::cout << std::setw(8) << std::left << "Q" << std::right
                  << std::setw(14) << "peak (dB)"
                  << std::setw(14) << "@1 kHz"
                  << std::setw(16) << "pole radius" << "\n";

        auto saw = makeSaw(110.0, 0.8, sr);
        std::vector<float> demo;

        for (double Q : { 0.5, 0.7071, 1.0, 2.0, 5.0, 10.0, 20.0 })
        {
            const auto c = BiquadCoeffs::design(FilterType::LowPass, 1000.0, sr, Q);

            double peakDbVal = -300.0;
            for (double f = 200.0; f < 4000.0; f *= 1.005)
                peakDbVal = std::max(peakDbVal, c.magnitudeDb(f, sr));

            const double poleRadius = std::sqrt(std::fabs(c.a2));

            std::cout << std::setw(8) << std::left << Q << std::right
                      << std::fixed << std::setprecision(2)
                      << std::setw(14) << peakDbVal
                      << std::setw(14) << c.magnitudeDb(1000.0, sr)
                      << std::setw(16) << std::setprecision(5) << poleRadius << "\n";

            Biquad f;
            f.setSampleRate(sr);
            f.setCoefficients(c);
            auto seg = saw;
            for (auto& s : seg) s = f.processSample(s, 0);
            demo.insert(demo.end(), seg.begin(), seg.end());
        }
        writeWav("biquad_resonance.wav", demo, static_cast<int>(sr), 1, true);
    }

    // ---- 3. Butterworth cascades ------------------------------------
    {
        std::cout << "\n--- Butterworth cascades, 1 kHz low-pass ---\n";
        std::cout << std::setw(8) << std::left << "order" << std::right
                  << std::setw(12) << "slope"
                  << std::setw(12) << "@1 kHz"
                  << std::setw(12) << "@2 kHz"
                  << std::setw(12) << "@4 kHz" << "  stage Qs\n";

        for (int order : { 2, 4, 6, 8 })
        {
            BiquadCascade casc;
            casc.setSampleRate(sr);
            casc.setButterworth(FilterType::LowPass, 1000.0, order);

            std::cout << std::setw(8) << std::left << order << std::right
                      << std::setw(12) << (order * 6)
                      << std::fixed << std::setprecision(2)
                      << std::setw(12) << casc.magnitudeDb(1000.0)
                      << std::setw(12) << casc.magnitudeDb(2000.0)
                      << std::setw(12) << casc.magnitudeDb(4000.0)
                      << "  ";

            for (double q : butterworthQ(order))
                std::cout << std::setprecision(4) << q << " ";
            std::cout << "\n";
        }

        // The common mistake: identical Q in every stage.
        BiquadCascade wrong;
        wrong.setSampleRate(sr);
        Biquad a, b;
        a.setSampleRate(sr); a.setFilter(FilterType::LowPass, 1000.0, 0.7071);
        b.setSampleRate(sr); b.setFilter(FilterType::LowPass, 1000.0, 0.7071);
        const double atCutoff = a.coefficients().magnitudeDb(1000.0, sr)
                              + b.coefficients().magnitudeDb(1000.0, sr);
        std::cout << "\n  two Q=0.7071 stages give " << std::setprecision(2)
                  << atCutoff << " dB at the cutoff, not -3.01 dB.\n";
    }

    // ---- 4. the classic sweep ---------------------------------------
    {
        Oscillator osc;
        osc.setSampleRate(sr);
        osc.setFrequency(55.0);
        osc.setWaveform(Waveform::Saw);

        Biquad filt;
        filt.setSampleRate(sr);

        const size_t n = static_cast<size_t>(6.0 * sr);
        std::vector<float> out(n);

        double smoothedCutoff = 8000.0;

        for (size_t i = 0; i < n; ++i)
        {
            const double t      = static_cast<double>(i) / static_cast<double>(n);
            const double target = 8000.0 * std::pow(100.0 / 8000.0, t);

            // One-pole parameter smoothing: avoids zipper noise (s23.8).
            smoothedCutoff += (target - smoothedCutoff) * 0.002;

            if (i % 16 == 0)
                filt.setFilter(FilterType::LowPass, smoothedCutoff, 6.0);

            out[i] = filt.processSample(osc.nextSample() * 0.35f, 0);
        }

        const double p = peak(out);
        if (p > 0.0) for (auto& s : out) s = static_cast<float>(s * 0.9 / p);

        writeWav("biquad_sweep.wav", out, static_cast<int>(sr), 1, true);
        std::cout << "\nWrote biquad_sweep.wav -- the sound of subtractive synthesis\n";
    }

    std::cout << "Wrote biquad_types.wav, biquad_resonance.wav, biquad_sweep.wav\n";
    return 0;
}
