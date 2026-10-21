cmake_minimum_required(VERSION 3.22)
project(MasterBus VERSION 0.1.0)

set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")   # universal binary

add_subdirectory(JUCE)   # or find_package(JUCE CONFIG REQUIRED)

juce_add_plugin(MasterBus
    COMPANY_NAME             "YourCompany"
    PLUGIN_MANUFACTURER_CODE Yrco
    PLUGIN_CODE              Mbus
    PRODUCT_NAME             "MasterBus"
    FORMATS                  AU
    AU_MAIN_TYPE             kAudioUnitType_Effect
    IS_SYNTH                 FALSE
    NEEDS_MIDI_INPUT         FALSE
    NEEDS_MIDI_OUTPUT        FALSE
    COPY_PLUGIN_AFTER_BUILD  TRUE)

target_sources(MasterBus PRIVATE
    PluginProcessor.cpp
    PluginEditor.cpp)

target_compile_features(MasterBus PRIVATE cxx_std_17)

target_compile_definitions(MasterBus PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(MasterBus
    PRIVATE
        juce::juce_audio_processors
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
