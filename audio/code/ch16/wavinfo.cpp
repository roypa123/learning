// Chapter 16 - wavinfo.cpp
// Prints a WAV file's chunk layout and format, and runs round-trip tests.
//
// Build:  g++ -std=c++17 -Wall -Wextra -O2 wavinfo.cpp wavreader.cpp ../ch09/wavwriter.cpp -o wavinfo.exe
// Run:    ./wavinfo.exe somefile.wav
//         ./wavinfo.exe                 (no args: runs the round-trip tests)

#include "wavreader.h"
#include "../ch09/wavwriter.h"
#include "../ch11/decibels.h"

#include <iostream>
#include <iomanip>
#include <vector>
#include <cmath>
#include <string>
#include <algorithm>

constexpr double kPi = 3.14159265358979323846;

bool roundTripTest(double sampleRate, int channels, const char* label)
{
    const size_t frames = 10000;
    std::vector<std::vector<float>> original(
        static_cast<size_t>(channels), std::vector<float>(frames));

    for (int c = 0; c < channels; ++c)
        for (size_t i = 0; i < frames; ++i)
            original[static_cast<size_t>(c)][i] =
                static_cast<float>(0.8 * std::sin(2.0 * kPi * (220.0 * (c + 1))
                                   * static_cast<double>(i) / sampleRate));

    WavWriter::writePlanar("roundtrip.wav", original,
                           static_cast<int>(sampleRate), false);

    std::string err;
    WavFile loaded = WavReader::read("roundtrip.wav", err);

    std::cout << "  " << std::setw(30) << std::left << label << std::right;

    if (!loaded.isValid())      { std::cout << "FAIL: " << err << "\n";        return false; }
    if (loaded.numChannels != channels)
                                { std::cout << "FAIL: channel count\n";        return false; }
    if (loaded.sampleRate != static_cast<int>(sampleRate))
                                { std::cout << "FAIL: sample rate\n";          return false; }
    if (loaded.numFrames != frames)
                                { std::cout << "FAIL: frame count\n";          return false; }

    double maxError = 0.0;
    for (int c = 0; c < channels; ++c)
        for (size_t i = 0; i < frames; ++i)
            maxError = std::max(maxError,
                std::fabs(static_cast<double>(original[static_cast<size_t>(c)][i])
                        - static_cast<double>(loaded.channels[static_cast<size_t>(c)][i])));

    const bool pass = maxError < 1.0 / 32000.0;

    std::cout << "max error " << std::scientific << std::setprecision(4) << maxError
              << std::fixed << std::setprecision(2)
              << "  (" << db::gainToDb(maxError) << " dBFS)  "
              << (pass ? "PASS" : "FAIL") << "\n";

    return pass;
}

int main(int argc, char** argv)
{
    if (argc > 1)
    {
        if (!WavReader::printInfo(argv[1]))
            return 1;

        std::string err;
        WavFile f = WavReader::read(argv[1], err);
        if (!f.isValid())
        {
            std::cerr << "Could not load audio: " << err << "\n";
            return 1;
        }

        std::cout << "\n  Measurements (channel 0):\n" << std::fixed << std::setprecision(2);
        std::cout << "    peak  " << db::peakDb(f.channels[0])  << " dBFS\n";
        std::cout << "    RMS   " << db::rmsDb(f.channels[0])   << " dBFS\n";
        std::cout << "    crest " << db::crestFactorDb(f.channels[0]) << " dB\n";
        std::cout << "    DC    " << std::setprecision(6)
                  << db::dcOffset(f.channels[0]) << "\n";
        return 0;
    }

    std::cout << "Round-trip tests (write then read, compare):\n";
    bool all = true;
    all &= roundTripTest(44100.0, 1, "mono   44100 Hz");
    all &= roundTripTest(44100.0, 2, "stereo 44100 Hz");
    all &= roundTripTest(48000.0, 2, "stereo 48000 Hz");
    all &= roundTripTest(44100.0, 6, "5.1    44100 Hz");

    std::cout << "\n" << (all ? "All tests passed." : "SOME TESTS FAILED.") << "\n";
    std::cout << "\nExpected max error is 1/32768 = 3.0518e-05, one 16-bit "
                 "quantization step.\n";

    return all ? 0 : 1;
}
