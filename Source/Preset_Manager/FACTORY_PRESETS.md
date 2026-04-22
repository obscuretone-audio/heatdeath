# Creating Factory Presets

Factory presets are baked into the plugin binary at build time using JUCE's
BinaryData system. They are read-only — users can load them but cannot overwrite
or delete them.

The authoring workflow uses your own plugin as the sound design tool, then
promotes the saved files into the project.

---

## Step 1 — Disable factory preset registration temporarily

In `PluginProcessor.cpp`, comment out all `registerFactoryPreset()` calls so the
browser has no factory presets loaded. This ensures the names you save won't be
blocked by the factory-name guard.

```cpp
// Comment these out while authoring:
// presetManager->registerFactoryPreset("01 - The Analog Bus", ...);
// presetManager->registerFactoryPreset("02 - Smooth Bus", ...);
```

---

## Step 2 — Build and open the plugin

Build and load the plugin in a DAW or the standalone target — whichever is more
comfortable for sound design.

---

## Step 3 — Dial in a sound and save

Use the preset browser's **Save As** button. Name each preset exactly as you want
it to appear in the list. The `01 -` number prefix is optional but recommended —
it keeps the list sorted correctly.

```
01 - The Analog Bus
02 - Smooth Bus
03 - Contour
```

Repeat for every factory preset you want to ship.

---

## Step 4 — Find the saved files

The `.preset` files are saved to:

| Platform | Path |
|----------|------|
| macOS    | `~/Music/<PluginName>/User Presets/` |
| Windows  | `Documents\<PluginName>\User Presets\` |

Each file is plain XML — you can open and edit values directly if needed.

---

## Step 5 — Copy them into your project

Create a `Resources/Factory Presets/` folder in your project and copy the files in:

```
MyPlugin/
└── Resources/
    └── Factory Presets/
        ├── 01 - The Analog Bus.preset
        ├── 02 - Smooth Bus.preset
        └── 03 - Contour.preset
```

---

## Step 6 — Add them to CMakeLists.txt

Register each file with `juce_add_binary_data`:

```cmake
juce_add_binary_data(MyPluginData
    SOURCES
        "Resources/Factory Presets/01 - The Analog Bus.preset"
        "Resources/Factory Presets/02 - Smooth Bus.preset"
        "Resources/Factory Presets/03 - Contour.preset"
)
```

---

## Step 7 — Build once and check the generated names

JUCE mangles filenames into valid C++ identifiers. After building, open:

```
build/MyPlugin_artefacts/JuceLibraryCode/BinaryData.h
```

You will see declarations like:

```cpp
extern const char* _01___The_Analog_Bus_preset;
extern const char* _02___Smooth_Bus_preset;
extern const char* _03___Contour_preset;
```

The mangling rules are:
- Leading digits get a `_` prefix
- Spaces and hyphens become `_`
- The file extension `.preset` becomes `_preset`

---

## Step 8 — Register the presets in PluginProcessor.cpp

Uncomment and fill in the `registerFactoryPreset()` calls using the exact
BinaryData names from Step 7:

```cpp
presetManager->registerFactoryPreset(
    "01 - The Analog Bus",
    BinaryData::_01___The_Analog_Bus_preset
);
presetManager->registerFactoryPreset(
    "02 - Smooth Bus",
    BinaryData::_02___Smooth_Bus_preset
);
presetManager->registerFactoryPreset(
    "03 - Contour",
    BinaryData::_03___Contour_preset
);
```

The first argument is the display name shown in the browser. The second is the
BinaryData symbol.

---

## Step 9 — Delete the files from your user preset folder

Now that the presets are baked into the binary, delete the copies from your local
user preset folder so they don't appear twice in the browser:

- **macOS**: `~/Music/<PluginName>/User Presets/`
- **Windows**: `Documents\<PluginName>\User Presets\`

---

## Updating a factory preset

To update the sound of an existing factory preset:

1. Open the `.preset` file from `Resources/Factory Presets/` — it is plain XML
2. Edit the parameter values directly, or load the preset in the plugin, tweak,
   and use **Save As** with a temporary name to get fresh XML, then copy the
   contents back into the original file
3. Rebuild — the new values are picked up automatically via BinaryData

No changes to `CMakeLists.txt` or `PluginProcessor.cpp` are needed unless you
are adding or removing presets.

---

## Adding a preset to a shipped plugin

If you add a new factory preset in an update:

1. Follow Steps 3–8 above for the new preset only
2. Do not remove or rename existing factory presets — users may have DAW sessions
   that reference them by name
3. Bump your plugin version number so DAWs detect the update
