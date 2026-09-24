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

# -----------------------------------------------------------------------------
# CC0 Martin HD28 acoustic samples
# 2017 Martin HD28 Vintage Series recorded by Jeff Learman for the Discord SFZ
# GM Bank. The instrument's SFZ header explicitly declares Creative Commons CC0.
# We pin the upstream repository to the exact commit used by the provenance audit
# in ferrosintesis so future upstream changes cannot silently change the source.
# -----------------------------------------------------------------------------
set(MARTIN_HD28_COMMIT 05d5ed8befa042fd9d99a6d159dfc3673d3f8edc)
set(MARTIN_HD28_BASE_URL
    "https://raw.githubusercontent.com/sfzinstruments/Discord-SFZ-GM-Bank/${MARTIN_HD28_COMMIT}/Discord%20GM/Melodic/026-Acoustic%20Guitar%20%28steel%29")
set(MARTIN_HD28_DIR "${CMAKE_BINARY_DIR}/martin_hd28_cc0")
file(MAKE_DIRECTORY "${MARTIN_HD28_DIR}")

set(MARTIN_HD28_NAMES
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

set(MARTIN_HD28_FILES)
foreach(sample_name IN LISTS MARTIN_HD28_NAMES)
    set(sample_path "${MARTIN_HD28_DIR}/${sample_name}")
    if(NOT EXISTS "${sample_path}")
        message(STATUS "Downloading CC0 Martin HD28 sample: ${sample_name}")
        file(DOWNLOAD
            "${MARTIN_HD28_BASE_URL}/${sample_name}"
            "${sample_path}"
            STATUS download_status
            TLS_VERIFY ON
        )
        list(GET download_status 0 download_code)
        list(GET download_status 1 download_message)
        if(NOT download_code EQUAL 0)
            message(FATAL_ERROR "Could not download ${sample_name}: ${download_message}")
        endif()
    endif()
    list(APPEND MARTIN_HD28_FILES "${sample_path}")
endforeach()

juce_add_binary_data(MartinHD28Samples
    HEADER_NAME MartinHD28Data.h
    NAMESPACE MartinHD28Data
    SOURCES ${MARTIN_HD28_FILES}
)

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

# The source uses <JuceHeader.h>. CMake does not create that header unless this
# is called explicitly. This fixes the compile error from build #2.
juce_generate_juce_header(AcousticGuitarByJGK)

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
    MartinHD28Samples
    juce::juce_audio_utils
    juce::juce_audio_formats
    juce::juce_dsp
)
