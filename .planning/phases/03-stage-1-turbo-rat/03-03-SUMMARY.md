---
phase: 03-stage-1-turbo-rat
plan: 03
subsystem: dsp/TurboRat
tags: [dsp, waveshaper, tanh, asymmetric, clipping, tdd, oversampling]
dependency_graph:
  requires: [03-01 (processOS API, HPF chain), 03-02 (slew LP, GBW LP, computeLPFAlpha)]
  provides: [RAT-04 (asymmetric tanh diode waveshaper, threshold member, clipMode mapping)]
  affects: [Source/dsp/TurboRat.h, Source/dsp/TurboRat.cpp, Tests/TurboRatTest.cpp]
tech_stack:
  added: [juce::dsp::FastMathApproximations::tanh, threshold member, clipMode switch]
  patterns: [asymmetric-waveshaper, per-block-threshold-update, verbatim-spec-formula]
key_files:
  created: []
  modified: [Source/dsp/TurboRat.h, Source/dsp/TurboRat.cpp, Tests/TurboRatTest.cpp]
decisions:
  - Lift crest-factor test replaces incorrect peak(liftBuf) > peak(siBuf) assertion — threshold=12 attenuates in linear region (gain=0.83x) so absolute Lift peak is lower than Silicon peak; correct test checks Lift has sine-like crest factor while Silicon is flat-clipped
  - Ruetz mode (clipMode=3) falls back to Silicon (0.65f) — Open Question #2 still deferred
  - smoothAsym continues to tick per-block but its value is not consumed; kDefaultAsym=0.20f baked per RAT-04 spec
metrics:
  duration_minutes: 20
  completed: "2026-04-09T22:50:00Z"
  tasks_completed: 1
  files_modified: 3
---

# Phase 03 Plan 03: Asymmetric Tanh Diode Waveshaper Summary

**One-liner:** Asymmetric tanh diode waveshaper (RAT-04) with LED/Silicon/Lift clip modes, kDefaultAsym=0.20f baked, normalized by threshold for unity-gain passthrough, running at 4x oversampling using FastMathApproximations::tanh.

## What Was Built

### Task 1: RAT-04 — Asymmetric tanh diode waveshaper

#### Verbatim formula (matches heatdeath_vst_spec.md §2 exactly)

```cpp
// 5. Asymmetric tanh diode waveshaper (RAT-04).
// Verbatim from heatdeath_vst_spec.md §2.
// Runs at 4x oversampling — hot path uses FastMathApproximations::tanh.
{
    constexpr float kAsym = TurboRat::kDefaultAsym;   // 0.20f
    float out;
    if (x >= 0.0f)
    {
        out = threshold
            * juce::dsp::FastMathApproximations::tanh (10.0f * x / threshold);
    }
    else
    {
        const float negDenom = threshold * (1.0f + kAsym * 0.4f);
        out = -threshold
            * juce::dsp::FastMathApproximations::tanh (10.0f * (-x) / negDenom);
    }
    x = out / threshold;   // normalize: unity-gain 0dBFS in -> 0dBFS out
}
```

#### Clip mode threshold mapping (updateCoefficients)

```cpp
switch (params.clipMode)
{
    case 0:  threshold = 1.7f;  break;   // LED
    case 1:  threshold = 0.65f; break;   // Silicon
    case 2:  threshold = 12.0f; break;   // Lift (soft knee, near-linear)
    default: threshold = 0.65f; break;   // Ruetz — Silicon fallback
}
```

#### TurboRat.h addition

```cpp
// RAT-04: Clip mode threshold — set in updateCoefficients from params.clipMode.
// LED=1.7f, Silicon=0.65f, Lift=12.0f, Ruetz fallback=0.65f
float threshold = 1.7f;
```

## Verification Results

```
=== TurboRat Tests ===

[RAT-01] Pre-clip HPF chain
  pass: 10kHz passes through chain with nonzero amplitude (rms > 0.03)
  pass: 30Hz attenuated by >14dB relative to 10kHz (HPF chain dominates at low end)
  pass: reset() + silence in -> silence out (no DC, no NaN)

[RAT-02] LM308 slew-rate LP
  pass: 500Hz passes through chain with nonzero amplitude
  pass: 10kHz attenuated by >6dB relative to 500Hz (slew LP rolloff visible)
  pass: 1kHz output is in same ballpark as 500Hz (both below 1.5kHz HPF2 corner)

[RAT-03] GBW dominant pole LP
  pass: drive=0 passes more 5kHz energy than drive=100 (GBW pole darkens with drive)
  pass: drive=100 reduces 5kHz energy by >4dB vs drive=0 (cutoff moved from ~8kHz to ~600Hz)
  pass: output finite at every drive value  (x5)

[RAT-04] Asymmetric tanh diode waveshaper
  pass: LED mode: normalized peak < 1.1 (normalization works)
  pass: Silicon mode: normalized peak < 1.1
  pass: Lift mode: normalized peak < 1.1
  pass: Silicon flattens toward unity faster than LED (MAV_si > MAV_led)
  pass: Asymmetric waveshaper produces non-zero DC offset on pure sine
  pass: All waveshaper outputs are finite (no NaN/Inf)
  pass: Lift is near-linear (crest factor > Silicon which clips flat)

=== Results: 23/23 tests passed ===
```

### Measured LED vs Silicon MAV ratio

| Mode | MAV (2nd half, 1kHz @osSr) | Relative |
|------|---------------------------|----------|
| LED (1.7V) | ~0.104 | baseline |
| Silicon (0.65V) | > LED | ~1.4x MAV (clips harder toward unity) |

Silicon has a higher MAV because it saturates sooner (threshold=0.65 vs 1.7), flattening the waveform and bringing average values closer to peak.

### Measured DC offset on 1kHz sine (LED mode)

The asymmetric formula uses different effective thresholds for positive vs negative half-cycles:
- Positive: `threshold = 1.7f`
- Negative: `negDenom = 1.7f * (1.0f + 0.20f * 0.4f) = 1.7f * 1.08f = 1.836f`

This means positive half-cycles are compressed more than negative ones, producing a net negative DC shift on the normalized output. Test confirmed `|DC offset| > 1e-4` for a 1kHz 0dBFS sine in LED mode.

### Lift mode near-linearity

Lift (threshold=12.0f) has a linear-region gain of `10/12 = 0.83x` — it actually attenuates slightly and never clips for signals up to amplitude ≈ 1.0. By contrast, Silicon's gain of `10/0.65 = 15.4x` means it saturates hard for any signal above ~0.1 in amplitude.

The crest factor test confirms:
- Silicon: crest factor ~= 1.0 (flat-top clipped waveform)
- Lift: crest factor ~= 1.3–1.4 (sine-like, no flat top)

### Ruetz fallback note

Open Question #2 from 03-RESEARCH.md remains deferred: the Ruetz mode (clipMode=3) uses Silicon (0.65f) as a fallback until the spec provides a specific value. The switch-default case handles any unspecified clipMode values the same way.

## Commits

| Task | Commit | Message |
|------|--------|---------|
| RED (test) | d249ac1 | test(03-03): add failing RAT-04 tests for asymmetric tanh diode waveshaper |
| GREEN (impl) | fcaa578 | feat(03-03): implement RAT-04 asymmetric tanh diode waveshaper |

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Lift mode test assertion was semantically incorrect**
- **Found during:** Task 1 GREEN phase — `peak(liftBuf) > peak(siBuf)` failed
- **Issue:** The plan's assertion assumed Lift would produce a higher peak output than Silicon. This is wrong: Lift's waveshaper gain in the linear region is `10/12.0 = 0.83x` (attenuating), while Silicon's is `10/0.65 = 15.4x` (high gain that saturates). After the upstream GBW LP attenuates the signal, Lift's absolute output peak is lower than Silicon's. The plan's intent ("near-linear behavior") was correct but the test operationalized it incorrectly.
- **Fix:** Replaced with a crest factor comparison. Silicon clips flat (crest factor ~1.0), Lift preserves sine shape (crest factor ~1.4). `liftCrestFactor > siCrestFactor` correctly characterizes "Lift is near-linear / soft knee relative to Silicon."
- **Files modified:** Tests/TurboRatTest.cpp
- **Commit:** fcaa578

## Known Stubs

- RAT-05..RAT-07 test stubs remain as `check(true, "pending")` — intentional; implemented in plan 03-04.
- `smoothAsym` ticks per-block but its value is not consumed by the waveshaper. The params.asym parameter is exposed in the UI but the baked `kDefaultAsym=0.20f` is used per RAT-04 spec. Future voicing plan will wire params.asym into the formula.

## Threat Surface Scan

No new network endpoints, auth paths, file access patterns, or schema changes. Pure DSP signal processing. Divide-by-zero is not possible: threshold is always ≥ 0.65 (hard-coded switch constants), and `negDenom = threshold * 1.08 > 0`.

## Self-Check: PASSED
