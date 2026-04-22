# =============================================================================
#  PresetBrowser — CMakeLists.txt integration snippet
# =============================================================================
#
#  This is NOT a standalone CMakeLists — it shows the lines you need to add
#  to your existing plugin's CMakeLists.txt.
#
#  Assumes you already have a juce_add_plugin() target called MyPlugin
#  and a juce_add_binary_data() target called MyPluginData.
# =============================================================================

# 1. Add the PresetBrowser source files to your plugin target
target_sources(MyPlugin
    PRIVATE
        Source/PresetBrowser/PresetManager.cpp
        Source/PresetBrowser/PresetBrowserComponent.cpp
)

# 2. Add the PresetBrowser folder to your include path
#    (allows #include "PresetBrowser/PresetManager.h" from anywhere in Source/)
target_include_directories(MyPlugin
    PRIVATE
        Source
)

# 3. Factory presets — add your .preset XML files to BinaryData
#    Then register them in your PluginProcessor constructor via:
#    presetManager->registerFactoryPreset("01 - Name", BinaryData::_01_Name_preset)
juce_add_binary_data(MyPluginData
    SOURCES
        Resources/Factory\ Presets/01\ -\ The\ Analog\ Bus.preset
        Resources/Factory\ Presets/02\ -\ Smooth\ Bus.preset
        # ... add one line per factory preset
)

target_link_libraries(MyPlugin
    PRIVATE
        MyPluginData
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_gui_basics
)
