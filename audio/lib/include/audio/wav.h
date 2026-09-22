// libaudio - wav.h
// WAV file reading and writing. See Chapters 9 and 16.

#pragma once

#include <audio/types.h>

#include <string>
#include <vector>
#include <cstdint>

namespace audio {

struct WavFile
{
    std::vector<std::vector<float>> channels;   // planar: channels[c][frame]
    int    sampleRate    = 44100;
    int    numChannels   = 0;
    int    bitsPerSample = 0;
    int    audioFormat   = 0;                   // 1 = PCM, 3 = float
    size_t numFrames     = 0;

    double durationSeconds() const
    {
        return sampleRate > 0
             ? static_cast<double>(numFrames) / static_cast<double>(sampleRate)
             : 0.0;
    }

    bool isValid() const { return numChannels > 0 && numFrames > 0; }

    AudioBuffer toBuffer() const;
};

// --- writing -------------------------------------------------------------
// Samples are float in -1.0 .. +1.0; anything outside is clamped.
// `dither` adds TPDF noise before quantizing -- use it for anything you
// intend to listen to seriously. See Chapter 9.

bool writeWav(const std::string& path,
              const std::vector<float>& interleavedSamples,
              int sampleRate  = 44100,
              int numChannels = 1,
              bool dither     = false);

bool writeWav(const std::string& path,
              const std::vector<std::vector<float>>& planarChannels,
              int sampleRate = 44100,
              bool dither    = false);

bool writeWav(const std::string& path,
              const AudioBuffer& buffer,
              bool dither = false);

// --- reading -------------------------------------------------------------
// Walks the chunk list, so it survives files with bext / iXML / LIST chunks
// before the audio. Handles 8/16/24/32-bit PCM and 32/64-bit float.

WavFile readWav(const std::string& path, std::string& error);

inline WavFile readWav(const std::string& path)
{
    std::string ignored;
    return readWav(path, ignored);
}

// Prints the chunk layout and format without loading audio.
bool printWavInfo(const std::string& path);

}   // namespace audio
