# ==============================================================================
#  PvP Adaptive Dynamics — Phase 1: plugin shell + 3D hardware UI (NO DSP)
# ==============================================================================
cmake_minimum_required(VERSION 3.22)
project(PvPAdaptiveDynamics VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

set(JUCE_PATH "$ENV{HOME}/JUCE" CACHE PATH "Path to the JUCE source tree")
option(PAD_BUILD_STANDALONE "Also build a Standalone app (handy for UI iteration)" ON)

add_subdirectory(${JUCE_PATH} JUCE EXCLUDE_FROM_ALL)

set(PAD_FORMATS VST3)
if(PAD_BUILD_STANDALONE)
    list(APPEND PAD_FORMATS Standalone)
endif()

juce_add_plugin(PvPAdaptiveDynamics
    COMPANY_NAME                "Khris Audio"
    PLUGIN_MANUFACTURER_CODE    Khra
    PLUGIN_CODE                 PvAd
    FORMATS                     ${PAD_FORMATS}
    PRODUCT_NAME                "PvP Adaptive Dynamics"
    BUNDLE_ID                   "com.khrisaudio.pvpadaptivedynamics"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES             Fx Dynamics
    COPY_PLUGIN_AFTER_BUILD     TRUE)

target_sources(PvPAdaptiveDynamics PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp

    Source/Parameters/ParameterSpecs.cpp
    Source/Parameters/ParameterBridge.cpp

    Source/Config/UIConfig.cpp

    Source/UI/HardwareView.cpp
    Source/UI/Render/GLResources.cpp
    Source/UI/Scene/GeometryFactory.cpp
    Source/UI/Scene/PanelArtwork.cpp
    Source/UI/Scene/HardwareRenderer.cpp)

target_include_directories(PvPAdaptiveDynamics PRIVATE Source)

target_compile_definitions(PvPAdaptiveDynamics PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(PvPAdaptiveDynamics
    PRIVATE
        juce::juce_audio_utils
        juce::juce_opengl
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
