#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "ui/HeatdeathLookAndFeel.h"

class HeatDeathProcessor;

//==============================================================================
class HeatDeathEditor : public juce::AudioProcessorEditor,
                        public juce::Timer
{
public:
    explicit HeatDeathEditor (HeatDeathProcessor&);
    ~HeatDeathEditor() override;

    void paint   (juce::Graphics&) override;
    void resized () override;
    void timerCallback() override;

private:
    //==========================================================================
    // Helpers
    //==========================================================================
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;

    void setupSlider (juce::Slider&, juce::Label& valueLabel,
                      std::function<juce::String(double)> fmt);
    void setupButton (juce::TextButton&, const juce::String& text,
                      bool toggleable = false);

    // Paint helpers
    void drawSignalDots    (juce::Graphics&) const;
    void drawDivider       (juce::Graphics&, int x, int yTop, int height) const;
    void drawStampCircle   (juce::Graphics&, int cx, int cy, int stage, bool bypassed) const;
    void mouseDown         (const juce::MouseEvent&) override;
    void drawSubHead       (juce::Graphics&, int x, int y, int w,
                            const juce::String& text) const;
    void drawKnobName      (juce::Graphics&, const juce::Slider&,
                            const juce::String& name) const;
    void drawTapeReel      (juce::Graphics&) const;

    // Clip mode / shape / wide state
    void setClipMode (int mode);
    void setShape    (int idx);

    //==========================================================================
    // LookAndFeel
    //==========================================================================
    HeatdeathLookAndFeel laf;

    //==========================================================================
    // State
    //==========================================================================
    int   clipMode  = 0;   // 0=LED 1=Si 2=Lift
    int   shapeIdx  = 0;   // 0–4
    float reelAngle = 0.0f;

    //==========================================================================
    // RAT controls
    //==========================================================================
    juce::Slider     sRatDrive, sRatFilter, sRatVol;
    juce::Label      vRatDrive, vRatFilter, vRatVol;
    juce::TextButton bClipLed, bClipSi, bClipLift;
    std::unique_ptr<SA> aRatDrive, aRatFilter, aRatVol;

    //==========================================================================
    // MicroPitch controls
    //==========================================================================
    juce::Slider     sMpDetuneL, sMpDetuneR, sMpMix;
    juce::Label      vMpDetuneL, vMpDetuneR, vMpMix;
    juce::Slider     sMpFocus;
    juce::Label      vMpFocus;
    // bMpStyleI, bMpStyleII removed (type I/II removed from UI)
    std::unique_ptr<SA> aMpDetuneL, aMpDetuneR, aMpMix, aMpFocus;

    //==========================================================================
    // Undulator controls
    //==========================================================================
    juce::Slider     sUndDepth, sUndSpeed, sUndSpace, sUndWaver, sUndMix;
    juce::Label      vUndDepth, vUndSpeed, vUndSpace, vUndWaver, vUndMix;
    // sUndGrit / vUndGrit removed (grit knob commented out)
    juce::TextButton bShape[5];   // SIN TRI PKK RND ENV
    juce::TextButton bWide;
    std::unique_ptr<SA> aUndDepth, aUndSpeed, aUndSpace, aUndWaver, aUndMix;
    // aUndGrit removed

    //==========================================================================
    // Burn-In controls
    //==========================================================================
    juce::Slider  sBurn, sBurnMix;
    juce::Label   vBurn, vBurnMix;
    std::unique_ptr<SA> aBurn, aBurnMix;

    //==========================================================================
    // Global — Master knob in top bar
    //==========================================================================
    juce::Slider  sMaster;
    juce::Label   vMaster;
    std::unique_ptr<SA> aMaster;

    //==========================================================================
    HeatDeathProcessor& processorRef;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HeatDeathEditor)
};
