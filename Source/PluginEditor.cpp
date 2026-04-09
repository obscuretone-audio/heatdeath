#include "PluginEditor.h"
#include "PluginProcessor.h"

HeatDeathEditor::HeatDeathEditor (HeatDeathProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p)
{
    setSize (800, 540);          // UI-01: fixed 800x540, non-resizable (Phase 7 wires LookAndFeel)
    setResizable (false, false);
}

HeatDeathEditor::~HeatDeathEditor() = default;

void HeatDeathEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff111111));   // #111111 per UI-02
    g.setColour (juce::Colour (0xffc8a96e)); // worn gold accent per UI-02
    g.setFont (juce::Font (juce::Font::getDefaultMonospacedFontName(), 24.0f, juce::Font::plain));
    g.drawText ("HEATDEATH", getLocalBounds(), juce::Justification::centred, false);
}

void HeatDeathEditor::resized() {}
