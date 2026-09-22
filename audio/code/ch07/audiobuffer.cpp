// Chapter 7 - audiobuffer.cpp
// A first AudioBuffer type: planar multi-channel sample storage with
// peak / RMS / gain / normalise helpers.
// Build:  g++ -std=c++17 -Wall -Wextra -O2 audiobuffer.cpp -o audiobuffer.exe
// Run:    ./audiobuffer.exe

#include <iostream>
#include <iomanip>
#include <vector>
#include <cmath>
#include <algorithm>

// A block of audio samples: one or more channels, stored planar
// (each channel is its own contiguous vector).
struct AudioBuffer
{
    std::vector<std::vector<float>> channels;
    double sampleRate = 44100.0;

    AudioBuffer(int numChannels, int numFrames, double rate)
        : channels(static_cast<size_t>(numChannels),
                   std::vector<float>(static_cast<size_t>(numFrames), 0.0f)),
          sampleRate(rate)
    {
    }

    int numChannels() const { return static_cast<int>(channels.size()); }
    int numFrames()   const { return channels.empty()
                                     ? 0
                                     : static_cast<int>(channels[0].size()); }

    double durationSeconds() const { return numFrames() / sampleRate; }

    std::vector<float>&       channel(int c)       { return channels[static_cast<size_t>(c)]; }
    const std::vector<float>& channel(int c) const { return channels[static_cast<size_t>(c)]; }

    void clear()
    {
        for (auto& ch : channels)
            std::fill(ch.begin(), ch.end(), 0.0f);
    }

    void applyGain(float gain)
    {
        for (auto& ch : channels)
            for (float& s : ch)
                s *= gain;
    }

    // Largest absolute sample value across all channels.
    float peak() const
    {
        float p = 0.0f;
        for (const auto& ch : channels)
            for (float s : ch)
                p = std::max(p, std::fabs(s));
        return p;
    }

    // Root mean square across all channels: correlates with perceived loudness.
    // NOTE: accumulate in double. Summing 44100+ squared floats into a float
    // accumulator loses precision badly.
    float rms() const
    {
        double sumOfSquares = 0.0;
        size_t count = 0;

        for (const auto& ch : channels)
            for (float s : ch)
            {
                sumOfSquares += static_cast<double>(s) * static_cast<double>(s);
                ++count;
            }

        if (count == 0)
            return 0.0f;

        return static_cast<float>(std::sqrt(sumOfSquares / static_cast<double>(count)));
    }

    // Scale so that the loudest sample sits at `targetPeak`.
    // 0.99 rather than 1.0 leaves room for inter-sample peaks (Chapter 4).
    void normalise(float targetPeak = 0.99f)
    {
        const float p = peak();
        if (p > 0.0f)
            applyGain(targetPeak / p);
    }
};

int main()
{
    AudioBuffer buf(2, 44100, 44100.0);     // stereo, one second

    std::cout << "Channels : " << buf.numChannels() << "\n";
    std::cout << "Frames   : " << buf.numFrames() << "\n";
    std::cout << "Duration : " << buf.durationSeconds() << " s\n";
    std::cout << "Peak     : " << buf.peak() << "  (silent buffer)\n\n";

    const double twoPi = 2.0 * 3.14159265358979323846;
    const double freq  = 440.0;
    const double inc   = twoPi * freq / buf.sampleRate;

    double phase = 0.0;
    for (int i = 0; i < buf.numFrames(); ++i)
    {
        const float s = static_cast<float>(std::sin(phase));
        buf.channel(0)[static_cast<size_t>(i)] = s * 0.5f;
        buf.channel(1)[static_cast<size_t>(i)] = s * 0.25f;

        phase += inc;
        if (phase >= twoPi)
            phase -= twoPi;
    }

    std::cout << std::fixed << std::setprecision(4);
    std::cout << "After filling with a 440 Hz sine:\n";
    std::cout << "Peak     : " << buf.peak() << "\n";
    std::cout << "RMS      : " << buf.rms()  << "\n";
    std::cout << "Peak/RMS : " << buf.peak() / buf.rms() << "\n\n";

    buf.normalise();
    std::cout << "After normalise():\n";
    std::cout << "Peak     : " << buf.peak() << "\n";
    std::cout << "RMS      : " << buf.rms()  << "\n";

    return 0;
}
