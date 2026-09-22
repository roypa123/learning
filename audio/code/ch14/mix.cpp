// Chapter 14 - mix.cpp
// A four-track mix rendered three ways: hard-clipped, saturated, normalised.
// Build:  g++ -std=c++17 -Wall -Wextra -O2 mix.cpp ../ch09/wavwriter.cpp -o mix.exe
// Run:    ./mix.exe [outputGainDb]

#include "mixer.h"
#include "dcblocker.h"
#include "../ch09/wavwriter.h"
#include "../ch11/decibels.h"
#include "../ch13/adsr.h"

#include <iostream>
#include <iomanip>
#include <vector>
#include <cmath>
#include <random>
#include <string>

constexpr double kPi    = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;

// ------------------------------------------------------------- sources

std::vector<float> makeSaw(double freq, double amp, double seconds, double sr)
{
    std::vector<float> out(static_cast<size_t>(seconds * sr));
    double phase = 0.0;
    const double inc = freq / sr;
    for (auto& s : out)
    {
        s = static_cast<float>(amp * (2.0 * phase - 1.0));
        phase += inc;
        if (phase >= 1.0) phase -= 1.0;
    }
    return out;
}

// Three saws a major triad apart, each with a pad envelope.
std::vector<float> makeChord(double rootHz, double amp, double seconds, double sr)
{
    const size_t n = static_cast<size_t>(seconds * sr);
    std::vector<float> out(n, 0.0f);

    const double ratios[3] = { 1.0, 1.25992105, 1.49830708 };   // root, maj3, fifth

    for (double r : ratios)
    {
        ADSR env;
        env.setSampleRate(sr);
        env.setParameters(0.4, 0.2, 0.8, 0.8);
        env.noteOn();

        double phase = 0.0;
        const double inc = (rootHz * r) / sr;

        for (size_t i = 0; i < n; ++i)
        {
            if (i == n * 3 / 4) env.noteOff();
            out[i] += static_cast<float>(amp * (2.0 * phase - 1.0) * env.nextSample());
            phase += inc;
            if (phase >= 1.0) phase -= 1.0;
        }
    }
    return out;
}

// Filtered noise bursts on eighth notes at 120 bpm.
std::vector<float> makeHats(double seconds, double sr)
{
    const size_t n = static_cast<size_t>(seconds * sr);
    std::vector<float> out(n, 0.0f);

    std::mt19937 rng(2024);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

    const size_t stepSamples = static_cast<size_t>(sr * 0.25);   // 120 bpm eighths

    for (size_t start = 0; start < n; start += stepSamples)
    {
        ADSR env;
        env.setSampleRate(sr);
        env.setParameters(0.0005, 0.06, 0.0, 0.01);
        env.noteOn();

        float hp = 0.0f;                     // crude one-pole high-pass state
        for (size_t i = start; i < n && i < start + stepSamples; ++i)
        {
            const float white = dist(rng);
            hp = 0.85f * hp + 0.15f * white; // low-passed copy
            const float bright = white - hp; // subtract it -> high-passed
            out[i] += bright * env.nextSample() * 0.4f;
        }
    }
    return out;
}

// ------------------------------------------------------------- shaping

float softClipTanh(float x, float drive) { return std::tanh(x * drive); }

void writeVariant(const std::string& path,
                  std::vector<std::vector<float>> stereo,   // by value: we modify
                  double sr,
                  const std::string& mode,
                  double targetDb = -1.0)
{
    if (mode == "clip")
    {
        for (auto& ch : stereo)
            for (auto& s : ch)
                s = std::clamp(s, -1.0f, 1.0f);
    }
    else if (mode == "tanh")
    {
        for (auto& ch : stereo)
            for (auto& s : ch)
                s = softClipTanh(s, 1.0f);
    }
    else // normalise
    {
        double p = 0.0;
        for (const auto& ch : stereo)
            p = std::max(p, db::peak(ch));

        if (p > 0.0)
        {
            const float g = static_cast<float>(db::dbToGain(targetDb) / p);
            for (auto& ch : stereo)
                for (auto& s : ch)
                    s *= g;
        }
    }

    WavWriter::writePlanar(path, stereo, static_cast<int>(sr), true);

    std::cout << std::fixed << std::setprecision(2)
              << "  " << std::setw(22) << std::left << path << std::right
              << "  peak " << std::setw(7) << db::peakDb(stereo[0]) << " dBFS"
              << "  RMS " << std::setw(7) << db::rmsDb(stereo[0]) << " dBFS"
              << "  crest " << std::setw(6) << db::crestFactorDb(stereo[0]) << " dB\n";
}

// ------------------------------------------------------------- main

int main(int argc, char** argv)
{
    const double sr = 44100.0;

    double outputGainDb = -1.0;
    if (argc > 1)
    {
        try              { outputGainDb = std::stod(argv[1]); }
        catch (...)      { std::cerr << "Bad gain value, using -1.0 dB\n"; }
    }

    Mixer mixer;
    mixer.addTrack({ "bass",  makeSaw(55.0, 0.6, 4.0, sr),     1.0f,  0.0f, false, false });
    mixer.addTrack({ "chord", makeChord(220.0, 0.35, 4.0, sr), 0.8f, -0.3f, false, false });
    mixer.addTrack({ "lead",  makeSaw(440.0, 0.4, 4.0, sr),    0.6f,  0.4f, false, false });
    mixer.addTrack({ "hats",  makeHats(4.0, sr),               0.5f,  0.7f, false, false });

    auto stereo = mixer.render();

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Raw bus peak: L " << db::peakDb(stereo[0])
              << "  R " << db::peakDb(stereo[1]) << " dBFS\n";
    std::cout << "Raw bus DC:   L " << db::dcOffset(stereo[0])
              << "  R " << db::dcOffset(stereo[1]) << "\n\n";

    // Remove DC, per channel.
    for (auto& ch : stereo)
    {
        DCBlocker dc;
        dc.setSampleRate(sr, 20.0);
        for (auto& s : ch)
            s = dc.process(s);
    }

    std::cout << "After DC blocker: L " << db::dcOffset(stereo[0])
              << "  R " << db::dcOffset(stereo[1]) << "\n\n";

    writeVariant("mix_normalised.wav", stereo, sr, "normalise", outputGainDb);
    writeVariant("mix_saturated.wav",  stereo, sr, "tanh");
    writeVariant("mix_clipped.wav",    stereo, sr, "clip");

    return 0;
}
