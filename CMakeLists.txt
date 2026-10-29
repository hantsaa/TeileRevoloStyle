cmake_minimum_required(VERSION 3.22)

# Runtime MSVC statique : le .vst3 tourne sans installer les redistribuables Visual C++
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(RevoloStyle VERSION 0.1.0)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(RevoloStyle
    COMPANY_NAME "Hantsa"
    PLUGIN_MANUFACTURER_CODE Hnts
    PLUGIN_CODE Rvl1
    FORMATS VST3 Standalone
    PRODUCT_NAME "RevoloStyle"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Reverb)

target_sources(RevoloStyle PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_features(RevoloStyle PRIVATE cxx_std_17)
target_compile_definitions(RevoloStyle PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(RevoloStyle
    PRIVATE juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
