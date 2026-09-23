// libaudio - stft.h
// Short-Time Fourier Transform, spectrograms, and PGM image output.
// See Chapter 27.

#pragma once

#include <audio/types.h>
#include <audio/fft.h>
#include <audio/window.h>

#include <vector>
#include <string>
#include <fstream>
#include <cstdint>
#include <algorithm>

namespace audio {

struct STFTFrame
{
    std::vector<Complex> bins;          // fftSize bins (0..N/2 are unique)
    size_t startSample = 0;
};

class STFT
{
public:
    void prepare(size_t fftSize, size_t hopSize,
                 AnalysisWindow win = AnalysisWindow::Hann)
    {
        fftSize_ = nextPowerOfTwo(fftSize);
        hopSize_ = std::max<size_t>(hopSize, 1);
        window_  = makeWindow(win, fftSize_);
        gain_    = coherentGain(window_);
    }

    size_t fftSize() const { return fftSize_; }
    size_t hopSize() const { return hopSize_; }
    double windowCoherentGain() const { return gain_; }

    std::vector<STFTFrame> analyse(const std::vector<float>& x) const
    {
        std::vector<STFTFrame> frames;
        if (x.empty() || fftSize_ == 0 || x.size() < fftSize_) return frames;

        for (size_t start = 0; start + fftSize_ <= x.size(); start += hopSize_)
        {
            std::vector<Complex> buf(fftSize_);
            for (size_t n = 0; n < fftSize_; ++n)
                buf[n] = Complex(static_cast<double>(x[start + n]) * window_[n], 0.0);

            fft(buf, false);
            frames.push_back({ std::move(buf), start });
        }
        return frames;
    }

    // Weighted overlap-add. Dividing by the accumulated squared-window sum
    // is exact and handles the partial overlap at the edges correctly.
    std::vector<float> synthesise(const std::vector<STFTFrame>& frames,
                                  size_t outputLength) const
    {
        std::vector<float>  out(outputLength, 0.0f);
        std::vector<double> norm(outputLength, 0.0);

        for (const auto& f : frames)
        {
            auto buf = f.bins;
            fft(buf, true);

            for (size_t n = 0; n < fftSize_; ++n)
            {
                const size_t idx = f.startSample + n;
                if (idx >= outputLength) break;

                out[idx]  += static_cast<float>(buf[n].real() * window_[n]);
                norm[idx] += window_[n] * window_[n];
            }
        }

        for (size_t n = 0; n < outputLength; ++n)
            if (norm[n] > 1e-9)
                out[n] = static_cast<float>(static_cast<double>(out[n]) / norm[n]);

        return out;
    }

private:
    size_t              fftSize_ = 1024;
    size_t              hopSize_ = 256;
    std::vector<double> window_;
    double              gain_ = 1.0;
};

// ---------------------------------------------------------------- images

// PGM: the simplest image format there is. Every viewer opens it.
inline bool writePGM(const std::string& path, const std::vector<uint8_t>& pixels,
                     size_t width, size_t height)
{
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;

    f << "P5\n" << width << " " << height << "\n255\n";
    f.write(reinterpret_cast<const char*>(pixels.data()),
            static_cast<std::streamsize>(pixels.size()));
    return f.good();
}

// PPM: the colour equivalent. pixels is RGB triples.
inline bool writePPM(const std::string& path, const std::vector<uint8_t>& rgb,
                     size_t width, size_t height)
{
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;

    f << "P6\n" << width << " " << height << "\n255\n";
    f.write(reinterpret_cast<const char*>(rgb.data()),
            static_cast<std::streamsize>(rgb.size()));
    return f.good();
}

// A greyscale spectrogram with a LOGARITHMIC frequency axis (because hearing
// is logarithmic) and a dB magnitude scale with a floor.
inline bool writeSpectrogram(const std::string& path,
                             const std::vector<float>& signal,
                             double sampleRate,
                             size_t fftSize = 1024,
                             size_t hopSize = 256,
                             double floorDb = -90.0,
                             bool logFrequency = true,
                             size_t imageHeight = 512)
{
    STFT stft;
    stft.prepare(fftSize, hopSize);
    auto frames = stft.analyse(signal);
    if (frames.empty()) return false;

    const size_t width   = frames.size();
    const size_t height  = imageHeight;
    const size_t numBins = stft.fftSize() / 2 + 1;

    std::vector<uint8_t> pixels(width * height, 0);

    for (size_t x = 0; x < width; ++x)
    {
        const auto mag = magnitudeSpectrumDb(frames[x].bins);

        for (size_t y = 0; y < height; ++y)
        {
            // y = 0 is the TOP of the image, so invert: high frequency up.
            const double v = 1.0 - static_cast<double>(y)
                                 / static_cast<double>(height - 1);

            size_t bin;
            if (logFrequency)
            {
                const double fMin = 20.0;
                const double fMax = sampleRate / 2.0;
                const double f    = fMin * std::pow(fMax / fMin, v);
                bin = static_cast<size_t>(f * static_cast<double>(stft.fftSize())
                                            / sampleRate);
            }
            else
            {
                bin = static_cast<size_t>(v * static_cast<double>(numBins - 1));
            }

            if (bin >= numBins) bin = numBins - 1;

            const double db = std::max(mag[bin], floorDb);
            const double t  = (db - floorDb) / (0.0 - floorDb);
            pixels[y * width + x] =
                static_cast<uint8_t>(std::clamp(t, 0.0, 1.0) * 255.0);
        }
    }

    return writePGM(path, pixels, width, height);
}

}   // namespace audio
