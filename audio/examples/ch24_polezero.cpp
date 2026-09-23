// Example - ch24_polezero.cpp
// Pole/zero analysis, an ASCII z-plane plot, the stability triangle, and
// resonators from radius 0.9 to 1.0001. See Chapter 24.
//
// The unstable case is rendered at very low level and clamped. Volume down.

#include <audio/audio.h>

#include <iostream>
#include <iomanip>
#include <vector>
#include <complex>
#include <array>
#include <string>

using namespace audio;

// Roots of z^2 + a1*z + a2 = 0
std::pair<Complex, Complex> polesOf(const BiquadCoeffs& c)
{
    const double disc = c.a1 * c.a1 - 4.0 * c.a2;

    if (disc >= 0.0)
    {
        const double s = std::sqrt(disc);
        return { Complex((-c.a1 + s) * 0.5, 0.0),
                 Complex((-c.a1 - s) * 0.5, 0.0) };
    }

    const double re = -c.a1 * 0.5;
    const double im = std::sqrt(-disc) * 0.5;
    return { Complex(re, im), Complex(re, -im) };
}

std::pair<Complex, Complex> zerosOf(const BiquadCoeffs& c)
{
    if (std::fabs(c.b0) < 1e-12)
        return { Complex(0, 0), Complex(0, 0) };

    const double a = c.b0, b = c.b1, d = c.b2;
    const double disc = b * b - 4.0 * a * d;

    if (disc >= 0.0)
    {
        const double s = std::sqrt(disc);
        return { Complex((-b + s) / (2 * a), 0.0),
                 Complex((-b - s) / (2 * a), 0.0) };
    }

    const double re = -b / (2 * a);
    const double im = std::sqrt(-disc) / (2 * a);
    return { Complex(re, im), Complex(re, -im) };
}

void plotZPlane(const BiquadCoeffs& c, const std::string& label)
{
    const int W = 45, H = 21;
    std::vector<std::string> grid(H, std::string(W, ' '));

    auto plot = [&](double re, double im, char ch) {
        const int x = static_cast<int>((re + 1.4) / 2.8 * (W - 1) + 0.5);
        const int y = static_cast<int>((1.4 - im) / 2.8 * (H - 1) + 0.5);
        if (x >= 0 && x < W && y >= 0 && y < H) grid[static_cast<size_t>(y)][static_cast<size_t>(x)] = ch;
    };

    for (int i = 0; i < 180; ++i)         // the unit circle
    {
        const double t = kTwoPi * i / 180.0;
        plot(std::cos(t), std::sin(t), '.');
    }
    plot(0, 0, '+');                       // origin

    const auto p = polesOf(c);
    const auto z = zerosOf(c);
    plot(z.first.real(),  z.first.imag(),  'o');
    plot(z.second.real(), z.second.imag(), 'o');
    plot(p.first.real(),  p.first.imag(),  'x');
    plot(p.second.real(), p.second.imag(), 'x');

    std::cout << "\n  " << label << "   (o = zero, x = pole)\n";
    for (const auto& row : grid) std::cout << "  " << row << "\n";

    const double pr = std::abs(p.first);
    const double pa = std::arg(p.first);

    std::cout << std::fixed << std::setprecision(4);
    std::cout << "  pole radius " << pr
              << "   angle " << pa << " rad"
              << "   -> " << std::setprecision(1) << pa * 44100.0 / kTwoPi << " Hz\n";
    std::cout << "  stable: " << (c.isStable() ? "yes" : "NO")
              << "    n60 = " << std::setprecision(0)
              << (pr > 0.0 && pr < 1.0 ? -6.907755 / std::log(pr) : 0.0)
              << " samples\n";
}

int main()
{
    const double sr = 44100.0;

    // ---- 1. z-plane plots for three filter types ---------------------
    plotZPlane(BiquadCoeffs::design(FilterType::LowPass,  1000.0, sr, 0.707),
               "Low-pass 1 kHz Q=0.707  (zeros at z = -1, Nyquist)");
    plotZPlane(BiquadCoeffs::design(FilterType::HighPass, 1000.0, sr, 0.707),
               "High-pass 1 kHz Q=0.707 (zeros at z = +1, DC)");
    plotZPlane(BiquadCoeffs::design(FilterType::Notch,    1000.0, sr, 8.0),
               "Notch 1 kHz Q=8         (zeros ON the circle)");

    // ---- 2. pole radius -> ring time ---------------------------------
    std::cout << "\n--- pole radius and decay ---\n";
    std::cout << std::setw(12) << std::left << "radius" << std::right
              << std::setw(16) << "n60 (samples)"
              << std::setw(16) << "time @44.1k"
              << "   character\n";

    struct R { double r; const char* what; };
    const R radii[] = {
        { 0.5000,  "dead"        },
        { 0.9000,  "damped"      },
        { 0.9900,  "resonant"    },
        { 0.9990,  "ringing"     },
        { 0.9999,  "nearly sine" },
        { 1.0000,  "oscillator"  },
        { 1.0001,  "UNSTABLE"    },
    };

    for (const auto& e : radii)
    {
        const double n60 = (e.r < 1.0) ? -6.907755 / std::log(e.r) : -1.0;
        std::cout << std::setw(12) << std::left << std::fixed << std::setprecision(4)
                  << e.r << std::right;
        if (n60 > 0.0)
            std::cout << std::setw(16) << std::setprecision(0) << n60
                      << std::setw(14) << std::setprecision(1) << n60 / sr * 1000.0 << " ms";
        else
            std::cout << std::setw(16) << "inf" << std::setw(16) << "forever";
        std::cout << "   " << e.what << "\n";
    }

    // ---- 3. resonators: hear the radius ------------------------------
    {
        std::vector<float> demo;

        for (double radius : { 0.9, 0.99, 0.999, 0.9999 })
        {
            const double theta = kTwoPi * 220.0 / sr;

            BiquadCoeffs c;
            c.b0 = 1.0 - radius;
            c.b1 = 0.0;
            c.b2 = 0.0;
            c.a1 = -2.0 * radius * std::cos(theta);
            c.a2 = radius * radius;

            Biquad f;
            f.setSampleRate(sr);
            f.setCoefficients(c);

            std::vector<float> seg(static_cast<size_t>(2.0 * sr), 0.0f);
            for (size_t i = 0; i < seg.size(); ++i)
                seg[i] = f.processSample(i == 0 ? 1.0f : 0.0f, 0);

            const double p = peak(seg);
            if (p > 0.0) for (auto& s : seg) s = static_cast<float>(s * 0.6 / p);

            demo.insert(demo.end(), seg.begin(), seg.end());
        }

        writeWav("resonators.wav", demo, static_cast<int>(sr), 1, true);
        std::cout << "\nWrote resonators.wav  (220 Hz at radius 0.9, 0.99, 0.999, 0.9999)\n";
        std::cout << "  This progression is percussive -> sustained, from one number.\n";
    }

    // ---- 4. the stability triangle -----------------------------------
    {
        std::cout << "\n--- stability triangle test ---\n";
        int agree = 0, total = 0;
        FastRandom rng(99);

        for (int i = 0; i < 20000; ++i)
        {
            BiquadCoeffs c;
            c.a1 = rng.nextFloat() * 3.0;
            c.a2 = rng.nextFloat() * 1.5;

            const auto p = polesOf(c);
            const bool byRadius   = std::abs(p.first) < 1.0 && std::abs(p.second) < 1.0;
            const bool byTriangle = c.isStable();

            ++total;
            if (byRadius == byTriangle) ++agree;
        }
        std::cout << "  triangle test agrees with pole radii on "
                  << agree << " / " << total << " random coefficient pairs\n";
    }

    // ---- 5. what instability looks like (volume at zero) -------------
    {
        std::cout << "\n--- instability (a2 = 1.01) ---\n";
        BiquadCoeffs c = BiquadCoeffs::design(FilterType::LowPass, 1000.0, sr, 2.0);
        c.a2 = 1.01;
        std::cout << "  isStable(): " << (c.isStable() ? "yes" : "no") << "\n";

        double s1 = 0.0, s2 = 0.0;
        for (int n = 0; n < 2000; ++n)
        {
            const double x = (n == 0) ? 1.0 : 0.0;
            const double y = c.b0 * x + s1;
            s1 = c.b1 * x - c.a1 * y + s2;
            s2 = c.b2 * x - c.a2 * y;

            if (n % 400 == 0)
                std::cout << "    n=" << std::setw(5) << n
                          << "   y = " << std::scientific << std::setprecision(3) << y << "\n";
            if (!std::isfinite(y)) { std::cout << "    -> inf at n=" << n << "\n"; break; }
        }
        std::cout << std::fixed;
    }

    return 0;
}
