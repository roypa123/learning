// Chapter 15 - noisegen.h
// White, pink (two algorithms), brown and blue noise generators.

#pragma once

#include <cstdint>
#include <array>
#include <algorithm>

// ------------------------------------------------- fast PRNG (xorshift32)
// Six integer ops per sample, no allocation, real-time safe.
class FastRandom
{
public:
    explicit FastRandom(uint32_t seed = 22222) : state(seed ? seed : 1u) {}

    uint32_t nextUInt()
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }

    // Uniform in [-1, 1)
    float nextFloat()
    {
        return static_cast<float>(nextUInt() >> 8) * (2.0f / 16777216.0f) - 1.0f;
    }

private:
    uint32_t state;
};

// ------------------------------------------------- pink: Voss-McCartney
class PinkVoss
{
public:
    explicit PinkVoss(uint32_t seed = 1) : rng(seed)
    {
        for (auto& v : values) v = rng.nextFloat();
        for (float v : values) runningSum += v;
    }

    float next()
    {
        ++counter;

        // counter ^ (counter-1) marks every bit that changed on this
        // increment, so generator k updates every 2^k samples.
        const uint32_t diff = counter ^ (counter - 1u);

        for (int k = 0; k < kNumGenerators; ++k)
        {
            if (diff & (1u << k))
            {
                runningSum -= values[static_cast<size_t>(k)];
                values[static_cast<size_t>(k)] = rng.nextFloat();
                runningSum += values[static_cast<size_t>(k)];
            }
        }

        return runningSum / static_cast<float>(kNumGenerators);
    }

private:
    static constexpr int kNumGenerators = 16;
    FastRandom rng;
    std::array<float, kNumGenerators> values{};
    float    runningSum = 0.0f;
    uint32_t counter    = 0;
};

// ------------------------------------------------- pink: Paul Kellett filter
// Accurate to about +/-0.05 dB from 10 Hz to 20 kHz. Seven MACs per sample.
class PinkKellett
{
public:
    float process(float white)
    {
        b0 = 0.99886f * b0 + white * 0.0555179f;
        b1 = 0.99332f * b1 + white * 0.0750759f;
        b2 = 0.96900f * b2 + white * 0.1538520f;
        b3 = 0.86650f * b3 + white * 0.3104856f;
        b4 = 0.55000f * b4 + white * 0.5329522f;
        b5 = -0.7616f * b5 - white * 0.0168980f;

        const float out = b0 + b1 + b2 + b3 + b4 + b5 + b6 + white * 0.5362f;

        b6 = white * 0.115926f;

        return out * 0.11f;
    }

    void reset() { b0 = b1 = b2 = b3 = b4 = b5 = b6 = 0.0f; }

private:
    float b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
};

// ------------------------------------------------- brown (leaky integrator)
class Brown
{
public:
    float process(float white)
    {
        // Leaky: the 0.9995 factor pulls the walk back toward zero so it
        // cannot drift into unbounded DC.
        state = 0.9995f * state + white * 0.02f;
        return std::clamp(state * 3.5f, -1.0f, 1.0f);
    }

    void reset() { state = 0.0f; }

private:
    float state = 0.0f;
};

// ------------------------------------------------- blue (differentiated white)
class Blue
{
public:
    float process(float white)
    {
        const float out = white - prev;     // difference = +6 dB/octave
        prev = white;
        return out * 0.5f;
    }

    void reset() { prev = 0.0f; }

private:
    float prev = 0.0f;
};
