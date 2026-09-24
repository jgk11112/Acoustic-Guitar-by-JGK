#include "PluginProcessor.h"
#include <juce_audio_utils/juce_audio_utils.h>

AcousticGuitarByJGKAudioProcessor::AcousticGuitarByJGKAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMS", createLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout AcousticGuitarByJGKAudioProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add(std::make_unique<juce::AudioParameterInt>("capo", "Capo", 0, 12, 0));
    layout.add(std::make_unique<juce::AudioParameterFloat>("strum", "Strum", juce::NormalisableRange<float>(0.0f, 120.0f, 0.1f), 28.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("humanize", "Humanize", 0.0f, 1.0f, 0.15f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("tone", "Tone", 0.0f, 1.0f, 0.65f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("room", "Room", 0.0f, 1.0f, 0.12f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("mute", "Palm Mute", 0.0f, 1.0f, 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("output", "Output", juce::NormalisableRange<float>(-24.0f, 6.0f, 0.1f), -6.0f));
    return layout;
}

void AcousticGuitarByJGKAudioProcessor::prepareToPlay(double sr, int)
{
    engine.prepare(sr);
}

bool AcousticGuitarByJGKAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto set = layouts.getMainOutputChannelSet();
    return set == juce::AudioChannelSet::mono() || set == juce::AudioChannelSet::stereo();
}

void AcousticGuitarByJGKAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    engine.setCapo((int) *apvts.getRawParameterValue("capo"));
    engine.setStrumMs(*apvts.getRawParameterValue("strum"));
    engine.setHumanize(*apvts.getRawParameterValue("humanize"));
    engine.setTone(*apvts.getRawParameterValue("tone"));
    engine.setRoom(*apvts.getRawParameterValue("room"));
    engine.setPalmMute(*apvts.getRawParameterValue("mute"));
    engine.setOutputDb(*apvts.getRawParameterValue("output"));

    int rendered = 0;
    auto renderSlice = [&] (int start, int count)
    {
        if (count <= 0)
            return;
        juce::AudioBuffer<float> slice(buffer.getArrayOfWritePointers(), buffer.getNumChannels(), start, count);
        engine.process(slice, count);
    };

    auto it = midi.begin();
    while (it != midi.end())
    {
        const int eventPos = juce::jlimit(0, buffer.getNumSamples(), (*it).samplePosition);
        renderSlice(rendered, eventPos - rendered);
        rendered = eventPos;

        std::vector<std::pair<int, float>> noteOns;
        std::vector<int> noteOffs;

        while (it != midi.end() && (*it).samplePosition == eventPos)
        {
            const auto msg = (*it).getMessage();
            if (msg.isNoteOn())
                noteOns.emplace_back(msg.getNoteNumber(), msg.getFloatVelocity());
            else if (msg.isNoteOff())
                noteOffs.push_back(msg.getNoteNumber());
            else if (msg.isAllNotesOff() || msg.isAllSoundOff())
                engine.reset();
            ++it;
        }

        if (!noteOns.empty())
            engine.strumChord(noteOns);
        for (const int note : noteOffs)
            engine.noteOff(note);
    }

    renderSlice(rendered, buffer.getNumSamples() - rendered);
}

juce::AudioProcessorEditor* AcousticGuitarByJGKAudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor(*this);
}

void AcousticGuitarByJGKAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void AcousticGuitarByJGKAudioProcessor::setStateInformation(const void* data, int size)
{
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AcousticGuitarByJGKAudioProcessor();
}
