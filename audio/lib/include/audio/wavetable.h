// libaudio - wavetable.h
// Mipmapped, morphing wavetable oscillator. See Chapter 32.

#pragma once

#include <audio/types.h>
#include <audio/osc.h>

#include <vector>
#include <cmath>
#include <algorithm>

namespace audio {

// One band-limited table per octave. Each is generated additively with only
// the harmonics that fit below Nyquist at the TOP of its range, so playing
// anywhere in the range is alias-free.
class WavetableMipmap
{
public:
    void build(Waveform shape, double sampleRate,
               size_t tableSize = 2048, int numTables = 11,
               double lowestFreq = 20.0)
    {
        tableSize_ = tableSize;
        lowest_    = lowestFreq;
        tables_.clear();

        const double nyquist = sampleRate * 0.5;
        double topFreq = lowestFreq;

        for (int t = 0; t < numTables; ++t)
        {
            topFreq *= 2.0;

            int maxHarmonic = static_cast<int>(nyquist / topFreq);
            maxHarmonic = std::max(1, maxHarmonic);

            std::vector<float> table(tableSize, 0.0f);

            for (int h = 1; h <= maxHarmonic; ++h)
            {
                double amp = 0.0;

                switch (shape)
                {
                    case Waveform::Saw:
                        amp = (2.0 / kPi) * ((h % 2) ? 1.0 : -1.0) / h;
                        break;

                    case Waveform::Square:
                    case Waveform::Pulse:
                        if (h % 2 == 0) continue;
                        amp = (4.0 / kPi) / h;
                        break;

                    case Waveform::Triangle:
                        if (h % 2 == 0) continue;
                        amp = (8.0 / (kPi * kPi))
                            * ((((h - 1) / 2) % 2) ? -1.0 : 1.0) / (h * h);
                        break;

                    case Waveform::Sine:
                    default:
                        amp = (h == 1) ? 1.0 : 0.0;
                        break;
                }

                if (amp == 0.0) continue;

                for (size_t i = 0; i < tableSize; ++i)
                    table[i] += static_cast<float>(
                        amp * std::sin(kTwoPi * h * static_cast<double>(i)
                                       / static_cast<double>(tableSize)));
            }

            tables_.push_back(std::move(table));
        }
    }

    size_t numTables() const { return tables_.size(); }
    size_t tableSize() const { return tableSize_; }

    // Which mip level covers this frequency, as a fractional value so the
    // caller can crossfade between adjacent tables (see s32.2).
    double levelFor(double frequency) const
    {
        if (tables_.empty() || frequency <= lowest_) return 0.0;
        const double octaves = std::log2(frequency / lowest_);
        return std::clamp(octaves, 0.0, static_cast<double>(tables_.size() - 1));
    }

    float readLinear(size_t level, double phase) const
    {
        if (tables_.empty()) return 0.0f;
        level = std::min(level, tables_.size() - 1);

        const auto& t = tables_[level];
        const double idx = phase * static_cast<double>(tableSize_);
        const size_t i0  = static_cast<size_t>(idx) % tableSize_;
        const size_t i1  = (i0 + 1) % tableSize_;
        const double frac = idx - std::floor(idx);

        return static_cast<float>(t[i0] * (1.0 - frac) + t[i1] * frac);
    }

    // Crossfades between adjacent mip levels, so pitch glides do not click.
    float read(double frequency, double phase) const
    {
        if (tables_.empty()) return 0.0f;

        const double lv = levelFor(frequency);
        const size_t l0 = static_cast<size_t>(lv);
        const size_t l1 = std::min(l0 + 1, tables_.size() - 1);
        const double blend = lv - static_cast<double>(l0);

        const float a = readLinear(l0, phase);
        const float b = readLinear(l1, phase);

        return static_cast<float>(a * (1.0 - blend) + b * blend);
    }

private:
    std::vector<std::vector<float>> tables_;
    size_t tableSize_ = 2048;
    double lowest_    = 20.0;
};

// ------------------------------------------------- morphing oscillator

// A series of mipmapped frames with a continuously variable position.
// This is the basis of PPG / Microwave / Massive / Serum style synthesis.
class WavetableOsc
{
public:
    void setSampleRate(double sr)
    {
        sampleRate_    = sr;
        invSampleRate_ = 1.0 / sr;
        updateIncrement();
    }

    // Build a default morph: sine -> triangle -> square -> saw.
    void buildDefaultFrames(double sampleRate, size_t tableSize = 2048)
    {
        frames_.clear();
        for (Waveform w : { Waveform::Sine, Waveform::Triangle,
                            Waveform::Square, Waveform::Saw })
        {
            WavetableMipmap m;
            m.build(w, sampleRate, tableSize);
            frames_.push_back(std::move(m));
        }
    }

    void addFrame(WavetableMipmap m) { frames_.push_back(std::move(m)); }

    void   setFrequency(double hz) { frequency_ = hz; updateIncrement(); }
    void   setPosition(double p)   { position_ = std::clamp(p, 0.0, 1.0); }
    double position() const        { return position_; }
    size_t numFrames() const       { return frames_.size(); }

    void setPhase(double p) { phase_ = p - std::floor(p); }
    void reset()            { phase_ = 0.0; }

    float nextSample(double pmAmount = 0.0)
    {
        if (frames_.empty()) return 0.0f;

        const double readPhase = [&] {
            double p = phase_ + pmAmount;
            return p - std::floor(p);
        }();

        const double scaled = position_ * static_cast<double>(frames_.size() - 1);
        const size_t fa = static_cast<size_t>(scaled);
        const size_t fb = std::min(fa + 1, frames_.size() - 1);
        const double blend = scaled - static_cast<double>(fa);

        const float a = frames_[fa].read(frequency_, readPhase);
        const float b = frames_[fb].read(frequency_, readPhase);

        phase_ += increment_;
        while (phase_ >= 1.0) phase_ -= 1.0;
        while (phase_ <  0.0) phase_ += 1.0;

        return static_cast<float>(a * (1.0 - blend) + b * blend);
    }

private:
    void updateIncrement() { increment_ = frequency_ * invSampleRate_; }

    std::vector<WavetableMipmap> frames_;
    double sampleRate_    = kDefaultRate;
    double invSampleRate_ = 1.0 / kDefaultRate;
    double frequency_     = 440.0;
    double increment_     = 440.0 / kDefaultRate;
    double phase_         = 0.0;
    double position_      = 0.0;
};

}   // namespace audio
