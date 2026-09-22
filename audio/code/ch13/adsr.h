// Chapter 13 - adsr.h
// A four-stage ADSR envelope generator with exponential curves.
//
// Usage:
//     ADSR env;
//     env.setSampleRate(44100.0);
//     env.setParameters(0.01, 0.2, 0.7, 0.5);   // A, D, S, R
//     env.noteOn();
//     for each sample:  out = signal * env.nextSample();
//     env.noteOff();    // starts the release stage

#pragma once

#include <cmath>
#include <algorithm>

class ADSR
{
public:
    enum class Stage { Idle, Attack, Decay, Sustain, Release };

    void setSampleRate(double sr)
    {
        sampleRate = sr;
        recalculate();
    }

    // attack, decay, release in SECONDS; sustain is a LEVEL 0..1.
    void setParameters(double attackSec, double decaySec,
                       double sustainLevel, double releaseSec)
    {
        attackTime  = std::max(attackSec,  0.0);
        decayTime   = std::max(decaySec,   0.0);
        sustain     = std::clamp(sustainLevel, 0.0, 1.0);
        releaseTime = std::max(releaseSec, 0.0);
        recalculate();
    }

    void noteOn()
    {
        stage = Stage::Attack;
        // We do NOT reset `value` to 0: starting the attack from the current
        // level makes retriggering click-free. See section 13.7.
    }

    void noteOff()
    {
        if (stage != Stage::Idle)
            stage = Stage::Release;
    }

    // Hard reset: silence, no release.
    void reset()
    {
        stage = Stage::Idle;
        value = 0.0;
    }

    bool  isActive()     const { return stage != Stage::Idle; }
    Stage currentStage() const { return stage; }

    // Advance one sample and return the envelope value (0..1).
    float nextSample()
    {
        switch (stage)
        {
            case Stage::Idle:
                value = 0.0;
                break;

            case Stage::Attack:
                // Aim ABOVE 1.0 so the exponential passes through 1.0 with a
                // usable slope instead of crawling asymptotically.
                value = attackTarget + (value - attackTarget) * attackCoeff;
                if (value >= 1.0)
                {
                    value = 1.0;
                    stage = (decayTime > 0.0) ? Stage::Decay : Stage::Sustain;
                }
                break;

            case Stage::Decay:
                value = decayTarget + (value - decayTarget) * decayCoeff;
                if (value <= sustain + 0.0001)
                {
                    value = sustain;
                    stage = Stage::Sustain;
                }
                break;

            case Stage::Sustain:
                value = sustain;
                break;

            case Stage::Release:
                value = releaseTarget + (value - releaseTarget) * releaseCoeff;
                if (value <= 0.0001)
                {
                    value = 0.0;
                    stage = Stage::Idle;
                }
                break;
        }

        return static_cast<float>(value);
    }

private:
    // Coefficient for a one-pole exponential with time constant `timeSec`.
    static double calcCoeff(double timeSec, double sampleRate)
    {
        if (timeSec <= 0.0)
            return 0.0;                       // instantaneous
        return std::exp(-1.0 / (timeSec * sampleRate));
    }

    void recalculate()
    {
        attackCoeff  = calcCoeff(attackTime,  sampleRate);
        decayCoeff   = calcCoeff(decayTime,   sampleRate);
        releaseCoeff = calcCoeff(releaseTime, sampleRate);

        attackTarget  = 1.0 + attackOvershoot;
        decayTarget   = sustain - decayUndershoot;
        releaseTarget = -releaseUndershoot;
    }

    double sampleRate  = 44100.0;

    double attackTime  = 0.01;
    double decayTime   = 0.10;
    double sustain     = 0.70;
    double releaseTime = 0.30;

    double attackCoeff  = 0.0, decayCoeff  = 0.0, releaseCoeff  = 0.0;
    double attackTarget = 0.0, decayTarget = 0.0, releaseTarget = 0.0;

    // How far past the destination each curve aims. Larger = straighter
    // (more "digital"); smaller = more curved (more "analogue").
    static constexpr double attackOvershoot   = 0.3;
    static constexpr double decayUndershoot   = 0.05;
    static constexpr double releaseUndershoot = 0.05;

    double value = 0.0;
    Stage  stage = Stage::Idle;
};
