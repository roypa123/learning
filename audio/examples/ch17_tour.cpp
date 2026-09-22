// Example - ch17_tour.cpp
// Everything from Part I, using the library. Compare this with the build
// lines and duplicated helpers of Chapters 9-16.
//
//     cmake -S . -B build && cmake --build build
//     ./build/bin/ch17_tour

#include <audio/audio.h>

#include <iostream>
#include <iomanip>

using namespace audio;

int main()
{
    const double sr = 44100.0;

    // ---- a note: saw oscillator through an ADSR --------------------
    AudioBuffer note(1, static_cast<int>(sr * 2.0), sr);

    Oscillator osc;
    osc.setSampleRate(sr);
    osc.setFrequency(midiToFrequency(45));      // A2, 110 Hz
    osc.setWaveform(Waveform::Saw);

    ADSR env;
    env.setSampleRate(sr);
    env.setParameters(0.01, 0.3, 0.6, 0.5);
    env.noteOn();

    auto& ch = note.channel(0);
    for (size_t i = 0; i < ch.size(); ++i)
    {
        if (i == ch.size() * 3 / 4)
            env.noteOff();
        ch[i] = osc.nextSample() * env.nextSample() * 0.5f;
    }

    // ---- run it through a processor chain ---------------------------
    Chain chain;
    chain.add(std::make_unique<Gain>(0.8f));
    chain.prepare(sr, 512);
    chain.process(note);

    note.normalise(0.9f);
    writeWav("tour_note.wav", note, true);

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "tour_note.wav   peak " << peakDb(ch)
              << " dBFS   RMS " << rmsDb(ch)
              << " dBFS   crest " << crestFactorDb(ch) << " dB\n";

    // ---- a stereo noise bed -----------------------------------------
    AudioBuffer bed(2, static_cast<int>(sr * 3.0), sr);

    FastRandom  rngL(1), rngR(2);
    PinkKellett pinkL, pinkR;
    DCBlocker   dcL, dcR;
    dcL.setSampleRate(sr);
    dcR.setSampleRate(sr);

    for (int i = 0; i < bed.numFrames(); ++i)
    {
        bed.channel(0)[static_cast<size_t>(i)] = dcL.process(pinkL.process(rngL.nextFloat()) * 0.5f);
        bed.channel(1)[static_cast<size_t>(i)] = dcR.process(pinkR.process(rngR.nextFloat()) * 0.5f);
    }

    bed.fadeIn(static_cast<int>(sr * 0.5));
    bed.fadeOut(static_cast<int>(sr * 0.5));
    bed.normalise(0.7f);
    writeWav("tour_bed.wav", bed, true);

    std::cout << "tour_bed.wav    peak " << peakDb(bed.channel(0))
              << " dBFS   DC " << std::setprecision(6)
              << dcOffset(bed.channel(0)) << "\n";

    // ---- read one back and report -----------------------------------
    std::string err;
    WavFile loaded = readWav("tour_note.wav", err);
    if (!loaded.isValid())
    {
        std::cerr << "Read failed: " << err << "\n";
        return 1;
    }

    std::cout << "\nread back: " << loaded.numChannels << " ch, "
              << loaded.sampleRate << " Hz, "
              << loaded.bitsPerSample << " bit, "
              << std::setprecision(3) << loaded.durationSeconds() << " s\n";

    return 0;
}
