#include "PluginEditor.h"

AcousticGuitarByJGKAudioProcessorEditor::AcousticGuitarByJGKAudioProcessorEditor(AcousticGuitarByJGKAudioProcessor& p)
: AudioProcessorEditor(&p), processor(p)
{
    setSize(760, 430);

    capo.setSliderStyle(juce::Slider::LinearHorizontal);
    capo.setTextBoxStyle(juce::Slider::TextBoxRight, false, 78, 24);
    capo.setTextValueSuffix(" fret");
    capo.setNumDecimalPlacesToDisplay(0);
    addAndMakeVisible(capo);
    setupKnob(strum, " ms");
    setupKnob(humanize);
    setupKnob(tone);
    setupKnob(room);
    setupKnob(mute);
    setupKnob(output, " dB");

    capoLabel.setText("CAPO", juce::dontSendNotification);
    capoLabel.setJustificationType(juce::Justification::centred);
    capoLabel.setFont(juce::FontOptions(18.0f).withStyle("Bold"));
    addAndMakeVisible(capoLabel);

    hint.setText("Full MIDI chords -> guitar voicings. Short notes chop; long notes ring. Palm Mute is fully automatable.", juce::dontSendNotification);
    hint.setJustificationType(juce::Justification::centred);
    hint.setFont(juce::FontOptions(14.0f));
    addAndMakeVisible(hint);

    aCapo = std::make_unique<SliderAttachment>(processor.apvts, "capo", capo);
    aStrum = std::make_unique<SliderAttachment>(processor.apvts, "strum", strum);
    aHumanize = std::make_unique<SliderAttachment>(processor.apvts, "humanize", humanize);
    aTone = std::make_unique<SliderAttachment>(processor.apvts, "tone", tone);
    aRoom = std::make_unique<SliderAttachment>(processor.apvts, "room", room);
    aMute = std::make_unique<SliderAttachment>(processor.apvts, "mute", mute);
    aOutput = std::make_unique<SliderAttachment>(processor.apvts, "output", output);
}

void AcousticGuitarByJGKAudioProcessorEditor::setupKnob(juce::Slider& s, const juce::String& suffix)
{
    s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 76, 22);
    s.setTextValueSuffix(suffix);
    addAndMakeVisible(s);
}

void AcousticGuitarByJGKAudioProcessorEditor::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.fillAll(juce::Colour::fromRGB(24, 20, 17));

    juce::ColourGradient wood(juce::Colour::fromRGB(78, 53, 35), 0, 0,
                              juce::Colour::fromRGB(35, 27, 22), b.getWidth(), b.getHeight(), false);
    g.setGradientFill(wood);
    g.fillRoundedRectangle(b.reduced(12.0f), 18.0f);

    g.setColour(juce::Colours::white.withAlpha(0.95f));
    g.setFont(juce::FontOptions(34.0f).withStyle("Bold"));
    g.drawText("ACOUSTIC GUITAR", 28, 24, getWidth() - 56, 44, juce::Justification::centredLeft);
    g.setFont(juce::FontOptions(17.0f));
    g.setColour(juce::Colours::white.withAlpha(0.65f));
    g.drawText("by JGK", 31, 66, 200, 24, juce::Justification::centredLeft);

    // Capo neck: 0-12 fret positions behind the horizontal capo control.
    auto neck = juce::Rectangle<float>(55.0f, 112.0f, getWidth() - 110.0f, 58.0f);
    g.setColour(juce::Colour::fromRGB(103, 72, 47));
    g.fillRoundedRectangle(neck, 8.0f);
    g.setColour(juce::Colours::white.withAlpha(0.22f));
    for (int f = 0; f <= 12; ++f)
    {
        float x = neck.getX() + neck.getWidth() * (float) f / 12.0f;
        g.drawVerticalLine((int) x, neck.getY() + 5.0f, neck.getBottom() - 5.0f);
    }
    g.setColour(juce::Colours::white.withAlpha(0.70f));
    g.setFont(juce::FontOptions(11.0f));
    for (int f = 0; f <= 12; ++f)
    {
        float x = neck.getX() + neck.getWidth() * (float) f / 12.0f;
        g.drawText(juce::String(f), (int) x - 10, 92, 20, 18, juce::Justification::centred);
    }

    g.setColour(juce::Colours::white.withAlpha(0.13f));
    g.drawRoundedRectangle(juce::Rectangle<float>(22, 185, getWidth() - 44.0f, 170), 14.0f, 1.0f);

    g.setColour(juce::Colours::white.withAlpha(0.82f));
    g.setFont(juce::FontOptions(13.0f).withStyle("Bold"));
    const char* names[] = { "STRUM", "HUMANIZE", "TONE", "ROOM", "PALM MUTE", "OUTPUT" };
    int xs[] = { 130, 240, 350, 460, 570, 680 };
    for (int i = 0; i < 6; ++i)
        g.drawText(names[i], xs[i] - 45, 325, 90, 22, juce::Justification::centred);
}

void AcousticGuitarByJGKAudioProcessorEditor::resized()
{
    capo.setBounds(70, 120, getWidth() - 140, 44);
    capoLabel.setBounds(25, 120, 52, 30);

    juce::Slider* knobs[] = { &strum, &humanize, &tone, &room, &mute, &output };
    int x = 80;
    for (auto* k : knobs)
    {
        k->setBounds(x, 205, 100, 120);
        x += 110;
    }
    hint.setBounds(25, 380, getWidth() - 50, 28);
}
