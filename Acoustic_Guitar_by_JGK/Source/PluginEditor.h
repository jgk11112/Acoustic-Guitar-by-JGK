#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class AcousticGuitarByJGKAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit AcousticGuitarByJGKAudioProcessorEditor(AcousticGuitarByJGKAudioProcessor&);
    ~AcousticGuitarByJGKAudioProcessorEditor() override = default;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    AcousticGuitarByJGKAudioProcessor& processor;
    juce::Slider capo, strum, humanize, tone, room, mute, output;
    juce::Label capoLabel, hint;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SliderAttachment> aCapo, aStrum, aHumanize, aTone, aRoom, aMute, aOutput;

    void setupKnob(juce::Slider& s, const juce::String& suffix = {});
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AcousticGuitarByJGKAudioProcessorEditor)
};
