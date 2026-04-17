#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "Parameters.h"
#include <BinaryData.h>

//==============================================================================
// Layout constants — virtual canvas 800×440; scaled at runtime in resized/paint
//==============================================================================
namespace L
{
    // Column x-starts and widths (inner content x=18..782)
    static constexpr int ratX  = 18,  ratW  = 172;
    static constexpr int mpX   = 208, mpW   = 186;
    static constexpr int undX  = 412, undW  = 238;
    static constexpr int bnX   = 668, bnW   = 114;

    static constexpr int mainY = 82;   // top of column content (expanded header)
    static constexpr int topY  = 18;   // top bar y

    // Stamp header height
    static constexpr int hdrH  = 46;

    // Base canvas size — resized() and paint() scale to match actual window size
    static constexpr int baseW = 800;
    static constexpr int baseH = 440;
}

//==============================================================================
// Constructor
//==============================================================================
HeatDeathEditor::HeatDeathEditor (HeatDeathProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p)
{
    setSize (L::baseW, L::baseH);
    setOpaque (true);

    //--- RAT ---------------------------------------------------------------
    setupSlider (sRatDrive, vRatDrive, [](double v){ return juce::String (v, 0); });
    setupSlider (sRatFilter, vRatFilter, [](double v){
        const double hz = 475.0 + (100.0 - v) / 100.0 * 31525.0;
        return hz > 9999 ? juce::String (hz / 1000.0, 1) + "kHz"
                         : juce::String (int (hz)) + "Hz";
    });
    setupSlider (sRatVol, vRatVol, [](double v){ return juce::String (v, 0); });

    setupSlider (sRatMix, vRatMix, [](double v){ return juce::String (v, 0) + "%"; });

    aRatDrive  = std::make_unique<SA> (p.apvts, Params::RAT_DRIVE,   sRatDrive);
    aRatFilter = std::make_unique<SA> (p.apvts, Params::RAT_FILTER,  sRatFilter);
    aRatVol    = std::make_unique<SA> (p.apvts, Params::RAT_VOLUME,  sRatVol);
    aRatMix    = std::make_unique<SA> (p.apvts, Params::RAT_MIX,     sRatMix);

    // Init value labels from current slider values
    vRatDrive.setText  (juce::String (sRatDrive.getValue(),  0), juce::dontSendNotification);
    vRatFilter.setText ([&]() -> juce::String {
        double hz = 475.0 + (100.0 - sRatFilter.getValue()) / 100.0 * 31525.0;
        return hz > 9999 ? juce::String (hz / 1000.0, 1) + "kHz"
                         : juce::String (int (hz)) + "Hz";
    }(), juce::dontSendNotification);
    vRatVol.setText (juce::String (sRatVol.getValue(), 0), juce::dontSendNotification);
    vRatMix.setText (juce::String (sRatMix.getValue(), 0) + "%", juce::dontSendNotification);

    setupButton (bClipLed,  "LED",  false);
    setupButton (bClipSi,   "Si",   false);
    setupButton (bClipLift, "Lift", false);
    bClipLed.setToggleState (true, juce::dontSendNotification);

    bClipLed.onClick  = [this]{ setClipMode (0); };
    bClipSi.onClick   = [this]{ setClipMode (1); };
    bClipLift.onClick = [this]{ setClipMode (2); };

    //--- MicroPitch --------------------------------------------------------
    setupSlider (sMpDetuneL, vMpDetuneL, [](double v){
        return juce::String (v, 1) + juce::String (juce::CharPointer_UTF8 ("\xc2\xa2"));
    });
    setupSlider (sMpDetuneR, vMpDetuneR, [](double v){
        return juce::String ("+") + juce::String (v, 1) + juce::String (juce::CharPointer_UTF8 ("\xc2\xa2"));
    });
    setupSlider (sMpMix, vMpMix, [](double v){ return juce::String (v, 0) + "%"; });

    // Focus — wired to APVTS (beat reference frequency)
    setupSlider (sMpFocus, vMpFocus, [](double v){ return juce::String (v, 0) + "Hz"; });

    aMpDetuneL = std::make_unique<SA> (p.apvts, Params::PITCH_DETUNE_L, sMpDetuneL);
    aMpDetuneR = std::make_unique<SA> (p.apvts, Params::PITCH_DETUNE_R, sMpDetuneR);
    aMpMix     = std::make_unique<SA> (p.apvts, Params::PITCH_MIX,      sMpMix);
    aMpFocus   = std::make_unique<SA> (p.apvts, Params::PITCH_FOCUS,    sMpFocus);

    vMpDetuneL.setText (juce::String (sMpDetuneL.getValue(), 1) + juce::String (juce::CharPointer_UTF8 ("\xc2\xa2")),
                        juce::dontSendNotification);
    vMpDetuneR.setText (juce::String ("+") + juce::String (sMpDetuneR.getValue(), 1) + juce::String (juce::CharPointer_UTF8 ("\xc2\xa2")),
                        juce::dontSendNotification);
    vMpMix.setText   (juce::String (sMpMix.getValue(),   0) + "%",  juce::dontSendNotification);
    vMpFocus.setText (juce::String (sMpFocus.getValue(), 0) + "Hz", juce::dontSendNotification);

    // bMpStyleI / bMpStyleII removed from UI

    //--- Undulator ----------------------------------------------------------
    setupSlider (sUndDepth, vUndDepth, [](double v){ return juce::String (v, 0); });
    setupSlider (sUndSpeed, vUndSpeed, [](double v){ return juce::String (v, 2) + "Hz"; });
    setupSlider (sUndSpace, vUndSpace, [](double v){ return juce::String (v, 0); });
    setupSlider (sUndWaver, vUndWaver, [](double v){ return juce::String (v, 0); });
    // setupSlider (sUndGrit, ...) — grit knob removed
    setupSlider (sUndMix,   vUndMix,   [](double v){ return juce::String (v, 0) + "%"; });

    aUndDepth = std::make_unique<SA> (p.apvts, Params::UND_DEPTH,     sUndDepth);
    aUndSpeed = std::make_unique<SA> (p.apvts, Params::UND_RATE,      sUndSpeed);
    aUndSpace = std::make_unique<SA> (p.apvts, Params::UND_SPREAD,    sUndSpace);
    aUndWaver = std::make_unique<SA> (p.apvts, Params::UND_MOD_DEPTH, sUndWaver);
    // aUndGrit removed
    aUndMix   = std::make_unique<SA> (p.apvts, Params::UND_MIX,       sUndMix);

    vUndDepth.setText (juce::String (sUndDepth.getValue(), 0), juce::dontSendNotification);
    vUndSpeed.setText (juce::String (sUndSpeed.getValue(), 2) + "Hz", juce::dontSendNotification);
    vUndSpace.setText (juce::String (sUndSpace.getValue(), 0), juce::dontSendNotification);
    vUndWaver.setText (juce::String (sUndWaver.getValue(), 0), juce::dontSendNotification);
    // vUndGrit removed
    vUndMix.setText   (juce::String (sUndMix.getValue(),   0) + "%", juce::dontSendNotification);

    // Shape buttons: SIN TRI PKK RND ENV (maps directly to DSP shape 0–4)
    static const char* shapeNames[] = { "SIN","TRI","PKK","RND","ENV" };
    for (int i = 0; i < 5; ++i)
    {
        setupButton (bShape[i], juce::CharPointer_UTF8 (shapeNames[i]), false);
        const int idx = i;
        bShape[i].onClick = [this, idx]{ setShape (idx); };
    }
    bShape[0].setToggleState (true, juce::dontSendNotification);

    setupButton (bWide, "L=R IN-PHASE", true);
    bWide.setToggleState (false, juce::dontSendNotification);
    bWide.onClick = [this]
    {
        if (auto* param = processorRef.apvts.getParameter (Params::UND_PHASE))
        {
            const float deg = bWide.getToggleState() ? 180.0f : 0.0f;
            param->setValueNotifyingHost (param->convertTo0to1 (deg));
        }
        bWide.setButtonText (bWide.getToggleState()
            ? juce::CharPointer_UTF8 ("L\xe2\x86\x94R ANTI-PHASE")
            : "L=R IN-PHASE");
    };

    //--- Burn-In ------------------------------------------------------------
    setupSlider (sBurn,    vBurn,    [](double v){ return juce::String (v, 0) + "%"; });
    setupSlider (sBurnMix, vBurnMix, [](double v){ return juce::String (v, 0) + "%"; });
    aBurn    = std::make_unique<SA> (p.apvts, Params::BURNIN_AMOUNT, sBurn);
    aBurnMix = std::make_unique<SA> (p.apvts, Params::BURNIN_MIX,    sBurnMix);
    vBurn.setText    (juce::String (sBurn.getValue(),    0) + "%", juce::dontSendNotification);
    vBurnMix.setText (juce::String (sBurnMix.getValue(), 0) + "%", juce::dontSendNotification);

    //--- Master (top bar) ---------------------------------------------------
    setupSlider (sMaster, vMaster, [](double v){ return juce::String (v, 0) + "%"; });
    aMaster = std::make_unique<SA> (p.apvts, Params::GLOBAL_MIX, sMaster);
    vMaster.setText (juce::String (sMaster.getValue(), 0) + "%", juce::dontSendNotification);

    startTimerHz (30);
}

HeatDeathEditor::~HeatDeathEditor()
{
    stopTimer();
    // Clear LookAndFeel before components are destroyed
    setLookAndFeel (nullptr);
}

//==============================================================================
void HeatDeathEditor::timerCallback()
{
    const float burn = float (sBurn.getValue()) / 100.0f;
    reelAngle += 0.008f + burn * 0.05f;
    // Only repaint the reel area to avoid full-editor redraws at 30fps
    repaint (L::bnX, L::mainY + L::hdrH, L::bnW, 90);
}

//==============================================================================
// Setup helpers
//==============================================================================
void HeatDeathEditor::setupSlider (juce::Slider& s, juce::Label& vl,
                                    std::function<juce::String(double)> fmt)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    s.setRotaryParameters (HD::rotaryStart, HD::rotaryEnd, true);
    s.setLookAndFeel (&laf);
    addAndMakeVisible (s);

    vl.setFont (HD::monoFont (7.5f));
    vl.setColour (juce::Label::textColourId, juce::Colour (HD::colAccent));
    vl.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (vl);

    s.onValueChange = [&s, &vl, fmt]()
    {
        vl.setText (fmt (s.getValue()), juce::dontSendNotification);
    };
}

void HeatDeathEditor::setupButton (juce::TextButton& b, const juce::String& text, bool toggleable)
{
    b.setButtonText (text);
    b.setClickingTogglesState (toggleable);
    b.setLookAndFeel (&laf);
    addAndMakeVisible (b);
}

//==============================================================================
// State helpers
//==============================================================================
void HeatDeathEditor::setClipMode (int mode)
{
    clipMode = mode;
    bClipLed.setToggleState  (mode == 0, juce::dontSendNotification);
    bClipSi.setToggleState   (mode == 1, juce::dontSendNotification);
    bClipLift.setToggleState (mode == 2, juce::dontSendNotification);

    if (auto* param = processorRef.apvts.getParameter (Params::RAT_CLIP_MODE))
        param->setValueNotifyingHost (param->convertTo0to1 (float (mode)));
}

void HeatDeathEditor::setShape (int idx)
{
    shapeIdx = idx;
    for (int i = 0; i < 5; ++i)
        bShape[i].setToggleState (i == idx, juce::dontSendNotification);

    if (auto* param = processorRef.apvts.getParameter (Params::UND_SHAPE))
        param->setValueNotifyingHost (param->convertTo0to1 (float (idx)));
}

void HeatDeathEditor::mouseDown (const juce::MouseEvent& e)
{
    struct Circle { int cx; const char* param; };
    static constexpr Circle circles[] = {
        { 35,  Params::RAT_BYPASS    },
        { 225, Params::PITCH_BYPASS  },
        { 429, Params::UND_BYPASS    },
        { 685, Params::BURNIN_BYPASS }
    };
    const int cy = L::mainY + 23;
    constexpr int r2 = 17 * 17;
    for (auto& c : circles)
    {
        const int dx = e.x - c.cx, dy = e.y - cy;
        if (dx * dx + dy * dy <= r2)
        {
            if (auto* p = processorRef.apvts.getParameter (c.param))
                p->setValueNotifyingHost (p->getValue() > 0.5f ? 0.0f : 1.0f);
            repaint();
            return;
        }
    }
}

//==============================================================================
// resized — all positions in virtual 800×440 space, scaled to actual window
//==============================================================================
void HeatDeathEditor::resized()
{
    auto S = [](int x, int y, int w, int h) -> juce::Rectangle<int>
    {
        return { x, y, w, h };
    };

    const int mainY = L::mainY + L::hdrH;  // virtual: 82+46=128

    //=== RAT ===
    {
        const int x = L::ratX, w = L::ratW;
        const int cellW = w / 3;
        const int kY = mainY + 6;    // 134
        const int kSz = 46;

        sRatDrive .setBounds (S (x + (cellW - kSz) / 2,             kY, kSz, kSz));
        sRatFilter.setBounds (S (x + cellW + (cellW - kSz) / 2,     kY, kSz, kSz));
        sRatVol   .setBounds (S (x + cellW*2 + (cellW - kSz) / 2,   kY, kSz, kSz));

        const int vY = kY + kSz + 15;   // 195
        vRatDrive .setBounds (S (x,           vY, cellW, 12));
        vRatFilter.setBounds (S (x + cellW,   vY, cellW, 12));
        vRatVol   .setBounds (S (x + cellW*2, vY, cellW, 12));

        const int clipY  = kY + kSz + 36;   // 216
        const int clipBW = (w - 8) / 3;
        bClipLed .setBounds (S (x,                   clipY, clipBW, 24));
        bClipSi  .setBounds (S (x + clipBW + 4,      clipY, clipBW, 24));
        bClipLift.setBounds (S (x + 2*(clipBW + 4),  clipY, w - 2*(clipBW + 4), 24));

        // Mix knob — below clip buttons
        const int sz3  = 40;
        const int mixY = clipY + 24 + 36;   // 316
        sRatMix.setBounds (S (x + (w - sz3) / 2, mixY, sz3, sz3));
        vRatMix.setBounds (S (x, mixY + sz3 + 15, w, 12));
    }

    //=== MicroPitch ===
    {
        const int x = L::mpX, w = L::mpW;
        const int halfW = w / 2;

        // Detune (44×44) at mainY+20
        const int detY = mainY + 20;   // 148
        const int sz1  = 44;
        sMpDetuneL.setBounds (S (x + (halfW - sz1)/2,          detY, sz1, sz1));
        sMpDetuneR.setBounds (S (x + halfW + (halfW - sz1)/2,  detY, sz1, sz1));
        vMpDetuneL.setBounds (S (x,        detY + sz1 + 15, halfW, 12));
        vMpDetuneR.setBounds (S (x + halfW, detY + sz1 + 15, halfW, 12));

        // Focus + Mix (40×40) — below detune row
        const int sz2  = 40;
        const int outY = detY + sz1 + 56;   // 248
        sMpFocus.setBounds (S (x + (halfW - sz2)/2,          outY, sz2, sz2));
        sMpMix  .setBounds (S (x + halfW + (halfW - sz2)/2,  outY, sz2, sz2));
        vMpFocus.setBounds (S (x,         outY + sz2 + 15, halfW, 12));
        vMpMix  .setBounds (S (x + halfW, outY + sz2 + 15, halfW, 12));
        // Style buttons removed (type I/II removed from UI)
    }

    //=== Undulator ===
    {
        const int x = L::undX, w = L::undW;
        const int halfW  = w / 2;
        const int thirdW = w / 3;

        // Depth + Speed (44×44)
        const int tremY = mainY + 20;   // 148
        const int sz1   = 44;
        sUndDepth.setBounds (S (x + (halfW - sz1)/2,          tremY, sz1, sz1));
        sUndSpeed.setBounds (S (x + halfW + (halfW - sz1)/2,  tremY, sz1, sz1));
        vUndDepth.setBounds (S (x,         tremY + sz1 + 15, halfW, 12));
        vUndSpeed.setBounds (S (x + halfW, tremY + sz1 + 15, halfW, 12));

        // Shape buttons — one row of 5, start after value labels + "Shape" sub-head gap
        const int und_val_bot = tremY + sz1 + 27;   // 219
        const int shY1 = und_val_bot + 26;           // 245
        const int sh1W = (w - 12) / 5;
        for (int i = 0; i < 5; ++i)
            bShape[i].setBounds (S (x + i * (sh1W + 3), shY1, sh1W, 22));

        // Space / Waver / Mix (38×38) — after shape row + sub-head gap
        const int sh1_bot = shY1 + 22;   // 267
        const int sz2     = 38;
        const int swY     = sh1_bot + 26; // 293
        sUndSpace.setBounds (S (x + (thirdW - sz2)/2,             swY, sz2, sz2));
        sUndWaver.setBounds (S (x + thirdW + (thirdW - sz2)/2,    swY, sz2, sz2));
        sUndMix  .setBounds (S (x + thirdW*2 + (thirdW - sz2)/2,  swY, sz2, sz2));
        vUndSpace.setBounds (S (x,             swY + sz2 + 15, thirdW, 12));
        vUndWaver.setBounds (S (x + thirdW,    swY + sz2 + 15, thirdW, 12));
        vUndMix  .setBounds (S (x + thirdW*2,  swY + sz2 + 15, thirdW, 12));

        // Wide button — below value labels
        const int sw_val_bot = swY + sz2 + 27;   // 384
        bWide.setBounds (S (x + 8, sw_val_bot + 8, w - 16, 22));   // 392
    }

    //=== Burn-In ===
    {
        const int x     = L::bnX, w = L::bnW;
        const int halfW = w / 2;          // 57
        const int sz    = 46;
        const int burnKY = mainY + 76;    // 204
        sBurn   .setBounds (S (x + (halfW - sz) / 2,          burnKY, sz, sz));
        sBurnMix.setBounds (S (x + halfW + (halfW - sz) / 2,  burnKY, sz, sz));
        vBurn   .setBounds (S (x,        burnKY + sz + 15, halfW, 12));
        vBurnMix.setBounds (S (x + halfW, burnKY + sz + 15, halfW, 12));
    }

    //=== Master (top bar) ===
    {
        sMaster.setBounds (S (742, 14, 34, 34));   // virtual: y=14, bottom=48
        vMaster.setBounds (S (726, 50, 64, 10));   // virtual: y=50 (value %)
    }
}

//==============================================================================
// paint
//==============================================================================
void HeatDeathEditor::paint (juce::Graphics& g)
{
    // Background
    {
        auto img = juce::ImageCache::getFromMemory (BinaryData::bground1_png, BinaryData::bground1_pngSize);
        if (img.isValid())
            g.drawImage (img, 0, 0, L::baseW, L::baseH, 0, 0, img.getWidth(), img.getHeight());
        else
            g.fillAll (juce::Colour (HD::colBackground));
    }

    // Top bar separator
    g.setColour (juce::Colour (HD::colAccent));
    g.drawLine (18.0f, float (L::mainY), 782.0f, float (L::mainY), 1.5f);

    // Plugin title
    g.setFont (HD::monoFont (16.0f));
    g.setColour (juce::Colour (HD::colAccent));
    g.drawText ("HEATDEATH", 18, 18, 300, 22, juce::Justification::left, false);

    // Signal dots removed (top-right circles removed per UI spec)

    // Global Mix label — below value readout, with breathing room above separator
    g.setFont (HD::monoFont (7.5f));
    g.setColour (juce::Colour (HD::colLabel));
    g.drawText ("GLOBAL MIX", 726, 62, 64, 10, juce::Justification::centred, false);

    // Vertical dividers — height fills virtual canvas below header
    const int divH = L::baseH - L::mainY;   // 440-82=358
    drawDivider (g, 190, L::mainY, divH);
    drawDivider (g, 394, L::mainY, divH);
    drawDivider (g, 650, L::mainY, divH);

    // ---- Stage headers ----
    auto isBypassed = [&](const char* id) -> bool {
        if (auto* p = processorRef.apvts.getRawParameterValue (id))
            return p->load() > 0.5f;
        return false;
    };
    drawStampCircle (g, 35,  L::mainY + 23, 0, isBypassed (Params::RAT_BYPASS));
    drawStampCircle (g, 225, L::mainY + 23, 1, isBypassed (Params::PITCH_BYPASS));
    drawStampCircle (g, 429, L::mainY + 23, 2, isBypassed (Params::UND_BYPASS));
    drawStampCircle (g, 685, L::mainY + 23, 3, isBypassed (Params::BURNIN_BYPASS));

    g.setFont (HD::monoFont (8.5f));
    g.setColour (juce::Colour (HD::colAccent));
    g.drawText ("Turbo RAT",      56,  L::mainY + 16, 120, 14, juce::Justification::left);
    g.drawText (juce::CharPointer_UTF8 ("\xce\xbcPitch"),
                                  246, L::mainY + 16, 120, 14, juce::Justification::left);
    g.drawText ("H3000 Undulator", 450, L::mainY + 16, 180, 14, juce::Justification::left);
    g.drawText ("Burn-In",         706, L::mainY + 16, 70,  14, juce::Justification::left);

    // ---- RAT ----
    {
        drawKnobName (g, sRatDrive,  "Drive");
        drawKnobName (g, sRatFilter, "Filter");
        drawKnobName (g, sRatVol,    "Vol");
        drawKnobName (g, sRatMix,    "Mix");
        const int outY = int (bClipLed.getBottom() / 1) + 6;
        drawSubHead (g, L::ratX, outY, L::ratW, "Mix");
    }

    // ---- MicroPitch ----
    // Sub-head offset from knob bottom (virtual): val_gap(15)+val_h(12)+gap(8)=35
    {
        const int x = L::mpX, w = L::mpW;
        drawSubHead (g, x, L::mainY + L::hdrH + 6,                      w, "Detune");
        drawSubHead (g, x, int (sMpDetuneL.getBottom() / 1) + 35, w, "Rate / Mix");
        drawKnobName (g, sMpDetuneL, "L Cents");
        drawKnobName (g, sMpDetuneR, "R Cents");
        drawKnobName (g, sMpFocus,   "Rate");
        drawKnobName (g, sMpMix,     "Mix");
    }

    // ---- Undulator ----
    {
        const int x = L::undX, w = L::undW;
        drawSubHead (g, x, L::mainY + L::hdrH + 6,                      w, "Tremolo");
        // "Shape" sub-head: below depth/speed value labels (offset 35 from knob bottom)
        drawSubHead (g, x, int (sUndDepth.getBottom() / 1) + 35,  w, "Shape");
        // "Space/Waver/Mix" sub-head: below last shape row
        drawSubHead (g, x, int (bShape[4].getBottom() / 1) + 8,   w, "Space / Waver / Mix");
        drawKnobName (g, sUndDepth, "Depth");
        drawKnobName (g, sUndSpeed, "Speed");
        drawKnobName (g, sUndSpace, "Space");
        drawKnobName (g, sUndWaver, "Waver");
        // drawKnobName (g, sUndGrit, "Grit") — removed
        drawKnobName (g, sUndMix,   "Mix");
    }

    // ---- Burn-In ----
    drawTapeReel (g);
    drawKnobName (g, sBurn,    "Burn");
    drawKnobName (g, sBurnMix, "Mix");
}

//==============================================================================
// Paint helpers
//==============================================================================
void HeatDeathEditor::drawSignalDots (juce::Graphics& g) const
{
    // Separator before master
    g.setColour (juce::Colour (HD::colDivider));
    g.drawLine (728.0f, float (L::topY), 728.0f, float (L::mainY), 1.0f);

    static const char* dotLabels[] = { "IN","RAT","\xce\xbcP","UND","TAPE","OUT" };
    const int startX = 540;
    const int dotSpacing = 30;

    g.setFont (HD::monoFont (8.0f));
    for (int i = 0; i < 6; ++i)
    {
        const int cx = startX + i * dotSpacing;
        const int cy = L::topY + 14;
        const bool lit = (i == 0);

        if (lit)
        {
            g.setColour (juce::Colour (HD::colAccent));
            g.fillEllipse (float (cx - 3), float (cy - 3), 7.0f, 7.0f);
        }
        else
        {
            g.setColour (juce::Colour (HD::colBorder));
            g.drawEllipse (float (cx - 3), float (cy - 3), 7.0f, 7.0f, 1.5f);
        }

        g.setColour (juce::Colour (HD::colLabelDim));
        g.drawText (juce::CharPointer_UTF8 (dotLabels[i]),
                    cx - 12, cy + 6, 24, 10, juce::Justification::centred, false);
    }
}

void HeatDeathEditor::drawDivider (juce::Graphics& g, int x, int yTop, int height) const
{
    const float fx  = float (x) + 9.0f;
    const float yt  = float (yTop);
    const float yb  = float (yTop + height);
    const float mid = yt + float (height) * 0.5f;

    // Dashed line
    juce::Path p;
    p.startNewSubPath (fx, yt);
    p.lineTo (fx, yb);
    juce::PathStrokeType stroke (1.0f);
    float dashes[] = { 2.0f, 4.0f };
    juce::Path dashed;
    stroke.createDashedStroke (dashed, p, dashes, 2);
    g.setColour (juce::Colour (HD::colAccent));
    g.fillPath (dashed);

    // Arrow (downward triangle at midpoint)
    juce::Path arrow;
    arrow.addTriangle (fx - 5.0f, mid - 5.0f,
                       fx + 5.0f, mid - 5.0f,
                       fx,        mid + 7.0f);
    g.fillPath (arrow);

    // Short horizontal ticks
    g.drawLine (fx - 5.0f, yt + float (height) * 0.25f,
                fx + 5.0f, yt + float (height) * 0.25f, 1.0f);
    g.drawLine (fx - 5.0f, yt + float (height) * 0.75f,
                fx + 5.0f, yt + float (height) * 0.75f, 1.0f);
}

void HeatDeathEditor::drawStampCircle (juce::Graphics& g, int cx, int cy, int stage, bool bypassed) const
{
    const float r = 17.0f;
    const float fcx = float (cx), fcy = float (cy);

    // Circle fill — dim when bypassed
    g.setColour (bypassed ? juce::Colour (HD::colBorder) : juce::Colour (HD::colAccent));
    g.fillEllipse (fcx - r, fcy - r, r * 2.0f, r * 2.0f);

    // Icon colour — dimmer when bypassed
    g.setColour (bypassed ? juce::Colour (HD::colLabelDim) : juce::Colour (HD::colBackground));

    switch (stage)
    {
        case 0: // RAT — crosshair
        {
            g.drawLine (fcx, fcy - 11, fcx, fcy + 11, 2.5f);
            g.drawLine (fcx - 11, fcy, fcx + 11, fcy, 2.5f);
            g.saveState();
            g.setColour (juce::Colour (HD::colBackground).withAlpha (0.6f));
            g.drawLine (fcx - 7, fcy - 7, fcx + 7, fcy + 7, 1.1f);
            g.drawLine (fcx + 7, fcy - 7, fcx - 7, fcy + 7, 1.1f);
            g.drawEllipse (fcx - 10, fcy - 10, 20, 20, 0.9f);
            g.restoreState();
            break;
        }
        case 1: // μPitch — Y-fork
        {
            g.drawLine (fcx, fcy - 10, fcx, fcy, 2.0f);
            g.drawLine (fcx, fcy, fcx - 8, fcy + 9, 1.5f);
            g.drawLine (fcx, fcy, fcx + 8, fcy + 9, 1.5f);
            g.saveState();
            g.setColour (juce::Colour (HD::colBackground).withAlpha (0.6f));
            g.drawLine (fcx - 11, fcy + 9, fcx - 5, fcy + 9, 1.0f);
            g.drawLine (fcx +  5, fcy + 9, fcx + 11, fcy + 9, 1.0f);
            g.restoreState();
            break;
        }
        case 2: // Undulator — H3000 equalizer bars
        {
            g.drawLine (fcx, fcy - 12, fcx, fcy + 12, 2.0f);
            g.drawLine (fcx - 10, fcy - 6, fcx + 10, fcy - 6, 1.4f);
            g.drawLine (fcx - 12, fcy,     fcx + 12, fcy,     1.4f);
            g.drawLine (fcx - 10, fcy + 6, fcx + 10, fcy + 6, 1.4f);
            g.saveState();
            g.setColour (juce::Colour (HD::colBackground).withAlpha (0.6f));
            g.drawLine (fcx - 5, fcy - 9, fcx - 5, fcy - 3, 1.0f);
            g.drawLine (fcx + 5, fcy - 9, fcx + 5, fcy - 3, 1.0f);
            g.drawLine (fcx - 6, fcy + 3, fcx - 6, fcy + 9, 1.0f);
            g.drawLine (fcx + 6, fcy + 3, fcx + 6, fcy + 9, 1.0f);
            g.restoreState();
            break;
        }
        case 3: // Burn-In — tape reels
        {
            const float rl = fcx - 6, rr = fcx + 6, ry = fcy - 4;
            g.drawEllipse (rl - 5, ry - 5, 10, 10, 1.3f);
            g.drawEllipse (rr - 5, ry - 5, 10, 10, 1.3f);
            g.saveState();
            g.setColour (juce::Colour (HD::colBackground).withAlpha (0.6f));
            g.fillEllipse (rl - 1.8f, ry - 1.8f, 3.6f, 3.6f);
            g.fillEllipse (rr - 1.8f, ry - 1.8f, 3.6f, 3.6f);
            g.restoreState();
            // Tape path below reels
            g.drawLine (rl, ry + 5, rl, fcy + 10, 1.1f);
            g.drawLine (rr, ry + 5, rr, fcy + 10, 1.1f);
            g.drawLine (rl, fcy + 10, rr, fcy + 10, 1.1f);
            g.drawLine (rl - 2, ry + 3, rr + 2, ry + 3, 1.4f);
            break;
        }
        default: break;
    }

    // Bypass slash — diagonal line across circle
    if (bypassed)
    {
        g.setColour (juce::Colour (HD::colBackground).withAlpha (0.7f));
        g.drawLine (fcx - 10, fcy + 10, fcx + 10, fcy - 10, 2.0f);
    }
}

void HeatDeathEditor::drawSubHead (juce::Graphics& g, int x, int y, int w,
                                    const juce::String& text) const
{
    g.setFont (HD::monoFont (7.5f));
    g.setColour (juce::Colour (HD::colLabelDim));
    const int textW = HD::monoFont (7.5f).getStringWidth (text.toUpperCase()) + 8;
    g.drawText (text.toUpperCase(), x, y, textW, 13, juce::Justification::left, false);
    g.setColour (juce::Colour (HD::colDivider));
    g.drawLine (float (x + textW + 3), float (y + 7), float (x + w), float (y + 7), 1.0f);
}

void HeatDeathEditor::drawKnobName (juce::Graphics& g, const juce::Slider& s,
                                     const juce::String& name) const
{
    if (s.getWidth() == 0) return;
    // Component bounds are in actual pixels; unscale to virtual space for the transformed context
    const auto r = s.getBounds();
    const int vx = int (r.getX()      / 1);
    const int vy = int (r.getBottom() / 1);
    const int vw = int (r.getWidth()  / 1);
    g.setFont (HD::monoFont (7.5f));
    g.setColour (juce::Colour (HD::colLabel));
    g.drawText (name.toUpperCase(), vx - 8, vy + 3, vw + 16, 12,
                juce::Justification::centred, false);
}

void HeatDeathEditor::drawTapeReel (juce::Graphics& g) const
{
    const int bnCX = L::bnX + L::bnW / 2;
    const int reelTopY = L::mainY + L::hdrH + 8;
    const int W = 64, H = 64;
    const int ox = bnCX - W / 2, oy = reelTopY;

    // Background patch (same as shell bg)
    g.setColour (juce::Colour (HD::colKnobFace));
    g.fillRect (ox, oy, W, H);

    const float burn = float (const_cast<HeatDeathEditor*>(this)->sBurn.getValue()) / 100.0f;

    auto drawSingleReel = [&](float cx, float cy, float reelR, float angle)
    {
        g.setColour (juce::Colour (HD::colAccent));
        g.drawEllipse (cx - reelR, cy - reelR, reelR * 2, reelR * 2, 1.2f);
        g.drawEllipse (cx - reelR * 0.35f, cy - reelR * 0.35f, reelR * 0.7f, reelR * 0.7f, 1.0f);
        g.fillEllipse (cx - reelR * 0.12f, cy - reelR * 0.12f, reelR * 0.24f, reelR * 0.24f);

        const int spokes = 6;
        for (int i = 0; i < spokes; ++i)
        {
            const float a = angle + float (i) / float (spokes) * juce::MathConstants<float>::twoPi;
            g.drawLine (cx + reelR * 0.35f * std::sin (a), cy - reelR * 0.35f * std::cos (a),
                        cx + reelR * 0.9f  * std::sin (a), cy - reelR * 0.9f  * std::cos (a), 1.0f);
        }
    };

    drawSingleReel (float (ox + 17), float (oy + 21), 12.0f, reelAngle);
    drawSingleReel (float (ox + 47), float (oy + 21), 12.0f, reelAngle * 0.9f);

    // Tape path bridge
    g.setColour (juce::Colour (HD::colAccent));
    g.drawLine (float (ox + 17), float (oy + 33), float (ox + 17), float (oy + 48), 1.1f);
    g.drawLine (float (ox + 47), float (oy + 33), float (ox + 47), float (oy + 48), 1.1f);
    g.drawLine (float (ox + 17), float (oy + 48), float (ox + 47), float (oy + 48), 1.1f);
    g.setColour (juce::Colour (HD::colLabelDim));
    g.drawLine (float (ox + 17), float (oy + 40), float (ox + 47), float (oy + 40), 1.4f);

    // Wobble line when burn > 0
    if (burn > 0.0f)
    {
        const float wobble = std::sin (reelAngle * 3.0f) * burn * 1.4f;
        g.setColour (juce::Colour (HD::colAccent).withAlpha (0.3f));
        g.drawLine (float (ox + 17), 40.0f + float (oy) + wobble,
                    float (ox + 47), 40.0f + float (oy) - wobble * 0.5f, 0.8f);
    }
}
