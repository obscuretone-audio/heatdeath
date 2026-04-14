#include "HeatdeathLookAndFeel.h"

HeatdeathLookAndFeel::HeatdeathLookAndFeel()
{
    setColour (juce::Label::textColourId, juce::Colour (HD::colLabel));
    setColour (juce::Slider::rotarySliderFillColourId,    juce::Colour (HD::colAccent));
    setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (HD::colTrack));
    setColour (juce::TextButton::buttonColourId,   juce::Colours::transparentBlack);
    setColour (juce::TextButton::buttonOnColourId, juce::Colour (HD::colAccent));
    setColour (juce::TextButton::textColourOffId,  juce::Colour (HD::colLabelDim));
    setColour (juce::TextButton::textColourOnId,   juce::Colour (HD::colBackground));
}

//==============================================================================
void HeatdeathLookAndFeel::drawRotarySlider (
    juce::Graphics& g,
    int x, int y, int width, int height,
    float sliderPos, float startAngle, float endAngle,
    juce::Slider&)
{
    const float cx = x + width  * 0.5f;
    const float cy = y + height * 0.5f;
    const float r  = std::min (width, height) * 0.5f - 4.0f;
    if (r < 2.0f) return;

    const float angle = startAngle + sliderPos * (endAngle - startAngle);

    // Knob face
    g.setColour (juce::Colour (HD::colKnobFace));
    g.fillEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);
    g.setColour (juce::Colour (HD::colBorder));
    g.drawEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f, 1.0f);

    // Track arc (full range, dim)
    {
        juce::Path p;
        p.addArc (cx - r, cy - r, r * 2.0f, r * 2.0f, startAngle, endAngle, true);
        g.setColour (juce::Colour (HD::colTrack));
        g.strokePath (p, juce::PathStrokeType (2.5f,
            juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Value arc (from start to current angle)
    if (std::abs (angle - startAngle) > 0.005f)
    {
        juce::Path p;
        p.addArc (cx - r, cy - r, r * 2.0f, r * 2.0f, startAngle, angle, true);
        g.setColour (juce::Colour (HD::colAccent));
        g.strokePath (p, juce::PathStrokeType (2.5f,
            juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Center dot
    g.setColour (juce::Colour (HD::colAccent));
    g.fillEllipse (cx - 2.5f, cy - 2.5f, 5.0f, 5.0f);

    // Indicator line from center to rim
    {
        const float s = std::sin (angle);
        const float c = -std::cos (angle);
        g.drawLine (cx, cy, cx + s * (r - 3.0f), cy + c * (r - 3.0f), 1.5f);
    }

    // 8 tick marks around the arc
    for (int i = 0; i < 8; ++i)
    {
        const float a  = startAngle + float (i) / 7.0f * (endAngle - startAngle);
        const float s  = std::sin (a);
        const float c  = -std::cos (a);
        g.setColour (juce::Colour (HD::colBorder));
        g.drawLine (cx + s * (r + 1.5f), cy + c * (r + 1.5f),
                    cx + s * (r + 3.5f), cy + c * (r + 3.5f), 0.8f);
    }
}

//==============================================================================
void HeatdeathLookAndFeel::drawButtonBackground (
    juce::Graphics& g, juce::Button& b,
    const juce::Colour& /*bgColour*/, bool /*mouseOver*/, bool /*isDown*/)
{
    const auto bounds = b.getLocalBounds().toFloat().reduced (0.5f);

    if (b.getToggleState())
    {
        g.setColour (juce::Colour (HD::colAccent));
        g.fillRoundedRectangle (bounds, 2.0f);
        g.drawRoundedRectangle (bounds, 2.0f, 1.5f);
    }
    else
    {
        g.setColour (juce::Colours::transparentBlack);
        g.fillRoundedRectangle (bounds, 2.0f);
        g.setColour (juce::Colour (HD::colBorder));
        g.drawRoundedRectangle (bounds, 2.0f, 1.5f);
    }
}

void HeatdeathLookAndFeel::drawButtonText (
    juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.setColour (b.getToggleState() ? juce::Colour (HD::colBackground)
                                    : juce::Colour (HD::colLabelDim));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds(),
                      juce::Justification::centred, 1);
}

juce::Font HeatdeathLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return HD::monoFont (std::min (float (buttonHeight) * 0.48f, 8.0f));
}
