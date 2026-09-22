// Chapter 14 - dcblocker.h
// The first IIR filter in the book: a one-pole/one-zero high-pass that
// removes DC offset.
//
//     y[n] = x[n] - x[n-1] + R * y[n-1]
//
// x[n] - x[n-1]  kills anything constant (a discrete derivative)
// + R * y[n-1]   feeds back, restoring the low frequencies in a controlled way

#pragma once

#include <cmath>

class DCBlocker
{
public:
    void setSampleRate(double sr, double cutoffHz = 20.0)
    {
        // R closer to 1.0 = lower cutoff.
        R = 1.0 - (2.0 * 3.14159265358979323846 * cutoffHz / sr);
        if (R < 0.0)       R = 0.0;
        if (R > 0.9999999) R = 0.9999999;
    }

    float process(float x)
    {
        const double y = static_cast<double>(x) - x1 + R * y1;
        x1 = x;
        y1 = y;
        return static_cast<float>(y);
    }

    void reset() { x1 = 0.0; y1 = 0.0; }

    double coefficient() const { return R; }

private:
    double R  = 0.9985;
    double x1 = 0.0;      // previous input   (state -- must be double)
    double y1 = 0.0;      // previous output
};
