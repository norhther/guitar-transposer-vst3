#include "PluginProcessor.h"
#include "PluginEditor.h"

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

TransposerAudioProcessor::TransposerAudioProcessor()
    : AudioProcessor(BusesProperties()
                          .withInput("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMETERS", createParameterLayout()) {
}

TransposerAudioProcessor::~TransposerAudioProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout TransposerAudioProcessor::createParameterLayout() {
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    params.push_back(std::make_unique<AudioParameterFloat>(
        ParamID::semitones, "Semitones", NormalisableRange<float>(-24.0f, 24.0f, 0.01f), 0.0f));
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParamID::formantSemitones, "Formant", NormalisableRange<float>(-24.0f, 24.0f, 0.01f), 0.0f));
    params.push_back(std::make_unique<AudioParameterBool>(
        ParamID::formantCompensate, "Formant Compensate", false));
    params.push_back(std::make_unique<AudioParameterChoice>(
        ParamID::latencyMode, "Latency Mode", StringArray{"Fast", "Balanced", "Quality"}, 1));
    params.push_back(std::make_unique<AudioParameterBool>(
        ParamID::advancedEnabled, "Advanced", false));
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParamID::advancedBlockMs, "Block ms", NormalisableRange<float>(20.0f, 200.0f, 0.1f), 90.0f));
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParamID::advancedOverlap, "Overlap", NormalisableRange<float>(1.0f, 8.0f, 0.1f), 4.0f));
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParamID::tonalityLimit, "Tonality Limit", NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));

    return { params.begin(), params.end() };
}

void TransposerAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    bridge = std::make_unique<SignalsmithBridge>(sampleRate, getTotalNumInputChannels());
    inputScratch.setSize(getTotalNumInputChannels(), samplesPerBlock);
    setLatencySamples(static_cast<int>(bridge->appliedLatencySamples()));
}

bool TransposerAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    return layouts.getMainInputChannelSet() == layouts.getMainOutputChannelSet()
        && !layouts.getMainOutputChannelSet().isDisabled();
}

void TransposerAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    if (!bridge) return;

    const auto numChannels = buffer.getNumChannels();
    const auto numSamples = buffer.getNumSamples();

    inputScratch.setSize(numChannels, numSamples, false, false, true);
    for (int ch = 0; ch < numChannels; ++ch)
        inputScratch.copyFrom(ch, 0, buffer, ch, 0, numSamples);

    bridge->setSemitones(apvts.getRawParameterValue(ParamID::semitones)->load());
    bridge->setFormantSemitones(apvts.getRawParameterValue(ParamID::formantSemitones)->load());
    bridge->setFormantCompensate(apvts.getRawParameterValue(ParamID::formantCompensate)->load() > 0.5f);
    bridge->requestLatencyMode(static_cast<TransposerLatencyMode>(
        static_cast<int>(apvts.getRawParameterValue(ParamID::latencyMode)->load())));
    bridge->setAdvancedEnabled(apvts.getRawParameterValue(ParamID::advancedEnabled)->load() > 0.5f);
    bridge->setAdvancedBlockMilliseconds(apvts.getRawParameterValue(ParamID::advancedBlockMs)->load());
    bridge->setAdvancedOverlap(apvts.getRawParameterValue(ParamID::advancedOverlap)->load());
    bridge->setTonalityLimit(apvts.getRawParameterValue(ParamID::tonalityLimit)->load());

    bridge->processInputs(inputScratch.getArrayOfReadPointers(), buffer.getArrayOfWritePointers(),
                           static_cast<uint32_t>(numSamples));

    setLatencySamples(static_cast<int>(bridge->appliedLatencySamples()));
}

juce::AudioProcessorEditor* TransposerAudioProcessor::createEditor() {
    return new TransposerAudioProcessorEditor(*this);
}

void TransposerAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    if (auto state = apvts.copyState(); auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void TransposerAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new TransposerAudioProcessor();
}
