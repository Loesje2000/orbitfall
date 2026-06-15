cmake_minimum_required(VERSION 3.22)

project(Orbitfall VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version" FORCE)

add_subdirectory("/Applications/JUCE" JUCE)

juce_add_plugin(Orbitfall
    COMPANY_NAME "Rubblesonic"
    BUNDLE_ID "com.rubblesonic.orbitfall"
    PLUGIN_MANUFACTURER_CODE RbSo
    PLUGIN_CODE Orbf
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "Orbitfall"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE)

target_sources(Orbitfall
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)

target_compile_definitions(Orbitfall
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Orbitfall
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if (APPLE)
    set_target_properties(Orbitfall PROPERTIES
        XCODE_ATTRIBUTE_ENABLE_APP_SANDBOX "NO"
        XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "NO"
        XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO")
endif()
