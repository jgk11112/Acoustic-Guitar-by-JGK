#include "GuitarEngine.h"
#include <MartinHD28Data.h>
#include <cmath>
#include <algorithm>

namespace
{
    constexpr int openStrings[6] = { 40, 45, 50, 55, 59, 64 }; // E2 A2 D3 G3 B3 E4
    int pc(int n) { return ((n % 12) + 12) % 12; }

    bool contains(const std::set<int>& pcs, int notePc)
    {
        return pcs.find(pc(notePc)) != pcs.end();
    }

    float releaseCoefficientFor(double sampleRate, float seconds)
    {
        const float safeSeconds = juce::jmax(0.004f, seconds);
        return std::exp(std::log(0.001f) / (safeSeconds * (float) sampleRate));
    }
}

void GuitarEngine::PluckedString::prepare(double sr, int maxDelaySamples)
{
    sampleRate = sr;
    delay.assign((size_t) juce::jmax(8, maxDelaySamples), 0.0f);
    reset();
}

void GuitarEngine::PluckedString::reset()
{
    std::fill(delay.begin(), delay.end(), 0.0f);
    writePos = 0;
    prev = 0.0f;
    modelGain = 0.0f;
    chokeMul = 1.0f;

    sample = nullptr;
    samplePosition = 0.0;
    sampleStep = 1.0;
    sampleGain = 0.0f;
    sampleEnvelope = 0.0f;
    releaseCoefficient = 1.0f;
    palmCoefficient = 1.0f;
    sampleLowpass = 0.0f;
    palmAmount = 0.0f;
    sampleActive = false;
}

void GuitarEngine::PluckedString::trigger(float frequency,
                                          float velocity,
                                          float brightness,
                                          float palm,
                                          int stringIndex,
                                          std::mt19937& rng,
                                          const GuitarSample* realSample,
                                          int targetMidi)
{
    if (frequency <= 0.0f || delay.empty())
        return;

    palmAmount = juce::jlimit(0.0f, 1.0f, palm);
    const float velocityCurve = std::pow(juce::jlimit(0.0f, 1.0f, velocity), 0.72f);

    // -------------------------------------------------------------------------
    // Real CC0 Martin HD28 sample. The nearest recorded pitch is resampled by a
    // small ratio, so each string still follows the selected guitar voicing.
    // -------------------------------------------------------------------------
    sample = realSample;
    samplePosition = 0.0;
    sampleEnvelope = 1.0f;
    releaseCoefficient = 1.0f;
    sampleLowpass = 0.0f;
    sampleActive = (sample != nullptr && sample->audio.getNumSamples() > 1);

    if (sampleActive)
    {
        const double semitones = (double) targetMidi - (double) sample->rootMidi;
        sampleStep = (sample->sourceRate / sampleRate) * std::pow(2.0, semitones / 12.0);

        std::uniform_real_distribution<float> tiny(-1.0f, 1.0f);
        const float stringGain = 0.92f + 0.018f * (float) stringIndex;
        sampleGain = velocityCurve * stringGain * (1.0f + tiny(rng) * 0.018f);

        // Palm mute remains a real sample articulation at the source, then is
        // shaped with a fast acoustic-style damping envelope and darker top end.
        const float palmSeconds = juce::jmap(palmAmount, 0.0f, 1.0f, 1.20f, 0.075f);
        palmCoefficient = palmAmount < 0.01f ? 1.0f : releaseCoefficientFor(sampleRate, palmSeconds);
    }

    // -------------------------------------------------------------------------
    // Very quiet model layer. It gives tiny string-to-string differences and
    // fills any pitch gaps, but the recorded Martin sample is the main sound.
    // -------------------------------------------------------------------------
    delaySamples = juce::jlimit(2, (int) delay.size() - 1, (int) std::round(sampleRate / frequency));
    writePos = 0;

    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    const float naturalFeedback = 0.9935f + 0.0045f * brightness;
    feedback = (naturalFeedback - 0.050f * palmAmount) * (1.0f + dist(rng) * 0.0010f);
    chokeMul = 1.0f;
    modelGain = sampleActive ? velocityCurve * 0.055f : velocityCurve * 0.85f;

    float lastNoise = 0.0f;
    const float excitationSmooth = 0.08f + 0.72f * palmAmount;
    const float pickPosition = 0.16f + 0.035f * (float) stringIndex;
    for (int i = 0; i < delaySamples; ++i)
    {
        const float white = dist(rng);
        lastNoise += (white - lastNoise) * (1.0f - excitationSmooth);
        const float phase = (float) i / (float) delaySamples;
        const float notch = 0.72f + 0.28f * std::abs(std::sin(juce::MathConstants<float>::pi * phase / pickPosition));
        const float taper = 0.68f + 0.32f * std::sin(juce::MathConstants<float>::pi * phase);
        delay[(size_t) i] = lastNoise * notch * taper * modelGain;
    }
}

float GuitarEngine::PluckedString::process(float toneAmount)
{
    float real = 0.0f;

    if (sampleActive && sample != nullptr)
    {
        const auto& b = sample->audio;
        const int length = b.getNumSamples();
        const int i0 = (int) samplePosition;

        if (i0 >= 0 && i0 < length - 1)
        {
            const int i1 = i0 + 1;
            const float frac = (float) (samplePosition - (double) i0);
            const float a = b.getSample(0, i0);
            const float c = b.getSample(0, i1);
            const float raw = a + (c - a) * frac;
            samplePosition += sampleStep;

            // Tone and palm-mute-aware one-pole low-pass. At 0 palm mute the
            // recording remains bright; at 100% it becomes a tight muted thud.
            const float cutoff = juce::jlimit(1200.0f, 15000.0f,
                2200.0f + 12500.0f * juce::jlimit(0.0f, 1.0f, toneAmount) * (1.0f - 0.72f * palmAmount));
            const float pole = std::exp(-2.0f * juce::MathConstants<float>::pi * cutoff / (float) sampleRate);
            sampleLowpass = (1.0f - pole) * raw + pole * sampleLowpass;

            sampleEnvelope *= releaseCoefficient;
            sampleEnvelope *= palmCoefficient;
            real = sampleLowpass * sampleGain * sampleEnvelope;

            if (sampleEnvelope < 0.0002f)
                sampleActive = false;
        }
        else
        {
            sampleActive = false;
        }
    }

    // Quiet physical string layer/fallback.
    float model = 0.0f;
    if (!delay.empty())
    {
        int readPos = writePos - delaySamples;
        if (readPos < 0)
            readPos += (int) delay.size();

        const float current = delay[(size_t) readPos];
        const float smoothing = 0.30f + 0.55f * (1.0f - toneAmount);
        const float filtered = (1.0f - smoothing) * current + smoothing * prev;
        prev = filtered;

        delay[(size_t) writePos] = filtered * feedback * chokeMul;
        writePos = (writePos + 1) % (int) delay.size();
        chokeMul = juce::jmax(0.84f, chokeMul * 0.99998f);
        model = current * 0.25f;
    }

    return real + model;
}

void GuitarEngine::PluckedString::choke(float damping)
{
    const float d = juce::jlimit(0.0f, 1.0f, damping);
    // About 12-35 ms depending on how hard the strings are damped.
    releaseCoefficient = releaseCoefficientFor(sampleRate, juce::jmap(d, 0.0f, 1.0f, 0.035f, 0.012f));
    chokeMul = 0.55f - 0.50f * d;
    feedback *= (0.70f - 0.40f * d);
}

void GuitarEngine::loadRealSamples()
{
    realSamples.clear();

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    for (int i = 0; i < MartinHD28Data::namedResourceListSize; ++i)
    {
        const juce::String resourceName(MartinHD28Data::namedResourceList[i]);
        const int marker = resourceName.indexOf("MartinGM2_");
        if (marker < 0)
            continue;

        const juce::String midiText = resourceName.substring(marker + 10, marker + 13);
        const int rootMidi = midiText.getIntValue();
        if (rootMidi <= 0)
            continue;

        int dataSize = 0;
        const char* data = MartinHD28Data::getNamedResource(resourceName.toRawUTF8(), dataSize);
        if (data == nullptr || dataSize <= 0)
            continue;

        auto stream = std::make_unique<juce::MemoryInputStream>(data, (size_t) dataSize, false);
        std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(std::move(stream)));
        if (reader == nullptr || reader->lengthInSamples <= 1)
            continue;

        GuitarSample entry;
        entry.rootMidi = rootMidi;
        entry.sourceRate = reader->sampleRate;
        entry.audio.setSize(1, (int) reader->lengthInSamples);
        reader->read(&entry.audio, 0, entry.audio.getNumSamples(), 0, true, false);
        realSamples.push_back(std::move(entry));
    }

    std::sort(realSamples.begin(), realSamples.end(), [] (const GuitarSample& a, const GuitarSample& b)
    {
        return a.rootMidi < b.rootMidi;
    });
}

const GuitarEngine::GuitarSample* GuitarEngine::findNearestSample(int targetMidi) const
{
    if (realSamples.empty())
        return nullptr;

    const GuitarSample* best = &realSamples.front();
    int bestDistance = std::abs(targetMidi - best->rootMidi);

    for (const auto& s : realSamples)
    {
        const int distance = std::abs(targetMidi - s.rootMidi);
        if (distance < bestDistance)
        {
            best = &s;
            bestDistance = distance;
        }
    }
    return best;
}

void GuitarEngine::prepare(double newSampleRate, int)
{
    sampleRate = newSampleRate;
    const int maxDelay = (int) std::ceil(sampleRate / 55.0) + 8;
    for (auto& s : strings)
        s.prepare(sampleRate, maxDelay);

    loadRealSamples();

    reverb.setSampleRate(sampleRate);

    juce::Reverb::Parameters p;
    p.roomSize = room;
    p.damping = 0.42f;
    p.wetLevel = room * 0.25f;
    p.dryLevel = 1.0f;
    p.width = 0.78f;
    p.freezeMode = 0.0f;
    reverb.setParameters(p);

    // Trim a little dreadnought boom so the HD28 samples sit closer to the
    // tight, intimate small-bodied singer-songwriter sound we are aiming for.
    auto coeff = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
        sampleRate, 175.0, 0.72, juce::Decibels::decibelsToGain(-2.4f));
    bodyFilterL.coefficients = coeff;
    bodyFilterR.coefficients = coeff;
}

void GuitarEngine::reset()
{
    for (auto& s : strings)
        s.reset();
    pending.clear();
    heldNotes.clear();
    reverb.reset();
}

void GuitarEngine::noteOn(int midiNote, float velocity, int)
{
    heldNotes.insert(midiNote);
    lastVelocity = juce::jlimit(0.05f, 1.0f, velocity);

    // FL Studio normally sends all notes of a piano-roll chord at one sample
    // position. Each added note replaces the pending strum, so only the final,
    // complete chord is heard when rendering resumes.
    scheduleChord(lastVelocity, 0);
}

void GuitarEngine::noteOff(int midiNote, int)
{
    heldNotes.erase(midiNote);
    if (heldNotes.empty())
        chokeAll();
}

GuitarEngine::ChordInfo GuitarEngine::detectChord() const
{
    ChordInfo result;
    if (heldNotes.empty())
        return result;

    std::set<int> pcs;
    for (int n : heldNotes)
        pcs.insert(pc(n));

    const int lowestPc = pc(*heldNotes.begin());
    std::array<int, 12> roots {};
    for (int i = 0; i < 12; ++i)
        roots[(size_t) i] = (lowestPc + i) % 12;

    struct Pattern { ChordInfo::Quality q; std::vector<int> intervals; };
    const std::vector<Pattern> patterns = {
        { ChordInfo::major,     {0,4,7} },
        { ChordInfo::minor,     {0,3,7} },
        { ChordInfo::dominant7, {0,4,7,10} },
        { ChordInfo::major7,    {0,4,7,11} },
        { ChordInfo::minor7,    {0,3,7,10} },
        { ChordInfo::sus2,      {0,2,7} },
        { ChordInfo::sus4,      {0,5,7} },
        { ChordInfo::power,     {0,7} }
    };

    int bestScore = -999;
    for (int root : roots)
    {
        for (const auto& pattern : patterns)
        {
            int matches = 0;
            for (int iv : pattern.intervals)
                if (contains(pcs, root + iv))
                    ++matches;

            int extras = 0;
            for (int p : pcs)
            {
                bool inPattern = false;
                for (int iv : pattern.intervals)
                    if (p == pc(root + iv))
                        inPattern = true;
                if (!inPattern)
                    ++extras;
            }

            int score = matches * 4 - extras * 2 - (int) pattern.intervals.size();
            if (root == lowestPc)
                score += 2;

            if (matches == (int) pattern.intervals.size() && score > bestScore)
            {
                bestScore = score;
                result.rootPc = root;
                result.quality = pattern.q;
                result.valid = true;
            }
        }
    }
    return result;
}

std::array<int, 6> GuitarEngine::buildVoicing(const ChordInfo& chord) const
{
    // Shape-relative frets. Capo is applied to the sounding pitch later.
    struct Shape { int root; ChordInfo::Quality q; std::array<int,6> f; };
    static const std::vector<Shape> shapes = {
        {0, ChordInfo::major, { -1,3,2,0,1,0 }}, // C
        {0, ChordInfo::minor, { -1,3,5,5,4,3 }},
        {2, ChordInfo::major, { -1,-1,0,2,3,2 }}, // D
        {2, ChordInfo::minor, { -1,-1,0,2,3,1 }},
        {4, ChordInfo::major, { 0,2,2,1,0,0 }}, // E
        {4, ChordInfo::minor, { 0,2,2,0,0,0 }},
        {5, ChordInfo::major, { 1,3,3,2,1,1 }}, // F
        {5, ChordInfo::minor, { 1,3,3,1,1,1 }},
        {7, ChordInfo::major, { 3,2,0,0,0,3 }}, // G
        {7, ChordInfo::minor, { 3,5,5,3,3,3 }},
        {9, ChordInfo::major, { -1,0,2,2,2,0 }}, // A
        {9, ChordInfo::minor, { -1,0,2,2,1,0 }},
        {11,ChordInfo::major, { -1,2,4,4,4,2 }}, // B
        {11,ChordInfo::minor, { -1,2,4,4,3,2 }},
        {7, ChordInfo::dominant7, {3,2,0,0,0,1}},
        {2, ChordInfo::sus2, {-1,-1,0,2,3,0}},
        {2, ChordInfo::sus4, {-1,-1,0,2,3,3}}
    };

    for (const auto& s : shapes)
        if (s.root == chord.rootPc && s.q == chord.quality)
            return s.f;

    int rootFret = (chord.rootPc - pc(openStrings[0]) + 12) % 12;
    if (rootFret == 0)
        rootFret = 12;

    std::array<int,6> f = { rootFret, rootFret + 2, rootFret + 2, rootFret + 1, rootFret, rootFret };
    if (chord.quality == ChordInfo::minor || chord.quality == ChordInfo::minor7)
        f[3] = rootFret;
    if (chord.quality == ChordInfo::power)
        f[3] = f[4] = f[5] = -1;
    return f;
}

void GuitarEngine::scheduleChord(float velocity, int initialOffset)
{
    const auto chord = detectChord();
    if (!chord.valid)
        return;

    pending.clear();
    const auto frets = buildVoicing(chord);

    std::uniform_real_distribution<float> rand01(-1.0f, 1.0f);
    const float baseStep = strumMs * 0.001f * (float) sampleRate / 5.0f;

    for (int s = 0; s < 6; ++s)
    {
        if (frets[(size_t) s] < 0)
            continue;

        const float jitter = rand01(rng) * humanize * baseStep * 0.7f;
        const int offset = initialOffset + (int) std::round((float) s * baseStep + jitter);
        const int soundingMidi = openStrings[s] + frets[(size_t) s] + capo;
        const float vj = velocity * (1.0f + rand01(rng) * humanize * 0.10f);
        pending.push_back({ juce::jmax(0, offset), s, soundingMidi, juce::jlimit(0.05f, 1.0f, vj) });
    }
}

void GuitarEngine::chokeAll()
{
    for (auto& s : strings)
        s.choke(0.58f + 0.42f * palmMute);
    pending.clear();
}

int GuitarEngine::stringOpenMidi(int stringIndex) const
{
    return openStrings[juce::jlimit(0, 5, stringIndex)];
}

void GuitarEngine::process(juce::AudioBuffer<float>& buffer, int numSamples)
{
    buffer.clear();
    auto* left = buffer.getWritePointer(0);
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;

    auto rp = reverb.getParameters();
    rp.roomSize = 0.12f + 0.58f * room;
    rp.wetLevel = room * 0.24f;
    rp.damping = 0.48f;
    rp.width = 0.82f;
    reverb.setParameters(rp);

    for (int i = 0; i < numSamples; ++i)
    {
        for (auto it = pending.begin(); it != pending.end();)
        {
            if (it->countdown <= 0)
            {
                const float hz = 440.0f * std::pow(2.0f, (it->midiNote - 69) / 12.0f);
                strings[(size_t) it->stringIndex].trigger(
                    hz,
                    it->velocity,
                    tone,
                    palmMute,
                    it->stringIndex,
                    rng,
                    findNearestSample(it->midiNote),
                    it->midiNote);
                it = pending.erase(it);
            }
            else
            {
                --it->countdown;
                ++it;
            }
        }

        float l = 0.0f;
        float r = 0.0f;
        static constexpr float stringPan[6] = { -0.20f, -0.12f, -0.05f, 0.05f, 0.12f, 0.20f };

        for (int s = 0; s < 6; ++s)
        {
            const float value = strings[(size_t) s].process(
                juce::jlimit(0.0f, 1.0f, tone - palmMute * 0.14f));
            const float pan = stringPan[s];
            l += value * (0.70f - pan * 0.22f);
            r += value * (0.70f + pan * 0.22f);
        }

        const float outGain = juce::Decibels::decibelsToGain(outputDb);
        l *= outGain;
        r *= outGain;

        left[i] = bodyFilterL.processSample(l);
        if (right != nullptr)
            right[i] = bodyFilterR.processSample(r);
    }

    if (right != nullptr)
        reverb.processStereo(left, right, numSamples);
    else
        reverb.processMono(left, numSamples);
}
