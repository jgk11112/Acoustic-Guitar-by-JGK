#include "PluginProcessor.h"
#include "PluginEditor.h"

AcousticGuitarByJGKAudioProcessor::AcousticGuitarByJGKAudioProcessor()
: AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
  apvts(*this, nullptr, "PARAMS", createLayout())
{}

juce::AudioProcessorValueTreeState::ParameterLayout AcousticGuitarByJGKAudioProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    p.push_back(std::make_unique<juce::AudioParameterInt>("capo", "Capo", 0, 12, 0));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("strum", "Strum", juce::NormalisableRange<float>(4.0f, 120.0f, 0.1f), 28.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("humanize", "Humanize", 0.0f, 1.0f, 0.18f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("tone", "Tone", 0.0f, 1.0f, 0.58f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("room", "Room", 0.0f, 1.0f, 0.18f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("mute", "Palm Mute", 0.0f, 1.0f, 0.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("output", "Output", juce::NormalisableRange<float>(-24.0f, 6.0f, 0.1f), -6.0f));
    return { p.begin(), p.end() };
}

void AcousticGuitarByJGKAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    engine.prepare(sampleRate, samplesPerBlock);
}

void AcousticGuitarByJGKAudioProcessor::releaseResources() {}

bool AcousticGuitarByJGKAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        || layouts.getMainOutputChannelSet() == juce::AudioChannelSet::mono();
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

    // Render up to each MIDI event before changing the held-note state.
    // This is essential for FL Studio piano-roll chords: short notes still
    // produce a real strum before note-off chokes the strings.
    int renderPosition = 0;
    auto renderSlice = [&] (int startSample, int numSamples)
    {
        if (numSamples <= 0)
            return;

        juce::AudioBuffer<float> slice(buffer.getArrayOfWritePointers(),
                                       buffer.getNumChannels(),
                                       startSample,
                                       numSamples);
        engine.process(slice, numSamples);
    };

    for (const auto metadata : midi)
    {
        const int eventPosition = juce::jlimit(0, buffer.getNumSamples(), metadata.samplePosition);
        renderSlice(renderPosition, eventPosition - renderPosition);
        renderPosition = eventPosition;

        const auto m = metadata.getMessage();
        if (m.isNoteOn())
            engine.noteOn(m.getNoteNumber(), m.getFloatVelocity(), 0);
        else if (m.isNoteOff())
            engine.noteOff(m.getNoteNumber(), 0);
        else if (m.isAllNotesOff() || m.isAllSoundOff())
            engine.reset();
    }

    renderSlice(renderPosition, buffer.getNumSamples() - renderPosition);
}

juce::AudioProcessorEditor* AcousticGuitarByJGKAudioProcessor::createEditor()
{
    return new AcousticGuitarByJGKAudioProcessorEditor(*this);
}

void AcousticGuitarByJGKAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void AcousticGuitarByJGKAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AcousticGuitarByJGKAudioProcessor();
}
