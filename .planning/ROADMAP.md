# Roadmap: HEATDEATH

## Overview

Starting from a fresh JUCE AudioPlugin project, HEATDEATH delivers a four-stage hardware emulation chain: Turbo RAT distortion, H3000 MicroPitch stereo detuning, H3000 Undulator AM/FM tremolo, and Tape Burn-In hysteresis saturation. The build is organized so each phase produces a verifiably working stage before the next begins — no stage is stubbed and shipped; each must pass its own signal integrity test. The final phases layer UI, hidden features, cross-platform build, and DAW integration verification on top of a fully functional DSP chain.

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

- [x] **Phase 1: Project Scaffold** - Fresh JUCE CMake project with correct VST3/AU targets, source tree, and plugin identity
- [ ] **Phase 2: Core Infrastructure** - APVTS parameter system, signal chain skeleton, oversampling, DC blocking, bypass, and smoothing
- [ ] **Phase 3: Stage 1 — Turbo RAT** - Complete LM308 slew-rate distortion DSP with HPF chain, asymmetric diode clipper, tone filter, and JFET buffer
- [ ] **Phase 4: Stage 2 — MicroPitch** - Dual-voice SSB pitch shift with asymmetric cent defaults, stereo spread, and delay shimmer
- [ ] **Phase 5: Stage 3 — Undulator** - Dual-LFO AM/FM tremolo with 9 shapes, Waver/Space/Grit macros, and double-precision accumulators
- [ ] **Phase 6: Stage 4 — Tape Burn-In** - Jiles-Atherton ODE with RK4 at 4x oversampling, Burn macro, wow/flutter, hiss, and thermal persistence
- [ ] **Phase 7: UI + Presets** - HeatdeathLookAndFeel, 800x540 four-column panel, all stage controls, inter-stage trims, and three factory presets
- [ ] **Phase 8: Hidden Features + Cross-Platform Build** - Disintegration Timer, Acetate Mode, M/S mode, global feedback UI, universal binary, AU target, and state serialization
- [ ] **Phase 9: DAW Integration Testing** - Load/scan in Ableton Live (VST3 + AU), parameter automation, audio at 44.1kHz/48kHz, state save/restore, and preset verification

## Phase Details

### Phase 1: Project Scaffold
**Goal**: A buildable JUCE CMake project exists with correct targets, source tree, plugin identity, and no DSP
**Depends on**: Nothing (first phase)
**Requirements**: SCAF-01, SCAF-02, SCAF-03, SCAF-04
**Success Criteria** (what must be TRUE):
  1. `cmake --build` completes without error on macOS producing a VST3 bundle and AU component
  2. The plugin loads in a DAW or plugin validator showing name "HEATDEATH", mono-in/stereo-out, no MIDI, 4.0s tail
  3. Source tree contains `Source/dsp/`, `Source/ui/`, `Source/utils/` with all specified headers and .cpp stubs present
  4. JUCE is included via CMake (submodule or FetchContent) and not vendored manually
**Plans**: 3 plans

Plans:
- [x] 01-01: Initialize CMakeLists.txt with JUCE FetchContent/submodule, define VST3 + AU targets with correct plugin identity (SCAF-01, SCAF-03, SCAF-04)
- [x] 01-02: Create source tree scaffold — `Source/dsp/`, `Source/ui/`, `Source/utils/`; stub all headers referenced in `Parameters.h` and `PluginProcessor.h` (SCAF-02)
- [x] 01-03: Verify clean build on macOS, confirm plugin validator accepts the bundle with correct name, bus layout, tail length, and no MIDI flag (SCAF-04)

### Phase 2: Core Infrastructure
**Goal**: The plugin processes audio end-to-end through a signal chain skeleton with APVTS parameters, oversampling, DC blocking, per-stage bypass, and parameter smoothing — all stages pass-through but the plumbing is complete
**Depends on**: Phase 1
**Requirements**: PARAMS-01, PARAMS-02, PARAMS-03, PARAMS-04, CHAIN-01, CHAIN-02, CHAIN-03, CHAIN-04, CHAIN-05, CHAIN-06, CHAIN-07
**Success Criteria** (what must be TRUE):
  1. All 50+ parameters appear in the plugin with correct IDs, ranges, and defaults — no raw string lookups in `processBlock`
  2. Audio passes from mono-summed input through four stage slots and out as stereo with no crash, no DC offset, and no clicks when bypassing stages
  3. 4x oversampling is active at the waveshaper stage boundaries; DC-blocking HPF fires after each stage output
  4. Parameter changes on any knob produce no audible click (20ms smoothing; 50ms for und_speed)
  5. Dry buffer and feedback buffer are pre-allocated in `prepareToPlay`; no allocations occur inside `processBlock`
**Plans**: 4 plans

Plans:
- [x] 02-01: Implement full APVTS in `Parameters.cpp` from `Parameters.h` constants; add parameter groups (RAT / MicroPitch / Undulator / Burn-In / Global); cache all pointers in `cacheParameterPointers()` (PARAMS-01, PARAMS-02, PARAMS-03)
- [x] 02-02: Implement parameter smoothing infrastructure — `juce::SmoothedValue` for all float params at 20ms, `und_speed` at 50ms, bypass crossfades at 10ms; attach to all parameter updates (PARAMS-04, CHAIN-05)
- [x] 02-03: Build `processBlock` skeleton — mono sum input, input limiter (soft tanh), four stage slots (pass-through stubs), DC block after each stage, per-stage bypass crossfade, global parallel wet/dry blend; pre-allocate dry buffer and feedback buffer in `prepareToPlay` (CHAIN-01, CHAIN-03, CHAIN-04, CHAIN-05, BUILD-04)
- [ ] 02-04: Wire 4x polyphase FIR oversampling at plugin boundary; add inter-stage trim controls (±12dB) at each stage junction; wire global feedback path stub with 100Hz LP filter (CHAIN-02, CHAIN-06, CHAIN-07)
**UI hint**: no

### Phase 3: Stage 1 — Turbo RAT
**Goal**: The Turbo RAT stage produces authentic LM308 slew-rate distortion with pre-clip HPF chain, asymmetric diode clipping, reverse-wired tone filter, and JFET output buffer running at 4x oversampling
**Depends on**: Phase 2
**Requirements**: RAT-01, RAT-02, RAT-03, RAT-04, RAT-05, RAT-06, RAT-07
**Success Criteria** (what must be TRUE):
  1. A sine at 0dBFS input passes through RAT producing audible harmonic distortion whose character changes with Drive, Filter, and Clip mode controls
  2. LED clip mode (1.7V threshold) preserves more dynamic headroom than Silicon mode (0.65V threshold) — measurable as lower peak gain reduction at equivalent drive
  3. Filter=0 produces a dark, rolled-off output (~475Hz LPF); Filter=1 produces a bright full-spectrum output (~32kHz) — reverse of intuitive direction confirmed
  4. Gain-dependent GBW pole (`600/drive`) shifts audibly as Drive increases; slew-rate limiting is active at high drive values
  5. No per-sample filter coefficient recalculation — coefficients update per-block from smoothed values only
**Plans**: 4 plans

Plans:
- [ ] 03-01: Implement pre-clip HPF chain — two cascaded first-order HPFs at 60Hz (40dB/decade) and 1.5kHz (20dB/decade); update coefficients per-block from smoothed drive value (RAT-01)
- [ ] 03-02: Implement LM308 gain path — slew-rate LP with baked `kDefaultSlew=0.68f` (~1040Hz bandwidth) and GBW dominant pole `gbwHz=600/max(drive,0.01)` clamped [200, 8000Hz]; both applied before waveshaper (RAT-02, RAT-03)
- [ ] 03-03: Implement asymmetric tanh diode waveshaper — LED threshold 1.7V, Silicon 0.65V, Lift soft knee; baked asymmetry `kDefaultAsym=0.20f`; normalized to 0dBFS unity gain; runs at 4x oversample rate (RAT-04)
- [ ] 03-04: Implement post-clip tone LPF (reverse-wired, 475Hz–32kHz) and JFET output buffer (18kHz LP + volume scalar 0–2.0); wire all RAT params through 20ms smoothers; per-block coefficient update (RAT-05, RAT-06, RAT-07)

### Phase 4: Stage 2 — MicroPitch
**Goal**: The MicroPitch stage converts mono RAT output to stereo via dual-voice SSB pitch shift with asymmetric cent defaults, pre-shimmer delay, stereo width control, and wet/dry blend
**Depends on**: Phase 3
**Requirements**: PITCH-01, PITCH-02, PITCH-03, PITCH-04, PITCH-05, PITCH-06
**Success Criteria** (what must be TRUE):
  1. Stereo output from MicroPitch contains two audibly distinct pitch-shifted voices (left detuned ~-7¢, right ~+11¢ at defaults) producing a beating stereo image
  2. The stereo image never fully collapses to mono even with Width=0 (chorus-like center remains distinct from hard-pan at Width=100)
  3. Style I and Style II modes produce perceptibly different pitch modulation character on a sustained tone
  4. `mp_pitch_mix` audibly shifts the stage between pure SSB phase modulation and shimmer/delay character
  5. Pre-shimmer delays (`mp_delay_l` 8ms default, `mp_delay_r` 18ms default) produce thickening independent of pitch beating
**Plans**: 3 plans

Plans:
- [ ] 04-01: Implement SSB pitch shift core — `freq_shift = focus_hz × (2^(cents/1200) − 1)`, apply `cos(2π × freq_shift × t)` per sample for each channel; implement Style I (preset #231) and Style II (preset #519) modulation character modes (PITCH-01, PITCH-02, PITCH-03)
- [ ] 04-02: Implement pre-shimmer delay lines (`mp_delay_l` / `mp_delay_r`, 0–30ms); implement `mp_pitch_mix` blend between SSB and delay-based shimmer path (PITCH-04, PITCH-05)
- [ ] 04-03: Implement `mp_width` stereo spread (0 = both voices center, 100 = hard L/R pan); wire `mp_mix` overall wet/dry; verify asymmetric defaults (-7¢ / +11¢) are correctly set in parameter definition (PITCH-06, PITCH-01)

### Phase 5: Stage 3 — Undulator
**Goal**: The Undulator stage delivers dual-LFO AM/FM tremolo with all 9 primary waveform shapes, Waver/Space/Grit macros, 180-degree phase offset, and double-precision accumulators that do not drift over 30-minute sessions
**Depends on**: Phase 4
**Requirements**: UND-01, UND-02, UND-03, UND-04, UND-05, UND-06, UND-07, UND-08
**Success Criteria** (what must be TRUE):
  1. All 9 LFO shapes (Sine, Triangle, Peak, Random, Ramp, Square, S&H, Envelope, ADSR) produce audibly distinct tremolo characters on a sustained tone
  2. Wide toggle On produces hard L/R antiphase autopanner effect; Wide toggle Off produces in-phase stereo tremolo
  3. Increasing Waver macro simultaneously increases LFO secondary rate, depth modulation, speed modulation, and introduces phase drift — these move as one
  4. Increasing Space macro spreads detuned delay and introduces feedback at fixed ratio (feedback = Space × 0.65) without mono collapse
  5. Grit applies audible digital crunch (TMS32010 overflow character) with pre/de-emphasis that disappears cleanly at Grit=0
  6. After a 30-minute test render, LFO phase accumulators show no measurable drift (double-precision verified)
**Plans**: 4 plans

Plans:
- [ ] 05-01: Implement primary LFO with all 9 shapes using double-precision accumulators; implement AM formula `am = (1−depth×0.5) + depth×0.5×(lfo×0.5+0.5)`; implement `und_phase` continuous parameter (0–360°) and Wide toggle convenience shortcut (UND-01, UND-03, UND-04, UND-07, UND-08)
- [ ] 05-02: Implement Waver macro — simultaneously drive secondary LFO rate (0.25–1.1Hz), depth modulation amount (0–70%), speed modulation amount (0–50%), and Brownian motion phase drift on accumulator; secondary shape locked to Sine (UND-02)
- [ ] 05-03: Implement Space macro — detuned delay spread with asymmetric L/R (left=half, right=full of Space-derived spread), feedback at fixed ratio (feedback=Space×0.65); prevent mono collapse (UND-05)
- [ ] 05-04: Implement Grit — TMS32010 16-bit two's-complement overflow inside feedback loop; pre-emphasis (+6dB HF shelf ~3kHz) before saturation, de-emphasis (mirror) after; runs at 4x oversampling (UND-06)

### Phase 6: Stage 4 — Tape Burn-In
**Goal**: The Tape Burn-In stage delivers Jiles-Atherton hysteresis ODE solved with RK4 at 4x oversampling, a single Burn macro that coherently expands all tape parameters, wow/flutter, hiss, head bump, and thermal state persistence
**Depends on**: Phase 5
**Requirements**: BURN-01, BURN-02, BURN-03, BURN-04, BURN-05, BURN-06, BURN-07, BURN-08, BURN-09
**Success Criteria** (what must be TRUE):
  1. At Burn=0 the stage applies audible head bump (low-end body at 90Hz) and HF rolloff but no saturation, wow, flutter, or hiss
  2. Increasing Burn from 0→1 coherently progresses from subtle tape color to heavy saturation with wow, flutter, hiss, and asperity noise — all moving together on the single knob
  3. Tape bias texture (zero-crossing granularity from under-biasing) is audible at high Burn values
  4. Wow and flutter are near-zero below Burn=0.5 and perceptible above — not present at low settings
  5. Freeze toggle locks the thermal accumulator; the Burn knob has no further effect while Freeze is active
  6. Plugin state saved with `burnin_persist=true` restores thermal state (persistedTemp values) identically after reload
**Plans**: 4 plans

Plans:
- [ ] 06-01: Implement Jiles-Atherton ODE core — RK4 solver at 4x oversampling with Ferric Oxide fixed parameters (Ms=3.5e5 A/m, k=27kA/m, a=22kA/m, c=0.17, α=1.6e-3); implement `tape_burn` macro expansion table for all internal parameters (BURN-01, BURN-02)
- [ ] 06-02: Implement tape bias (55kHz ultrasonic signal mixed pre-hysteresis), wow (LP-filtered 0.7Hz noise delay), and flutter (8Hz periodically-randomized LFO delay) (BURN-03, BURN-04)
- [ ] 06-03: Implement head bump (peaking biquad 90Hz, Q=1.5, 1.5–5.0dB), HF loss (2nd-order Butterworth LP at 10500Hz fixed), hiss (Paul Kellet 7-pole pink noise), and asperity noise (level-dependent, signal-envelope-scaled) (BURN-05, BURN-06, BURN-07)
- [ ] 06-04: Implement thermal state persistence — serialize `persistedTemp` and `persistedTempPrev` into plugin state when `burnin_persist=true`; implement Freeze toggle to hold thermal accumulator and make `tape_burn` read-only (BURN-08, BURN-09)

### Phase 7: UI + Presets
**Goal**: The plugin has a complete 800x540px non-resizable panel with HeatdeathLookAndFeel, all four stage columns, inter-stage trim controls, per-stage bypass toggles, and three loadable factory presets
**Depends on**: Phase 6
**Requirements**: UI-01, UI-02, UI-03, UI-04, UI-05, UI-06, PRST-01, PRST-02, PRST-03
**Success Criteria** (what must be TRUE):
  1. The plugin editor opens at exactly 800x540px and cannot be resized in any host
  2. All four stage panels are visible with correct `#111111` background, `#c8a96e` accent knobs, `#888888` labels, and monospace font — matching the HTML reference
  3. Every parameter exposed in the requirements has a corresponding UI control; no oscilloscope, spectrum display, or VU meter is present
  4. Per-stage bypass toggles and inter-stage trim controls are visible and functional in the UI
  5. Loading "The Piano Drop" preset sets Drive=72, Filter=35, Clip=LED, Detune L=-7c, R=+11c, Wide=On, Burn=35% (and all other values) without error
  6. All three factory presets load without error and produce audibly distinct characters when fed a signal
**Plans**: 4 plans

Plans:
- [ ] 07-01: Implement `HeatdeathLookAndFeel` — custom knob rendering, toggle rendering, color scheme (`#111111` bg, `#c8a96e` accent, `#888888` labels), monospace font throughout; no meters, no oscilloscope (UI-02, UI-03, UI-05)
- [ ] 07-02: Build main 800x540 panel with four-column layout; implement RAT and MicroPitch column panels with all parameter controls attached to APVTS; include per-stage bypass toggles (UI-01, UI-04)
- [ ] 07-03: Build Undulator and Burn-In column panels; add inter-stage trim controls (small, secondary) at each junction; wire all remaining stage parameters to APVTS; confirm non-resizable (UI-01, UI-04, UI-06)
- [ ] 07-04: Implement three factory presets as APVTS XML state files — "The Piano Drop" (PRST-01), "In The Fog" (PRST-02), "Scorched" (PRST-03); wire preset loading to the UI; verify each loads all parameter values correctly (PRST-01, PRST-02, PRST-03)
**UI hint**: yes

### Phase 8: Hidden Features + Cross-Platform Build
**Goal**: Disintegration Timer, Acetate Mode, M/S mode, and global feedback UI are implemented; the plugin builds as a macOS universal binary (VST3 + AU) and Windows VST3; state serialization covers all parameters and thermal state
**Depends on**: Phase 7
**Requirements**: HIDN-01, HIDN-02, HIDN-03, HIDN-04, BUILD-01, BUILD-02, BUILD-03, BUILD-04
**Success Criteria** (what must be TRUE):
  1. Disintegration Timer activates via its hidden UI trigger, drives `tape_burn` to 1.0 over the selected duration (10/20/40/74 min), locks the Burn knob, and survives plugin reload with timer position intact
  2. Acetate Mode (activated via hidden `acetate_mode` bool) produces perceptibly sharper oxide shedding and sudden pitch-drop artifacts at high temp versus standard Ferric mode
  3. M/S mode (global_ms_mode) correctly decodes stereo to M/S before Stage 4 and re-encodes after — center content and side content are processed independently
  4. Global feedback UI controls (amount + active toggle) are visible in an expert section and function correctly
  5. Plugin builds as macOS arm64+x86_64 universal binary producing both a VST3 bundle and AU component
  6. Plugin state saves and restores all parameter values and thermal persistence state without loss
**Plans**: 4 plans

Plans:
- [ ] 08-01: Implement Disintegration Timer — `tape_burn` automation ramp over 10/20/40/74 min, Burn knob locked while active, timer state serialized into plugin state, hidden UI trigger (HIDN-01)
- [ ] 08-02: Implement Acetate Mode (`acetate_mode` bool) — alternate tape physics branch with sharper oxide shedding, lower stick-slip threshold, sudden pitch-drop artifact at high temp, faster demagnetization; no UI label (HIDN-02); implement M/S decode/encode wrapper for Stage 4 (`global_ms_mode`) (HIDN-03)
- [ ] 08-03: Implement global feedback UI expert section — `global_feedback_amount` knob and `global_feedback_active` toggle wired to the CHAIN-06 feedback path already built in Phase 2 (HIDN-04); implement `getStateInformation`/`setStateInformation` via APVTS XML plus separate thermal state blob when `burnin_persist=true` (BUILD-03)
- [ ] 08-04: Configure CMake for macOS universal binary (arm64 + x86_64 lipo); confirm AU (AudioUnit v2) target builds and passes AU validation; confirm Windows VST3 cross-compile configuration is in place (BUILD-01, BUILD-02, BUILD-04)
**UI hint**: yes

### Phase 9: DAW Integration Testing
**Goal**: The plugin loads, scans, passes audio, saves state, and plays presets correctly in Ableton Live at both 44.1kHz and 48kHz — VST3 and AU verified
**Depends on**: Phase 8
**Requirements**: DAW-01, DAW-02, DAW-03, DAW-04, DAW-05, DAW-06
**Success Criteria** (what must be TRUE):
  1. HEATDEATH VST3 scans and instantiates in Ableton Live on macOS without crash, error, or warning in the scan log
  2. HEATDEATH AU scans and instantiates in Ableton Live on macOS without crash or error; AU validation utility reports pass
  3. All parameters appear in Ableton Live's automation lane with correct names, ranges, and default values — no missing or misnamed parameters
  4. Full four-stage audio chain processes correctly in Live at 44.1kHz and 48kHz with no dropouts, glitches, or DC offset accumulation after 10 minutes of continuous playback
  5. Plugin state (all parameter values and thermal persistence) saves and restores correctly across Ableton Live project save/reload cycles
  6. All three factory presets load without error; "The Piano Drop" produces audible stereo tremolo character when fed a piano signal in Live
**Plans**: 3 plans

Plans:
- [ ] 09-01: Install VST3 and AU builds into system plugin folders; run Ableton Live scan; confirm both formats load without crash; run AU validation utility; document any scan warnings (DAW-01, DAW-02)
- [ ] 09-02: Open HEATDEATH in Live; verify all automation lanes show correct parameter names/ranges/defaults; create a test Live Set with audio at 44.1kHz and 48kHz; confirm full chain passes audio with no dropouts, glitches, or DC offset over 10-minute playback (DAW-03, DAW-04)
- [ ] 09-03: Test state save/restore — save Live Set with specific parameter values and thermal state, reload, confirm all values match; load each factory preset and verify "The Piano Drop" produces stereo tremolo character on a piano signal (DAW-05, DAW-06)

## Progress

**Execution Order:**
Phases execute in numeric order: 1 → 2 → 3 → 4 → 5 → 6 → 7 → 8 → 9

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Project Scaffold | 3/3 | Complete | 2026-04-09 |
| 2. Core Infrastructure | 0/4 | Not started | - |
| 3. Stage 1 — Turbo RAT | 0/4 | Not started | - |
| 4. Stage 2 — MicroPitch | 0/3 | Not started | - |
| 5. Stage 3 — Undulator | 0/4 | Not started | - |
| 6. Stage 4 — Tape Burn-In | 0/4 | Not started | - |
| 7. UI + Presets | 0/4 | Not started | - |
| 8. Hidden Features + Cross-Platform Build | 0/4 | Not started | - |
| 9. DAW Integration Testing | 0/3 | Not started | - |
