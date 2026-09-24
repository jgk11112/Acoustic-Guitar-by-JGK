#include "GuitarEngine.h"
#include <cmath>

namespace
{
    constexpr int openStrings[6] = { 40, 45, 50, 55, 59, 64 }; // E2 A2 D3 G3 B3 E4
    int pc(int n) { return ((n % 12) + 12) % 12; }

    bool contains(const std::set<int>& pcs, int notePc)
    {
        return pcs.find(pc(notePc)) != pcs.end();
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
    gain = 0.0f;
    chokeMul = 1.0f;
}

void GuitarEngine::PluckedString::trigger(float frequency, float velocity, float brightness, float palmAmount, int stringIndex, std::mt19937& rng)
{
    if (frequency <= 0.0f || delay.empty()) return;
    delaySamples = juce::jlimit(2, (int) delay.size() - 1, (int) std::round(sampleRate / frequency));
    writePos = 0;

    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    const float velocityCurve = std::pow(juce::jlimit(0.0f, 1.0f, velocity), 0.72f);
    const float stringGain = 0.90f + 0.035f * (float) stringIndex;
    gain = juce::jlimit(0.0f, 1.25f, velocityCurve * stringGain);

    // Open strings sustain naturally; palm muting shortens the decay and darkens the excitation.
    const float naturalFeedback = 0.9935f + 0.0048f * brightness;
    feedback = naturalFeedback - 0.050f * palmAmount;
    feedback *= 1.0f + dist(rng) * 0.0012f; // tiny round-robin-like decay variation
    chokeMul = 1.0f;

    float lastNoise = 0.0f;
    const float excitationSmooth = 0.08f + 0.72f * palmAmount;
    const float pickPosition = 0.16f + 0.035f * (float) stringIndex;
    for (int i = 0; i < delaySamples; ++i)
    {
        const float white = dist(rng);
        lastNoise += (white - lastNoise) * (1.0f - excitationSmooth);

        // A simple pick-position notch makes the attack less like raw white noise.
        const float phase = (float) i / (float) delaySamples;
        const float notch = 0.72f + 0.28f * std::abs(std::sin(juce::MathConstants<float>::pi * phase / pickPosition));
        const float taper = 0.68f + 0.32f * std::sin(juce::MathConstants<float>::pi * phase);
        const float mutedLevel = 1.0f - 0.34f * palmAmount;
        delay[(size_t) i] = lastNoise * notch * taper * gain * mutedLevel;
    }
}

float GuitarEngine::PluckedString::process(float toneAmount)
{
    if (delay.empty()) return 0.0f;
    int readPos = writePos - delaySamples;
    if (readPos < 0) readPos += (int) delay.size();

    float current = delay[(size_t) readPos];
    float smoothing = 0.30f + 0.55f * (1.0f - toneAmount);
    float filtered = (1.0f - smoothing) * current + smoothing * prev;
    prev = filtered;

    delay[(size_t) writePos] = filtered * feedback * chokeMul;
    writePos = (writePos + 1) % (int) delay.size();

    chokeMul = juce::jmax(0.84f, chokeMul * 0.99998f);
    return current * 0.25f;
}

void GuitarEngine::PluckedString::choke(float damping)
{
    chokeMul = 0.55f - 0.50f * damping;
    feedback *= (0.70f - 0.40f * damping);
}

void GuitarEngine::prepare(double newSampleRate, int)
{
    sampleRate = newSampleRate;
    int maxDelay = (int) std::ceil(sampleRate / 55.0) + 8;
    for (auto& s : strings) s.prepare(sampleRate, maxDelay);

    reverb.setSampleRate(sampleRate);

    juce::Reverb::Parameters p;
    p.roomSize = room;
    p.damping = 0.35f;
    p.wetLevel = room * 0.28f;
    p.dryLevel = 1.0f;
    p.width = 0.8f;
    p.freezeMode = 0.0f;
    reverb.setParameters(p);

    auto coeff = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, 190.0, 0.8, juce::Decibels::decibelsToGain(2.0f));
    bodyFilterL.coefficients = coeff;
    bodyFilterR.coefficients = coeff;
}

void GuitarEngine::reset()
{
    for (auto& s : strings) s.reset();
    pending.clear();
    heldNotes.clear();
    reverb.reset();
}

void GuitarEngine::noteOn(int midiNote, float velocity, int)
{
    bool wasEmpty = heldNotes.empty();
    heldNotes.insert(midiNote);
    lastVelocity = juce::jlimit(0.05f, 1.0f, velocity);

    // For piano-roll chords, every note-on arrives at the same sample position.
    // Rebuild the chord on each added note. Pending events are replaced so only
    // the most complete chord at that instant is heard.
    if (!wasEmpty || heldNotes.size() == 1)
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
    if (heldNotes.empty()) return result;

    std::set<int> pcs;
    for (int n : heldNotes) pcs.insert(pc(n));

    // Lowest MIDI note is used as the preferred root when possible.
    int lowestPc = pc(*heldNotes.begin());
    std::array<int, 12> roots {};
    for (int i = 0; i < 12; ++i) roots[(size_t) i] = (lowestPc + i) % 12;

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
                if (contains(pcs, root + iv)) ++matches;

            int extras = 0;
            for (int p : pcs)
            {
                bool inPattern = false;
                for (int iv : pattern.intervals) if (p == pc(root + iv)) inPattern = true;
                if (!inPattern) ++extras;
            }

            int score = matches * 4 - extras * 2 - (int) pattern.intervals.size();
            if (root == lowestPc) score += 2;
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
    // Frets are shape-relative. Capo is added later to the sounding pitch.
    // Familiar open-position guitar shapes where possible, otherwise a compact barre.
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

    // Generic E-shape style movable voicing. This is intentionally guitar-like,
    // not a keyboard stack: root/fifth/third are distributed across six strings.
    int rootFret = (chord.rootPc - pc(openStrings[0]) + 12) % 12;
    if (rootFret == 0) rootFret = 12;
    std::array<int,6> f = { rootFret, rootFret + 2, rootFret + 2, rootFret + 1, rootFret, rootFret };
    if (chord.quality == ChordInfo::minor || chord.quality == ChordInfo::minor7)
        f[3] = rootFret;
    if (chord.quality == ChordInfo::power)
        f[3] = f[4] = f[5] = -1;
    return f;
}

void GuitarEngine::scheduleChord(float velocity, int initialOffset)
{
    auto chord = detectChord();
    if (!chord.valid) return;

    pending.clear();
    auto frets = buildVoicing(chord);

    std::uniform_real_distribution<float> rand01(-1.0f, 1.0f);
    const float baseStep = strumMs * 0.001f * (float) sampleRate / 5.0f;

    for (int s = 0; s < 6; ++s)
    {
        if (frets[(size_t) s] < 0) continue;
        float jitter = rand01(rng) * humanize * baseStep * 0.7f;
        int offset = initialOffset + (int) std::round((float) s * baseStep + jitter);

        int soundingMidi = openStrings[s] + frets[(size_t) s] + capo;
        float vj = velocity * (1.0f + rand01(rng) * humanize * 0.10f);
        pending.push_back({ juce::jmax(0, offset), s, soundingMidi, juce::jlimit(0.05f, 1.0f, vj) });
    }
}

void GuitarEngine::chokeAll()
{
    for (auto& s : strings) s.choke(0.58f + 0.42f * palmMute);
    pending.clear();
}

int GuitarEngine::stringOpenMidi(int stringIndex) const
{
    return openStrings[juce::jlimit(0,5,stringIndex)];
}

void GuitarEngine::process(juce::AudioBuffer<float>& buffer, int numSamples)
{
    buffer.clear();
    auto* left = buffer.getWritePointer(0);
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;

    juce::Reverb::Parameters rp = reverb.getParameters();
    rp.roomSize = 0.15f + 0.65f * room;
    rp.wetLevel = room * 0.28f;
    rp.damping = 0.45f;
    rp.width = 0.85f;
    reverb.setParameters(rp);

    for (int i = 0; i < numSamples; ++i)
    {
        for (auto it = pending.begin(); it != pending.end();)
        {
            if (it->countdown <= 0)
            {
                float hz = 440.0f * std::pow(2.0f, (it->midiNote - 69) / 12.0f);
                strings[(size_t) it->stringIndex].trigger(hz, it->velocity, tone, palmMute, it->stringIndex, rng);
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

        // Keep the six strings subtly spread like a close stereo acoustic recording.
        for (int s = 0; s < 6; ++s)
        {
            const float sample = strings[(size_t) s].process(juce::jlimit(0.0f, 1.0f, tone - palmMute * 0.18f));
            const float pan = stringPan[s];
            l += sample * (0.70f - pan * 0.22f);
            r += sample * (0.70f + pan * 0.22f);
        }

        const float outGain = juce::Decibels::decibelsToGain(outputDb);
        l *= outGain;
        r *= outGain;

        // Small-body acoustic focus: tight lows, clear mids, modest stereo room.
        left[i] = bodyFilterL.processSample(l);
        if (right) right[i] = bodyFilterR.processSample(r);
    }

    if (right)
        reverb.processStereo(left, right, numSamples);
    else
        reverb.processMono(left, numSamples);
}
