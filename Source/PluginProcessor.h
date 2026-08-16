#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "DSP/SignalsmithBridge.h"

class TransposerAudioProcessor : public juce::AudioProcessor {
public:
    TransposerAudioProcessor();
    ~TransposerAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    std::unique_ptr<SignalsmithBridge> bridge;
    // Non-aliasing scratch: JUCE's processBlock buffer is in-place (same memory for
    // in and out), but SignalsmithBridge::processInputs requires inputs/outputs to
    // not alias, same as the AUv3 render callback's separate buffers.
    juce::AudioBuffer<float> inputScratch;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransposerAudioProcessor)
};
