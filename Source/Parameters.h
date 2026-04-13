#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

//==============================================================================
// HEATDEATH.VST — Parameter IDs & Layout
//
// All parameter IDs are defined here as constexpr string literals inside the
// Params namespace. Use these constants everywhere — never write a raw string
// ID outside of this file. The APVTS will assert on any typo at construction.
//
// Version hint is 1 throughout. If a parameter ID ever needs to change in a
// future release, bump its version hint and add a migration handler in
// PluginProcessor::setStateInformation().
//
// Parameter groups mirror the four hardware stages. DAW automation lanes will
// show: RAT / MicroPitch / Undulator / Burn-In / Global.
//==============================================================================

namespace Params
{
    //==========================================================================
    // Stage 1 — Turbo RAT
    //==========================================================================

    // Main drive gain. Maps 0–100 → 0–67dB inside TurboRat.
    // Skew 0.7: slight log weighting — most musical range is 40–80.
    static constexpr auto RAT_DRIVE       = "rat_drive";

    // Tone control — reverse-wired LPF. 0 = bright (32kHz), 100 = dark (475Hz).
    // Linear. Labelled "Filter" in UI to match hardware.
    static constexpr auto RAT_FILTER      = "rat_filter";

    // Output level after tone control and JFET buffer.
    // Linear. 0 = −∞, 100 = unity (mapped internally to gain scalar).
    static constexpr auto RAT_VOLUME      = "rat_volume";

    // LM308 slew rate model. Controls drive-dependent HF rolloff curve.
    // 100 = full bandwidth, 0 = most rolled off.
    // Linear — the mapping to cutoff frequency is done inside TurboRat.
    static constexpr auto RAT_SLEW        = "rat_slew";

    // Half-cycle clipping asymmetry. 0 = fully symmetric, 100 = max asymmetric.
    // Linear. Affects asymmetry_factor inside the feedback solver.
    static constexpr auto RAT_ASYM        = "rat_asym";

    // Diode mode. 0=LED, 1=Silicon, 2=Lift, 3=Ruetz.
    // Ruetz (3) is the hidden mode — no label in the UI choice list, present in state.
    // AudioParameterChoice — not float.
    static constexpr auto RAT_CLIP_MODE   = "rat_clip_mode";

    // Power supply sag sensitivity. 0 = no sag, 100 = maximum sag under load.
    // Linear. Internal mapping to rail droop amount.
    static constexpr auto RAT_SAG         = "rat_sag";

    // Stage bypass. True = signal passes through unmodified.
    static constexpr auto RAT_BYPASS      = "rat_bypass";

    //==========================================================================
    // Stage 2 — H3000 MicroPitch
    //==========================================================================

    // Left channel pitch shift. Range −25¢ to −2¢. Default −7¢.
    // Note: both bounds are negative. Linear, 0.1¢ steps.
    static constexpr auto PITCH_DETUNE_L  = "pitch_detune_l";

    // Right channel pitch shift. Range +2¢ to +25¢. Default +11¢.
    // Linear, 0.1¢ steps.
    static constexpr auto PITCH_DETUNE_R  = "pitch_detune_r";

    // Wet/dry blend. 0 = dry mono, 100 = fully pitch-shifted stereo.
    // Linear. At 0% output is mono dry; at 100% output is fully shifted.
    static constexpr auto PITCH_MIX       = "pitch_mix";

    // Stereo width. 100 = hard L/R pan (default), 0 = both voices centre (chorus).
    // Linear.
    static constexpr auto PITCH_WIDTH     = "pitch_width";

    // Stage bypass.
    static constexpr auto PITCH_BYPASS    = "pitch_bypass";

    //==========================================================================
    // Stage 3 — H3000 Undulator
    //==========================================================================

    // Primary LFO rate. 0.5–8.5 Hz. Default 2.4 Hz.
    // Skew 0.4: log-ish weighting — most usable range is 0.5–4 Hz.
    static constexpr auto UND_RATE        = "und_rate";

    // Primary LFO depth (AM depth). 0–100. Default 82.
    // 0 = no modulation, 100 = approaches silence at trough.
    // Linear.
    static constexpr auto UND_DEPTH       = "und_depth";

    // L/R stereo phase offset between primary LFOs. 0–360°. Default 180°.
    // Linear, 1° steps. At 180° one channel is at maximum while other is minimum.
    static constexpr auto UND_PHASE       = "und_phase";

    // LFO phase randomisation drift over time. 0–100. Default 40.
    // Linear. Higher = more random phase wander.
    static constexpr auto UND_DRIFT       = "und_drift";

    // Secondary LFO rate. 0.1–2.1 Hz. Default 0.6 Hz.
    // Skew 0.4: weighted toward low end — secondary LFO is typically slow.
    static constexpr auto UND_MOD_RATE    = "und_mod_rate";

    // Secondary LFO → primary depth modulation amount. 0–100. Default 45.
    // Linear. Controls how much secondary LFO wobbles primary AM depth.
    static constexpr auto UND_MOD_DEPTH   = "und_mod_depth";

    // Secondary LFO → primary rate modulation amount. 0–100. Default 35.
    // Linear. Controls how much secondary LFO wobbles primary rate.
    static constexpr auto UND_MOD_SPEED   = "und_mod_speed";

    // Detuned delay spread in cents. 0–100. Default 55.
    // Linear. Higher = wider stereo delay spread before tremolo.
    static constexpr auto UND_SPREAD      = "und_spread";

    // Delay feedback. 0–100%. Default 38%.
    // Skew 0.65: weighted toward lower end — high feedback causes extreme swells.
    static constexpr auto UND_FEEDBACK    = "und_feedback";

    // TMS32010 16-bit wraparound character amount. 0–100. Default 62.
    // Linear. Applied inside the feedback loop, not at stage output.
    static constexpr auto UND_GRIT        = "und_grit";

    // Stage wet/dry. 0–100%. Default 100%.
    // Linear.
    static constexpr auto UND_MIX         = "und_mix";

    // Primary LFO waveform. 0=Sine, 1=Triangle, 2=Peak, 3=Random, 4=Envelope.
    // AudioParameterChoice — not float.
    static constexpr auto UND_SHAPE       = "und_shape";

    // Stage bypass.
    static constexpr auto UND_BYPASS      = "und_bypass";

    //==========================================================================
    // Stage 4 — Burn-In (Tape Saturation)
    //==========================================================================

    // Master heat rate. Sets thermal accumulation speed.
    // 0 = cold (no processing beyond pass-through), 100 = maximum heat rate.
    // Skew 0.6: slight log weighting — interesting range begins around 20%.
    // The thermal accumulator's nonlinearity provides the rest of the curve shape.
    static constexpr auto BURNIN_AMOUNT   = "burnin_amount";

    // Stage bypass.
    static constexpr auto BURNIN_BYPASS   = "burnin_bypass";

    //==========================================================================
    // Inter-Stage Trims
    // Applied at each stage output junction. Default 0dB (unity).
    // Exposed in UI as small trim controls. Allow ±12dB.
    //==========================================================================

    // Trim applied after Stage 1 RAT output, before Stage 2 MicroPitch input.
    static constexpr auto TRIM_POST_RAT   = "trim_post_rat";

    // Trim applied after Stage 2 MicroPitch output, before Stage 3 Undulator input.
    static constexpr auto TRIM_POST_PITCH = "trim_post_pitch";

    // Trim applied after Stage 3 Undulator output, before Stage 4 Burn-In input.
    static constexpr auto TRIM_POST_UND   = "trim_post_und";

    //==========================================================================
    // Global
    //==========================================================================

    // Global parallel wet/dry mix. 0 = fully dry (clean input), 100 = fully wet.
    // Linear. Clean mono input is held at plugin boundary and blended with stereo
    // processed output. Phase relationship between dry and wet is intentional.
    static constexpr auto GLOBAL_MIX              = "global_mix";

    // Expert: global feedback path amount. 0–100 maps internally to 0.0–0.15.
    // Fraction of final output routed back to Stage 1 input.
    // Skew 0.35: very heavily weighted toward low end — even small values are extreme.
    // Only active when GLOBAL_FEEDBACK_ACTIVE is true.
    static constexpr auto GLOBAL_FEEDBACK_AMOUNT  = "global_feedback_amount";

    // Expert: enables the global feedback path. Off by default.
    // AudioParameterBool.
    static constexpr auto GLOBAL_FEEDBACK_ACTIVE  = "global_feedback_active";

    // Expert: M/S processing mode for Burn-In stage.
    // When true: input is decoded to M/S before Stage 4, re-encoded after.
    // AudioParameterBool.
    static constexpr auto GLOBAL_MS_MODE          = "global_ms_mode";

    //==========================================================================
    // Acetate Mode (hidden feature — changes Burn-In breakup character)
    // Toggles backing material from polyester to acetate physics:
    // sharper oxide shedding transitions, lower stick-slip threshold,
    // sudden pitch-drop artifact at high temp, faster demagnetization fall.
    // AudioParameterBool. Default false. No label in UI.
    //==========================================================================

    static constexpr auto ACETATE_MODE    = "acetate_mode";

    //==========================================================================
    // Layout factory — defined in Parameters.cpp
    // Call once from PluginProcessor constructor to initialise the APVTS.
    //==========================================================================

    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

} // namespace Params
