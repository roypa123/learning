// Chapter 16 - wavreader.h
// A robust WAV reader: walks the chunk list, handles 8/16/24/32-bit PCM and
// 32/64-bit float, WAVE_FORMAT_EXTENSIBLE, odd-sized chunks and truncated files.

#pragma once

#include <string>
#include <vector>
#include <cstdint>

struct WavFile
{
    std::vector<std::vector<float>> channels;   // planar: channels[c][frame]
    int    sampleRate    = 44100;
    int    numChannels   = 0;
    int    bitsPerSample = 0;
    int    audioFormat   = 0;                   // 1 = PCM, 3 = float, 0xFFFE = extensible
    size_t numFrames     = 0;

    double durationSeconds() const
    {
        return sampleRate > 0
             ? static_cast<double>(numFrames) / static_cast<double>(sampleRate)
             : 0.0;
    }

    bool isValid() const { return numChannels > 0 && numFrames > 0; }
};

class WavReader
{
public:
    // Reads a WAV file into planar float channels.
    // On failure, returns a WavFile with isValid() == false and sets `error`.
    static WavFile read(const std::string& path, std::string& error);

    static WavFile read(const std::string& path)
    {
        std::string ignored;
        return read(path, ignored);
    }

    // Walks the chunk list and prints every field with its offset.
    // Loads no audio. Use this when a file will not open.
    static bool printInfo(const std::string& path);
};
