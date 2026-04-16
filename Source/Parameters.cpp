#include "Parameters.h"

//==============================================================================
// Shorthand aliases — keeps the layout function readable
//==============================================================================

using APVTS  = juce::AudioProcessorValueTreeState;
using Layout = APVTS::ParameterLayout;
using Group  = juce::AudioProcessorParameterGroup;
using Float  = juce::AudioParameterFloat;
using Bool   = juce::AudioParameterBool;
using Choice = juce::AudioParameterChoice;
using Range  = juce::NormalisableRange<float>;
using PID    = juce::ParameterID;
using Attr   = juce::AudioParameterFloatAttributes;

//==============================================================================
// Helper: builds a linear NormalisableRange with a given step size
//==============================================================================

static Range linear (float min, float max, float step = 0.01f)
{
    return Range (min, max, step);
}

//==============================================================================
// Helper: builds a skewed NormalisableRange
// skew < 1.0 → more resolution at lower end (for rate/frequency-style params)
// skew > 1.0 → more resolution at upper end
//==============================================================================

static Range skewed (float min, float max, float step, float skew)
{
    return Range (min, max, step, skew);
}

//==============================================================================
// Helper: shorthand for the ParameterID version stamp.
// Version 1 throughout. Bump per-parameter if ID or range changes in future.
//==============================================================================

static PID pid (const char* id)
{
    return PID { id, 1 };
}

//==============================================================================

namespace Params
{

Layout createParameterLayout()
{
    Layout layout;

    //==========================================================================
    // Stage 1 — Turbo RAT
    //==========================================================================

    auto ratGroup = std::make_unique<Group> ("rat", "RAT", "|",

        // Drive — 0–60, default 9.
        // Skew 0.7: slight log weighting. Range capped at 60 — above that the
        // op-amp gain becomes unusable in practice.
        std::make_unique<Float> (
            pid (RAT_DRIVE), "Drive",
            skewed (0.0f, 60.0f, 0.1f, 0.7f),
            9.0f,
            Attr().withLabel ("%")),

        // Filter — reverse-wired LPF. 0 = bright (32kHz), 100 = dark (475Hz).
        // Default 1.5 ≈ 30kHz (near-flat, just off maximum brightness).
        std::make_unique<Float> (
            pid (RAT_FILTER), "Filter",
            linear (0.0f, 100.0f, 0.1f),
            1.5f,
            Attr().withLabel ("%")),

        // Volume — output level.
        // 0–75 maps to an internal gain scalar. Range capped at 75 — higher values
        // produce excessive output gain in practice.
        std::make_unique<Float> (
            pid (RAT_VOLUME), "Volume",
            linear (0.0f, 75.0f, 0.1f),
            24.5f,
            Attr().withLabel ("%")),

        // Slew — LM308 HF rolloff curve. Higher = brighter.
        // 100 = least rolled off (~5kHz ceiling), 0 = most (~1kHz ceiling).
        std::make_unique<Float> (
            pid (RAT_SLEW), "LM308 Slew",
            linear (0.0f, 100.0f, 0.1f),
            68.0f,
            Attr().withLabel ("%")),

        // Asymmetry — half-cycle clipping difference.
        // 0 = symmetric (LM308 default), 100 = maximum asymmetric character.
        std::make_unique<Float> (
            pid (RAT_ASYM), "Asymmetry",
            linear (0.0f, 100.0f, 0.1f),
            20.0f,
            Attr().withLabel ("%")),

        // Sag — power supply sag sensitivity.
        // 0 = rigid supply (no sag), 100 = maximum rail droop under transients.
        std::make_unique<Float> (
            pid (RAT_SAG), "Sag",
            linear (0.0f, 100.0f, 0.1f),
            25.0f,
            Attr().withLabel ("%")),

        // Clip mode — diode selection.
        // Index: 0=LED, 1=Silicon, 2=Lift, 3=Ruetz.
        // "Ruetz" is not labelled in the UI choice widget — it exists in state
        // but is only selectable via the hidden Clip Mode area in expert panel.
        std::make_unique<Choice> (
            pid (RAT_CLIP_MODE), "Clip Mode",
            juce::StringArray { "LED", "Silicon", "Lift", "Ruetz" },
            0),   // default: LED

        // Bypass.
        std::make_unique<Bool> (
            pid (RAT_BYPASS), "RAT Bypass",
            false)
    );

    layout.add (std::move (ratGroup));

    //==========================================================================
    // Stage 2 — H3000 MicroPitch
    //==========================================================================

    auto pitchGroup = std::make_unique<Group> ("pitch", "MicroPitch", "|",

        // Detune L — left channel pitch shift.
        // Range: −25¢ to 0¢. Default −25¢ (maximum detune).
        // Linear, 0.1¢ steps. DAW will display negative values correctly.
        std::make_unique<Float> (
            pid (PITCH_DETUNE_L), "Detune L",
            linear (-25.0f, 0.0f, 0.1f),
            -25.0f,
            Attr().withLabel ("\u00a2")),   // ¢ symbol

        // Detune R — right channel pitch shift.
        // Range: 0¢ to +25¢. Default +25¢ (maximum detune).
        std::make_unique<Float> (
            pid (PITCH_DETUNE_R), "Detune R",
            linear (0.0f, 25.0f, 0.1f),
            25.0f,
            Attr().withLabel ("\u00a2")),

        // Mix — wet/dry. 0% = dry mono pass-through, 100% = full stereo shift.
        std::make_unique<Float> (
            pid (PITCH_MIX), "Mix",
            linear (0.0f, 100.0f, 0.1f),
            100.0f,
            Attr().withLabel ("%")),

        // Width — stereo image width.
        // 100% = hard L/R pan (default H3000 MicroPitch behaviour).
        // 0% = both voices panned centre (mono chorus character).
        std::make_unique<Float> (
            pid (PITCH_WIDTH), "Width",
            linear (0.0f, 100.0f, 0.1f),
            100.0f,
            Attr().withLabel ("%")),

        // Bypass.
        std::make_unique<Bool> (
            pid (PITCH_BYPASS), "MicroPitch Bypass",
            false)
    );

    layout.add (std::move (pitchGroup));

    //==========================================================================
    // Stage 3 — H3000 Undulator
    //==========================================================================

    auto undGroup = std::make_unique<Group> ("und", "Undulator", "|",

        // Rate — primary LFO rate. 0.5–8.5 Hz. Default 2.4 Hz.
        // Skew 0.4: log-weighted. Most usable range is 0.5–4 Hz.
        // The knob should feel like it covers slow-to-moderate rates in the
        // first 60% of travel, with fast rates compressed into the top 40%.
        std::make_unique<Float> (
            pid (UND_RATE), "Rate",
            skewed (0.5f, 8.5f, 0.001f, 0.4f),
            2.4f,
            Attr().withLabel ("Hz")),

        // Depth — AM depth. 0–100. Default 68.
        // 0 = no modulation. 82 = near-silence at trough (but never fully silent
        // — the AM envelope has a floor). 100 = approaches full silence at trough.
        std::make_unique<Float> (
            pid (UND_DEPTH), "Depth",
            linear (0.0f, 100.0f, 0.1f),
            68.0f,
            Attr().withLabel ("%")),

        // St. Phase — stereo phase offset between L and R LFOs. 0–360°.
        // Default 0°: both channels move identically (mono tremolo, no stereo movement).
        // 180° = one channel at maximum while other is at minimum (antiphase autopanner).
        std::make_unique<Float> (
            pid (UND_PHASE), "St. Phase",
            linear (0.0f, 360.0f, 1.0f),
            0.0f,
            Attr().withLabel ("\u00b0")),   // ° symbol

        // Drift — LFO phase randomisation. 0–100. Default 40.
        // Adds slow random walk to lfo_phase accumulator so rate never locks.
        std::make_unique<Float> (
            pid (UND_DRIFT), "Drift",
            linear (0.0f, 100.0f, 0.1f),
            40.0f,
            Attr().withLabel ("%")),

        // Mod Rate — secondary LFO rate. 0.1–2.1 Hz. Default 0.6 Hz.
        // Skew 0.4: weighted toward low end — secondary LFO is typically slow.
        // A mod rate near or above the primary rate produces erratic instability.
        std::make_unique<Float> (
            pid (UND_MOD_RATE), "Mod Rate",
            skewed (0.1f, 2.1f, 0.001f, 0.4f),
            0.6f,
            Attr().withLabel ("Hz")),

        // Mod Depth — secondary LFO → primary depth modulation amount.
        // 0 = secondary LFO does not affect depth. 100 = maximum depth wobble.
        std::make_unique<Float> (
            pid (UND_MOD_DEPTH), "Mod Depth",
            linear (0.0f, 100.0f, 0.1f),
            45.0f,
            Attr().withLabel ("%")),

        // Mod Speed — secondary LFO → primary rate modulation amount.
        // 0 = secondary LFO does not affect rate. 100 = maximum rate wobble.
        std::make_unique<Float> (
            pid (UND_MOD_SPEED), "Mod Speed",
            linear (0.0f, 100.0f, 0.1f),
            35.0f,
            Attr().withLabel ("%")),

        // Spread — detuned delay spread. 0–100. Default 55.
        // Controls the cent difference between L and R delay times before AM.
        // Higher = wider stereo spread; at high feedback this affects swell density.
        std::make_unique<Float> (
            pid (UND_SPREAD), "Spread",
            linear (0.0f, 100.0f, 0.1f),
            55.0f,
            Attr().withLabel ("%")),

        // Feedback — delay feedback amount. 0–100%. Default 38%.
        // Skew 0.65: weighted toward lower end. Values above 70% produce
        // self-reinforcing swells; values above 90% are deliberately extreme.
        // Hard ceiling enforced in DSP (chip_grit inside loop prevents runaway).
        std::make_unique<Float> (
            pid (UND_FEEDBACK), "Feedback",
            skewed (0.0f, 100.0f, 0.1f, 0.65f),
            38.0f,
            Attr().withLabel ("%")),

        // Chip Grit — TMS32010 16-bit wraparound character.
        // 0 = no quantisation effect. 100 = maximum wraparound distortion.
        // Applied inside the feedback loop — builds progressively with feedback.
        std::make_unique<Float> (
            pid (UND_GRIT), "Chip Grit",
            linear (0.0f, 100.0f, 0.1f),
            0.0f,
            Attr().withLabel ("%")),

        // Wet Mix — stage wet/dry. 100% = fully processed, 0% = bypass with
        // signal passing unchanged. Default 100%.
        std::make_unique<Float> (
            pid (UND_MIX), "Wet Mix",
            linear (0.0f, 100.0f, 0.1f),
            100.0f,
            Attr().withLabel ("%")),

        // LFO Shape — primary LFO waveform.
        // 0=Sine (smooth, default), 1=Triangle (linear),
        // 2=Peak (fast attack / exp decay), 3=Random (two inharmonic sines),
        // 4=Envelope (tracks input RMS).
        std::make_unique<Choice> (
            pid (UND_SHAPE), "LFO Shape",
            juce::StringArray { "Sine", "Triangle", "Peak", "Random", "Envelope" },
            0),   // default: Sine

        // Bypass.
        std::make_unique<Bool> (
            pid (UND_BYPASS), "Undulator Bypass",
            false)
    );

    layout.add (std::move (undGroup));

    //==========================================================================
    // Stage 4 — Burn-In
    //==========================================================================

    auto burninGroup = std::make_unique<Group> ("burnin", "Burn-In", "|",

        // Amount — direct burn level. 0 = fresh tape, 100 = fully degraded.
        std::make_unique<Float> (
            pid (BURNIN_AMOUNT), "Burn-In",
            skewed (0.0f, 100.0f, 0.1f, 0.6f),
            60.0f,
            Attr().withLabel ("%")),

        // Bypass.
        std::make_unique<Bool> (
            pid (BURNIN_BYPASS), "Burn-In Bypass",
            false)
    );

    layout.add (std::move (burninGroup));

    //==========================================================================
    // Inter-Stage Trims
    //==========================================================================

    auto trimGroup = std::make_unique<Group> ("trim", "Inter-Stage Trims", "|",

        // Post-RAT trim. Applied between Stage 1 output and Stage 2 input.
        // ±12dB, 0.1dB steps. Default 0dB.
        std::make_unique<Float> (
            pid (TRIM_POST_RAT), "Post-RAT Trim",
            linear (-12.0f, 12.0f, 0.1f),
            0.0f,
            Attr().withLabel ("dB")),

        // Post-MicroPitch trim. Applied between Stage 2 output and Stage 3 input.
        std::make_unique<Float> (
            pid (TRIM_POST_PITCH), "Post-Pitch Trim",
            linear (-12.0f, 12.0f, 0.1f),
            0.0f,
            Attr().withLabel ("dB")),

        // Post-Undulator trim. Applied between Stage 3 output and Stage 4 input.
        std::make_unique<Float> (
            pid (TRIM_POST_UND), "Post-Undulator Trim",
            linear (-12.0f, 12.0f, 0.1f),
            0.0f,
            Attr().withLabel ("dB"))
    );

    layout.add (std::move (trimGroup));

    //==========================================================================
    // Global
    //==========================================================================

    auto globalGroup = std::make_unique<Group> ("global", "Global", "|",

        // Mix — global parallel wet/dry.
        // Clean mono input is held at plugin boundary and blended with the fully
        // processed stereo output. Phase relationship between dry and processed
        // is intentional — do not compensate.
        std::make_unique<Float> (
            pid (GLOBAL_MIX), "Mix",
            linear (0.0f, 100.0f, 0.1f),
            100.0f,
            Attr().withLabel ("%")),

        // Feedback amount — fraction of final stereo output routed back to Stage 1.
        // 0–100 maps internally to 0.0–0.15 (hard ceiling enforced in DSP).
        // Skew 0.35: very heavily weighted toward low end.
        // Even a value of 5 (→ 0.0075 internal) is audibly significant.
        // Only active when GLOBAL_FEEDBACK_ACTIVE is true.
        std::make_unique<Float> (
            pid (GLOBAL_FEEDBACK_AMOUNT), "Feedback Amount",
            skewed (0.0f, 100.0f, 0.1f, 0.35f),
            0.0f,
            Attr().withLabel ("%")),

        // Feedback active — enables the global sub-oscillation feedback path.
        // Off by default. Expert feature — not prominently exposed in primary UI.
        std::make_unique<Bool> (
            pid (GLOBAL_FEEDBACK_ACTIVE), "Global Feedback",
            false),

        // M/S mode — Burn-In processes mid and side independently.
        // When true: M/S decode before Stage 4, re-encode after.
        // Off by default. Expert feature.
        std::make_unique<Bool> (
            pid (GLOBAL_MS_MODE), "M/S Mode",
            false)
    );

    layout.add (std::move (globalGroup));

//==========================================================================
    // Acetate Mode (hidden feature)
    // Changes Burn-In breakup physics from polyester to acetate tape character.
    // Not labelled in UI. Discoverable only by accident.
    //==========================================================================

    layout.add (std::make_unique<Bool> (
        pid (ACETATE_MODE), "Acetate Mode",
        false));

    return layout;
}

} // namespace Params
