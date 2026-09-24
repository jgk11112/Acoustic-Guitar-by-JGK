#include "GuitarEngine.h"
#include <BinaryData.h>
#include <algorithm>
#include <cmath>

float GuitarEngine::coefficientForSeconds(double sr, float seconds)
{
    const auto safe = juce::jmax(0.004f, seconds);
    return std::exp(std::log(0.001f) / (safe * (float) sr));
}

void GuitarEngine::prepare(double newSampleRate)
{
    sampleRate = newSampleRate;
    loadSamples();
    reverb.setSampleRate(sampleRate);
    reset();
}

void GuitarEngine::reset()
{
    for (auto& v : voices)
        v = {};
    heldNotes.clear();
    reverb.reset();
}

void GuitarEngine::loadSamples()
{
    samples.clear();

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
    {
        const juce::String resourceName(BinaryData::namedResourceList[i]);
        const int marker = resourceName.indexOf("MartinGM2_");
        if (marker < 0)
            continue;

        const auto midiText = resourceName.substring(marker + 10, marker + 13);
        const int root = midiText.getIntValue();
        if (root <= 0)
            continue;

        int dataSize = 0;
        const char* data = BinaryData::getNamedResource(resourceName.toRawUTF8(), dataSize);
        if (data == nullptr || dataSize <= 0)
            continue;

        auto input = std::make_unique<juce::MemoryInputStream>(data, (size_t) dataSize, false);
        std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(std::move(input)));
        if (reader == nullptr || reader->lengthInSamples < 2)
            continue;

        GuitarSample s;
        s.rootMidi = root;
        s.sampleRate = reader->sampleRate;
        s.audio.setSize(1, (int) reader->lengthInSamples);
        reader->read(&s.audio, 0, s.audio.getNumSamples(), 0, true, false);
        samples.push_back(std::move(s));
    }

    std::sort(samples.begin(), samples.end(), [] (const auto& a, const auto& b)
    {
        return a.rootMidi < b.rootMidi;
    });
}

const GuitarEngine::GuitarSample* GuitarEngine::nearestSample(int midiNote) const
{
    if (samples.empty())
        return nullptr;

    const GuitarSample* best = &samples.front();
    int distance = std::abs(midiNote - best->rootMidi);

    for (const auto& s : samples)
    {
        const int d = std::abs(midiNote - s.rootMidi);
        if (d < distance)
        {
            distance = d;
            best = &s;
        }
    }

    return best;
}

GuitarEngine::Voice& GuitarEngine::getVoice()
{
    for (auto& v : voices)
        if (!v.active)
            return v;

    auto* quietest = &voices.front();
    for (auto& v : voices)
        if (v.envelope < quietest->envelope)
            quietest = &v;
    return *quietest;
}

void GuitarEngine::startVoice(int midiNote, float velocity, int delay, float pan)
{
    const int targetMidi = juce::jlimit(24, 100, midiNote + capo);
    const auto* source = nearestSample(targetMidi);
    if (source == nullptr)
        return;

    auto& v = getVoice();
    v = {};
    v.sample = source;
    v.position = 0.0;
    v.step = (source->sampleRate / sampleRate)
           * std::pow(2.0, ((double) targetMidi - (double) source->rootMidi) / 12.0);

    std::uniform_real_distribution<float> variation(-1.0f, 1.0f);
    v.gain = std::pow(juce::jlimit(0.02f, 1.0f, velocity), 0.72f)
           * (1.0f + variation(rng) * 0.025f * humanize);
    v.pan = juce::jlimit(-0.65f, 0.65f, pan + variation(rng) * 0.05f * humanize);
    v.delaySamples = juce::jmax(0, delay);
    v.envelope = 1.0f;
    v.releaseMul = 1.0f;
    v.palmMul = palmMute < 0.01f
        ? 1.0f
        : coefficientForSeconds(sampleRate, juce::jmap(palmMute, 0.0f, 1.0f, 1.1f, 0.07f));
    v.active = true;
}

void GuitarEngine::strumChord(const std::vector<std::pair<int, float>>& inputNotes)
{
    if (inputNotes.empty())
        return;

    std::vector<std::pair<int, float>> notes = inputNotes;
    std::sort(notes.begin(), notes.end(), [] (const auto& a, const auto& b) { return a.first < b.first; });
    if (notes.size() > 6)
        notes.resize(6);

    for (const auto& n : notes)
        heldNotes.insert(n.first);

    const float totalSamples = strumMs * 0.001f * (float) sampleRate;
    const float gap = notes.size() > 1 ? totalSamples / (float) (notes.size() - 1) : 0.0f;
    std::uniform_real_distribution<float> jitter(-1.0f, 1.0f);

    for (size_t i = 0; i < notes.size(); ++i)
    {
        const float humanSamples = humanize * 0.004f * (float) sampleRate * jitter(rng);
        const int delay = juce::jmax(0, (int) std::round((float) i * gap + humanSamples));
        const float pan = notes.size() > 1
            ? juce::jmap((float) i, 0.0f, (float) notes.size() - 1.0f, -0.32f, 0.32f)
            : 0.0f;
        startVoice(notes[i].first, notes[i].second, delay, pan);
    }
}

void GuitarEngine::noteOff(int midiNote)
{
    heldNotes.erase(midiNote);
    if (heldNotes.empty())
        releaseAll(0.025f);
}

void GuitarEngine::releaseAll(float seconds)
{
    const float mul = coefficientForSeconds(sampleRate, seconds);
    for (auto& v : voices)
        if (v.active)
            v.releaseMul = mul;
}

void GuitarEngine::process(juce::AudioBuffer<float>& buffer, int numSamples)
{
    if (buffer.getNumChannels() < 1 || numSamples <= 0)
        return;

    auto* left = buffer.getWritePointer(0);
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : left;

    std::fill(left, left + numSamples, 0.0f);
    if (right != left)
        std::fill(right, right + numSamples, 0.0f);

    for (int n = 0; n < numSamples; ++n)
    {
        float l = 0.0f, r = 0.0f;

        for (auto& v : voices)
        {
            if (!v.active || v.sample == nullptr)
                continue;

            if (v.delaySamples > 0)
            {
                --v.delaySamples;
                continue;
            }

            const auto& audio = v.sample->audio;
            const int i0 = (int) v.position;
            if (i0 < 0 || i0 >= audio.getNumSamples() - 1)
            {
                v.active = false;
                continue;
            }

            const int i1 = i0 + 1;
            const float frac = (float) (v.position - (double) i0);
            const float raw = audio.getSample(0, i0)
                            + (audio.getSample(0, i1) - audio.getSample(0, i0)) * frac;
            v.position += v.step;

            const float cutoff = juce::jlimit(1000.0f, 16000.0f,
                1800.0f + 14000.0f * tone * (1.0f - 0.72f * palmMute));
            const float pole = std::exp(-2.0f * juce::MathConstants<float>::pi * cutoff / (float) sampleRate);
            v.lowpass = (1.0f - pole) * raw + pole * v.lowpass;

            v.envelope *= v.releaseMul;
            v.envelope *= v.palmMul;
            if (v.envelope < 0.00015f)
            {
                v.active = false;
                continue;
            }

            const float s = v.lowpass * v.gain * v.envelope;
            const float pan01 = (v.pan + 1.0f) * 0.5f;
            l += s * std::sqrt(1.0f - pan01);
            r += s * std::sqrt(pan01);
        }

        left[n] += l;
        right[n] += r;
    }

    juce::Reverb::Parameters rp;
    rp.roomSize = 0.12f + room * 0.45f;
    rp.damping = 0.48f;
    rp.wetLevel = room * 0.22f;
    rp.dryLevel = 1.0f;
    rp.width = 0.8f;
    rp.freezeMode = 0.0f;
    reverb.setParameters(rp);

    if (buffer.getNumChannels() > 1)
        reverb.processStereo(left, right, numSamples);
    else
        reverb.processMono(left, numSamples);

    buffer.applyGain(juce::Decibels::decibelsToGain(outputDb));
}
