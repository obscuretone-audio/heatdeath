# Requirements: HEATDEATH

**Defined:** 2026-04-09
**Core Value:** The four DSP stages must faithfully reproduce the specific hardware character Hecker used — not approximate it with generic waveshapers.

## v1 Requirements

### Project Scaffold

- [ ] **SCAF-01**: Fresh JUCE 7+ AudioPlugin project configured for VST3 + AU (macOS) and VST3 (Windows) targets using CMake
- [ ] **SCAF-02**: Source tree organized as specified: `Source/dsp/`, `Source/ui/`, `Source/utils/` with all headers and implementation files stubbed
- [ ] **SCAF-03**: CMakeLists.txt includes JUCE as a submodule or FetchContent dependency; builds cleanly on macOS and Windows
- [ ] **SCAF-04**: Plugin identity: name "HEATDEATH", no MIDI, mono-in/stereo-out bus layout, 4.0s tail length

### Parameter System

- [ ] **PARAMS-01**: Full APVTS definition in `Parameters.cpp` using all ID constants from `Parameters.h` (Params namespace); no raw string IDs outside that file
- [ ] **PARAMS-02**: All parameter pointers cached in `prepareToPlay()` via `cacheParameterPointers()` — no APVTS hash lookups inside `processBlock`
- [ ] **PARAMS-03**: Parameter groups match four hardware stages for DAW automation lane display (RAT / MicroPitch / Undulator / Burn-In / Global)
- [ ] **PARAMS-04**: All float parameters smoothed over 20ms; `und_speed` smoothed over 50ms; bypass toggles use 10ms crossfade — no audible clicks on any control change

### Signal Chain

- [ ] **CHAIN-01**: `processBlock` sums stereo input to mono (L+R × 0.5) before Stage 1; stereo field created entirely by Stage 2 (MicroPitch)
- [ ] **CHAIN-02**: 4× polyphase FIR oversampling applied at plugin boundary; waveshaper stages (RAT clipper, Undulator Grit, tape hysteresis) run at 4× rate
- [ ] **CHAIN-03**: DC blocking (first-order HPF at ~5Hz) applied after each stage output (post-RAT, post-MicroPitch, post-Undulator)
- [ ] **CHAIN-04**: Input limiter (soft tanh clip at unity threshold) applied at plugin boundary before Stage 1
- [ ] **CHAIN-05**: Per-stage bypass with 10ms dry/wet crossfade; global parallel wet/dry mix (clean mono input held at plugin boundary, blended with stereo processed output)
- [ ] **CHAIN-06**: Global feedback path — fraction of final output routed back to Stage 1 input (0–15%), heavy LP at 100Hz; only active when `global_feedback_active` is true
- [ ] **CHAIN-07**: Inter-stage trim controls (±12dB) at each stage junction (post-RAT, post-MicroPitch, post-Undulator)

### Stage 1 — Turbo RAT

- [ ] **RAT-01**: Two cascaded first-order HPFs before clipping: 60Hz (40dB/decade) and 1.5kHz (20dB/decade) — frequency-selective bass protection
- [ ] **RAT-02**: Gain-dependent LM308 slew-rate LP: baked constant `kDefaultSlew = 0.68f` → ~1040Hz bandwidth; cutoff not user-adjustable; applied before waveshaper
- [ ] **RAT-03**: GBW dominant pole LP: `gbwHz = 600 / max(drive, 0.01)` clamped to [200, 8000Hz]; peaks ~600Hz at max drive
- [ ] **RAT-04**: Asymmetric tanh diode waveshaper: LED threshold 1.7V, Silicon 0.65V, Lift ~∞ (soft knee only); baked asymmetry `kDefaultAsym = 0.20f`; normalized so 0dBFS in → 0dBFS out at unity gain
- [ ] **RAT-05**: Post-clip reverse-wired tone LPF: filter=0 → 475Hz (dark), filter=1 → 32kHz (bright); matches real RAT's counterintuitive wiring
- [ ] **RAT-06**: JFET output buffer: first-order LP at 18kHz + `volume` gain scalar (0–2.0)
- [ ] **RAT-07**: All float params smoothed per-sample (20ms); filter coefficients updated per-block from smoothed values, never per-sample

### Stage 2 — H3000 MicroPitch

- [ ] **PITCH-01**: Dual-voice pitch shift — left channel shifted by `mp_cents_l` (−2¢ to −25¢, default −7¢), right by `mp_cents_r` (+2¢ to +25¢, default +11¢); asymmetric defaults deliberate
- [ ] **PITCH-02**: Pitch shift implemented via phase modulation / SSB approximation — `cos(2π × freq_shift × t)` applied per sample; `freq_shift = focus_hz × (2^(cents/1200) − 1)`
- [ ] **PITCH-03**: Style I (H3000 preset #231) and Style II (H3000 preset #519) modes affecting the pitch modulation character
- [ ] **PITCH-04**: `mp_pitch_mix` controls blend between pure pitch-shift (SSB phase modulation) and delay-based shimmer; `mp_mix` controls overall wet/dry
- [ ] **PITCH-05**: `mp_delay_l` / `mp_delay_r` (0–30ms, defaults 8ms/18ms): fixed pre-shimmer delay per channel; creates thickening distinct from the pitch beating
- [ ] **PITCH-06**: `mp_width` (0–100): stereo spread — 100 = hard L/R pan (default), 0 = both voices center (chorus-like)

### Stage 3 — H3000 Undulator

- [ ] **UND-01**: Primary LFO with 9 waveform shapes: Sine, Triangle, Peak (fast attack/slow decay), Random (two inharmonic sines), Ramp, Square, S&H, Envelope (tracks input amplitude), ADSR (input triggers contour)
- [ ] **UND-02**: Waver macro (single knob) simultaneously drives secondary LFO rate (0.25–1.1Hz), depth modulation amount (0–70%), speed modulation amount (0–50%), and LFO phase drift (Brownian motion on accumulator); secondary shape locked to Sine internally
- [ ] **UND-03**: AM envelope formula: `am = (1 − depth×0.5) + depth×0.5×(lfo×0.5 + 0.5)` — never fully silences (unless depth=100%), never clips
- [ ] **UND-04**: Wide toggle: On = 180° L/R phase offset (autopanner mode, default On for Ravedeath character); Off = 0° (standard tremolo)
- [ ] **UND-05**: Space macro: controls detuned delay spread and feedback at fixed ratio (feedback = space × 0.65); left delay = half, right = full of Space-derived spread; asymmetric build prevents mono collapse
- [ ] **UND-06**: Grit — TMS32010 16-bit fixed-point overflow (two's-complement wraparound) applied inside the feedback loop; pre-emphasis (+6dB HF shelf, ~3kHz) before saturation, de-emphasis (mirror curve) after
- [ ] **UND-07**: All LFO phase accumulators double-precision to prevent drift over 30+ minute drone sessions
- [ ] **UND-08**: Stereo phase offset `und_phase` (0–360°, default 180°) exposed as a continuous parameter; Wide toggle is a convenience shortcut to 180°/0°

### Stage 4 — Tape Burn-In

- [ ] **BURN-01**: Jiles-Atherton magnetisation ODE solved per sample using 4th-order Runge-Kutta at 4× oversampling; Ferric Oxide parameters fixed: Ms=3.5×10⁵ A/m, k=27kA/m, a=22kA/m, c=0.17, α=1.6×10⁻³
- [ ] **BURN-02**: Single `tape_burn` knob macro-expands to all internal parameters: drive (0→0.55), saturation scalar (0.15→0.90), bias (0.75→0.40 decreasing), wow depth (0→0.007 quadratic), flutter depth (0→0.0025 quadratic), hiss level (0→0.22), asperity noise (0→0.45 quadratic), head bump gain (1.5→5.0dB)
- [ ] **BURN-03**: Tape bias: 55kHz ultrasonic signal (amplitude ~10× audio) mixed before hysteresis; under-biasing at high Burn creates zero-crossing granularity texture
- [ ] **BURN-04**: Wow: LP-filtered (0.7Hz) noise variable delay; Flutter: periodically-randomized LFO at 8Hz variable delay; both near-zero below Burn=0.5, perceptible above
- [ ] **BURN-05**: Head bump: peaking biquad at 90Hz, Q=1.5, gain 1.5–5.0dB; always on (even at Burn=0 provides low-end body)
- [ ] **BURN-06**: HF loss: 2nd-order Butterworth LP at 10500Hz (fixed 15ips tape speed)
- [ ] **BURN-07**: Hiss: pink noise (Paul Kellet 7-pole method); asperity noise: level-dependent, scales with signal envelope, disappears during silences
- [ ] **BURN-08**: Thermal state persistence: when `burnin_persist` is true, `persistedTemp` and `persistedTempPrev` are serialized into plugin state and restored on reload
- [ ] **BURN-09**: Freeze toggle: holds thermal accumulator at current value; `tape_burn` parameter becomes read-only while active

### UI

- [ ] **UI-01**: Fixed 800×540px single panel, non-resizable; four-column layout with column widths proportional to control count; Burn-In column carries intentional whitespace
- [ ] **UI-02**: Color scheme: `#111111` background, `#c8a96e` accent (worn gold), `#888888` labels; monospace font throughout
- [ ] **UI-03**: No oscilloscope, no spectrum display, no VU meter — UI matches the HTML reference design
- [ ] **UI-04**: Per-stage bypass toggles visible in UI; stage panels laid out as in the HTML reference
- [ ] **UI-05**: Custom LookAndFeel (`HeatdeathLookAndFeel`) handles all knob and toggle rendering
- [ ] **UI-06**: Inter-stage trim controls (small, secondary) visible at each stage junction

### Presets

- [ ] **PRST-01**: "The Piano Drop" factory preset — Drive=72, Filter=35, Vol=60, Clip=LED, Detune L=−7¢, R=+11¢, Mix=85%, Depth=82, Speed=2.4Hz, Shape=Sine, Space=47, Waver=42, Grit=62, Wide=On, Burn=35%
- [ ] **PRST-02**: "In The Fog" factory preset — Drive=35, Filter=50, Vol=65, Clip=Si, Detune L=−5¢, R=+7¢, Mix=70%, Depth=65, Speed=1.2Hz, Space=20, Waver=18, Grit=30, Wide=Off, Burn=18%
- [ ] **PRST-03**: "Scorched" factory preset — Drive=90, Filter=15, Vol=55, Clip=LED, Detune L=−11¢, R=+14¢, Mix=90%, Depth=95, Speed=3.8Hz, Space=75, Waver=78, Grit=85, Wide=On, Burn=82%

### Hidden Features

- [ ] **HIDN-01**: Disintegration Timer — drives `tape_burn` to 1.0 over selected duration (10/20/40/74 min); `tape_burn` knob locked while active; timer state serialized and survives plugin reload; activated via hidden UI trigger
- [ ] **HIDN-02**: Acetate Mode — alternate tape physics: sharper oxide shedding transitions, lower stick-slip threshold, sudden pitch-drop artifact at high temp, faster demagnetization fall; `acetate_mode` bool parameter, no UI label
- [ ] **HIDN-03**: M/S mode for Burn-In — input decoded to M/S before Stage 4, re-encoded after (`global_ms_mode` bool)
- [ ] **HIDN-04**: Global feedback path UI controls (expert section, `global_feedback_amount` + `global_feedback_active`)

### Build & Integration

- [ ] **BUILD-01**: Plugin builds as VST3 on macOS (arm64 + x86_64 universal) and Windows (x64)
- [ ] **BUILD-02**: Plugin builds as AU (AudioUnit v2) on macOS
- [ ] **BUILD-03**: State save/restore (`getStateInformation` / `setStateInformation`) via APVTS XML + thermal state persisted separately when `burnin_persist` is true
- [ ] **BUILD-04**: No memory allocation in `processBlock`; dry buffer and feedback buffer pre-allocated in `prepareToPlay`

### DAW Integration Testing

- [ ] **DAW-01**: Plugin loads and instantiates in Ableton Live as VST3 (macOS) without crash or error on scan
- [ ] **DAW-02**: Plugin loads and instantiates in Ableton Live as AU (macOS) without crash or error on scan
- [ ] **DAW-03**: All parameters appear correctly in Ableton Live's automation lane with correct names, ranges, and defaults
- [ ] **DAW-04**: Audio passes through the full signal chain (all four stages active) in Ableton Live with no dropouts, glitches, or DC offset accumulation at 44.1kHz and 48kHz sample rates
- [ ] **DAW-05**: Plugin state saves and restores correctly across Ableton Live project save/reload cycles (all parameter values and thermal state preserved)
- [ ] **DAW-06**: All three factory presets load without error; "The Piano Drop" preset produces audible stereo tremolo character when fed a signal in Live

## v2 Requirements

### Future Enhancements

- **V2-01**: MIDI learn for parameter automation
- **V2-02**: Resizable UI
- **V2-03**: Additional LFO shapes for Undulator
- **V2-04**: Oversampling quality selector (2×/4×/8×)

## Out of Scope

| Feature | Reason |
|---------|--------|
| Third-party DSP libraries (iPlug2, signalsmith, etc.) | Spec requires from-scratch implementation for exact hardware character |
| JUCE DSP module wrappers for signal processing | Only `juce::dsp::Oversampling` permitted; all DSP from scratch |
| Tempo sync / DAW BPM sync | Hecker's music deliberately avoids rhythmic lock; all rates in Hz only |
| Spectrum display, oscilloscope, visual metering | Spec explicitly excludes these from the UI |
| Standalone app target | Plugin-only (VST3 + AU) |
| MIDI control | Plugin does not accept MIDI input |
| Linux target | Mac + Windows only for v1 |
| Preset browser / cloud presets | Three factory presets sufficient for v1 |

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| SCAF-01 through SCAF-04 | Phase 1 | Pending |
| PARAMS-01 through PARAMS-04 | Phase 2 | Pending |
| CHAIN-01 through CHAIN-07 | Phase 2 | Pending |
| RAT-01 through RAT-07 | Phase 3 | Pending |
| PITCH-01 through PITCH-06 | Phase 4 | Pending |
| UND-01 through UND-08 | Phase 5 | Pending |
| BURN-01 through BURN-09 | Phase 6 | Pending |
| UI-01 through UI-06 | Phase 7 | Pending |
| PRST-01 through PRST-03 | Phase 7 | Pending |
| HIDN-01 through HIDN-04 | Phase 8 | Pending |
| BUILD-01 through BUILD-04 | Phase 8 | Pending |
| DAW-01 through DAW-06 | Phase 9 | Pending |

**Coverage:**
- v1 requirements: 50 total
- Mapped to phases: 50
- Unmapped: 0 ✓

---
*Requirements defined: 2026-04-09*
*Last updated: 2026-04-09 after initial definition*
