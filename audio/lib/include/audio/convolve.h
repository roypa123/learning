// libaudio - convolve.h
// Direct time-domain convolution and correlation. See Chapter 21.
//
// Cost is O(N*M). Fine for impulse responses up to ~1000 taps or for offline
// rendering. The FFT-based version needed for real reverbs is Chapter 48.

#pragma once

#include <vector>
#include <algorithm>
#include <cstddef>

namespace audio {

// Full convolution. Output length is x.size() + h.size() - 1.
inline std::vector<float> convolve(const std::vector<float>& x,
                                   const std::vector<float>& h)
{
    if (x.empty() || h.empty())
        return {};

    const size_t N = x.size();
    const size_t M = h.size();
    std::vector<float> y(N + M - 1, 0.0f);

    // Scatter form: for each input sample, add its scaled copy of the whole
    // impulse response into the output starting at n.
    for (size_t n = 0; n < N; ++n)
    {
        const double xn = static_cast<double>(x[n]);
        if (xn == 0.0)                      // exact test, and often a big win
            continue;

        for (size_t m = 0; m < M; ++m)
            y[n + m] += static_cast<float>(xn * static_cast<double>(h[m]));
    }

    return y;
}

// Gather form: computes one output at a time. Slower here, but this is the
// shape a streaming/real-time implementation takes.
inline std::vector<float> convolveGather(const std::vector<float>& x,
                                         const std::vector<float>& h)
{
    if (x.empty() || h.empty())
        return {};

    const size_t N = x.size();
    const size_t M = h.size();
    std::vector<float> y(N + M - 1, 0.0f);

    for (size_t n = 0; n < y.size(); ++n)
    {
        double acc = 0.0;
        for (size_t m = 0; m < M; ++m)
        {
            const long k = static_cast<long>(n) - static_cast<long>(m);
            if (k >= 0 && k < static_cast<long>(N))
                acc += static_cast<double>(x[static_cast<size_t>(k)])
                     * static_cast<double>(h[m]);
        }
        y[n] = static_cast<float>(acc);
    }

    return y;
}

// Only the centre N samples, same length as the input.
inline std::vector<float> convolveSame(const std::vector<float>& x,
                                       const std::vector<float>& h)
{
    const auto full = convolve(x, h);
    if (full.empty()) return {};

    const size_t offset = (h.size() - 1) / 2;
    std::vector<float> y(x.size(), 0.0f);
    for (size_t i = 0; i < x.size() && i + offset < full.size(); ++i)
        y[i] = full[i + offset];
    return y;
}

// Cross-correlation: convolution with the second signal reversed.
// Answers "how similar are these two, when one is shifted by n?"
inline std::vector<float> correlate(const std::vector<float>& x,
                                    const std::vector<float>& h)
{
    std::vector<float> flipped(h.rbegin(), h.rend());
    return convolve(x, flipped);
}

// Autocorrelation: a signal against itself. The first strong peak after
// zero lag gives the period -- the basis of pitch detection (Chapter 54).
inline std::vector<float> autocorrelate(const std::vector<float>& x,
                                        size_t maxLag)
{
    std::vector<float> r(maxLag, 0.0f);

    for (size_t lag = 0; lag < maxLag; ++lag)
    {
        double acc = 0.0;
        for (size_t n = 0; n + lag < x.size(); ++n)
            acc += static_cast<double>(x[n]) * static_cast<double>(x[n + lag]);
        r[lag] = static_cast<float>(acc);
    }

    // Normalise so r[0] == 1
    if (r[0] != 0.0f)
    {
        const float inv = 1.0f / r[0];
        for (auto& v : r) v *= inv;
    }

    return r;
}

// ---------------------------------------------------------------- streaming
// Overlap-add. Convolve each block with the full IR, output the head plus the
// previous block's tail, save the new tail. See Chapter 21 section 21.8.
//
// NOTE: process() allocates, so this is NOT real-time safe. Exercise 21.9
// asks you to fix that; Chapter 48 replaces the inner convolution with an FFT.
class OverlapAddConvolver
{
public:
    void prepare(const std::vector<float>& impulseResponse)
    {
        h_ = impulseResponse;
        tail_.assign(h_.empty() ? 0 : h_.size() - 1, 0.0f);
    }

    void process(const float* in, float* out, size_t numSamples)
    {
        if (h_.empty()) { std::copy(in, in + numSamples, out); return; }

        const auto full = convolve(std::vector<float>(in, in + numSamples), h_);

        for (size_t i = 0; i < numSamples; ++i)
            out[i] = full[i] + (i < tail_.size() ? tail_[i] : 0.0f);

        std::vector<float> newTail(h_.size() - 1, 0.0f);
        for (size_t i = 0; i + numSamples < tail_.size(); ++i)
            newTail[i] = tail_[i + numSamples];
        for (size_t i = numSamples; i < full.size(); ++i)
            newTail[i - numSamples] += full[i];

        tail_ = std::move(newTail);
    }

    void reset() { std::fill(tail_.begin(), tail_.end(), 0.0f); }

private:
    std::vector<float> h_, tail_;
};

}   // namespace audio
