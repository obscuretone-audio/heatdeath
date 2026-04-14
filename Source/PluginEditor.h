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
    void drawStampCircle   (juce::Graphics&, int cx, int cy, int stage) const;
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
    int   shapeIdx  = 0;   // 0–8
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
    // Delay L/R and Focus are visual-only (no APVTS param)
    juce::Slider     sMpDelayL, sMpDelayR, sMpFocus;
    juce::Label      vMpDelayL, vMpDelayR, vMpFocus;
    juce::TextButton bMpStyleI, bMpStyleII;
    std::unique_ptr<SA> aMpDetuneL, aMpDetuneR, aMpMix;

    //==========================================================================
    // Undulator controls
    //==========================================================================
    juce::Slider     sUndDepth, sUndSpeed, sUndSpace, sUndWaver, sUndGrit, sUndMix;
    juce::Label      vUndDepth, vUndSpeed, vUndSpace, vUndWaver, vUndGrit, vUndMix;
    juce::TextButton bShape[9];   // SIN TRI PKK RND RMP SQ S&H ENV ADS
    juce::TextButton bWide;
    std::unique_ptr<SA> aUndDepth, aUndSpeed, aUndSpace, aUndWaver, aUndGrit, aUndMix;

    //==========================================================================
    // Burn-In controls
    //==========================================================================
    juce::Slider  sBurn;
    juce::Label   vBurn;
    std::unique_ptr<SA> aBurn;

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
