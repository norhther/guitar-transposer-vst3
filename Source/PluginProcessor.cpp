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
    lastLatencyModeIndex = static_cast<int>(apvts.getRawParameterValue(ParamID::latencyMode)->load());
    lastAdvancedEnabled = apvts.getRawParameterValue(ParamID::advancedEnabled)->load() > 0.5f;
    lastAdvancedBlockMs = apvts.getRawParameterValue(ParamID::advancedBlockMs)->load();
    lastAdvancedOverlap = apvts.getRawParameterValue(ParamID::advancedOverlap)->load();
    lastAppliedLatencySamples = static_cast<int>(bridge->appliedLatencySamples());
    setLatencySamples(lastAppliedLatencySamples);
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
    const bool advancedEnabled = apvts.getRawParameterValue(ParamID::advancedEnabled)->load() > 0.5f;
    // Formant Compensate is only exposed (and editable) in Advanced mode in the UI -- match
    // that in the DSP too, rather than silently keeping a saved-but-hidden value applied.
    bridge->setFormantCompensate(advancedEnabled
        && apvts.getRawParameterValue(ParamID::formantCompensate)->load() > 0.5f);
    const int latencyModeIndex = static_cast<int>(apvts.getRawParameterValue(ParamID::latencyMode)->load());
    if (latencyModeIndex != lastLatencyModeIndex) {
        bridge->requestLatencyMode(static_cast<TransposerLatencyMode>(latencyModeIndex));
        lastLatencyModeIndex = latencyModeIndex;
    }
    if (advancedEnabled != lastAdvancedEnabled) {
        bridge->setAdvancedEnabled(advancedEnabled);
        lastAdvancedEnabled = advancedEnabled;
    }
    const float advancedBlockMs = apvts.getRawParameterValue(ParamID::advancedBlockMs)->load();
    if (advancedBlockMs != lastAdvancedBlockMs) {
        bridge->setAdvancedBlockMilliseconds(advancedBlockMs);
        lastAdvancedBlockMs = advancedBlockMs;
    }
    const float advancedOverlap = apvts.getRawParameterValue(ParamID::advancedOverlap)->load();
    if (advancedOverlap != lastAdvancedOverlap) {
        bridge->setAdvancedOverlap(advancedOverlap);
        lastAdvancedOverlap = advancedOverlap;
    }
    bridge->setTonalityLimit(apvts.getRawParameterValue(ParamID::tonalityLimit)->load());

    bridge->processInputs(inputScratch.getArrayOfReadPointers(), buffer.getArrayOfWritePointers(),
                           static_cast<uint32_t>(numSamples));

    const int appliedLatencySamples = static_cast<int>(bridge->appliedLatencySamples());
    if (appliedLatencySamples != lastAppliedLatencySamples) {
        setLatencySamples(appliedLatencySamples);
        lastAppliedLatencySamples = appliedLatencySamples;
    }
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
