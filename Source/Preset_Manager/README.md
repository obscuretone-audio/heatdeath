# PresetBrowser — Drop-in JUCE Preset System

A self-contained, reusable preset browser you can add to any JUCE VST3/AU plugin.
Matches the style in the reference screenshot: dark chrome, nav arrows, toolbar,
and a scrollable Factory / User preset list.

---

## Files

```
Source/PresetBrowser/
├── PresetManager.h / .cpp          ← file I/O, save/load, broadcasting
└── PresetBrowserComponent.h / .cpp ← the full UI (nav bar + popup)
```

---

## Integration — 5 steps

### 1. Copy the folder

Drop `Source/PresetBrowser/` into your plugin's `Source/` directory and add both
`.cpp` files to your CMakeLists target sources.

```cmake
target_sources(MyPlugin PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/PresetBrowser/PresetManager.cpp          # ← add
    Source/PresetBrowser/PresetBrowserComponent.cpp # ← add
)
```

---

### 2. Add PresetManager to your PluginProcessor

**PluginProcessor.h**
```cpp
#include "PresetBrowser/PresetManager.h"

class MyPluginAudioProcessor : public juce::AudioProcessor
{
public:
    juce::AudioProcessorValueTreeState apvts;
    std::unique_ptr<PresetManager>     presetManager;
    // ...
};
```

**PluginProcessor.cpp constructor**
```cpp
MyPluginAudioProcessor::MyPluginAudioProcessor()
    : apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    // "MyCompany/MyPlugin" → ~/Music/MyCompany/MyPlugin/User Presets/
    presetManager = std::make_unique<PresetManager>(apvts, "MyCompany/MyPlugin");

    // Register factory presets from BinaryData
    // (Add .preset XML files to your JUCE BinaryData target)
    presetManager->registerFactoryPreset("01 - The Analog Bus",
                                         BinaryData::_01_The_Analog_Bus_preset);
    // ... repeat for each factory preset
}
```

**getStateInformation / setStateInformation**
```cpp
void MyPluginAudioProcessor::getStateInformation (juce::MemoryBlock& data)
{
    auto state = apvts.copyState();
    presetManager->saveCurrentPresetNameToState(state); // ← saves preset name
    auto xml = state.createXml();
    copyXmlToBinary(*xml, data);
}

void MyPluginAudioProcessor::setStateInformation (const void* data, int size)
{
    auto xml   = getXmlFromBinary(data, size);
    auto state = juce::ValueTree::fromXml(*xml);
    apvts.replaceState(state);
    presetManager->loadCurrentPresetNameFromState(state); // ← restores preset name
}
```

---

### 3. Add the nav bar to your PluginEditor

**PluginEditor.h**
```cpp
#include "PresetBrowser/PresetBrowserComponent.h"

class MyPluginAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    // ...
private:
    MyPluginAudioProcessor& processor_;
    PresetBrowserNavBar     presetNavBar_;
};
```

**PluginEditor.cpp constructor**
```cpp
MyPluginAudioProcessorEditor::MyPluginAudioProcessorEditor(MyPluginAudioProcessor& p)
    : AudioProcessorEditor(&p)
    , processor_(p)
    , presetNavBar_(*p.presetManager)   // ← just pass the manager
{
    addAndMakeVisible(presetNavBar_);
    setSize(500, 400);
}
```

**resized()**
```cpp
void MyPluginAudioProcessorEditor::resized()
{
    auto r = getLocalBounds();
    presetNavBar_.setBounds(r.removeFromTop(36));  // ← 36px nav strip at top
    // ... lay out the rest of your UI in r
}
```

---

### 4. Create factory presets

1. Open your plugin, dial in a sound.
2. Call `presetManager->saveAsPreset("01 - The Analog Bus")` once to generate
   the XML on disk.
3. Copy the `.preset` file into a `Resources/Factory Presets/` folder.
4. Add it to your CMakeLists BinaryData:
   ```cmake
   juce_add_binary_data(MyPluginData SOURCES
       Resources/Factory Presets/01 - The Analog Bus.preset
       Resources/Factory Presets/02 - Smooth Bus.preset
   )
   target_link_libraries(MyPlugin PRIVATE MyPluginData)
   ```
5. Register them in the constructor (step 2 above).

---

### 5. User preset location

User presets are saved to:
- **macOS**: `~/Music/<pluginName>/User Presets/`
- **Windows**: `Documents/<pluginName>/User Presets/`

The "Reveal" button opens this folder in Finder/Explorer.

---

## Customising colours

All colours are defined as `static const` members in `PresetBrowserColours`
at the top of `PresetBrowserComponent.cpp`. Change them there or subclass and
override to integrate with your LookAndFeel.

```cpp
// Example overrides in PresetBrowserComponent.cpp:
const juce::Colour PresetBrowserColours::rowSelected { 0xFF7B3F00 }; // warm amber
const juce::Colour PresetBrowserColours::navBar      { 0xFF1A1A2E }; // deep navy
```

---

## Toolbar actions

| Button   | Action                                                          |
|----------|-----------------------------------------------------------------|
| Save     | Overwrites the current user preset (no-op on factory presets)  |
| Save As  | Prompts for a name, saves as new user preset                    |
| Refresh  | Re-scans the user preset folder                                 |
| Reveal   | Opens user preset folder in Finder / Explorer                   |
| Reset    | Restores parameter state from before the last load/save         |

---

## Dependencies

- JUCE 7 or 8 (uses `AudioProcessorValueTreeState`, `ListBoxModel`, `ChangeBroadcaster`)
- No third-party libraries
