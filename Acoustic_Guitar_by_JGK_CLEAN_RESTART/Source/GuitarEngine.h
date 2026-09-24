#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <set>
#include <vector>
#include <random>

class GuitarEngine
{
public:
    void prepare(double newSampleRate);
    void reset();

    void setCapo(int value)       { capo = juce::jlimit(0, 12, value); }
    void setStrumMs(float value)  { strumMs = juce::jlimit(0.0f, 120.0f, value); }
    void setHumanize(float value) { humanize = juce::jlimit(0.0f, 1.0f, value); }
    void setTone(float value)     { tone = juce::jlimit(0.0f, 1.0f, value); }
    void setRoom(float value)     { room = juce::jlimit(0.0f, 1.0f, value); }
    void setPalmMute(float value) { palmMute = juce::jlimit(0.0f, 1.0f, value); }
    void setOutputDb(float value) { outputDb = juce::jlimit(-24.0f, 6.0f, value); }

    void strumChord(const std::vector<std::pair<int, float>>& notes);
    void noteOff(int midiNote);
    void process(juce::AudioBuffer<float>& buffer, int numSamples);

private:
    struct GuitarSample
    {
        int rootMidi = 60;
        double sampleRate = 44100.0;
        juce::AudioBuffer<float> audio;
    };

    struct Voice
    {
        const GuitarSample* sample = nullptr;
        double position = 0.0;
        double step = 1.0;
        float gain = 0.0f;
        float envelope = 1.0f;
        float releaseMul = 1.0f;
        float palmMul = 1.0f;
        float lowpass = 0.0f;
        float pan = 0.0f;
        int delaySamples = 0;
        bool active = false;
    };

    void loadSamples();
    const GuitarSample* nearestSample(int midiNote) const;
    Voice& getVoice();
    void startVoice(int midiNote, float velocity, int delaySamples, float pan);
    void releaseAll(float seconds);
    static float coefficientForSeconds(double sampleRate, float seconds);

    double sampleRate = 44100.0;
    int capo = 0;
    float strumMs = 28.0f;
    float humanize = 0.15f;
    float tone = 0.65f;
    float room = 0.12f;
    float palmMute = 0.0f;
    float outputDb = -6.0f;

    std::vector<GuitarSample> samples;
    std::array<Voice, 32> voices;
    std::set<int> heldNotes;
    std::mt19937 rng { 0x4A474B31u };
    juce::Reverb reverb;
};
