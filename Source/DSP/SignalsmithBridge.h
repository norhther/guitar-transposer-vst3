#pragma once

#include <atomic>
#include <cstdint>
#include <memory>

enum class TransposerLatencyMode { Fast = 0, Balanced = 1, Quality = 2 };

/// Thread-safe façade over signalsmith::stretch::SignalsmithStretch<float>.
/// Ported from TransposerExtension/DSP/SignalsmithBridge.mm (iOS AUv3 sibling
/// project) — same atomics/generation-counter reload scheme, no ObjC/Foundation.
class SignalsmithBridge {
public:
    SignalsmithBridge(double sampleRate, int channelCount);
    ~SignalsmithBridge();

    SignalsmithBridge(const SignalsmithBridge&) = delete;
    SignalsmithBridge& operator=(const SignalsmithBridge&) = delete;

    /// Thread-safe from any thread. Takes effect on the render thread at the start of its
    /// next processInputs/outputs call.
    void requestLatencyMode(TransposerLatencyMode mode);

    /// Thread-safe from any thread. Total input+output latency, in samples, for the mode
    /// currently applied on the render thread.
    int64_t appliedLatencySamples() const;

    /// Thread-safe from any thread.
    void setSemitones(float semitones);

    /// Thread-safe from any thread.
    void setFormantSemitones(float semitones);

    /// Thread-safe from any thread.
    void setFormantCompensate(bool compensatePitch);

    /// Thread-safe from any thread. When enabled, block/overlap come from
    /// setAdvancedBlockMilliseconds/setAdvancedOverlap instead of the latency-mode preset.
    /// Takes effect on the render thread's next call, like requestLatencyMode.
    void setAdvancedEnabled(bool enabled);

    /// Thread-safe from any thread. STFT block length in milliseconds (advanced mode only).
    void setAdvancedBlockMilliseconds(float milliseconds);

    /// Thread-safe from any thread. Block/interval ratio, e.g. 4 = 4x overlap (advanced mode only).
    void setAdvancedOverlap(float overlap);

    /// Thread-safe from any thread. 0...1: how much of the spectrum gets phase-locked
    /// "tonal" treatment vs randomized "noise-like" treatment. Applied every render call,
    /// no reconfigure needed.
    void setTonalityLimit(float tonalityLimit);

    /// Thread-safe from any thread. Peak absolute sample value (0...1 for normal signal
    /// levels) observed on the most recent processInputs/outputs call.
    float inputPeak() const;
    float outputPeak() const;

    /// Render-thread only. Applies any pending latency-mode change, then processes
    /// frameCount frames from inputs into outputs (both [channel][frame] laid out,
    /// channelCount channels as given at construction). inputs and outputs must not alias.
    void processInputs(const float* const* inputs, float* const* outputs, uint32_t frameCount);

private:
    struct Impl;
    std::unique_ptr<Impl> impl;

    void applyLatencyMode(TransposerLatencyMode mode);

    double sampleRate;
    int channelCount;
    std::atomic<int> pendingLatencyMode;
    std::atomic<int64_t> appliedLatencySamplesValue;
    std::atomic<float> semitonesValue;
    std::atomic<float> formantSemitonesValue;
    std::atomic<bool> formantCompensateValue;
    std::atomic<float> inputPeakValue;
    std::atomic<float> outputPeakValue;
    std::atomic<bool> advancedEnabledValue;
    std::atomic<float> advancedBlockMsValue;
    std::atomic<float> advancedOverlapValue;
    std::atomic<float> tonalityLimitValue;
    // Bumped by every setter that changes block/interval sizing (latency mode, advanced
    // enable, block ms, overlap). The render thread reconfigures whenever this doesn't
    // match what it last applied -- one counter instead of one comparison per knob.
    std::atomic<int> configGeneration;
    std::atomic<int> appliedConfigGeneration;
};
