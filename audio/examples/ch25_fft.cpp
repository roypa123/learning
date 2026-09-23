// Example - ch25_fft.cpp
// Verifies the FFT four ways, shows spectral leakage, and reproduces
// Chapter 21's convolution reverb thousands of times faster. See Chapter 25.

#include <audio/audio.h>

#include <iostream>
#include <iomanip>
#include <vector>
#include <complex>
#include <chrono>
#include <string>
#include <algorithm>

using namespace audio;

// The synthetic room IR from Chapter 21.
std::vector<float> makeRoomIR(double rt60Seconds, double preDelayMs, double sampleRate,
                              double dampingCoeff = 0.35, uint32_t seed = 4242)
{
    const size_t length = static_cast<size_t>(rt60Seconds * 1.3 * sampleRate);
    std::vector<float> h(length, 0.0f);

    FastRandom rng(seed);
    const size_t preDelay = static_cast<size_t>(preDelayMs * 0.001 * sampleRate);
    h[0] = 1.0f;

    const double reflectionTimesMs[] = { 11, 17, 23, 29, 37, 41, 53, 61, 73, 89 };
    for (double ms : reflectionTimesMs)
    {
        const size_t pos = preDelay + static_cast<size_t>(ms * 0.001 * sampleRate);
        if (pos < length)
            h[pos] += static_cast<float>(0.7 * std::exp(-ms / 45.0)
                                         * (rng.nextFloat() > 0.0f ? 1.0 : -1.0));
    }

    const double decay = std::pow(10.0, -60.0 / (20.0 * rt60Seconds * sampleRate));
    double gain = 1.0, lpState = 0.0;
    const size_t lateStart = preDelay + static_cast<size_t>(0.02 * sampleRate);

    for (size_t n = lateStart; n < length; ++n)
    {
        lpState = lpState * dampingCoeff + rng.nextFloat() * (1.0 - dampingCoeff);
        h[n] += static_cast<float>(lpState * gain * 0.35);
        gain *= decay;
        if (gain < 1e-9) break;
    }

    const size_t fade = std::min(length / 10, static_cast<size_t>(0.05 * sampleRate));
    for (size_t i = 0; i < fade; ++i)
        h[length - 1 - i] *= static_cast<float>(i) / static_cast<float>(fade);

    return h;
}

int main()
{
    const double sr = 44100.0;

    // ================================================ verification
    std::cout << "--- verification ---\n";

    // 1. FFT vs naive DFT
    {
        FastRandom rng(5);
        std::vector<float> x(256);
        for (auto& s : x) s = rng.nextFloat();

        auto slow = dft(x);
        auto fast = fftReal(x, 256);

        double worst = 0.0;
        for (size_t k = 0; k < 256; ++k)
            worst = std::max(worst, std::abs(slow[k] - fast[k]));

        std::cout << "  1. FFT vs naive DFT (N=256)  max diff: "
                  << std::scientific << std::setprecision(3) << worst << "\n";
    }

    // 2. round trip
    {
        FastRandom rng(6);
        std::vector<float> x(1024);
        for (auto& s : x) s = rng.nextFloat();

        auto back = ifftReal(fftReal(x, 1024));
        std::cout << "  2. round trip ifft(fft(x))   max diff: "
                  << sig::maxDifference(x, back) << "\n";
    }

    // 3. a sine at exactly bin centre
    {
        const size_t N = 1024;
        const double f = 40.0 * sr / N;              // exactly bin 40
        auto x = sig::sine(N, f, sr, 0.5);
        auto X = fftReal(x, N);
        auto mag = magnitudeSpectrum(X);

        size_t peakBin = 0;
        for (size_t k = 1; k < mag.size(); ++k)
            if (mag[k] > mag[peakBin]) peakBin = k;

        double leaked = 0.0;
        for (size_t k = 0; k < mag.size(); ++k)
            if (k < peakBin - 1 || k > peakBin + 1) leaked += mag[k];

        std::cout << std::fixed << std::setprecision(4);
        std::cout << "  3. sine at bin centre (" << f << " Hz)\n";
        std::cout << "       peak bin " << peakBin
                  << "  amplitude " << mag[peakBin] << " (expected 0.5000)\n";
        std::cout << "       energy outside +/-1 bin: " << std::scientific << leaked << "\n";
    }

    // 4. Parseval
    {
        FastRandom rng(7);
        std::vector<float> x(1024);
        for (auto& s : x) s = rng.nextFloat();
        auto X = fftReal(x, 1024);
        std::cout << "  4. Parseval relative error:  " << parsevalError(x, X) << "\n";
    }

    // ================================================ leakage
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "\n--- spectral leakage (no window) ---\n";
    {
        const size_t N = 1024;

        for (double f : { 40.0 * sr / N, 40.5 * sr / N })
        {
            auto x = sig::sine(N, f, sr, 0.5);
            auto mag = magnitudeSpectrumDb(fftReal(x, N));

            int above60 = 0;
            for (double v : mag) if (v > -60.0) ++above60;

            std::cout << "  " << std::setw(9) << std::setprecision(2) << f << " Hz"
                      << "  (bin " << std::setprecision(1) << f * N / sr << ")"
                      << "   bins above -60 dB: " << above60 << "\n";
        }
        std::cout << "  A frequency between bins smears across the spectrum.\n"
                  << "  Chapter 26 fixes this with windowing.\n";
    }

    // ================================================ FFT sizes
    std::cout << "\n--- FFT size trade-off at 44.1 kHz ---\n";
    std::cout << std::setw(9) << std::left << "N" << std::right
              << std::setw(14) << "bin width"
              << std::setw(16) << "time window" << "\n";
    for (size_t N : { size_t{256}, size_t{1024}, size_t{4096}, size_t{16384} })
        std::cout << std::setw(9) << std::left << N << std::right
                  << std::setw(11) << std::setprecision(2) << binWidth(N, sr) << " Hz"
                  << std::setw(13) << std::setprecision(1) << N / sr * 1000.0 << " ms\n";

    // ================================================ speed
    std::cout << "\n--- DFT vs FFT ---\n";
    std::cout << std::setw(9) << std::left << "N" << std::right
              << std::setw(14) << "DFT (ms)" << std::setw(14) << "FFT (ms)"
              << std::setw(12) << "speedup" << "\n";

    for (size_t N : { size_t{256}, size_t{512}, size_t{1024}, size_t{2048} })
    {
        FastRandom rng(8);
        std::vector<float> x(N);
        for (auto& s : x) s = rng.nextFloat();

        auto t0 = std::chrono::steady_clock::now();
        auto slow = dft(x);
        auto t1 = std::chrono::steady_clock::now();
        auto fast = fftReal(x, N);
        auto t2 = std::chrono::steady_clock::now();

        const double dms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        const double fms = std::chrono::duration<double, std::milli>(t2 - t1).count();

        std::cout << std::setw(9) << std::left << N << std::right
                  << std::setw(14) << std::setprecision(3) << dms
                  << std::setw(14) << fms
                  << std::setw(11) << std::setprecision(1)
                  << (fms > 0.0 ? dms / fms : 0.0) << "x\n";
    }

    // ================================================ fast convolution
    std::cout << "\n--- fast convolution vs direct ---\n";
    std::cout << std::setw(13) << std::left << "space" << std::right
              << std::setw(10) << "taps"
              << std::setw(13) << "direct (s)"
              << std::setw(12) << "FFT (s)"
              << std::setw(11) << "speedup"
              << std::setw(14) << "max diff" << "\n";

    Oscillator osc;
    osc.setSampleRate(sr);
    osc.setFrequency(midiToFrequency(50));
    osc.setWaveform(Waveform::Saw);

    ADSR env;
    env.setSampleRate(sr);
    env.setParameters(0.002, 0.15, 0.0, 0.02);
    env.noteOn();

    std::vector<float> dry(static_cast<size_t>(0.4 * sr));
    for (auto& s : dry) s = osc.nextSample() * env.nextSample() * 0.6f;

    struct Space { const char* name; double rt60; double preDelay; double damping; };
    const Space spaces[] = {
        { "small_room", 0.4,  8.0, 0.55 },
        { "hall",       2.2, 25.0, 0.40 },
        { "cathedral",  6.5, 45.0, 0.30 },
    };

    for (const auto& s : spaces)
    {
        auto h = makeRoomIR(s.rt60, s.preDelay, sr, s.damping);

        auto t0 = std::chrono::steady_clock::now();
        auto slow = convolve(dry, h);
        auto t1 = std::chrono::steady_clock::now();
        auto fast = fastConvolve(dry, h);
        auto t2 = std::chrono::steady_clock::now();

        const double ds = std::chrono::duration<double>(t1 - t0).count();
        const double fs2 = std::chrono::duration<double>(t2 - t1).count();

        std::cout << std::setw(13) << std::left << s.name << std::right
                  << std::setw(10) << h.size()
                  << std::setw(13) << std::fixed << std::setprecision(3) << ds
                  << std::setw(12) << fs2
                  << std::setw(10) << std::setprecision(0)
                  << (fs2 > 0.0 ? ds / fs2 : 0.0) << "x"
                  << std::setw(14) << std::scientific << std::setprecision(2)
                  << sig::maxDifference(slow, fast) << std::fixed << "\n";

        std::vector<float> out = fast;
        for (auto& v : out) v *= 0.35f;
        for (size_t i = 0; i < dry.size(); ++i) out[i] += dry[i] * 0.8f;

        const double p = peak(out);
        if (p > 0.0) for (auto& v : out) v = static_cast<float>(v * 0.9 / p);

        writeWav(std::string("fft_reverb_") + s.name + ".wav", out,
                 static_cast<int>(sr), 1, true);
    }

    std::cout << "\nThe cathedral that took seconds in Chapter 21 now takes milliseconds.\n";
    return 0;
}
