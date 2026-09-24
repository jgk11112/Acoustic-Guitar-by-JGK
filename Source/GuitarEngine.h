#pragma once
#include <JuceHeader.h>
#include <array>
#include <vector>
#include <set>
#include <random>

class GuitarEngine
{
public:
    void prepare(double newSampleRate, int maxBlockSize);
    void reset();

    void setCapo(int semitones) { capo = juce::jlimit(0, 12, semitones); }
    void setStrumMs(float ms) { strumMs = juce::jlimit(4.0f, 120.0f, ms); }
    void setHumanize(float amount) { humanize = juce::jlimit(0.0f, 1.0f, amount); }
    void setTone(float amount) { tone = juce::jlimit(0.0f, 1.0f, amount); }
    void setRoom(float amount) { room = juce::jlimit(0.0f, 1.0f, amount); }
    void setOutputDb(float db) { outputDb = juce::jlimit(-24.0f, 6.0f, db); }
    void setPalmMute(float amount) { palmMute = juce::jlimit(0.0f, 1.0f, amount); }

    void noteOn(int midiNote, float velocity, int sampleOffset);
    void noteOff(int midiNote, int sampleOffset);
    void process(juce::AudioBuffer<float>& buffer, int numSamples);

private:
    struct GuitarSample
    {
        int rootMidi = 60;
        double sourceRate = 44100.0;
        juce::AudioBuffer<float> audio;
    };

    struct PluckedString
    {
        void prepare(double sr, int maxDelaySamples);
        void trigger(float frequency,
                     float velocity,
                     float brightness,
                     float palmAmount,
                     int stringIndex,
                     std::mt19937& rng,
                     const GuitarSample* realSample,
                     int targetMidi);
        float process(float toneAmount);
        void choke(float damping);
        void reset();

        // Physical model fallback/layer.
        std::vector<float> delay;
        int writePos = 0;
        int delaySamples = 2;
        float feedback = 0.995f;
        float prev = 0.0f;
        float modelGain = 0.0f;
        float chokeMul = 1.0f;

        // Real-sample playback state.
        const GuitarSample* sample = nullptr;
        double samplePosition = 0.0;
        double sampleStep = 1.0;
        float sampleGain = 0.0f;
        float sampleEnvelope = 0.0f;
        float releaseCoefficient = 1.0f;
        float palmCoefficient = 1.0f;
        float sampleLowpass = 0.0f;
        float palmAmount = 0.0f;
        bool sampleActive = false;

        double sampleRate = 44100.0;
    };

    struct PendingPluck
    {
        int countdown = 0;
        int stringIndex = 0;
        int midiNote = 60;
        float velocity = 0.8f;
    };

    struct ChordInfo
    {
        int rootPc = 0;
        enum Quality { major, minor, dominant7, major7, minor7, sus2, sus4, power, unknown } quality = unknown;
        std::array<int, 6> frets { -1, -1, -1, -1, -1, -1 };
        bool valid = false;
    };

    void loadRealSamples();
    const GuitarSample* findNearestSample(int targetMidi) const;
    ChordInfo detectChord() const;
    std::array<int, 6> buildVoicing(const ChordInfo& chord) const;
    void scheduleChord(float velocity, int initialOffset);
    void chokeAll();
    int stringOpenMidi(int stringIndex) const;

    double sampleRate = 44100.0;
    int capo = 0;
    float strumMs = 28.0f;
    float humanize = 0.18f;
    float tone = 0.62f;
    float room = 0.14f;
    float outputDb = -6.0f;
    float palmMute = 0.0f;

    std::array<PluckedString, 6> strings;
    std::vector<PendingPluck> pending;
    std::set<int> heldNotes;
    float lastVelocity = 0.8f;
    std::vector<GuitarSample> realSamples;

    std::mt19937 rng { 0x4A474B31u };
    juce::Reverb reverb;
    juce::dsp::IIR::Filter<float> bodyFilterL, bodyFilterR;
};
