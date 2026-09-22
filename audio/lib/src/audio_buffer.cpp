// libaudio - audio_buffer.cpp

#include <audio/types.h>
#include <audio/db.h>

#include <algorithm>
#include <cmath>

namespace audio {

double midiToFrequency(double noteNumber)
{
    return 440.0 * std::pow(2.0, (noteNumber - 69.0) / 12.0);
}

double frequencyToMidi(double hz)
{
    return 69.0 + 12.0 * std::log2(hz / 440.0);
}

AudioBuffer::AudioBuffer(int numChannels, int numFrames, double sampleRate)
    : channels_(static_cast<size_t>(std::max(numChannels, 0)),
                std::vector<float>(static_cast<size_t>(std::max(numFrames, 0)), 0.0f)),
      sampleRate_(sampleRate)
{
}

int AudioBuffer::numFrames() const
{
    return channels_.empty() ? 0 : static_cast<int>(channels_[0].size());
}

double AudioBuffer::durationSeconds() const
{
    return sampleRate_ > 0.0 ? numFrames() / sampleRate_ : 0.0;
}

void AudioBuffer::resize(int numChannels, int numFrames)
{
    channels_.assign(static_cast<size_t>(std::max(numChannels, 0)),
                     std::vector<float>(static_cast<size_t>(std::max(numFrames, 0)), 0.0f));
}

void AudioBuffer::clear()
{
    for (auto& ch : channels_)
        std::fill(ch.begin(), ch.end(), 0.0f);
}

void AudioBuffer::applyGain(float gain)
{
    for (auto& ch : channels_)
        for (float& s : ch)
            s *= gain;
}

void AudioBuffer::applyGainDb(double dB)
{
    applyGain(static_cast<float>(dbToGain(dB)));
}

void AudioBuffer::normalise(float targetPeak)
{
    const float p = peak();
    if (p > 0.0f)
        applyGain(targetPeak / p);
}

void AudioBuffer::fadeIn(int frames)
{
    const int n = std::min(frames, numFrames());
    if (n <= 1) return;

    for (auto& ch : channels_)
        for (int i = 0; i < n; ++i)
            ch[static_cast<size_t>(i)] *=
                static_cast<float>(i) / static_cast<float>(n - 1);
}

void AudioBuffer::fadeOut(int frames)
{
    const int total = numFrames();
    const int n = std::min(frames, total);
    if (n <= 1) return;

    for (auto& ch : channels_)
        for (int i = 0; i < n; ++i)
            ch[static_cast<size_t>(total - 1 - i)] *=
                static_cast<float>(i) / static_cast<float>(n - 1);
}

void AudioBuffer::mixFrom(const AudioBuffer& src, float gain)
{
    const int chans  = std::min(numChannels(), src.numChannels());
    const int frames = std::min(numFrames(),   src.numFrames());

    for (int c = 0; c < chans; ++c)
    {
        auto&       dst = channel(c);
        const auto& s   = src.channel(c);
        for (int i = 0; i < frames; ++i)
            dst[static_cast<size_t>(i)] += s[static_cast<size_t>(i)] * gain;
    }
}

void AudioBuffer::appendFrom(const AudioBuffer& src)
{
    if (channels_.empty())
    {
        channels_.assign(static_cast<size_t>(src.numChannels()), {});
        sampleRate_ = src.sampleRate();
    }

    const int chans = std::min(numChannels(), src.numChannels());
    for (int c = 0; c < chans; ++c)
    {
        auto&       dst = channel(c);
        const auto& s   = src.channel(c);
        dst.insert(dst.end(), s.begin(), s.end());
    }
}

void AudioBuffer::appendSilence(int frames)
{
    if (frames <= 0) return;
    for (auto& ch : channels_)
        ch.insert(ch.end(), static_cast<size_t>(frames), 0.0f);
}

float AudioBuffer::peak() const
{
    float p = 0.0f;
    for (const auto& ch : channels_)
        for (float s : ch)
            p = std::max(p, std::fabs(s));
    return p;
}

float AudioBuffer::rms() const
{
    double sumSquares = 0.0;         // accumulate in double
    size_t count = 0;

    for (const auto& ch : channels_)
        for (float s : ch)
        {
            sumSquares += static_cast<double>(s) * static_cast<double>(s);
            ++count;
        }

    return count == 0 ? 0.0f
                      : static_cast<float>(std::sqrt(sumSquares / static_cast<double>(count)));
}

std::vector<float> AudioBuffer::interleaved() const
{
    const size_t chans  = channels_.size();
    const size_t frames = static_cast<size_t>(numFrames());

    std::vector<float> out(frames * chans);

    for (size_t f = 0; f < frames; ++f)
        for (size_t c = 0; c < chans; ++c)
            out[f * chans + c] = channels_[c][f];

    return out;
}

}   // namespace audio
