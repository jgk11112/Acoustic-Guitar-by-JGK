cmake_minimum_required(VERSION 3.22)
project(AcousticGuitarByJGK VERSION 0.3.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
  JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.15
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(AcousticGuitarByJGK
    COMPANY_NAME "JGK"
    BUNDLE_ID "com.jgk.acousticguitar"
    PLUGIN_MANUFACTURER_CODE Jgk1
    PLUGIN_CODE AgJ1
    FORMATS VST3 Standalone
    PRODUCT_NAME "Acoustic Guitar by JGK"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

target_sources(AcousticGuitarByJGK PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/GuitarEngine.h
    Source/GuitarEngine.cpp
)

target_compile_definitions(AcousticGuitarByJGK PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(AcousticGuitarByJGK PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
)
