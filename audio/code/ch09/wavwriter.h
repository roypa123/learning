// Chapter 9 - wavwriter.h
// Minimal 16-bit PCM WAV writer, written from the RIFF specification.

#pragma once

#include <string>
#include <vector>
#include <cstdint>

// Writes uncompressed 16-bit PCM WAV files.
// Samples are float in the range -1.0 .. +1.0; anything outside is clamped.
class WavWriter
{
public:
    // Write interleaved float samples.
    //   samples    : interleaved, length = numFrames * numChannels
    //   sampleRate : e.g. 44100
    //   numChannels: 1 = mono, 2 = stereo
    //   dither     : add TPDF dither before quantizing (recommended for real audio)
    // Returns false if the file could not be written.
    static bool write(const std::string& path,
                      const std::vector<float>& samples,
                      int sampleRate  = 44100,
                      int numChannels = 1,
                      bool dither     = false);

    // Convenience: write planar channels (one vector per channel),
    // interleaving them on the way out.
    static bool writePlanar(const std::string& path,
                            const std::vector<std::vector<float>>& channels,
                            int sampleRate = 44100,
                            bool dither    = false);
};
