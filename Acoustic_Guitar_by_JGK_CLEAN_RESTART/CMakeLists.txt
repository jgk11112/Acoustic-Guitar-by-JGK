cmake_minimum_required(VERSION 3.22)
project(AcousticGuitarByJGK VERSION 0.4.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
  JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.15
)
FetchContent_MakeAvailable(JUCE)

# CC0 2017 Martin HD28 Vintage Series samples by Jeff Learman.
set(MARTIN_COMMIT 05d5ed8befa042fd9d99a6d159dfc3673d3f8edc)
set(MARTIN_BASE "https://raw.githubusercontent.com/sfzinstruments/Discord-SFZ-GM-Bank/${MARTIN_COMMIT}/Discord%20GM/Melodic/026-Acoustic%20Guitar%20%28steel%29")
set(SAMPLE_DIR "${CMAKE_BINARY_DIR}/martin_hd28_cc0")
file(MAKE_DIRECTORY "${SAMPLE_DIR}")

set(SAMPLE_NAMES
  MartinGM2_040__E2_1.wav
  MartinGM2_043__G2_1.wav
  MartinGM2_046_Bb2_1.wav
  MartinGM2_049_Db3_1.wav
  MartinGM2_052__E3_1.wav
  MartinGM2_055__G3_1.wav
  MartinGM2_058_Bb3_1.wav
  MartinGM2_061_Db4_1.wav
  MartinGM2_064__E4_1.wav
  MartinGM2_068_Ab4_1.wav
  MartinGM2_071__B4_1.wav
  MartinGM2_074__D5_1.wav
  MartinGM2_077__F5_1.wav
  MartinGM2_080_Ab5_1.wav
  MartinGM2_083__B5_1.wav
)

set(SAMPLE_FILES)
foreach(name IN LISTS SAMPLE_NAMES)
  set(dst "${SAMPLE_DIR}/${name}")
  if(NOT EXISTS "${dst}")
    message(STATUS "Downloading CC0 guitar sample: ${name}")
    file(DOWNLOAD "${MARTIN_BASE}/${name}" "${dst}" STATUS dl_status TLS_VERIFY ON)
    list(GET dl_status 0 dl_code)
    list(GET dl_status 1 dl_message)
    if(NOT dl_code EQUAL 0)
      message(FATAL_ERROR "Sample download failed for ${name}: ${dl_message}")
    endif()
  endif()
  list(APPEND SAMPLE_FILES "${dst}")
endforeach()

juce_add_binary_data(JGKGuitarSamples SOURCES ${SAMPLE_FILES})

juce_add_plugin(AcousticGuitarByJGK
  COMPANY_NAME "JGK"
  BUNDLE_ID "com.jgk.acousticguitar"
  PLUGIN_MANUFACTURER_CODE Jgk1
  PLUGIN_CODE AgJ1
  FORMATS VST3
  PRODUCT_NAME "Acoustic Guitar by JGK"
  IS_SYNTH TRUE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE
)

target_sources(AcousticGuitarByJGK PRIVATE
  Source/PluginProcessor.cpp
  Source/PluginProcessor.h
  Source/GuitarEngine.cpp
  Source/GuitarEngine.h
)

target_compile_definitions(AcousticGuitarByJGK PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(AcousticGuitarByJGK PRIVATE
  JGKGuitarSamples
  juce::juce_audio_utils
  juce::juce_audio_formats
  juce::juce_dsp
)
