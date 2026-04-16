#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
// Colour palette and font helper — matches heatdeath_ui.html exactly
//==============================================================================
namespace HD
{
    static constexpr uint32_t colBackground = 0xffd8d4cc;  // shell panel bg
    static constexpr uint32_t colOuter      = 0xffc0bbb3;  // outer body (unused in VST)
    static constexpr uint32_t colAccent     = 0xff2a2520;  // dark brown — active/text
    static constexpr uint32_t colTrack      = 0xffb0aa9f;  // knob track arc
    static constexpr uint32_t colKnobFace   = 0xffc8c4bc;  // knob body fill
    static constexpr uint32_t colBorder     = 0xff888888;  // borders, tick marks
    static constexpr uint32_t colLabel      = 0xff555555;  // knob name labels
    static constexpr uint32_t colLabelDim   = 0xff888888;  // dim labels / sub-heads
    static constexpr uint32_t colDivider    = 0xffb0aa9f;  // section lines

    // Rotary start/end angles: 225° to 495° (270° sweep, 7:30–4:30 on clock face)
    static constexpr float rotaryStart = juce::MathConstants<float>::pi * 1.25f;
    static constexpr float rotaryEnd   = juce::MathConstants<float>::pi * 2.75f;

    inline juce::Font monoFont (float size)
    {
        // Jost (https://fonts.google.com/specimen/Jost) — embed via BinaryData for distribution
        return juce::Font ("Jost", size, juce::Font::plain);
    }
}

//==============================================================================
class HeatdeathLookAndFeel : public juce::LookAndFeel_V4
{
public:
    HeatdeathLookAndFeel();
    ~HeatdeathLookAndFeel() override = default;

    // Knob (rotary slider)
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                           float sliderPos, float startAngle, float endAngle,
                           juce::Slider&) override;

    // TextButton — used for clip mode, shape, wide, and style selectors
    void drawButtonBackground (juce::Graphics&, juce::Button&,
                               const juce::Colour&, bool mouseOver, bool isDown) override;

    void drawButtonText (juce::Graphics&, juce::TextButton&,
                         bool mouseOver, bool isDown) override;

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HeatdeathLookAndFeel)
};
