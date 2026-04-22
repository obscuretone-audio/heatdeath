#pragma once
#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>

/**
 * PresetManager
 * =============
 * Drop-in preset management for any JUCE plugin.
 *
 * Handles:
 *   - Factory presets (bundled in BinaryData or a sub-folder)
 *   - User presets (stored in appdata / Documents)
 *   - Save / Save As / Load / Delete
 *   - Undo of last parameter state on Reset
 *   - Change broadcasting so the browser UI stays in sync
 *
 * Usage (in PluginProcessor):
 *   presetManager = std::make_unique<PresetManager>(apvts);
 *
 * To save the current preset name across DAW sessions, call
 *   presetManager->saveCurrentPresetNameToState(state)
 *   presetManager->loadCurrentPresetNameFromState(state)
 * from getStateInformation / setStateInformation.
 */
class PresetManager : public juce::ChangeBroadcaster
{
public:
    // -------------------------------------------------------------------------
    // Constants
    // -------------------------------------------------------------------------
    static constexpr const char* FACTORY_SUBFOLDER = "Factory Presets";
    static constexpr const char* USER_SUBFOLDER    = "User Presets";
    static constexpr const char* PRESET_EXTENSION  = ".preset";
    static constexpr const char* NO_PRESET_NAME    = "No preset";

    // -------------------------------------------------------------------------
    // Construction
    // -------------------------------------------------------------------------
    /**
     * @param apvts          The plugin's AudioProcessorValueTreeState
     * @param pluginName     Used to build the user-preset folder path
     *                       e.g. "MyCompany/MyPlugin"
     */
    explicit PresetManager (juce::AudioProcessorValueTreeState& apvts,
                            const juce::String& pluginName = "MyPlugin");

    ~PresetManager() override = default;

    // -------------------------------------------------------------------------
    // Preset lists
    // -------------------------------------------------------------------------
    /** Returns all factory preset names (no extension, sorted). */
    juce::StringArray getFactoryPresetNames() const;

    /** Returns all user preset names (no extension, sorted). */
    juce::StringArray getUserPresetNames() const;

    /** Combined list: factory first, then user. */
    juce::StringArray getAllPresetNames() const;

    // -------------------------------------------------------------------------
    // Loading
    // -------------------------------------------------------------------------
    /** Load a preset by exact name (searches factory then user). */
    bool loadPreset (const juce::String& name);

    /** Load the next preset in the combined list (wraps). */
    void loadNextPreset();

    /** Load the previous preset in the combined list (wraps). */
    void loadPreviousPreset();

    // -------------------------------------------------------------------------
    // Saving
    // -------------------------------------------------------------------------
    /**
     * Save the current parameters over an existing user preset.
     * If the current preset is a factory preset or "No preset", does nothing
     * and returns false — use saveAsPreset() instead.
     */
    bool saveCurrentPreset();

    /**
     * Save the current parameters as a new user preset with the given name.
     * Overwrites any existing user preset with the same name.
     */
    bool saveAsPreset (const juce::String& name);

    /**
     * Delete the named user preset from disk.
     * Returns false if it is a factory preset or doesn't exist.
     */
    bool deletePreset (const juce::String& name);

    // -------------------------------------------------------------------------
    // Reset
    // -------------------------------------------------------------------------
    /** Restore the parameter values to what they were before the last load/save. */
    void resetToDefault();

    // -------------------------------------------------------------------------
    // Query
    // -------------------------------------------------------------------------
    juce::String getCurrentPresetName() const { return currentPresetName_; }

    bool isFactoryPreset (const juce::String& name) const;
    bool isUserPreset    (const juce::String& name) const;

    /** Index into getAllPresetNames(), or -1 if "No preset". */
    int getCurrentPresetIndex() const;

    // -------------------------------------------------------------------------
    // DAW session persistence
    // -------------------------------------------------------------------------
    void saveCurrentPresetNameToState  (juce::ValueTree& state) const;
    void loadCurrentPresetNameFromState(const juce::ValueTree& state);

    // -------------------------------------------------------------------------
    // Factory preset folder (for BinaryData or bundled files)
    // -------------------------------------------------------------------------
    /**
     * Optional: call this once at startup to register XML strings from BinaryData.
     * Each entry is { "Preset Name", xmlString }.
     */
    void registerFactoryPreset (const juce::String& name,
                                const juce::String& xmlContent);

    // -------------------------------------------------------------------------
    // Reveal in Finder/Explorer
    // -------------------------------------------------------------------------
    void revealUserPresetsFolder() const;

private:
    // -------------------------------------------------------------------------
    juce::AudioProcessorValueTreeState& apvts_;
    juce::String pluginName_;

    juce::File userPresetsDir_;
    juce::String currentPresetName_;

    /** Saved before every load/save so Reset can undo it. */
    juce::ValueTree preResetState_;

    /** In-memory factory presets (name → XML string). */
    juce::HashMap<juce::String, juce::String> factoryPresets_;

    // -------------------------------------------------------------------------
    void setCurrentPresetName (const juce::String& name);
    bool loadPresetFromXml    (const juce::String& xmlString);
    juce::String serializeCurrentState() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetManager)
};
