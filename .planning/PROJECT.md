# HEATDEATH

## What This Is

HEATDEATH is a VST3/AU audio effects plugin (C++/JUCE) that precisely emulates Tim Hecker's documented signal chain from *Ravedeath, 1972*: Turbo RAT distortion → H3000 MicroPitch stereo detuning → H3000 Undulator AM/FM tremolo → Tape Burn-In saturation. It is built from scratch with no third-party DSP libraries — every stage implements the specific hardware character documented in the research spec. Target platforms: macOS (VST3 + AU) and Windows (VST3).

## Core Value

The four DSP stages must faithfully reproduce the specific hardware character Hecker used — the LM308 slew limiting, the H3000 chip saturation, the Jiles-Atherton tape physics — not approximate them with generic waveshapers.

## Requirements

### Validated

(None yet — ship to validate)

### Active

- [ ] JUCE AudioPlugin project scaffold (fresh, no DSP template) with correct VST3/AU targets for Mac + Windows
- [ ] Stage 1: Turbo RAT — pre-clip HPF chain, gain-dependent LM308 slew limiting, LED/Si/Lift diode waveshaper, reverse-wired tone LPF, JFET buffer; 4× oversampling
- [ ] Stage 2: H3000 MicroPitch — dual-voice pitch shift via SSB/delay modulation, asymmetric cent defaults (−7¢/+11¢), stereo panning, wet/dry blend
- [ ] Stage 3: H3000 Undulator — dual-LFO AM/FM system (9 shapes), Waver macro, Space/feedback detuned delay, TMS32010 chip saturation (Grit), Wide phase toggle; all LFO accumulators double-precision
- [ ] Stage 4: Burn-In — Jiles-Atherton hysteresis ODE (RK4, 4× oversampling, Ferric Oxide params), tape bias, wow/flutter, head bump, HF rolloff, hiss; single Burn macro knob
- [ ] Inter-stage DC blocking, per-stage bypass with 10ms crossfade, 20ms parameter smoothing (50ms for und_speed)
- [ ] Global parallel wet/dry mix, global feedback path, M/S mode for Burn-In
- [ ] Disintegration Timer hidden feature (drives Burn-In to 1.0 over 10/20/40/74 min)
- [ ] Acetate Mode hidden feature (alternate tape physics)
- [ ] UI: 800×540px single panel, four-column layout, `#111111` bg / `#c8a96e` accent / `#888888` labels, monospace font, no oscilloscope or spectrum display
- [ ] Three factory presets: "The Piano Drop", "In The Fog", "Scorched"
- [ ] Full APVTS parameter layout matching Parameters.h/Parameters.cpp spec

### Out of Scope

- Third-party DSP libraries (iPlug2, signalsmith, etc.) — spec requires from-scratch implementation
- JUCE DSP module wrappers for signal processing (only `juce::dsp::Oversampling` permitted)
- Tempo sync / DAW BPM sync — Hecker's music deliberately avoids rhythmic lock, all rates in Hz only
- Spectrum display, oscilloscope, or visual metering — spec explicitly excludes these
- Standalone app target — plugin-only (VST3 + AU)
- MIDI control — plugin does not accept MIDI
- Linux target — Mac + Windows only for v1

## Context

- Detailed DSP spec in `heatdeath_vst_spec.md` — covers circuit analysis, algorithm pseudocode, parameter ranges, and inter-stage interaction notes for all four stages
- Coding agent prompt in `heatdeath_agent_prompt.md` — full implementation brief with file tree, APVTS parameter table, and stage-by-stage DSP requirements
- Starter files in `cpp files for agent/`: `Parameters.h` (full Params namespace with all IDs), `PluginProcessor.h` (class skeleton with all parameter pointers)
- UI reference in `heatdeath_ui.html` — visual design for the 800×540 panel
- Tape Burn-In supplemental notes in `tape_burn_grk.md`
- Signal chain insight: distortion feeds tremolo — clipped harmonics from RAT make the Undulator tremolo feel alive; MicroPitch converts mono RAT output to stereo beating source before Undulator sees it
- The Turbo RAT LED clipping (1.7V threshold vs 0.65V silicon) preserves more dynamic range for the tremolo to shape — this is load-bearing for the Ravedeath character
- LM308 slew rate (0.3V/μs) and diode asymmetry (0.20) are baked internal constants, not exposed as parameters

## Constraints

- **Tech Stack**: C++17 / JUCE 7+ — no third-party DSP libraries; JUCE only for plugin infrastructure and oversampling infrastructure
- **Starting Point**: Fresh JUCE AudioPlugin project (no existing scaffold) — scaffold must be created as Phase 1
- **Oversampling**: Waveshaper stages (RAT clip, H3000 Grit, tape hysteresis) must run at 4× minimum; tape hysteresis ODE additionally runs at 4× internal rate
- **Precision**: All LFO phase accumulators must use double-precision to avoid drift over long drone sessions (30+ min)
- **Parameter Smoothing**: All params 20ms, und_speed 50ms, bypass crossfades 10ms — no clicks on any control
- **Platforms**: macOS (VST3 + AU) and Windows (VST3) — must build on both
- **UI Size**: Fixed 800×540px non-resizable panel

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Single Burn macro knob for tape stage | All tape parameters (drive, bias, wow, flutter, hiss, head bump) move in a musically coherent arc together; exposing separately creates large tuning surface with few useful configurations | — Pending |
| Waver macro collapses 4 secondary LFO params | Rate, depth modulation, speed modulation, and drift always want to move together; more Waver = more alive | — Pending |
| Space/feedback coupled at fixed ratio (feedback = space × 0.65) | Spread without feedback sounds thin, feedback without spread sounds cluttered; coupling keeps them balanced | — Pending |
| Wide toggle (180° phase offset) default On | The spatial disorientation on Ravedeath depends on the antiphase L/R relationship — this is not optional for the Hecker sound | — Pending |
| Asymmetric MicroPitch defaults (−7¢ / +11¢) | Different cent values produce different beat rates per channel, so stereo image never cycles predictably; matches LM308 asymmetric clipping character | — Pending |
| No tempo sync | Hecker's music deliberately avoids rhythmic lock | — Pending |
| Fresh JUCE scaffold (not GSD plugin template) | User confirmed no existing template available | — Pending |

## Evolution

This document evolves at phase transitions and milestone boundaries.

**After each phase transition** (via `/gsd-transition`):
1. Requirements invalidated? → Move to Out of Scope with reason
2. Requirements validated? → Move to Validated with phase reference
3. New requirements emerged? → Add to Active
4. Decisions to log? → Add to Key Decisions
5. "What This Is" still accurate? → Update if drifted

**After each milestone** (via `/gsd-complete-milestone`):
1. Full review of all sections
2. Core Value check — still the right priority?
3. Audit Out of Scope — reasons still valid?
4. Update Context with current state

---
*Last updated: 2026-04-09 after initialization*
