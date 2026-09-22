// Chapter 6 - notetable.cpp
// Prints MIDI note numbers, names, frequencies, and samples-per-cycle.
// Build:  g++ -std=c++17 -Wall -Wextra -O2 notetable.cpp -o notetable.exe
// Run:    ./notetable.exe

#include <iostream>
#include <iomanip>
#include <cmath>
#include <string>

const double kSampleRate = 44100.0;

// Convert a MIDI note number to frequency in hertz.
// MIDI note 69 is A4 = 440 Hz; each semitone is a factor of 2^(1/12).
// NOTE the 12.0 -- with 12 this would be integer division and the result
// would snap to octaves. See Chapter 6, section 6.4.
double frequencyFromMidiNote(int noteNumber)
{
    return 440.0 * std::pow(2.0, (noteNumber - 69) / 12.0);
}

// Return the note name for a MIDI note number, e.g. 60 -> "C4".
std::string nameFromMidiNote(int noteNumber)
{
    const std::string names[12] = { "C",  "C#", "D",  "D#", "E",  "F",
                                    "F#", "G",  "G#", "A",  "A#", "B" };

    const int pitchClass = noteNumber % 12;         // 0..11, which letter
    const int octave     = (noteNumber / 12) - 1;   // MIDI 60 = C4

    return names[pitchClass] + std::to_string(octave);
}

// How many samples does one cycle of this frequency occupy?
double samplesPerCycle(double frequency)
{
    return kSampleRate / frequency;
}

int main()
{
    std::cout << std::fixed << std::setprecision(3);
    std::cout << " MIDI  Note    Frequency (Hz)   Samples/cycle   Period (ms)\n";
    std::cout << "-----------------------------------------------------------\n";

    for (int note = 21; note <= 108; note += 12)   // A0 to C8, one per octave
    {
        const double freq     = frequencyFromMidiNote(note);
        const double perCyc   = samplesPerCycle(freq);
        const double periodMs = 1000.0 / freq;

        std::cout << std::setw(5)  << note
                  << std::setw(7)  << nameFromMidiNote(note)
                  << std::setw(17) << freq
                  << std::setw(16) << perCyc
                  << std::setw(14) << periodMs
                  << "\n";
    }

    std::cout << "\nNyquist at " << kSampleRate << " Hz is "
              << kSampleRate / 2.0 << " Hz.\n";

    const int    highest = 108;
    const double f       = frequencyFromMidiNote(highest);
    const int    harmonicsBelowNyquist = static_cast<int>((kSampleRate / 2.0) / f);

    std::cout << "The top note (" << nameFromMidiNote(highest) << ", " << f
              << " Hz) has room for only " << harmonicsBelowNyquist
              << " harmonics below Nyquist.\n";

    return 0;
}
