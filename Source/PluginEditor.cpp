#include "PluginEditor.h"
#include "PluginProcessor.h"

namespace ParamID {
static constexpr auto semitones = "semitones";
static constexpr auto formantSemitones = "formantSemitones";
static constexpr auto formantCompensate = "formantCompensate";
static constexpr auto latencyMode = "latencyMode";
static constexpr auto advancedEnabled = "advancedEnabled";
static constexpr auto advancedBlockMs = "advancedBlockMs";
static constexpr auto advancedOverlap = "advancedOverlap";
static constexpr auto tonalityLimit = "tonalityLimit";
}

static void setupSlider(juce::Slider& slider, juce::Label& label, const juce::String& text, juce::Component& parent) {
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.attachToComponent(&slider, false);
    parent.addAndMakeVisible(slider);
    parent.addAndMakeVisible(label);
}

TransposerAudioProcessorEditor::TransposerAudioProcessorEditor(TransposerAudioProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p) {
    setupSlider(semitonesSlider, semitonesLabel, "Semitones", *this);
    setupSlider(formantSlider, formantLabel, "Formant", *this);
    setupSlider(tonalitySlider, tonalityLabel, "Tonality Limit", *this);
    setupSlider(blockMsSlider, blockMsLabel, "Block ms", *this);
    setupSlider(overlapSlider, overlapLabel, "Overlap", *this);

    addAndMakeVisible(latencyModeBox);
    latencyModeBox.addItemList({"Fast", "Balanced", "Quality"}, 1);

    addAndMakeVisible(formantCompensateButton);
    addAndMakeVisible(advancedButton);

    auto& apvts = processorRef.apvts;
    semitonesAttachment = std::make_unique<SliderAttachment>(apvts, ParamID::semitones, semitonesSlider);
    formantAttachment = std::make_unique<SliderAttachment>(apvts, ParamID::formantSemitones, formantSlider);
    tonalityAttachment = std::make_unique<SliderAttachment>(apvts, ParamID::tonalityLimit, tonalitySlider);
    blockMsAttachment = std::make_unique<SliderAttachment>(apvts, ParamID::advancedBlockMs, blockMsSlider);
    overlapAttachment = std::make_unique<SliderAttachment>(apvts, ParamID::advancedOverlap, overlapSlider);
    formantCompensateAttachment = std::make_unique<ButtonAttachment>(apvts, ParamID::formantCompensate, formantCompensateButton);
    advancedAttachment = std::make_unique<ButtonAttachment>(apvts, ParamID::advancedEnabled, advancedButton);
    latencyModeAttachment = std::make_unique<ComboBoxAttachment>(apvts, ParamID::latencyMode, latencyModeBox);

    setSize(560, 220);
}

void TransposerAudioProcessorEditor::resized() {
    auto area = getLocalBounds().reduced(16);
    area.removeFromTop(20); // room for slider labels above the top row

    auto knobRow = area.removeFromTop(110);
    const int knobWidth = knobRow.getWidth() / 5;
    for (auto* slider : {&semitonesSlider, &formantSlider, &tonalitySlider, &blockMsSlider, &overlapSlider})
        slider->setBounds(knobRow.removeFromLeft(knobWidth).reduced(8));

    area.removeFromTop(12);
    auto controlsRow = area.removeFromTop(30);
    latencyModeBox.setBounds(controlsRow.removeFromLeft(140));
    controlsRow.removeFromLeft(12);
    formantCompensateButton.setBounds(controlsRow.removeFromLeft(180));
    controlsRow.removeFromLeft(12);
    advancedButton.setBounds(controlsRow.removeFromLeft(120));
}
