# ==============================================================================
#  ENH Master — adaptive clarity / footstep / sub enhancer with a 3D hardware UI
# ==============================================================================
cmake_minimum_required(VERSION 3.22)
project(EnhMaster VERSION 1.3.4.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

set(JUCE_PATH "$ENV{HOME}/JUCE" CACHE PATH "Path to the JUCE source tree")
option(ENH_BUILD_STANDALONE "Also build a Standalone app" ON)
option(ENH_BUILD_TESTS "Build the offline DSP test/benchmark tool" ON)
option(ENH_BUILD_TOOLS "Build LayoutViz (layout analysis / visualisation, no effect on the plugin)" ON)
option(ENH_COPY_PLUGIN "Install the VST3 into the user plug-in folder after building (replacing the installed one)" ON)

add_subdirectory(${JUCE_PATH} JUCE EXCLUDE_FROM_ALL)

# Shared 3D hardware model / material library (reused by other Khris Audio plugins)
set(HARDWAREKIT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../HardwareKit" CACHE PATH "HardwareKit checkout")
juce_add_module("${HARDWAREKIT_PATH}/modules/hardwarekit")
if(UNIX AND NOT APPLE)
    find_package(X11 REQUIRED)   # HardwareKit polls the pointer from the X server on Linux
endif()

set(ENH_FORMATS VST3)
if(ENH_BUILD_STANDALONE)
    list(APPEND ENH_FORMATS Standalone)
endif()

set(ENH_DSP_SOURCES
    Source/DSP/BandAnalyzer.cpp
    Source/DSP/SpectralAnalyzer.cpp
    Source/DSP/FootstepDetector.cpp
    Source/DSP/HarmonicPlanner.cpp
    Source/DSP/AdaptiveEQ.cpp
    Source/DSP/SubEnhancer.cpp
    Source/DSP/AnalogStage.cpp
    Source/DSP/Seraph.cpp
    Source/DSP/DynamicCompressor.cpp
    Source/DSP/SpectralLeveler.cpp
    Source/DSP/SpectralLimiter.cpp
    Source/DSP/SpectrumScope.cpp
    Source/DSP/EnhEngine.cpp)

juce_add_plugin(EnhMaster
    COMPANY_NAME                "Khris Audio"
    PLUGIN_MANUFACTURER_CODE    Khra
    PLUGIN_CODE                 EnhM
    FORMATS                     ${ENH_FORMATS}
    PRODUCT_NAME                "ENH Master"
    BUNDLE_ID                   "com.khrisaudio.enhmaster"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES             Fx Dynamics EQ
    COPY_PLUGIN_AFTER_BUILD     ${ENH_COPY_PLUGIN})

target_sources(EnhMaster PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp

    Source/Parameters/ParameterSpecs.cpp
    Source/Parameters/ParameterBridge.cpp
    Source/Parameters/PresetLibrary.cpp

    Source/Config/UIConfig.cpp

    ${ENH_DSP_SOURCES}

    Source/UI/HardwareView.cpp
    Source/UI/GlassPanel.cpp
    Source/UI/Scene/GeometryFactory.cpp
    Source/UI/Scene/PanelArtwork.cpp
    Source/UI/Scene/LayoutAudit.cpp
    Source/UI/Scene/HardwareRenderer.cpp)

target_include_directories(EnhMaster PRIVATE Source)

target_compile_definitions(EnhMaster PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(EnhMaster
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_opengl
        hardwarekit
        $<$<PLATFORM_ID:Linux>:X11::X11>
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

# ------------------------------------------------------------------------------
if(ENH_BUILD_TESTS)
    juce_add_console_app(EnhDspTests PRODUCT_NAME "EnhDspTests")

    target_sources(EnhDspTests PRIVATE Tests/EnhDspTests.cpp Source/Parameters/ParameterSpecs.cpp Source/Parameters/PresetLibrary.cpp ${ENH_DSP_SOURCES})
    target_include_directories(EnhDspTests PRIVATE Source)
    target_compile_definitions(EnhDspTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
                               ENH_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")   # finds Vault/ from any working directory
    target_link_libraries(EnhDspTests
        PRIVATE
            juce::juce_dsp
            juce::juce_audio_basics
            juce::juce_audio_formats
            juce::juce_audio_processors   # the parameter specs, for the factory-preset tests
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)
endif()

# ------------------------------------------------------------------------------
# LayoutViz: reads the real layout (DeviceLayout.h) and camera (CameraRig.h) and draws them as an SVG
# for a chosen editor size. Analysis only - it does not touch the plugin. See Tools/LayoutViz.cpp.
if(ENH_BUILD_TOOLS)
    # EnhAudioLab: renders audio through the engine and writes reports, spectrograms and spectra (Tools/AudioLab.cpp)
    juce_add_console_app(EnhAudioLab PRODUCT_NAME "EnhAudioLab")
    target_sources(EnhAudioLab PRIVATE Tools/AudioLab.cpp Source/Parameters/ParameterSpecs.cpp Source/Parameters/PresetLibrary.cpp ${ENH_DSP_SOURCES})
    target_include_directories(EnhAudioLab PRIVATE Source)
    target_compile_definitions(EnhAudioLab PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(EnhAudioLab
        PRIVATE
            juce::juce_dsp
            juce::juce_audio_basics
            juce::juce_audio_formats
            juce::juce_audio_processors
            juce::juce_graphics
        PUBLIC
            juce::juce_recommended_config_flags)

    juce_add_console_app(LayoutViz PRODUCT_NAME "LayoutViz")
    target_sources(LayoutViz PRIVATE Tools/LayoutViz.cpp Source/Parameters/ParameterSpecs.cpp)
    target_include_directories(LayoutViz PRIVATE Source)
    target_compile_definitions(LayoutViz PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(LayoutViz
        PRIVATE
            hardwarekit
            juce::juce_audio_processors
        PUBLIC
            juce::juce_recommended_config_flags)
endif()
