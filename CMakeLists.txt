cmake_minimum_required(VERSION 3.22)
project(HEATDEATH VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# macOS universal binary (arm64 + x86_64)
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "" FORCE)
endif()

# ==============================================================================
# Fetch JUCE 7+ via FetchContent
# ==============================================================================

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        7.0.12
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# ==============================================================================
# Plugin target declaration
# ==============================================================================

juce_add_plugin(HEATDEATH
    COMPANY_NAME                 "Tim Hecker Audio"
    BUNDLE_ID                    "com.timheckeraudio.heatdeath"
    PLUGIN_MANUFACTURER_CODE     TKHA
    PLUGIN_CODE                  HDTH
    PRODUCT_NAME                 "HEATDEATH"
    FORMATS                      VST3 AU
    VST3_CATEGORIES              "Fx" "Distortion"
    AU_MAIN_TYPE                 kAudioUnitType_Effect
    IS_SYNTH                     FALSE
    NEEDS_MIDI_INPUT             FALSE
    NEEDS_MIDI_OUTPUT            FALSE
    IS_MIDI_EFFECT               FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS  FALSE
    COPY_PLUGIN_AFTER_BUILD      TRUE
    PLUGIN_NAME                  "HEATDEATH"
)

# ==============================================================================
# Sources — files created in Plan 01-02
# ==============================================================================

target_sources(HEATDEATH PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Parameters.cpp
    Source/dsp/TurboRat.cpp
    Source/dsp/MicroPitch.cpp
    Source/dsp/H3000Undulator.cpp
    Source/dsp/TapeBurnIn.cpp
    Source/ui/HeatdeathLookAndFeel.cpp
    Source/ui/StagePanel.cpp
)

target_include_directories(HEATDEATH PRIVATE Source)

# ==============================================================================
# Compile definitions
# ==============================================================================

target_compile_definitions(HEATDEATH PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

# ==============================================================================
# Link JUCE modules
# ==============================================================================

target_link_libraries(HEATDEATH PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_audio_plugin_client
    juce::juce_dsp
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_graphics
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

# ==============================================================================
# Unit test target for SmoothParam (Phase 02-02)
# ==============================================================================

add_executable(SmoothParamTest Tests/SmoothParamTest.cpp)
target_include_directories(SmoothParamTest PRIVATE Source)
target_compile_features(SmoothParamTest PRIVATE cxx_std_17)
target_compile_definitions(SmoothParamTest PRIVATE
    JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1
    JUCE_MODULE_AVAILABLE_juce_core=1
)
target_link_libraries(SmoothParamTest PRIVATE
    juce::juce_core
)

# ==============================================================================
# Unit test target for TurboRat (Phase 03)
# ==============================================================================

add_executable(TurboRatTest Tests/TurboRatTest.cpp
    Source/dsp/TurboRat.cpp)
target_include_directories(TurboRatTest PRIVATE Source)
target_compile_features(TurboRatTest PRIVATE cxx_std_17)
target_link_libraries(TurboRatTest PRIVATE
    juce::juce_core
    juce::juce_audio_basics
    juce::juce_dsp
    juce::juce_audio_formats
    juce::juce_audio_processors
)
