// Chapter 14 - mixer.h
// A small mono-in / stereo-out mixer with gain, pan, mute and solo.

#pragma once

#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <utility>

struct Track
{
    std::string        name;
    std::vector<float> samples;      // mono source
    float gain = 1.0f;               // linear gain
    float pan  = 0.0f;               // -1 = hard left, 0 = centre, +1 = hard right
    bool  mute = false;
    bool  solo = false;
};

class Mixer
{
public:
    void addTrack(Track t) { tracks.push_back(std::move(t)); }

    size_t numTracks() const { return tracks.size(); }

    // Mix down to stereo. Returns {left, right}.
    std::vector<std::vector<float>> render() const
    {
        size_t longest = 0;
        for (const auto& t : tracks)
            longest = std::max(longest, t.samples.size());

        std::vector<std::vector<float>> out(2, std::vector<float>(longest, 0.0f));

        const bool anySolo = std::any_of(tracks.begin(), tracks.end(),
                                         [](const Track& t) { return t.solo; });

        for (const auto& t : tracks)
        {
            if (t.mute)             continue;
            if (anySolo && !t.solo) continue;

            // Constant-power pan: gains follow a quarter circle so that
            // gl^2 + gr^2 == 1 at every position. See Chapter 74.
            const double angle = (static_cast<double>(t.pan) + 1.0) * 0.25 * kPi;
            const float gl = static_cast<float>(std::cos(angle)) * t.gain;
            const float gr = static_cast<float>(std::sin(angle)) * t.gain;

            for (size_t i = 0; i < t.samples.size(); ++i)
            {
                out[0][i] += t.samples[i] * gl;
                out[1][i] += t.samples[i] * gr;
            }
        }

        return out;
    }

private:
    std::vector<Track> tracks;
    static constexpr double kPi = 3.14159265358979323846;
};
