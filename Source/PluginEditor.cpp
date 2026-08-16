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

// Mirrors SignalsmithBridge.cpp's TransposerBlockSamples()/applyLatencyMode(): non-advanced
// mode uses these block lengths and a fixed 4x overlap per latency-mode preset.
static float presetBlockMs(int latencyModeIndex) {
    switch (latencyModeIndex) {
        case 0: return 60.0f;  // Fast
        case 2: return 120.0f; // Quality
        default: return 90.0f; // Balanced
    }
}
static constexpr float presetOverlap = 4.0f;

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
    latencyModeBox.onChange = [this] { updateAdvancedControlsEnabled(); };

    addAndMakeVisible(formantCompensateButton);
    addAndMakeVisible(advancedButton);
    advancedButton.onClick = [this] { updateAdvancedControlsEnabled(); };

    addAndMakeVisible(inputMeter);
    addAndMakeVisible(outputMeter);
    inputMeterLabel.setText("In", juce::dontSendNotification);
    outputMeterLabel.setText("Out", juce::dontSendNotification);
    addAndMakeVisible(inputMeterLabel);
    addAndMakeVisible(outputMeterLabel);
    startTimerHz(30);

    addAndMakeVisible(aboutButton);
    aboutButton.onClick = [this] { showAboutDialog(); };

    auto& apvts = processorRef.apvts;
    semitonesAttachment = std::make_unique<SliderAttachment>(apvts, ParamID::semitones, semitonesSlider);
    formantAttachment = std::make_unique<SliderAttachment>(apvts, ParamID::formantSemitones, formantSlider);
    tonalityAttachment = std::make_unique<SliderAttachment>(apvts, ParamID::tonalityLimit, tonalitySlider);
    blockMsAttachment = std::make_unique<SliderAttachment>(apvts, ParamID::advancedBlockMs, blockMsSlider);
    overlapAttachment = std::make_unique<SliderAttachment>(apvts, ParamID::advancedOverlap, overlapSlider);
    formantCompensateAttachment = std::make_unique<ButtonAttachment>(apvts, ParamID::formantCompensate, formantCompensateButton);
    advancedAttachment = std::make_unique<ButtonAttachment>(apvts, ParamID::advancedEnabled, advancedButton);
    latencyModeAttachment = std::make_unique<ComboBoxAttachment>(apvts, ParamID::latencyMode, latencyModeBox);

    setSize(560, 260);
    updateAdvancedControlsEnabled();
}

void TransposerAudioProcessorEditor::updateAdvancedControlsEnabled() {
    const bool advanced = advancedButton.getToggleState();
    blockMsSlider.setEnabled(advanced);
    overlapSlider.setEnabled(advanced);
    formantCompensateButton.setEnabled(advanced);

    if (advanced) {
        // Coming back from a preset display override: show the real underlying param
        // values again (the attachments never touched them, only the display did).
        blockMsSlider.setValue(processorRef.apvts.getRawParameterValue(ParamID::advancedBlockMs)->load(),
                                juce::dontSendNotification);
        overlapSlider.setValue(processorRef.apvts.getRawParameterValue(ParamID::advancedOverlap)->load(),
                                juce::dontSendNotification);
        formantCompensateButton.setToggleState(
            processorRef.apvts.getRawParameterValue(ParamID::formantCompensate)->load() > 0.5f,
            juce::dontSendNotification);
    } else {
        // Display-only: show what's actually applied outside Advanced mode -- the
        // latency-mode preset's block/overlap, and formant compensate forced off
        // (PluginProcessor::processBlock gates it on advancedEnabled the same way).
        blockMsSlider.setValue(presetBlockMs(latencyModeBox.getSelectedItemIndex()), juce::dontSendNotification);
        overlapSlider.setValue(presetOverlap, juce::dontSendNotification);
        formantCompensateButton.setToggleState(false, juce::dontSendNotification);
    }
}

void TransposerAudioProcessorEditor::showAboutDialog() {
    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::NoIcon, "About Guitar Transposer",
        "Pitch shifting powered by Signalsmith Stretch and Signalsmith Linear,\n"
        "(c) Geraint Luff / Signalsmith Audio Ltd, MIT licensed.\n"
        "https://github.com/Signalsmith-Audio/signalsmith-stretch\n\n"
        "Built with JUCE.");
}

void TransposerAudioProcessorEditor::timerCallback() {
    inputMeter.level = processorRef.getInputPeak();
    outputMeter.level = processorRef.getOutputPeak();
    inputMeter.repaint();
    outputMeter.repaint();
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
    aboutButton.setBounds(controlsRow.removeFromRight(70));

    area.removeFromTop(12);
    auto meterRow = area.removeFromTop(24);
    inputMeterLabel.setBounds(meterRow.removeFromLeft(30));
    inputMeter.setBounds(meterRow.removeFromLeft((meterRow.getWidth() - 20) / 2));
    meterRow.removeFromLeft(20);
    outputMeterLabel.setBounds(meterRow.removeFromLeft(30));
    outputMeter.setBounds(meterRow);
}
