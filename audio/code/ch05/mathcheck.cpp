// Chapter 5 - mathcheck.cpp
// Confirms the maths library and floating-point formatting work.
// Build:  g++ -std=c++17 -Wall -Wextra -O2 mathcheck.cpp -o mathcheck.exe
// Run:    ./mathcheck.exe

#include <iostream>
#include <iomanip>
#include <cmath>

int main()
{
    const double sampleRate = 44100.0;
    const double frequency  = 440.0;

    const double period          = 1.0 / frequency;
    const double samplesPerCycle = sampleRate / frequency;
    const double nyquist         = sampleRate / 2.0;

    std::cout << std::fixed << std::setprecision(6);
    std::cout << "Sample rate      : " << sampleRate      << " Hz\n";
    std::cout << "Frequency        : " << frequency       << " Hz\n";
    std::cout << "Period           : " << period          << " s\n";
    std::cout << "Samples per cycle: " << samplesPerCycle << "\n";
    std::cout << "Nyquist          : " << nyquist         << " Hz\n";

    const double pi = 3.14159265358979323846;
    std::cout << "sin(pi/2)        : " << std::sin(pi / 2.0) << "\n";
    std::cout << "sqrt(2)          : " << std::sqrt(2.0)     << "\n";
    std::cout << "1/sqrt(2) in dB  : " << 20.0 * std::log10(1.0 / std::sqrt(2.0)) << "\n";

    return 0;
}
