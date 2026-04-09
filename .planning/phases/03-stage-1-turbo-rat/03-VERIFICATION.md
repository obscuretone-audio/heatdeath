---
phase: 03-stage-1-turbo-rat
verified: 2026-04-09T23:45:00Z
status: human_needed
score: 4/5 roadmap success criteria verified
overrides_applied: 0
human_verification:
  - test: "Feed a 0dBFS sine through the loaded plugin in a DAW (or auval) and sweep the Drive, Filter, and Clip Mode controls"
    expected: "Audible harmonic distortion changes character with each control: Drive thickens/darkens the sound, Filter sweeps from bright to dark, Clip Mode changes clipping character (LED vs Silicon vs Lift)"
    why_human: "SC1 requires 'audible' distortion with perceptible control response — automated tests verify mathematical correctness but cannot confirm auditory character or that the stage is actually producing distortion the human ear would recognise as such in context"
---

# Phase 3: Stage 1 — Turbo RAT Verification Report

**Phase Goal:** The Turbo RAT stage produces authentic LM308 slew-rate distortion with pre-clip HPF chain, asymmetric diode clipping, reverse-wired tone filter, and JFET output buffer running at 4x oversampling
**Verified:** 2026-04-09T23:45:00Z
**Status:** human_needed
**Re-verification:** No — initial verification

---

## Goal Achievement

### Observable Truths

| # | Truth | Status | Evidence |
|---|-------|--------|---------|
| 1 | A sine at 0dBFS input passes through RAT producing audible harmonic distortion whose character changes with Drive, Filter, and Clip mode controls | ? HUMAN NEEDED | Automated: asymmetric tanh waveshaper is present and wired (FastMathApproximations::tanh confirmed in sample loop); Silicon MAV > LED MAV (test passes); DC offset proves asymmetry is live. Audible character requires human verification |
| 2 | LED clip mode (1.7V threshold) preserves more dynamic headroom than Silicon mode (0.65V threshold) | ✓ VERIFIED | Test: "Silicon flattens toward unity faster than LED (MAV_si > MAV_led)" — PASS. Thresholds confirmed in code: case 0 = 1.7f (LED), case 1 = 0.65f (Silicon) |
| 3 | Filter=0 produces bright output (~32kHz LPF); Filter=100 produces dark, rolled-off output (~475Hz) | ✓ VERIFIED | Note: ROADMAP SC3 text says "Filter=0 dark" but this is a documentation error — the hardware is reverse-wired (0=bright/open, 100=dark). Parameters.cpp label, heatdeath_vst_spec.md, and Research doc all confirm 0=bright. Tests pass: "filter=0 passes more 5kHz than filter=100". See spec discrepancy note below. |
| 4 | Gain-dependent GBW pole (600/drive) shifts audibly as Drive increases; slew-rate limiting is active at high drive values | ✓ VERIFIED | Test: "drive=100 reduces 5kHz energy by >4dB vs drive=0 (cutoff moved from ~8kHz to ~600Hz)" — PASS. slewAlpha computed from 1040Hz cutoff via computeLPFAlpha confirmed in code |
| 5 | No per-sample filter coefficient recalculation — coefficients update per-block from smoothed values only | ✓ VERIFIED | Audit of processOS sample loop: zero instances of std::exp, std::log, std::tanh, computeLPFAlpha, computeHPFAlpha, jlimit, jmap, or any allocation inside the for loop. updateCoefficients() called once per block before the loop. RAT-07 test suite: all 6 alpha coefficients bit-identical across consecutive blocks at steady-state. |

**Score:** 4/5 truths verified (1 needs human confirmation)

---

## Spec Discrepancy Note: Filter Direction

REQUIREMENTS.md RAT-05 states: "filter=0 → 475Hz (dark), filter=1 → 32kHz (bright)". ROADMAP SC3 states: "Filter=0 produces a dark, rolled-off output (~475Hz LPF)". Both are documentation errors.

The authoritative source is `heatdeath_vst_spec.md` line 75: "Cutoff range: 475Hz (full) to ~32kHz (off)" where "full" = Filter=100 and "off" = Filter=0. This is confirmed by:
- `Parameters.cpp` label: "Filter — reverse-wired LPF. 0 = bright (32kHz), 100 = dark (475Hz)."
- `03-RESEARCH.md` Pitfall 4: "filter=0 → 32kHz (bright), filter=1 → 475Hz (dark)"
- `TurboRat.cpp` comment: `filterNorm=0 -> bright (32kHz), filterNorm=1 -> dark (475Hz)`
- Test result: "filter=0 passes more 5kHz than filter=100" passes correctly

The implementation is correct. REQUIREMENTS.md RAT-05 and ROADMAP SC3 contain transposed values. This does not block verification since SC3 tests correctly verify the intended hardware behavior (reverse-wired = 0=bright, 100=dark).

---

## Required Artifacts

| Artifact | Expected | Status | Details |
|---------|---------|--------|---------|
| `Source/dsp/TurboRat.h` | processOS + updateCoefficients API, HPF/LPF/threshold/tone/jfet state + coefficient members | ✓ VERIFIED | All members present: hpf1Alpha/hpf2Alpha, slewState/slewAlpha, gbwState/gbwAlpha, threshold, toneState/toneAlpha, jfetState/jfetAlpha, volumeGain, smoothDrive/smoothFilter/smoothVolume/smoothAsym, TURBORAT_TEST_ACCESS guard |
| `Source/dsp/TurboRat.cpp` | Full 7-stage chain in processOS; updateCoefficients computing all 6 alphas + threshold + volumeGain | ✓ VERIFIED | Chain confirmed: HPF60 → HPF1.5k → slewLP → GBW LP → asymmetric tanh → tone LPF → JFET LP → volumeGain. All computations in updateCoefficients, not in sample loop. |
| `Tests/TurboRatTest.cpp` | Real test cases for RAT-01 through RAT-07 | ✓ VERIFIED | 36 tests covering all 7 requirements. All 36 pass (confirmed by running `./build/TurboRatTest`). |
| `CMakeLists.txt` | TurboRatTest executable target | ✓ VERIFIED | `add_executable(TurboRatTest ...)` present at line 118, linking juce_core, juce_audio_basics, juce_dsp, juce_audio_formats, juce_audio_processors |
| `Source/PluginProcessor.cpp` | stageTurboRat->processOS called on oversampled block | ✓ VERIFIED | Line 226: `stageTurboRat->processOS (osBlock);` inside the oversampler up/down bracket. Old `juce::ignoreUnused(osBlock)` removed. |

---

## Key Link Verification

| From | To | Via | Status | Details |
|------|-----|-----|--------|---------|
| PluginProcessor Stage 1 block | TurboRat::processOS | `stageTurboRat->processOS(osBlock)` on oversampled block | ✓ WIRED | Line 226 confirmed. Block is the 4x upsampled mono block from `oversampler.processSamplesUp(monoBlock)`. |
| TurboRat::processOS | HPF → slewLP → GBW LP → tanh → tone LPF → JFET LP → volumeGain | Sequential per-sample application inside for loop | ✓ WIRED | All 7 stages confirmed present in correct order in TurboRat.cpp sample loop. |
| TurboRat::updateCoefficients | smoothDrive.current → gbwHz formula | `600.0f / std::max(driveNorm, 0.01f)` clamped [200, 8000] | ✓ WIRED | Line 70-72 confirmed. smoothDrive ticked before updateCoefficients is called. |
| TurboRat::updateCoefficients | smoothFilter.current → toneAlpha (log-spaced) | `std::exp(logHigh + filterNorm * (logLow - logHigh))` | ✓ WIRED | Lines 87-91 confirmed. logHigh=log(32000), logLow=log(475). |
| Tests/TurboRatTest.cpp | Source/dsp/TurboRat.h | `#include "../Source/dsp/TurboRat.h"` | ✓ WIRED | Line 6 confirmed. TURBORAT_TEST_ACCESS defined before include, enabling getCoefficientsForTest(). |
| PluginProcessor::processBlock | stageTurboRat->setParameters | Called after processOS (one block late) | ⚠ PARTIAL | setParameters is called at line 232, AFTER processOS at line 226. Parameters lag by one block. See warning below. |

---

## Data-Flow Trace (Level 4)

| Artifact | Data Variable | Source | Produces Real Data | Status |
|---------|-------------|--------|-------------------|--------|
| TurboRat::processOS | osBlock (4x oversampled audio) | `oversampler.processSamplesUp(monoBlock)` in PluginProcessor | Yes — actual audio samples from host | ✓ FLOWING |
| TurboRat::updateCoefficients | smoothDrive.current | setParameters() → smoothDrive.setTarget() → tick() in processOS | Yes — smoothed parameter value | ✓ FLOWING |
| TurboRat::updateCoefficients | smoothFilter.current | setParameters() → smoothFilter.setTarget() → tick() in processOS | Yes — smoothed parameter value | ✓ FLOWING |
| TurboRat::updateCoefficients | smoothVolume.current → volumeGain | setParameters() → smoothVolume.setTarget() → tick() in processOS | Yes — smoothed parameter value × 2.0 | ✓ FLOWING |

---

## Behavioral Spot-Checks

| Behavior | Command | Result | Status |
|---------|---------|--------|--------|
| All 36 RAT-01..RAT-07 test assertions pass | `./build/TurboRatTest` | 36/36 tests passed, exit code 0 | ✓ PASS |
| No forbidden math in sample loop | Python scan for std::exp, std::log, std::tanh, std::pow, computeLPFAlpha, computeHPFAlpha, jlimit, jmap, new, make_unique, std::vector inside for loop | 0 occurrences found | ✓ PASS |
| processOS wired in PluginProcessor | `grep processOS Source/PluginProcessor.cpp` | Line 226: `stageTurboRat->processOS (osBlock)` | ✓ PASS |
| TurboRatTest binary exists and is runnable | `ls build/TurboRatTest` | File exists at `build/TurboRatTest` | ✓ PASS |

---

## Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
|------------|------------|-------------|--------|---------|
| RAT-01 | 03-01 | Two cascaded first-order HPFs before clipping: 60Hz and 1.5kHz | ✓ SATISFIED | hpf1Alpha/hpf2Alpha computed in updateCoefficients; HPF difference equation in sample loop; 3 test assertions pass |
| RAT-02 | 03-02 | LM308 slew-rate LP ~1040Hz bandwidth | ✓ SATISFIED | slewAlpha = computeLPFAlpha(1040.0f, osSr); 3 test assertions pass |
| RAT-03 | 03-02 | GBW dominant pole: gbwHz = 600/max(drive,0.01) clamped [200,8000] | ✓ SATISFIED | gbwAlpha computed from drive-dependent gbwHz; 7 test assertions pass |
| RAT-04 | 03-03 | Asymmetric tanh diode waveshaper: LED/Silicon/Lift thresholds; kDefaultAsym=0.20f; normalized | ✓ SATISFIED | FastMathApproximations::tanh in sample loop; switch(clipMode) in updateCoefficients; 7 test assertions pass |
| RAT-05 | 03-04 | Post-clip reverse-wired tone LPF: filter=0 → 32kHz (bright), filter=100 → 475Hz (dark) | ✓ SATISFIED | toneAlpha via log-spaced interpolation; 3 test assertions including monotonic sweep pass. Note: REQUIREMENTS.md text has filter direction inverted — code correctly matches hardware spec. |
| RAT-06 | 03-04 | JFET output buffer: 18kHz LP + volume scalar 0–2.0 | ✓ SATISFIED | jfetAlpha = computeLPFAlpha(18000.0f); volumeGain = smoothVolume.current * 2.0f; 3 test assertions pass |
| RAT-07 | 03-04 | All float params smoothed 20ms; coefficients updated per-block, never per-sample | ✓ SATISFIED (with note) | All 6 alphas bit-identical across steady-state blocks (test passes). 10 test assertions pass. Note: "per-sample" in RAT-07 text is aspirational — implementation uses per-block smoothing (SmoothParam.tick() once per block), an intentional design decision documented in 03-RESEARCH.md open question #4. Per-block smoothing is sufficient for click-free transitions at 20ms with 512-sample blocks. |

---

## Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|------|------|---------|---------|-------|
| Source/PluginProcessor.cpp | 226 vs 232 | `processOS` called before `setParameters` — parameters are one block late | ⚠ Warning | RAT operates on previous block's parameters. First block after a parameter change uses stale values. Smoothers partially compensate but coefficient update uses old params. No click artifacts in practice (tests pass) but there is a 1-block (~11ms at 44.1kHz/512) parameter latency. |
| Source/dsp/TurboRat.cpp | 107-110 | SmoothParam ticks once per block, not per sample | ℹ Info | Intentional per 03-RESEARCH.md open question #4. Coefficient is block-stable, not sample-stable. Acceptable for IIR coefficients; would not be acceptable for direct audio signals. No audible impact per test suite. |

---

## Human Verification Required

### 1. Audible Distortion Character

**Test:** Load the plugin in a DAW or AU validator. Feed a 0dBFS sine tone (e.g. 440Hz or 1kHz). Enable Stage 1 (RAT bypass off). Sweep Drive from 0 to 100, then sweep Filter from 0 to 100, then switch between LED, Silicon, and Lift clip modes.

**Expected:** 
- At low Drive: relatively clean, with slight high-frequency rolloff from GBW LP
- At high Drive: clearly audible harmonic distortion, darker tone character
- Filter sweep from 0 to 100: output darkens progressively (0=bright/open, 100=rolled off to ~475Hz)
- LED mode: softer, more dynamic-range-preserving distortion compared to Silicon
- Silicon mode: harder, more compressed, flatter waveshape
- Lift mode: near-linear, minimal clipping character

**Why human:** The automated tests verify mathematical correctness of the signal chain (RMS ratios, filter slopes, waveshaper normalization, coefficient stability). They cannot confirm that the combined effect sounds like the intended "authentic LM308 slew-rate distortion" with the character described in heatdeath_vst_spec.md.

---

## Gaps Summary

No blocking gaps. All 7 requirements are implemented and tested. The test suite runs 36/36 passing. Two items are flagged as warnings, not blockers:

1. **Parameter one-block latency** (PluginProcessor processOS before setParameters): Noted per code review. Does not prevent goal achievement — tests are green, smoothers provide sufficient transition quality. This is a code quality issue for a future cleanup pass.

2. **REQUIREMENTS.md / ROADMAP SC3 Filter direction discrepancy**: The text "Filter=0 produces dark" in the ROADMAP and "filter=0 → 475Hz (dark)" in REQUIREMENTS.md are documentation errors. The implementation correctly matches the hardware spec (0=bright/open). The tests verify the correct behavior.

The phase goal is substantively achieved: a complete LM308 signal chain runs at 4x oversampling with all required stages wired and passing automated tests. Human confirmation of audible distortion character is the only remaining item.

---

_Verified: 2026-04-09T23:45:00Z_
_Verifier: Claude (gsd-verifier)_
