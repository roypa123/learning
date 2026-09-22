// libaudio tests - test_main.cpp
// Build with the project, then run ./build/bin/audio_tests

#include <audio/audio.h>
#include <audio/test.h>

#include <vector>
#include <cmath>
#include <algorithm>

using namespace audio;

// ---------------------------------------------------------------- decibels

void testDecibels()
{
    test::section("decibels");

    CHECK_CLOSE(gainToDb(1.0),  0.0,     0.001);
    CHECK_CLOSE(gainToDb(0.5), -6.0206,  0.001);
    CHECK_CLOSE(gainToDb(2.0),  6.0206,  0.001);
    CHECK_CLOSE(gainToDb(0.70710678), -3.0103, 0.001);

    CHECK_CLOSE(dbToGain(0.0),   1.0,     0.0001);
    CHECK_CLOSE(dbToGain(-6.0),  0.501187, 0.0001);
    CHECK_CLOSE(dbToGain(-20.0), 0.1,     0.0001);
    CHECK_CLOSE(dbToGain(20.0),  10.0,    0.0001);

    // Round trip
    for (double d : { 0.0, -3.0, -12.0, -40.0, 6.0 })
        CHECK_CLOSE(gainToDb(dbToGain(d)), d, 0.0001);

    // A negative gain is a polarity inversion, not a negative level.
    CHECK_CLOSE(gainToDb(-0.5), -6.0206, 0.001);

    // The zero guard: must stay finite.
    CHECK(std::isfinite(gainToDb(0.0)));
    CHECK(dbToGain(-300.0) == 0.0);

    // Fader law: unity at the top, silent at the bottom.
    CHECK_CLOSE(faderToGain(1.0), 1.0, 1e-9);
    CHECK(faderToGain(0.0) == 0.0);
}

// ---------------------------------------------------------------- oscillator

void testOscillator()
{
    test::section("oscillator");

    Oscillator osc;
    osc.setSampleRate(44100.0);
    osc.setFrequency(441.0);            // exactly 100 samples per cycle
    osc.setWaveform(Waveform::Sine);

    std::vector<float> buf(400);
    for (auto& s : buf) s = osc.nextSample();

    // Periodic with period 100
    for (int i = 0; i < 300; ++i)
        CHECK_CLOSE(buf[static_cast<size_t>(i)],
                    buf[static_cast<size_t>(i + 100)], 1e-5);

    CHECK_CLOSE(peak(buf), 1.0,        0.001);
    CHECK_CLOSE(rms(buf),  0.70710678, 0.001);
    CHECK_CLOSE(dcOffset(buf), 0.0,    1e-6);

    // A sine's crest factor is exactly 20*log10(sqrt(2)) = 3.01 dB
    CHECK_CLOSE(crestFactorDb(buf), 3.0103, 0.01);

    // A square's crest factor is 0 dB: peak == RMS
    osc.reset();
    osc.setWaveform(Waveform::Square);
    for (auto& s : buf) s = osc.nextSample();
    CHECK_CLOSE(crestFactorDb(buf), 0.0, 0.01);
    CHECK_CLOSE(rms(buf), 1.0, 0.001);

    // A saw covers -1..+1 evenly, so RMS is 1/sqrt(3)
    osc.reset();
    osc.setWaveform(Waveform::Saw);
    for (auto& s : buf) s = osc.nextSample();
    CHECK_CLOSE(rms(buf), 0.57735, 0.01);

    // A 50% pulse has no DC; a 10% pulse has a DC of 2d-1 = -0.8
    osc.reset();
    osc.setWaveform(Waveform::Pulse);
    osc.setPulseWidth(0.1);
    for (auto& s : buf) s = osc.nextSample();
    CHECK_CLOSE(dcOffset(buf), -0.8, 0.02);

    // Boundary: zero frequency must not divide by zero or produce NaN
    osc.reset();
    osc.setWaveform(Waveform::Sine);
    osc.setFrequency(0.0);
    for (auto& s : buf) s = osc.nextSample();
    CHECK(!hasNaN(buf));
}

// ---------------------------------------------------------------- envelope

void testEnvelope()
{
    test::section("envelope");

    ADSR env;
    env.setSampleRate(44100.0);
    env.setParameters(0.01, 0.05, 0.5, 0.1);

    CHECK(!env.isActive());

    env.noteOn();
    CHECK(env.isActive());

    // Envelope must stay within 0..1 at all times.
    float maxV = 0.0f, minV = 1.0f;
    for (int i = 0; i < 44100; ++i)
    {
        const float v = env.nextSample();
        maxV = std::max(maxV, v);
        minV = std::min(minV, v);
    }
    CHECK(maxV <= 1.0001f);
    CHECK(minV >= -0.0001f);

    // After decay it should be sitting at the sustain level.
    CHECK_CLOSE(env.nextSample(), 0.5, 0.01);

    // Release must reach zero and go idle.
    env.noteOff();
    for (int i = 0; i < 44100; ++i)
        env.nextSample();
    CHECK(!env.isActive());
    CHECK_CLOSE(env.nextSample(), 0.0, 1e-6);

    // reset() must be immediate.
    env.noteOn();
    for (int i = 0; i < 1000; ++i) env.nextSample();
    env.reset();
    CHECK(env.nextSample() == 0.0f);
}

// ---------------------------------------------------------------- noise

void testNoise()
{
    test::section("noise");

    FastRandom rng(12345);

    std::vector<float> buf(100000);
    for (auto& s : buf) s = rng.nextFloat();

    // Uniform in [-1,1): mean ~0, RMS ~1/sqrt(3)
    CHECK_CLOSE(dcOffset(buf), 0.0,     0.01);
    CHECK_CLOSE(rms(buf),      0.57735, 0.01);
    CHECK(peak(buf) <= 1.0);
    CHECK(!hasNaN(buf));

    // Same seed must give the same sequence.
    FastRandom a(999), b(999);
    for (int i = 0; i < 100; ++i)
        CHECK(a.nextFloat() == b.nextFloat());

    // Seed 0 must not lock the generator at zero.
    FastRandom zero(0);
    bool anyNonZero = false;
    for (int i = 0; i < 100; ++i)
        if (zero.nextFloat() != 0.0f) anyNonZero = true;
    CHECK(anyNonZero);

    // Pink noise must stay bounded and finite.
    PinkKellett pink;
    for (auto& s : buf) s = pink.process(rng.nextFloat());
    CHECK(!hasNaN(buf));
    CHECK(peak(buf) < 4.0);

    // Brown noise must not drift into unbounded DC.
    Brown brown;
    for (auto& s : buf) s = brown.process(rng.nextFloat());
    CHECK(!hasNaN(buf));
    CHECK(std::fabs(dcOffset(buf)) < 0.6);
}

// ---------------------------------------------------------------- dc blocker

void testDCBlocker()
{
    test::section("dc blocker");

    DCBlocker dc;
    dc.setSampleRate(44100.0, 20.0);

    // A constant input must decay to nothing.
    float last = 0.0f;
    for (int i = 0; i < 44100; ++i)
        last = dc.process(0.5f);
    CHECK_CLOSE(last, 0.0, 0.001);

    // A sine well above the cutoff must pass essentially unchanged.
    dc.reset();
    Oscillator osc;
    osc.setSampleRate(44100.0);
    osc.setFrequency(1000.0);

    std::vector<float> in(44100), out(44100);
    for (size_t i = 0; i < in.size(); ++i)
    {
        in[i]  = osc.nextSample() * 0.5f;
        out[i] = dc.process(in[i]);
    }

    // Compare the settled portion only (skip the filter's start-up).
    std::vector<float> inTail (in.begin()  + 4410, in.end());
    std::vector<float> outTail(out.begin() + 4410, out.end());
    CHECK_CLOSE(rmsDb(outTail), rmsDb(inTail), 0.1);

    // A sine plus DC: the DC must go, the sine must stay.
    dc.reset();
    osc.reset();
    for (size_t i = 0; i < out.size(); ++i)
        out[i] = dc.process(osc.nextSample() * 0.5f + 0.3f);

    std::vector<float> tail(out.begin() + 4410, out.end());
    CHECK_CLOSE(dcOffset(tail), 0.0, 0.001);
    CHECK(!hasNaN(out));
}

// ---------------------------------------------------------------- buffers

void testAudioBuffer()
{
    test::section("audio buffer");

    AudioBuffer buf(2, 44100, 44100.0);

    CHECK(buf.numChannels() == 2);
    CHECK(buf.numFrames()   == 44100);
    CHECK_CLOSE(buf.durationSeconds(), 1.0, 1e-9);
    CHECK(buf.peak() == 0.0f);

    Oscillator osc;
    osc.setSampleRate(44100.0);
    osc.setFrequency(441.0);

    for (int i = 0; i < buf.numFrames(); ++i)
    {
        const float s = osc.nextSample() * 0.5f;
        buf.channel(0)[static_cast<size_t>(i)] = s;
        buf.channel(1)[static_cast<size_t>(i)] = s;
    }

    CHECK_CLOSE(buf.peak(), 0.5, 0.001);
    CHECK_CLOSE(buf.rms(), 0.5 * 0.70710678, 0.001);

    buf.applyGain(0.5f);
    CHECK_CLOSE(buf.peak(), 0.25, 0.001);

    buf.normalise(0.9f);
    CHECK_CLOSE(buf.peak(), 0.9, 0.001);

    // Fades must start and end at silence.
    buf.fadeIn(1000);
    buf.fadeOut(1000);
    CHECK(buf.channel(0)[0] == 0.0f);
    CHECK(buf.channel(0)[static_cast<size_t>(buf.numFrames() - 1)] == 0.0f);

    // Interleaving must alternate L,R,L,R
    AudioBuffer small(2, 3, 44100.0);
    small.channel(0) = { 1.0f, 2.0f, 3.0f };
    small.channel(1) = { 4.0f, 5.0f, 6.0f };
    const auto il = small.interleaved();
    CHECK(il.size() == 6);
    CHECK(il[0] == 1.0f); CHECK(il[1] == 4.0f);
    CHECK(il[2] == 2.0f); CHECK(il[3] == 5.0f);
    CHECK(il[4] == 3.0f); CHECK(il[5] == 6.0f);

    // Boundaries: empty buffers must not crash.
    AudioBuffer empty;
    CHECK(empty.numChannels() == 0);
    CHECK(empty.numFrames()   == 0);
    CHECK(empty.peak() == 0.0f);
    CHECK(empty.rms()  == 0.0f);
}

// ---------------------------------------------------------------- wav

void testWavRoundTrip()
{
    test::section("wav round trip");

    for (int channels : { 1, 2, 6 })
    {
        std::vector<std::vector<float>> original(
            static_cast<size_t>(channels), std::vector<float>(1000));

        for (int c = 0; c < channels; ++c)
            for (size_t i = 0; i < 1000; ++i)
                original[static_cast<size_t>(c)][i] =
                    static_cast<float>(0.8 * std::sin(static_cast<double>(i)
                                       * 0.1 * (c + 1)));

        CHECK(writeWav("test_tmp.wav", original, 44100, false));

        std::string err;
        WavFile loaded = readWav("test_tmp.wav", err);

        CHECK(loaded.isValid());
        CHECK(loaded.numChannels == channels);
        CHECK(loaded.sampleRate  == 44100);
        CHECK(loaded.numFrames   == 1000);

        double maxError = 0.0;
        for (int c = 0; c < channels; ++c)
            for (size_t i = 0; i < 1000; ++i)
                maxError = std::max(maxError,
                    std::fabs(static_cast<double>(original[static_cast<size_t>(c)][i])
                            - static_cast<double>(loaded.channels[static_cast<size_t>(c)][i])));

        // One 16-bit quantization step is 1/32768 = 3.05e-05
        CHECK(maxError < 1.0 / 32000.0);
    }
}

// ---------------------------------------------------------------- processors

void testProcessors()
{
    test::section("processors");

    AudioBuffer buf(1, 100, 44100.0);
    for (auto& s : buf.channel(0)) s = 1.0f;

    Gain g(0.5f);
    g.process(buf);
    CHECK_CLOSE(buf.channel(0)[0], 0.5, 1e-6);

    // A chain of two 0.5 gains must equal one 0.25 gain.
    AudioBuffer a(1, 10, 44100.0), b(1, 10, 44100.0);
    for (auto& s : a.channel(0)) s = 1.0f;
    for (auto& s : b.channel(0)) s = 1.0f;

    Chain chain;
    chain.add(std::make_unique<Gain>(0.5f));
    chain.add(std::make_unique<Gain>(0.5f));
    chain.process(a);

    Gain quarter(0.25f);
    quarter.process(b);

    for (size_t i = 0; i < 10; ++i)
        CHECK_CLOSE(a.channel(0)[i], b.channel(0)[i], 1e-7);

    // Chains nest, because Chain is itself a Processor.
    auto inner = std::make_unique<Chain>();
    inner->add(std::make_unique<Gain>(2.0f));
    Chain outer;
    outer.add(std::move(inner));
    outer.process(b);
    CHECK_CLOSE(b.channel(0)[0], 0.5, 1e-7);
}

// ---------------------------------------------------------------- notes

void testNoteConversion()
{
    test::section("note conversion");

    CHECK_CLOSE(midiToFrequency(69), 440.0,    0.001);
    CHECK_CLOSE(midiToFrequency(57), 220.0,    0.001);
    CHECK_CLOSE(midiToFrequency(81), 880.0,    0.001);
    CHECK_CLOSE(midiToFrequency(60), 261.6256, 0.001);

    for (int n = 0; n < 128; ++n)
        CHECK_CLOSE(frequencyToMidi(midiToFrequency(n)), n, 1e-6);
}

// ---------------------------------------------------------------- main

int main()
{
    testDecibels();
    testOscillator();
    testEnvelope();
    testNoise();
    testDCBlocker();
    testAudioBuffer();
    testWavRoundTrip();
    testProcessors();
    testNoteConversion();

    return test::summary();
}
