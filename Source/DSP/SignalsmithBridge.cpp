#include "SignalsmithBridge.h"

#include <algorithm>
#include <cmath>
#include <cfenv>

#include "signalsmith-stretch.h"

struct SignalsmithBridge::Impl {
    signalsmith::stretch::SignalsmithStretch<float> stretch;
};

/// ARM doesn't auto-flush denormals like x86 SSE. A decaying sustained note pushes the
/// STFT's internal buffers into denormal range, and denormal float math runs 10-100x
/// slower on some cores -> can miss the render deadline -> dropout on long notes/chords.
/// FE_DFL_DISABLE_DENORMS_ENV flushes denormals to zero for this thread.
static void TransposerEnableFlushToZero() {
    fesetenv(FE_DFL_DISABLE_DENORMS_ENV);
}

/// Block length in seconds per mode.
///
/// These must stay in the same ballpark as the library's own presets, which are what
/// the reference demo uses: presetDefault is block = SR*0.12, interval = SR*0.03;
/// presetCheaper is 0.1 / 0.04. The block length sets the STFT's frequency resolution
/// (bin spacing = SR/blockSamples), so shrinking it to chase latency destroys pitch
/// tracking: a 16 ms block gives ~62 Hz bins, which cannot separate a low-E guitar
/// fundamental (82 Hz) from its neighbours -> phase smearing and audible artifacts.
/// 60 ms (~16 Hz bins) is about the floor for guitar-range material.
static int TransposerBlockSamples(double sampleRate, TransposerLatencyMode mode) {
    double blockSeconds;
    switch (mode) {
        case TransposerLatencyMode::Fast:     blockSeconds = 0.06; break;
        case TransposerLatencyMode::Balanced: blockSeconds = 0.09; break;
        case TransposerLatencyMode::Quality:  blockSeconds = 0.12; break;
        default:                              blockSeconds = 0.09; break;
    }
    return std::max(256, static_cast<int>(sampleRate * blockSeconds));
}

SignalsmithBridge::SignalsmithBridge(double sampleRateIn, int channelCountIn)
    : impl(std::make_unique<Impl>()),
      sampleRate(sampleRateIn),
      channelCount(channelCountIn),
      pendingLatencyMode(static_cast<int>(TransposerLatencyMode::Balanced)),
      appliedLatencySamplesValue(0),
      semitonesValue(0.0f),
      formantSemitonesValue(0.0f),
      formantCompensateValue(false),
      inputPeakValue(0.0f),
      outputPeakValue(0.0f),
      advancedEnabledValue(false),
      advancedBlockMsValue(90.0f),
      advancedOverlapValue(4.0f),
      // 0 is a special case the library treats as "unlimited" (full-spectrum tonal
      // treatment, matches the behavior before this parameter existed). Small >0
      // values restrict tonal/phase-locked treatment to low frequencies only (more
      // noise-like texture up top); values near/above 1 are close to unlimited again.
      tonalityLimitValue(0.0f),
      configGeneration(0),
      appliedConfigGeneration(-1) {
    applyLatencyMode(TransposerLatencyMode::Balanced);
}

SignalsmithBridge::~SignalsmithBridge() = default;

void SignalsmithBridge::applyLatencyMode(TransposerLatencyMode mode) {
    int blockSamples;
    int intervalSamples;
    if (advancedEnabledValue.load(std::memory_order_relaxed)) {
        float ms = advancedBlockMsValue.load(std::memory_order_relaxed);
        float overlap = std::max(1.0f, advancedOverlapValue.load(std::memory_order_relaxed));
        blockSamples = std::max(256, static_cast<int>(sampleRate * (ms / 1000.0)));
        intervalSamples = std::max(64, static_cast<int>(blockSamples / overlap));
    } else {
        blockSamples = TransposerBlockSamples(sampleRate, mode);
        // 4x overlap, matching presetDefault's 0.12 / 0.03 ratio.
        intervalSamples = std::max(64, blockSamples / 4);
    }
    // splitComputation=true: block can be 60-200ms, way bigger than a render callback
    // (~5-10ms). Without splitting, hitting a block boundary makes process() do the
    // whole STFT synchronously on the render thread in one call -> can starve the
    // callback deadline -> dropout, not spectral smearing, but same symptom. Splitting
    // spreads that work across calls (presetCheaper does the same for this reason).
    impl->stretch.configure(channelCount, blockSamples, intervalSamples, true);
    appliedConfigGeneration.store(configGeneration.load(std::memory_order_relaxed), std::memory_order_relaxed);
    int64_t total = static_cast<int64_t>(impl->stretch.inputLatency()) + static_cast<int64_t>(impl->stretch.outputLatency());
    appliedLatencySamplesValue.store(total, std::memory_order_relaxed);
}

void SignalsmithBridge::requestLatencyMode(TransposerLatencyMode mode) {
    pendingLatencyMode.store(static_cast<int>(mode), std::memory_order_relaxed);
    configGeneration.fetch_add(1, std::memory_order_relaxed);
}

void SignalsmithBridge::setAdvancedEnabled(bool enabled) {
    advancedEnabledValue.store(enabled, std::memory_order_relaxed);
    configGeneration.fetch_add(1, std::memory_order_relaxed);
}

void SignalsmithBridge::setAdvancedBlockMilliseconds(float milliseconds) {
    advancedBlockMsValue.store(milliseconds, std::memory_order_relaxed);
    configGeneration.fetch_add(1, std::memory_order_relaxed);
}

void SignalsmithBridge::setAdvancedOverlap(float overlap) {
    advancedOverlapValue.store(overlap, std::memory_order_relaxed);
    configGeneration.fetch_add(1, std::memory_order_relaxed);
}

void SignalsmithBridge::setTonalityLimit(float tonalityLimit) {
    tonalityLimitValue.store(tonalityLimit, std::memory_order_relaxed);
}

int64_t SignalsmithBridge::appliedLatencySamples() const {
    return appliedLatencySamplesValue.load(std::memory_order_relaxed);
}

void SignalsmithBridge::setSemitones(float semitones) {
    semitonesValue.store(semitones, std::memory_order_relaxed);
}

void SignalsmithBridge::setFormantSemitones(float semitones) {
    formantSemitonesValue.store(semitones, std::memory_order_relaxed);
}

void SignalsmithBridge::setFormantCompensate(bool compensatePitch) {
    formantCompensateValue.store(compensatePitch, std::memory_order_relaxed);
}

float SignalsmithBridge::inputPeak() const {
    return inputPeakValue.load(std::memory_order_relaxed);
}

float SignalsmithBridge::outputPeak() const {
    return outputPeakValue.load(std::memory_order_relaxed);
}

static float TransposerPeakAbs(const float* samples, uint32_t frameCount) {
    float peak = 0.0f;
    for (uint32_t i = 0; i < frameCount; i++) {
        peak = std::max(peak, std::fabs(samples[i]));
    }
    return peak;
}

static float TransposerPeakAbsAllChannels(const float* const* channels, int channelCount, uint32_t frameCount) {
    float peak = 0.0f;
    for (int c = 0; c < channelCount; c++) {
        peak = std::max(peak, TransposerPeakAbs(channels[c], frameCount));
    }
    return peak;
}

void SignalsmithBridge::processInputs(const float* const* inputs, float* const* outputs, uint32_t frameCount) {
    TransposerEnableFlushToZero();
    if (configGeneration.load(std::memory_order_relaxed) != appliedConfigGeneration.load(std::memory_order_relaxed)) {
        int mode = pendingLatencyMode.load(std::memory_order_relaxed);
        applyLatencyMode(static_cast<TransposerLatencyMode>(mode));
    }
    inputPeakValue.store(TransposerPeakAbsAllChannels(inputs, channelCount, frameCount), std::memory_order_relaxed);
    impl->stretch.setTransposeSemitones(semitonesValue.load(std::memory_order_relaxed),
                                         tonalityLimitValue.load(std::memory_order_relaxed));
    impl->stretch.setFormantSemitones(formantSemitonesValue.load(std::memory_order_relaxed),
                                       formantCompensateValue.load(std::memory_order_relaxed));
    impl->stretch.process(inputs, static_cast<int>(frameCount), outputs, static_cast<int>(frameCount));
    outputPeakValue.store(TransposerPeakAbsAllChannels(outputs, channelCount, frameCount), std::memory_order_relaxed);
}
