// Example - ch19_phasors.cpp
// Complex numbers as rotation, Euler's formula, a phasor oscillator, and
// negative frequency demonstrated. See Chapter 19.
//
//     cmake --build build && ./build/bin/ch19_phasors

#include <audio/audio.h>

#include <complex>
#include <iostream>
#include <iomanip>
#include <vector>

using namespace audio;
using Complex = std::complex<double>;

int main()
{
    std::cout << std::fixed << std::setprecision(2);

    // --- 1. j really is a quarter turn --------------------------------
    std::cout << "--- rotation by j ---\n";
    {
        Complex z(1.0, 0.0);
        for (int k = 0; k <= 4; ++k)
        {
            std::cout << "  j^" << k << " * 1 = ("
                      << std::setw(6) << z.real() << ", "
                      << std::setw(6) << z.imag() << "j)"
                      << "   magnitude " << std::abs(z)
                      << "   angle " << std::setw(8)
                      << std::arg(z) * 180.0 / kPi << " deg\n";
            z *= Complex(0.0, 1.0);          // multiply by j
        }
    }

    // --- 2. multiplication = multiply magnitudes, add angles ----------
    std::cout << "\n--- multiplication ---\n";
    {
        const Complex a = std::polar(2.0, 30.0 * kPi / 180.0);
        const Complex b = std::polar(3.0, 45.0 * kPi / 180.0);
        const Complex c = a * b;

        std::cout << "  a  : mag " << std::abs(a)
                  << "  angle " << std::arg(a) * 180.0 / kPi << "\n";
        std::cout << "  b  : mag " << std::abs(b)
                  << "  angle " << std::arg(b) * 180.0 / kPi << "\n";
        std::cout << "  a*b: mag " << std::abs(c)
                  << "  angle " << std::arg(c) * 180.0 / kPi
                  << "    (expected mag 6.00, angle 75.00)\n";
    }

    // --- 3. Euler's formula -------------------------------------------
    std::cout << "\n--- Euler: e^(j*theta) = cos + j sin ---\n";
    for (double deg : { 0.0, 90.0, 180.0, 270.0, 360.0 })
    {
        const double  th = deg * kPi / 180.0;
        const Complex e  = std::exp(Complex(0.0, th));
        std::cout << "  e^(j*" << std::setw(6) << deg << " deg) = ("
                  << std::setw(6) << e.real() << ", "
                  << std::setw(6) << e.imag() << "j)"
                  << "   cos=" << std::setw(6) << std::cos(th)
                  << "  sin=" << std::setw(6) << std::sin(th) << "\n";
    }

    // --- 4. A phasor oscillator (no sin() in the loop) ----------------
    std::cout << "\n--- phasor oscillator ---\n";
    {
        const double sr   = 44100.0;
        const double freq = 441.0;
        const double w    = kTwoPi * freq / sr;

        const Complex rotate = std::polar(1.0, w);   // one step, precomputed

        Complex phasor(1.0, 0.0);
        std::vector<float> out(static_cast<size_t>(sr * 2.0));

        for (size_t i = 0; i < out.size(); ++i)
        {
            out[i] = static_cast<float>(0.5 * phasor.imag());   // sine
            phasor *= rotate;                                    // ONE multiply

            // Rounding makes the magnitude drift off the unit circle, so the
            // amplitude would slowly grow or decay. Pull it back periodically.
            if ((i & 0x3FF) == 0)
                phasor /= std::abs(phasor);
        }

        writeWav("phasor_sine.wav", out, static_cast<int>(sr), 1);
        std::cout << "  wrote phasor_sine.wav   peak " << peakDb(out)
                  << " dBFS   RMS " << rmsDb(out) << " dBFS\n";

        // Compare against std::sin to prove it is the same signal.
        auto reference = sig::sine(out.size(), freq, sr, 0.5);
        std::cout << "  max difference from std::sin version: "
                  << std::scientific << sig::maxDifference(out, reference)
                  << std::fixed << "\n";
    }

    // --- 5. Two counter-rotating phasors make a real cosine ----------
    std::cout << "\n--- negative frequency ---\n";
    {
        const double w = 0.3;
        for (int n = 0; n < 5; ++n)
        {
            const Complex pos = std::exp(Complex(0.0,  w * n));
            const Complex neg = std::exp(Complex(0.0, -w * n));
            const Complex sum = 0.5 * (pos + neg);

            std::cout << "  n=" << n
                      << "  0.5(e^+jwn + e^-jwn) = (" << std::setw(6) << sum.real()
                      << ", " << std::setw(6) << sum.imag() << "j)"
                      << "    cos(wn) = " << std::setw(6) << std::cos(w * n) << "\n";
        }
        std::cout << "  The imaginary parts cancel exactly. That is what\n"
                  << "  negative frequency is for.\n";
    }

    // --- 6. A quadrature oscillator: sine and cosine together --------
    {
        const double sr = 44100.0;
        const Complex rotate = std::polar(1.0, kTwoPi * 220.0 / sr);
        Complex phasor(1.0, 0.0);

        std::vector<std::vector<float>> stereo(
            2, std::vector<float>(static_cast<size_t>(sr * 2.0)));

        for (size_t i = 0; i < stereo[0].size(); ++i)
        {
            stereo[0][i] = static_cast<float>(0.5 * phasor.real());   // cosine
            stereo[1][i] = static_cast<float>(0.5 * phasor.imag());   // sine
            phasor *= rotate;
            if ((i & 0x3FF) == 0) phasor /= std::abs(phasor);
        }

        writeWav("quadrature.wav", stereo, static_cast<int>(sr), true);
        std::cout << "\n  wrote quadrature.wav (L = cos, R = sin, 90 degrees apart)\n";
    }

    return 0;
}
