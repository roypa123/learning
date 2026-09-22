// libaudio - types.h
// Core constants and the AudioBuffer type.

#pragma once

#include <vector>
#include <cstddef>

namespace audio {

// ---------------------------------------------------------------- constants

constexpr double kPi          = 3.14159265358979323846;
constexpr double kTwoPi       = 2.0 * kPi;
constexpr double kHalfPi      = 0.5 * kPi;
constexpr double kDefaultRate = 44100.0;

// Speed of sound in air at 20 C, metres per second. Used from Chapter 77.
constexpr double kSpeedOfSound = 343.0;

// ---------------------------------------------------------------- utilities

inline double secondsToSamples(double seconds, double sampleRate)
{
    return seconds * sampleRate;
}

inline double samplesToSeconds(double samples, double sampleRate)
{
    return samples / sampleRate;
}

// MIDI note 69 = A4 = 440 Hz. Note the 12.0: integer division would
// snap every result to an octave. See Chapter 6, section 6.4.
double midiToFrequency(double noteNumber);
double frequencyToMidi(double hz);

// ---------------------------------------------------------------- AudioBuffer

// A block of audio: one or more channels stored planar (each channel is its
// own contiguous vector). Planar costs one pointer hop per channel and makes
// every DSP loop a simple contiguous pass. See Chapter 7.
class AudioBuffer
{
public:
    AudioBuffer() = default;
    AudioBuffer(int numChannels, int numFrames, double sampleRate = kDefaultRate);

    int    numChannels() const { return static_cast<int>(channels_.size()); }
    int    numFrames()   const;
    double sampleRate()  const { return sampleRate_; }
    double durationSeconds() const;

    void setSampleRate(double sr) { sampleRate_ = sr; }
    void resize(int numChannels, int numFrames);

    std::vector<float>&       channel(int c)       { return channels_[static_cast<size_t>(c)]; }
    const std::vector<float>& channel(int c) const { return channels_[static_cast<size_t>(c)]; }

    // Bounds-checked access, for use while developing new DSP.
    std::vector<float>&       channelAt(int c)       { return channels_.at(static_cast<size_t>(c)); }
    const std::vector<float>& channelAt(int c) const { return channels_.at(static_cast<size_t>(c)); }

    void clear();
    void applyGain(float gain);
    void applyGainDb(double dB);
    void normalise(float targetPeak = 0.99f);

    void fadeIn (int frames);
    void fadeOut(int frames);

    // Add src into this buffer, scaled. Channel counts must match; lengths
    // need not (the shorter one limits the copy).
    void mixFrom(const AudioBuffer& src, float gain = 1.0f);

    // Append src to the end of this buffer. Channel counts must match.
    void appendFrom(const AudioBuffer& src);

    // Append `frames` frames of silence.
    void appendSilence(int frames);

    float peak() const;
    float rms()  const;

    // Interleaved copy, for file writing and hardware APIs.
    std::vector<float> interleaved() const;

private:
    std::vector<std::vector<float>> channels_;
    double sampleRate_ = kDefaultRate;
};

}   // namespace audio
