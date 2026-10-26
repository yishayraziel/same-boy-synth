cmake_minimum_required(VERSION 3.22)

set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal binary")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(SameBoy VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE: use ./JUCE if present, otherwise fetch it
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
                              GIT_TAG 8.0.4 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

set(SAMEBOY_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND SAMEBOY_FORMATS AU)
endif()

juce_add_plugin(SameBoy
    COMPANY_NAME "YRaziel"
    COMPANY_WEBSITE "https://yraziel.com"
    BUNDLE_ID com.yraziel.sameboy
    PLUGIN_MANUFACTURER_CODE Yraz
    PLUGIN_CODE SmBy
    PRODUCT_NAME "Same Boy"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    FORMATS ${SAMEBOY_FORMATS})

juce_add_binary_data(SameBoyData SOURCES Resources/plate.png)

target_sources(SameBoy PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(SameBoy PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(SameBoy
    PRIVATE
        SameBoyData
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

option(SAMEBOY_TESTS "Build offline render test" OFF)
if(SAMEBOY_TESTS)
    juce_add_console_app(SameBoyTest PRODUCT_NAME "SameBoyTest")
    target_sources(SameBoyTest PRIVATE tests/RenderTest.cpp)
    target_compile_definitions(SameBoyTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(SameBoyTest PRIVATE SameBoy juce::juce_audio_utils juce::juce_dsp)
endif()
