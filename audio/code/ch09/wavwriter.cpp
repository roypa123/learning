// Chapter 9 - wavwriter.cpp

#include "wavwriter.h"

#include <fstream>
#include <cmath>
#include <random>
#include <algorithm>

namespace
{
    // --- little-endian primitives ------------------------------------

    void writeU16LE(std::ostream& out, uint16_t v)
    {
        out.put(static_cast<char>( v       & 0xFF));
        out.put(static_cast<char>((v >> 8) & 0xFF));
    }

    void writeU32LE(std::ostream& out, uint32_t v)
    {
        out.put(static_cast<char>( v        & 0xFF));
        out.put(static_cast<char>((v >>  8) & 0xFF));
        out.put(static_cast<char>((v >> 16) & 0xFF));
        out.put(static_cast<char>((v >> 24) & 0xFF));
    }

    void writeTag(std::ostream& out, const char* tag)
    {
        out.write(tag, 4);
    }

    // --- sample conversion -------------------------------------------
    // Clamp, then scale by 32767 (not 32768), then round to nearest.
    // See Chapter 9 section 9.4 for why each step matters.
    int16_t floatToInt16(float sample)
    {
        sample = std::clamp(sample, -1.0f, 1.0f);
        return static_cast<int16_t>(std::lround(sample * 32767.0f));
    }
}

bool WavWriter::write(const std::string& path,
                      const std::vector<float>& samples,
                      int sampleRate,
                      int numChannels,
                      bool dither)
{
    if (numChannels < 1 || sampleRate < 1)
        return false;

    std::ofstream out(path, std::ios::binary);
    if (!out)
        return false;

    const uint16_t bitsPerSample = 16;
    const uint16_t blockAlign    = static_cast<uint16_t>(numChannels * bitsPerSample / 8);
    const uint32_t byteRate      = static_cast<uint32_t>(sampleRate) * blockAlign;
    const uint32_t dataSize      = static_cast<uint32_t>(samples.size() * sizeof(int16_t));

    // ---- RIFF header ----
    writeTag  (out, "RIFF");
    writeU32LE(out, 36 + dataSize);
    writeTag  (out, "WAVE");

    // ---- fmt chunk ----
    writeTag  (out, "fmt ");                                  // note the trailing space
    writeU32LE(out, 16);                                      // PCM fmt chunk size
    writeU16LE(out, 1);                                       // AudioFormat = PCM
    writeU16LE(out, static_cast<uint16_t>(numChannels));
    writeU32LE(out, static_cast<uint32_t>(sampleRate));
    writeU32LE(out, byteRate);
    writeU16LE(out, blockAlign);
    writeU16LE(out, bitsPerSample);

    // ---- data chunk ----
    writeTag  (out, "data");
    writeU32LE(out, dataSize);

    std::vector<int16_t> pcm(samples.size());

    if (dither)
    {
        std::mt19937 rng(12345);                              // fixed seed: reproducible
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);

        for (size_t i = 0; i < samples.size(); ++i)
        {
            const float noise = (dist(rng) - dist(rng)) / 32768.0f;   // TPDF, ~1 LSB
            pcm[i] = floatToInt16(samples[i] + noise);
        }
    }
    else
    {
        for (size_t i = 0; i < samples.size(); ++i)
            pcm[i] = floatToInt16(samples[i]);
    }

    out.write(reinterpret_cast<const char*>(pcm.data()),
              static_cast<std::streamsize>(pcm.size() * sizeof(int16_t)));

    return out.good();
}

bool WavWriter::writePlanar(const std::string& path,
                            const std::vector<std::vector<float>>& channels,
                            int sampleRate,
                            bool dither)
{
    if (channels.empty())
        return false;

    const size_t numChannels = channels.size();
    const size_t numFrames   = channels[0].size();

    for (const auto& ch : channels)
        if (ch.size() != numFrames)
            return false;                      // all channels must be the same length

    std::vector<float> interleaved(numFrames * numChannels);

    for (size_t frame = 0; frame < numFrames; ++frame)
        for (size_t c = 0; c < numChannels; ++c)
            interleaved[frame * numChannels + c] = channels[c][frame];

    return write(path, interleaved, sampleRate,
                 static_cast<int>(numChannels), dither);
}
