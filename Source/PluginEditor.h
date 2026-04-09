#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

class HeatDeathProcessor;  // forward-declare using actual class name from PluginProcessor.h

class HeatDeathEditor : public juce::AudioProcessorEditor
{
public:
    explicit HeatDeathEditor (HeatDeathProcessor&);
    ~HeatDeathEditor() override;

    void paint   (juce::Graphics&) override;
    void resized () override;

private:
    HeatDeathProcessor& processorRef;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HeatDeathEditor)
};
