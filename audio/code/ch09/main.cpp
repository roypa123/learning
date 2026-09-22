// Chapter 9 - main.cpp
// Exercises the WAV writer: silence, a mono ramp, and an inverted stereo ramp.
// Build:  g++ -std=c++17 -Wall -Wextra -O2 main.cpp wavwriter.cpp -o wavtest.exe
// Run:    ./wavtest.exe
// Then:   xxd -l 64 ramp.wav

#include "wavwriter.h"

#include <iostream>
#include <vector>
#include <cmath>

int main()
{
    const int    sampleRate = 44100;
    const double seconds    = 2.0;
    const int    numFrames  = static_cast<int>(sampleRate * seconds);

    // ---- 1. Silence -------------------------------------------------
    {
        std::vector<float> silence(static_cast<size_t>(numFrames), 0.0f);
        if (WavWriter::write("silence.wav", silence, sampleRate, 1))
            std::cout << "Wrote silence.wav  (" << numFrames << " frames)\n";
        else
            std::cerr << "FAILED to write silence.wav\n";
    }

    // ---- 2. A linear ramp: -1.0 up to +1.0 --------------------------
    {
        std::vector<float> ramp(static_cast<size_t>(numFrames));
        for (int i = 0; i < numFrames; ++i)
            ramp[static_cast<size_t>(i)] =
                -1.0f + 2.0f * static_cast<float>(i) / static_cast<float>(numFrames - 1);

        if (WavWriter::write("ramp.wav", ramp, sampleRate, 1))
            std::cout << "Wrote ramp.wav\n";
    }

    // ---- 3. Stereo: left ramps up, right ramps down -----------------
    {
        std::vector<std::vector<float>> stereo(2,
            std::vector<float>(static_cast<size_t>(numFrames)));

        for (int i = 0; i < numFrames; ++i)
        {
            const float t = static_cast<float>(i) / static_cast<float>(numFrames - 1);
            stereo[0][static_cast<size_t>(i)] =  2.0f * t - 1.0f;   // -1 -> +1
            stereo[1][static_cast<size_t>(i)] =  1.0f - 2.0f * t;   // +1 -> -1
        }

        if (WavWriter::writePlanar("ramp_stereo.wav", stereo, sampleRate))
            std::cout << "Wrote ramp_stereo.wav\n";
    }

    // ---- 4. Report the expected file sizes --------------------------
    const int monoBytes   = 44 + numFrames * 1 * 2;
    const int stereoBytes = 44 + numFrames * 2 * 2;
    std::cout << "\nExpected sizes:\n";
    std::cout << "  silence.wav / ramp.wav : " << monoBytes   << " bytes\n";
    std::cout << "  ramp_stereo.wav        : " << stereoBytes << " bytes\n";

    return 0;
}
