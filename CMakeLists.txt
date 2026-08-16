cmake_minimum_required(VERSION 3.22)

project(GuitarTransposerVST3 VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(Vendor/JUCE)

juce_add_plugin(GuitarTransposerVST3
    COMPANY_NAME "Norhther"
    PLUGIN_MANUFACTURER_CODE Nort
    PLUGIN_CODE Trns
    FORMATS VST3 Standalone
    PRODUCT_NAME "Transposer"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Transposer needs mic input to pitch-shift your input signal."
    VST3_CATEGORIES Fx)

target_sources(GuitarTransposerVST3 PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/DSP/SignalsmithBridge.cpp)

target_include_directories(GuitarTransposerVST3 PRIVATE
    Vendor/signalsmith-stretch
    Vendor)

target_compile_definitions(GuitarTransposerVST3 PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(GuitarTransposerVST3 PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)
