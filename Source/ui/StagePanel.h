#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class StagePanel : public juce::Component
{
public:
    StagePanel()  = default;
    ~StagePanel() override = default;

    void paint   (juce::Graphics&) override {}
    void resized () override {}
};
