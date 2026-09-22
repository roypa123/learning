// libaudio - processor.h
// The Processor interface: the abstraction every effect in Part IV implements.
// See Chapter 17, section 17.5.

#pragma once

#include <audio/types.h>
#include <audio/db.h>

#include <memory>
#include <vector>
#include <utility>

namespace audio {

class Processor
{
public:
    // MANDATORY: without a virtual destructor, deleting through a base
    // pointer never runs the derived destructor. See Chapter 17.
    virtual ~Processor() = default;

    // Called before processing starts. Allocate here, never in process().
    virtual void prepare(double sampleRate, int maxBlockSize) = 0;

    // Process a block in place.
    virtual void process(AudioBuffer& buffer) = 0;

    // Clear internal state: filter memory, delay lines, envelopes.
    virtual void reset() = 0;

    virtual const char* name() const = 0;
};

// ---------------------------------------------------------------- Gain

class Gain : public Processor
{
public:
    explicit Gain(float linearGain = 1.0f) : gain_(linearGain) {}

    void prepare(double, int) override {}

    void process(AudioBuffer& buffer) override
    {
        for (int c = 0; c < buffer.numChannels(); ++c)
            for (float& s : buffer.channel(c))
                s *= gain_;
    }

    void reset() override {}

    const char* name() const override { return "Gain"; }

    void  setGain(float g)      { gain_ = g; }
    void  setGainDb(double dB)  { gain_ = static_cast<float>(dbToGain(dB)); }
    float getGain() const       { return gain_; }

private:
    float gain_ = 1.0f;
};

// ---------------------------------------------------------------- Chain
// A Processor that runs other Processors in order. Because Chain is itself
// a Processor, chains nest.

class Chain : public Processor
{
public:
    void add(std::unique_ptr<Processor> p) { stages_.push_back(std::move(p)); }

    size_t size() const { return stages_.size(); }

    void prepare(double sampleRate, int maxBlockSize) override
    {
        for (auto& s : stages_)
            s->prepare(sampleRate, maxBlockSize);
    }

    void process(AudioBuffer& buffer) override
    {
        for (auto& s : stages_)
            s->process(buffer);
    }

    void reset() override
    {
        for (auto& s : stages_)
            s->reset();
    }

    const char* name() const override { return "Chain"; }

private:
    std::vector<std::unique_ptr<Processor>> stages_;
};

}   // namespace audio
