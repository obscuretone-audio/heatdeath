---
phase: 03-stage-1-turbo-rat
plan: 04
subsystem: dsp/TurboRat
tags: [dsp, tone-lpf, jfet, volume, coefficient-invariant, tdd, oversampling, phase3-complete]
dependency_graph:
  requires: [03-01 (processOS API, HPF chain), 03-02 (slew LP, GBW LP), 03-03 (waveshaper, threshold)]
  provides: [RAT-05 (reverse-wired tone LPF), RAT-06 (JFET LP + volume scalar), RAT-07 (block-stable coefficients)]
  affects: [Source/dsp/TurboRat.h, Source/dsp/TurboRat.cpp, Tests/TurboRatTest.cpp]
tech_stack:
  added: [toneState/toneAlpha, jfetState/jfetAlpha, volumeGain, TURBORAT_TEST_ACCESS guard, getCoefficientsForTest()]
  patterns: [log-spaced-frequency-interpolation, per-block-coefficient-update, test-accessor-guard, reverse-wired-lpf]
key_files:
  created: []
  modified: [Source/dsp/TurboRat.h, Source/dsp/TurboRat.cpp, Tests/TurboRatTest.cpp]
decisions:
  - Log-spaced (geometric) interpolation between 32kHz and 475Hz for tone LPF cutoff — perceptually correct spacing vs linear
  - TURBORAT_TEST_ACCESS compile-time guard keeps getCoefficientsForTest() out of shipped binary while enabling RAT-07 assertion
  - RAT-01 and RAT-04 existing tests updated: filter=0/volume=50 to isolate each test's target stage from the now-complete chain
metrics:
  duration_minutes: 25
  completed: "2026-04-09T23:15:00Z"
  tasks_completed: 2
  files_modified: 3
---

# Phase 03 Plan 04: Tone LPF + JFET Buffer + Coefficient Invariant Summary

**One-liner:** Post-clip reverse-wired tone LPF (log-spaced 32kHz→475Hz), fixed 18kHz JFET output buffer with volume scalar [0..2.0], and block-stable coefficient invariant completing the full LM308 TurboRat chain at 4x oversampling.

## What Was Built

### Final processOS sample loop (verbatim)

```cpp
for (int i = 0; i < numOsSamples; ++i)
{
    float x = data[i];

    // HPF 60Hz: y[n] = alpha * (y[n-1] + x[n] - x[n-1])
    const float y1 = hpf1Alpha * (hpf1PrevOut + x - hpf1PrevIn);
    hpf1PrevIn  = x;
    hpf1PrevOut = y1;
    x = y1;

    // HPF 1.5kHz
    const float y2 = hpf2Alpha * (hpf2PrevOut + x - hpf2PrevIn);
    hpf2PrevIn  = x;
    hpf2PrevOut = y2;
    x = y2;

    // 3. Slew-rate LP (~1040Hz, fixed)
    slewState = slewAlpha * slewState + (1.0f - slewAlpha) * x;
    x = slewState;

    // 4. GBW dominant pole LP (drive-dependent, block-stable)
    gbwState = gbwAlpha * gbwState + (1.0f - gbwAlpha) * x;
    x = gbwState;

    // 5. Asymmetric tanh diode waveshaper (RAT-04).
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

    // 6. Post-clip tone LPF (reverse-wired, RAT-05)
    toneState = toneAlpha * toneState + (1.0f - toneAlpha) * x;
    x = toneState;

    // 7. JFET output buffer: 18kHz LP (RAT-06)
    jfetState = jfetAlpha * jfetState + (1.0f - jfetAlpha) * x;
    x = jfetState * volumeGain;

    data[i] = x;
}
```

### Sample-loop audit result

Grepped TurboRat.cpp processOS for forbidden patterns inside the `for (int i = 0; i < numOsSamples; ++i)` loop:

| Pattern | Found inside loop? | Result |
|---------|-------------------|--------|
| `std::exp` | NO — only in `computeLPFAlpha` and `updateCoefficients` | PASS |
| `std::log` | NO — only in `updateCoefficients` | PASS |
| `std::tanh` (bare) | NO — only `FastMathApproximations::tanh` | PASS |
| `std::pow` | NO | PASS |
| `computeLPFAlpha` / `computeHPFAlpha` | NO — only in `updateCoefficients` | PASS |
| `jlimit` / `jmap` | NO — only in `updateCoefficients` | PASS |
| allocation (`new`, `make_unique`, `std::vector`) | NO — none anywhere in processOS | PASS |

**Audit conclusion:** The hot path is branchless multiply-add with a single conditional for the asymmetric waveshaper. All expensive math lives in `updateCoefficients`, called once per block.

### updateCoefficients additions (RAT-05 + RAT-06)

```cpp
// RAT-05: Reverse-wired tone LPF
const float filterNorm = smoothFilter.current;
const float logHigh = std::log (32000.0f);
const float logLow  = std::log (475.0f);
const float toneCutoff = std::exp (logHigh + filterNorm * (logLow - logHigh));
toneAlpha = computeLPFAlpha (toneCutoff, osSampleRate);

// RAT-06: JFET output buffer
jfetAlpha  = computeLPFAlpha (18000.0f, osSampleRate);
volumeGain = smoothVolume.current * 2.0f;
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

[RAT-05] Reverse-wired tone LPF
  pass: filter=0 passes more 5kHz than filter=100 (reverse-wired: 0=bright)
  pass: filter=100 attenuates 5kHz by >12dB vs filter=0
  pass: Filter sweep 0->100 monotonically darkens 5kHz

[RAT-06] JFET output buffer
  pass: volume=0 -> silence
  pass: volume=100 (2.0x) produces ~2x more RMS than volume=50 (1.0x)
  pass: 20kHz attenuated by JFET LP (18kHz) vs 1kHz

[RAT-07] Coefficient stability + smoothing
  pass: hpf1Alpha stable across blocks at steady-state
  pass: hpf2Alpha stable across blocks at steady-state
  pass: slewAlpha stable across blocks at steady-state
  pass: gbwAlpha  stable across blocks at steady-state
  pass: toneAlpha stable across blocks at steady-state
  pass: jfetAlpha stable across blocks at steady-state
  pass: Changing drive target moves gbwAlpha after one block
  pass: gbwAlpha still moving after block 2 (smoothing active, not snapping)
  pass: No click on drive flip: adjacent-sample delta < 0.5
  pass: Full chain: silence in -> silence out after reset()

=== Results: 36/36 tests passed ===
```

SmoothParamTest: 13/13 passed. Full plugin (HEATDEATH_VST3) builds clean and installs to system VST3 path.

### Measured filter sweep RMS values (5kHz sine, drive=20, Lift mode, volume=50)

| filter % | filterNorm | toneCutoff (approx) | Result |
|----------|-----------|---------------------|--------|
| 0 | 0.00 | 32000 Hz | r0 (highest) |
| 25 | 0.25 | ~12300 Hz | r25 < r0 |
| 50 | 0.50 | ~3900 Hz | r50 < r25 |
| 75 | 0.75 | ~1360 Hz | r75 < r50 |
| 100 | 1.00 | 475 Hz | r100 (lowest, >12dB below r0) |

Monotonic darkening confirmed. r100 < r0 * 0.25 (>12dB attenuation at 5kHz).

### Volume scalar linearity

| volume % | volumeGain | Result |
|----------|-----------|--------|
| 0 | 0.0 | silence (< 1e-3 RMS) |
| 50 | 1.0 | baseline |
| 100 | 2.0 | > 1.7x baseline RMS (linear scalar confirmed) |

### 20ms smoothing visible in coefficient ramp test

After `drive` target changes from 50 to 100:
- `c3.gbw != c1.gbw` — gbwAlpha moved after block 1 (PASS)
- `c3.gbw != c4.gbw` — gbwAlpha still moving after block 2 (PASS)

At 44.1kHz host rate, 20ms = 882 host samples. A 2048-sample block spans ~46ms >> one time constant, confirming the smoother has not yet reached steady-state after one block — smoothing is active, not snapping.

## Phase 3 Completion Checklist

| Requirement | Status | Test |
|-------------|--------|------|
| RAT-01: HPF 60Hz + 1.5kHz pre-clip cascade | DONE | 3 passing assertions |
| RAT-02: LM308 slew-rate LP ~1040Hz | DONE | 3 passing assertions |
| RAT-03: GBW dominant pole LP (drive-dependent) | DONE | 7 passing assertions |
| RAT-04: Asymmetric tanh diode waveshaper (LED/Si/Lift) | DONE | 7 passing assertions |
| RAT-05: Post-clip reverse-wired tone LPF (32kHz→475Hz) | DONE | 3 passing assertions |
| RAT-06: JFET output buffer 18kHz LP + volume scalar | DONE | 3 passing assertions |
| RAT-07: Block-stable coefficients, 20ms smoothing, click-free | DONE | 10 passing assertions |

**Phase 3 goal achieved:** "Authentic LM308 slew-rate distortion with pre-clip HPF chain, asymmetric diode clipping, reverse-wired tone filter, and JFET output buffer running at 4x oversampling."

## Commits

| Task | Commit | Message |
|------|--------|---------|
| RED (failing tests) | 76907b4 | test(03-04): add failing tests for RAT-05, RAT-06, RAT-07 |
| GREEN (implementation) | 10acb2d | feat(03-04): implement RAT-05 tone LPF, RAT-06 JFET buffer, RAT-07 test accessor |

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] RAT-01 and RAT-04 tests broke when tone LPF + volumeGain became active**
- **Found during:** Task 1 GREEN phase — 3 of 36 tests failed
- **Issue:** RAT-01 test used default `filter=50` (tone LPF at ~3.9kHz) which attenuated the 10kHz test signal, breaking the `rms30 < rms10k * 0.20f` ratio check. RAT-04 test used `volume=65` (volumeGain=1.3) causing waveshaper peak outputs > 1.1. Both were pre-existing tests written before the tone LPF + JFET stages existed.
- **Fix:** Set `filter=0.0f` (bright, 32kHz tone LPF out of the way) and `volume=50.0f` (1.0x gain, unity) in RAT-01 and RAT-04 tests to isolate each test's target behavior.
- **Files modified:** Tests/TurboRatTest.cpp
- **Commit:** 10acb2d

## Known Stubs

- `smoothAsym` ticks per-block but its current value is not consumed; `kDefaultAsym=0.20f` is baked per RAT-04 spec. Future voicing plan will wire `params.asym` dynamically.
- Ruetz mode (clipMode=3) uses Silicon (0.65f) as fallback — Open Question #2 from 03-RESEARCH.md remains deferred until spec provides a Ruetz threshold value.

## Follow-ups for STATE.md

1. `params.asym` smoother (`smoothAsym`) ticks per block but its value is unused in the waveshaper (baked as `kDefaultAsym=0.20f`). Future plan should either wire it dynamically or remove the smoother overhead.
2. Ruetz fallback (clipMode=3 -> Silicon) is still TODO pending spec clarification.
3. The TurboRat chain is complete and tested. Phase 4 (MicroPitch) can proceed.

## Threat Surface Scan

No new network endpoints, auth paths, file access patterns, or schema changes. Pure DSP. The `TURBORAT_TEST_ACCESS` guard compiles to nothing in Release builds — verified: it is a preprocessor `#ifdef` with no runtime cost.

## Self-Check: PASSED
