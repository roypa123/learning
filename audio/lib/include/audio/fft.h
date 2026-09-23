// libaudio - fft.h
// Radix-2 iterative FFT, the naive DFT (as a reference), and fast
// convolution. See Chapter 25.

#pragma once

#include <audio/types.h>
#include <audio/db.h>

#include <vector>
#include <complex>
#include <cmath>
#include <algorithm>

namespace audio {

using Complex = std::complex<double>;

inline bool isPowerOfTwo(size_t n) { return n != 0 && (n & (n - 1)) == 0; }

inline size_t nextPowerOfTwo(size_t n)
{
    size_t p = 1;
    while (p < n) p <<= 1;
    return p;
}

// ---------------------------------------------------------------- FFT

// In-place radix-2 FFT. data.size() MUST be a power of two.
// inverse == true computes the inverse transform, including the 1/N scaling.
inline void fft(std::vector<Complex>& data, bool inverse = false)
{
    const size_t N = data.size();
    if (N <= 1) return;

    // ---- bit-reversal permutation -----------------------------------
    for (size_t i = 1, j = 0; i < N; ++i)
    {
        size_t bit = N >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;

        if (i < j)
            std::swap(data[i], data[j]);
    }

    // ---- butterflies: log2(N) levels --------------------------------
    for (size_t len = 2; len <= N; len <<= 1)
    {
        const double angle = (inverse ? kTwoPi : -kTwoPi) / static_cast<double>(len);
        const Complex wlen(std::cos(angle), std::sin(angle));

        for (size_t i = 0; i < N; i += len)
        {
            Complex w(1.0, 0.0);

            for (size_t j = 0; j < len / 2; ++j)
            {
                const Complex u = data[i + j];
                const Complex v = data[i + j + len / 2] * w;

                data[i + j]           = u + v;
                data[i + j + len / 2] = u - v;

                w *= wlen;          // phasor rotation (Chapter 19)
            }
        }
    }

    if (inverse)
        for (auto& c : data)
            c /= static_cast<double>(N);
}

// ---------------------------------------------------------------- wrappers

// Real input -> complex spectrum, zero-padded to a power of two.
inline std::vector<Complex> fftReal(const std::vector<float>& x, size_t fftSize = 0)
{
    const size_t N = (fftSize > 0) ? nextPowerOfTwo(fftSize) : nextPowerOfTwo(x.size());

    std::vector<Complex> data(N, Complex(0.0, 0.0));
    for (size_t i = 0; i < std::min(N, x.size()); ++i)
        data[i] = Complex(static_cast<double>(x[i]), 0.0);

    fft(data, false);
    return data;
}

inline std::vector<float> ifftReal(std::vector<Complex> X)
{
    fft(X, true);

    std::vector<float> x(X.size());
    for (size_t i = 0; i < X.size(); ++i)
        x[i] = static_cast<float>(X[i].real());
    return x;
}

// Magnitude in dB for bins 0 .. N/2, correctly scaled.
// NOTE: bins 0 (DC) and N/2 (Nyquist) have no mirror partner, so they do
// not get the factor of 2. Getting this wrong makes DC read 6 dB low.
inline std::vector<double> magnitudeSpectrumDb(const std::vector<Complex>& X)
{
    const size_t N    = X.size();
    const size_t bins = N / 2 + 1;

    std::vector<double> out(bins);
    for (size_t k = 0; k < bins; ++k)
    {
        const double scale = (k == 0 || k == N / 2) ? 1.0 : 2.0;
        out[k] = gainToDb(scale * std::abs(X[k]) / static_cast<double>(N));
    }
    return out;
}

inline std::vector<double> magnitudeSpectrum(const std::vector<Complex>& X)
{
    const size_t N    = X.size();
    const size_t bins = N / 2 + 1;

    std::vector<double> out(bins);
    for (size_t k = 0; k < bins; ++k)
    {
        const double scale = (k == 0 || k == N / 2) ? 1.0 : 2.0;
        out[k] = scale * std::abs(X[k]) / static_cast<double>(N);
    }
    return out;
}

inline double binFrequency(size_t k, size_t fftSize, double sampleRate)
{
    return static_cast<double>(k) * sampleRate / static_cast<double>(fftSize);
}

inline double binWidth(size_t fftSize, double sampleRate)
{
    return sampleRate / static_cast<double>(fftSize);
}

// ---------------------------------------------------------------- naive DFT
// O(N^2). Kept as a reference to verify the FFT against.

inline std::vector<Complex> dft(const std::vector<float>& x)
{
    const size_t N = x.size();
    std::vector<Complex> X(N);

    for (size_t k = 0; k < N; ++k)
    {
        Complex sum(0.0, 0.0);
        for (size_t n = 0; n < N; ++n)
        {
            const double angle = -kTwoPi * static_cast<double>(k)
                                         * static_cast<double>(n)
                                         / static_cast<double>(N);
            sum += static_cast<double>(x[n]) * Complex(std::cos(angle), std::sin(angle));
        }
        X[k] = sum;
    }

    return X;
}

// ---------------------------------------------------------------- fast convolution

// x * h via the convolution theorem. Thousands of times faster than the
// direct method for long impulse responses. See Chapter 25 section 25.7.
inline std::vector<float> fastConvolve(const std::vector<float>& x,
                                       const std::vector<float>& h)
{
    if (x.empty() || h.empty()) return {};

    const size_t resultLength = x.size() + h.size() - 1;

    // ZERO-PAD to at least N+M-1, or the tail wraps round to the beginning.
    const size_t N = nextPowerOfTwo(resultLength);

    auto X = fftReal(x, N);
    auto H = fftReal(h, N);

    for (size_t k = 0; k < N; ++k)
        X[k] *= H[k];

    auto y = ifftReal(X);
    y.resize(resultLength);
    return y;
}

// ---------------------------------------------------------------- checks

// Parseval: sum|x[n]|^2 == (1/N) sum|X[k]|^2. The strongest single test
// of an FFT implementation, because it catches scaling errors.
inline double parsevalError(const std::vector<float>& x, const std::vector<Complex>& X)
{
    double timeEnergy = 0.0;
    for (float s : x) timeEnergy += static_cast<double>(s) * static_cast<double>(s);

    double freqEnergy = 0.0;
    for (const auto& c : X) freqEnergy += std::norm(c);
    freqEnergy /= static_cast<double>(X.size());

    const double scale = std::max(timeEnergy, 1e-30);
    return std::fabs(timeEnergy - freqEnergy) / scale;
}

}   // namespace audio
