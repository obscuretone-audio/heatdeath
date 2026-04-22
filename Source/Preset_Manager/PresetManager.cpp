#include "PresetManager.h"

// =============================================================================
PresetManager::PresetManager (juce::AudioProcessorValueTreeState& apvts,
                              const juce::String& pluginName)
    : apvts_ (apvts)
    , pluginName_ (pluginName)
    , currentPresetName_ (NO_PRESET_NAME)
{
    // Build user-preset directory:
    //   macOS : ~/Music/<pluginName>/User Presets/
    //   Win   : Documents/<pluginName>/User Presets/
    auto base = juce::File::getSpecialLocation (
                    juce::File::userMusicDirectory)
                    .getChildFile (pluginName)
                    .getChildFile (USER_SUBFOLDER);

    userPresetsDir_ = base;
    if (! userPresetsDir_.exists())
        userPresetsDir_.createDirectory();
}

// =============================================================================
//  Factory presets
// =============================================================================
void PresetManager::registerFactoryPreset (const juce::String& name,
                                           const juce::String& xmlContent)
{
    factoryPresets_.set (name, xmlContent);
}

// =============================================================================
//  Lists
// =============================================================================
juce::StringArray PresetManager::getFactoryPresetNames() const
{
    juce::StringArray names;

    // From in-memory registry
    for (auto it = factoryPresets_.begin(); it != factoryPresets_.end(); ++it)
        names.add (it.getKey());

    names.sortNatural();
    return names;
}

juce::StringArray PresetManager::getUserPresetNames() const
{
    juce::StringArray names;

    auto files = userPresetsDir_.findChildFiles (
                     juce::File::findFiles, false,
                     juce::String ("*") + PRESET_EXTENSION);

    for (auto& f : files)
        names.add (f.getFileNameWithoutExtension());

    names.sortNatural();
    return names;
}

juce::StringArray PresetManager::getAllPresetNames() const
{
    auto all = getFactoryPresetNames();
    all.addArray (getUserPresetNames());
    return all;
}

// =============================================================================
//  Loading
// =============================================================================
bool PresetManager::loadPreset (const juce::String& name)
{
    // Snapshot for Reset
    preResetState_ = apvts_.copyState();

    // Try factory first
    if (factoryPresets_.contains (name))
    {
        if (loadPresetFromXml (factoryPresets_[name]))
        {
            setCurrentPresetName (name);
            return true;
        }
        return false;
    }

    // Try user preset on disk
    auto file = userPresetsDir_.getChildFile (name + PRESET_EXTENSION);
    if (file.existsAsFile())
    {
        if (loadPresetFromXml (file.loadFileAsString()))
        {
            setCurrentPresetName (name);
            return true;
        }
    }

    return false;
}

void PresetManager::loadNextPreset()
{
    auto all = getAllPresetNames();
    if (all.isEmpty()) return;

    int idx = getCurrentPresetIndex();
    loadPreset (all[(idx + 1) % all.size()]);
}

void PresetManager::loadPreviousPreset()
{
    auto all = getAllPresetNames();
    if (all.isEmpty()) return;

    int idx = getCurrentPresetIndex();
    if (idx <= 0) idx = all.size();
    loadPreset (all[(idx - 1) % all.size()]);
}

// =============================================================================
//  Saving
// =============================================================================
bool PresetManager::saveCurrentPreset()
{
    if (currentPresetName_ == NO_PRESET_NAME)
        return false;
    if (isFactoryPreset (currentPresetName_))
        return false;

    return saveAsPreset (currentPresetName_);
}

bool PresetManager::saveAsPreset (const juce::String& name)
{
    // Prevent user presets from shadowing factory preset names
    if (isFactoryPreset (name))
        return false;

    auto file = userPresetsDir_.getChildFile (name + PRESET_EXTENSION);
    auto xml  = serializeCurrentState();
    if (file.replaceWithText (xml))
    {
        setCurrentPresetName (name);
        return true;
    }
    return false;
}

bool PresetManager::deletePreset (const juce::String& name)
{
    if (isFactoryPreset (name)) return false;

    auto file = userPresetsDir_.getChildFile (name + PRESET_EXTENSION);
    if (file.existsAsFile() && file.deleteFile())
    {
        if (currentPresetName_ == name)
            setCurrentPresetName (NO_PRESET_NAME);
        sendChangeMessage();
        return true;
    }
    return false;
}

// =============================================================================
//  Reset
// =============================================================================
void PresetManager::resetToDefault()
{
    if (preResetState_.isValid())
    {
        apvts_.replaceState (preResetState_);
        setCurrentPresetName (NO_PRESET_NAME);
    }
}

// =============================================================================
//  Query helpers
// =============================================================================
bool PresetManager::isFactoryPreset (const juce::String& name) const
{
    return factoryPresets_.contains (name);
}

bool PresetManager::isUserPreset (const juce::String& name) const
{
    return userPresetsDir_.getChildFile (name + PRESET_EXTENSION).existsAsFile();
}

int PresetManager::getCurrentPresetIndex() const
{
    return getAllPresetNames().indexOf (currentPresetName_);
}

// =============================================================================
//  DAW session persistence
// =============================================================================
void PresetManager::saveCurrentPresetNameToState (juce::ValueTree& state) const
{
    state.setProperty ("currentPreset", currentPresetName_, nullptr);
}

void PresetManager::loadCurrentPresetNameFromState (const juce::ValueTree& state)
{
    currentPresetName_ = state.getProperty ("currentPreset",
                                            juce::String (NO_PRESET_NAME));
}

// =============================================================================
//  Reveal
// =============================================================================
void PresetManager::revealUserPresetsFolder() const
{
    userPresetsDir_.revealToUser();
}

// =============================================================================
//  Private helpers
// =============================================================================
void PresetManager::setCurrentPresetName (const juce::String& name)
{
    currentPresetName_ = name;
    sendChangeMessage();
}

bool PresetManager::loadPresetFromXml (const juce::String& xmlString)
{
    auto xml = juce::XmlDocument::parse (xmlString);
    if (xml == nullptr) return false;

    auto tree = juce::ValueTree::fromXml (*xml);
    if (! tree.isValid()) return false;

    apvts_.replaceState (tree);
    return true;
}

juce::String PresetManager::serializeCurrentState() const
{
    auto xml = apvts_.copyState().createXml();
    return xml ? xml->toString() : juce::String{};
}
