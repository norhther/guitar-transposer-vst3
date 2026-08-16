# Transposer (VST3)

macOS VST3 real-time pitch shifter. Started as a port of
[guitar-transposer-au](https://github.com/norhther/guitar-transposer-au) (an
iOS AUv3 plugin) but works on any signal, not guitar-specific. Same DSP core —
[Signalsmith Stretch](https://github.com/Signalsmith-Audio/signalsmith-stretch)
— rewired through [JUCE](https://juce.com) instead of AVAudioUnit.

## Features

- Real-time semitone transposition, ±24 semitones
- Formant shift + formant compensation
- Latency mode switch (fast/balanced/quality)
- Advanced mode exposing block size and overlap directly

## Building

```bash
git clone --recurse-submodules <this repo>
cd guitar-transposer-vst3
cmake -B build -G Xcode
cmake --build build --config Release
```

Requires Xcode and CMake 3.22+. If you cloned without `--recurse-submodules`,
run `git submodule update --init --recursive` first — JUCE and the Signalsmith
DSP vendor code are both submodules.

## Project layout

```
Source/
  PluginProcessor.*   juce::AudioProcessor — parameters, processBlock
  PluginEditor.*       juce::AudioProcessorEditor — UI
  DSP/                 SignalsmithBridge — ported from the AUv3 sibling project
Vendor/
  JUCE/                          submodule
  signalsmith-stretch/           submodule
  signalsmith-linear/            submodule
```

## License

Signalsmith Stretch and Signalsmith Linear are pulled in as submodules under
their own licenses. JUCE is dual-licensed (GPLv3 / commercial) — see
[juce.com/get-juce](https://juce.com/get-juce) before distributing binaries.
