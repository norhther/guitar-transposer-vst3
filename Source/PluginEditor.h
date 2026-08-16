#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

class TransposerAudioProcessor;

class TransposerAudioProcessorEditor : public juce::AudioProcessorEditor {
public:
    explicit TransposerAudioProcessorEditor(TransposerAudioProcessor&);

    void resized() override;

private:
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
