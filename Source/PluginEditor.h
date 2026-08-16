#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

class TransposerAudioProcessor;

class TransposerAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit TransposerAudioProcessorEditor(TransposerAudioProcessor&);

    void resized() override;

private:
    void timerCallback() override;
    void updateAdvancedControlsEnabled();

    // 0...1 peak level bars, driven by SignalsmithBridge::inputPeak/outputPeak via
    // TransposerAudioProcessor::getInputPeak/getOutputPeak (30Hz poll, not audio-thread-safe
    // beyond the atomic reads already in the bridge).
    struct PeakMeter : public juce::Component {
        float level = 0.0f;
        void paint(juce::Graphics& g) override {
            auto bounds = getLocalBounds().toFloat();
            g.setColour(juce::Colours::darkgrey);
            g.fillRect(bounds);
            g.setColour(juce::Colours::limegreen);
            g.fillRect(bounds.removeFromLeft(bounds.getWidth() * juce::jlimit(0.0f, 1.0f, level)));
        }
    };
    PeakMeter inputMeter, outputMeter;
    juce::Label inputMeterLabel, outputMeterLabel;

    juce::TextButton aboutButton{"About"};
    void showAboutDialog();

    TransposerAudioProcessor& processorRef;

    juce::Slider semitonesSlider, formantSlider, tonalitySlider, blockMsSlider, overlapSlider;
    juce::Label semitonesLabel, formantLabel, tonalityLabel, blockMsLabel, overlapLabel;
    juce::ComboBox latencyModeBox;
    juce::ToggleButton formantCompensateButton{"Formant Compensate"}, advancedButton{"Advanced"};

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::unique_ptr<SliderAttachment> semitonesAttachment, formantAttachment, tonalityAttachment,
        blockMsAttachment, overlapAttachment;
    std::unique_ptr<ButtonAttachment> formantCompensateAttachment, advancedAttachment;
    std::unique_ptr<ComboBoxAttachment> latencyModeAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransposerAudioProcessorEditor)
};
