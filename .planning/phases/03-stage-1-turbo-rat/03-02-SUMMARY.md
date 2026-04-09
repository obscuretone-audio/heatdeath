---
phase: 03-stage-1-turbo-rat
plan: 02
subsystem: dsp/TurboRat
tags: [dsp, lpf, slew, gbw, lm308, tdd, oversampling]
dependency_graph:
  requires: [03-01 (processOS API, HPF chain, TurboRatTest harness)]
  provides: [RAT-02 (slew LP 1040Hz), RAT-03 (GBW LP drive-dependent), computeLPFAlpha]
  affects: [Source/dsp/TurboRat.h, Source/dsp/TurboRat.cpp, Tests/TurboRatTest.cpp]
tech_stack:
  added: [computeLPFAlpha (exp(-2pi*fc/sr) formula)]
  patterns: [one-pole-LPF, per-block-coefficient-update, drive-dependent-cutoff]
key_files:
  created: []
  modified: [Source/dsp/TurboRat.h, Source/dsp/TurboRat.cpp, Tests/TurboRatTest.cpp]
decisions:
  - 1040Hz used directly for slew LP alpha — kDefaultSlew=0.68f is a spec annotation not a literal alpha
  - smoothDrive.current read (not .target) inside updateCoefficients for block-stable GBW coefficient
  - RAT-01 test updated to use drive=0 to isolate HPF chain from drive-dependent GBW LP
metrics:
  duration_minutes: 25
  completed: "2026-04-09T22:30:00Z"
  tasks_completed: 2
  files_modified: 3
---

# Phase 03 Plan 02: LM308 Slew LP + GBW Dominant Pole Summary

**One-liner:** Fixed 1040Hz slew-rate LP (RAT-02) and drive-dependent GBW dominant pole LP (RAT-03) inserted after HPF chain using exp(-2pi*fc/sr) alpha formula, both updated once per block in updateCoefficients.

## What Was Built

### Task 1: RAT-02 — LM308 slew-rate LP (fixed 1040Hz)

#### Why 1040Hz instead of kDefaultSlew=0.68f

The spec annotation `kDefaultSlew=0.68f` represents an alpha value, not a cutoff frequency. Per 03-RESEARCH.md open question #1: using `alpha=0.68f` literally at `osSr=176400Hz` would produce a cutoff of `fc = -ln(0.68) * 176400 / (2*pi) ≈ 10,800Hz` — almost 10x too high. The 1040Hz cutoff is used directly via `computeLPFAlpha(1040.0f, osSr)`, which produces `alpha ≈ 0.9636` at 176400Hz. This is the correct LM308 emulation per `heatdeath_vst_spec.md §2`.

#### TurboRat.h additions

```cpp
// Public static helper:
static float computeLPFAlpha (float cutoffHz, double sampleRate) noexcept;

// Private members:
float slewState = 0.0f;
float slewAlpha = 0.0f;
```

#### TurboRat.cpp additions

`computeLPFAlpha`:
```cpp
return static_cast<float>(
    std::exp(-juce::MathConstants<double>::twoPi * cutoffHz / sampleRate));
```

`updateCoefficients` extension:
```cpp
slewAlpha = computeLPFAlpha(1040.0f, osSampleRate);
```

`processOS` sample loop (after HPF2):
```cpp
// 3. Slew-rate LP (~1040Hz, fixed)
slewState = slewAlpha * slewState + (1.0f - slewAlpha) * x;
x = slewState;
```

`reset()` addition: `slewState = 0.0f;`

### Task 2: RAT-03 — GBW dominant pole LP (drive-dependent)

#### TurboRat.h additions

```cpp
float gbwState = 0.0f;
float gbwAlpha = 0.0f;
```

#### TurboRat.cpp additions

`updateCoefficients` extension (reads smoothDrive.current, not .target):
```cpp
const float driveNorm = smoothDrive.current;
const float gbwHz = juce::jlimit(200.0f, 8000.0f,
                                 600.0f / std::max(driveNorm, 0.01f));
gbwAlpha = computeLPFAlpha(gbwHz, osSampleRate);
```

`processOS` sample loop (after slew LP):
```cpp
// 4. GBW dominant pole LP (drive-dependent, block-stable)
gbwState = gbwAlpha * gbwState + (1.0f - gbwAlpha) * x;
x = gbwState;
```

`reset()` addition: `gbwState = 0.0f;`

#### smoothDrive.current vs .target

`smoothDrive.current` is the correct read point. In `processOS`, `smoothDrive.tick()` is called once at block start to advance the smoother one step, then `updateCoefficients(osSr)` is called. At that point, `.current` holds the block-start smoothed drive value (post-tick), which is what should drive the GBW coefficient for this block. Reading `.target` would use the unsmoothed destination value, bypassing the 20ms smoothing and potentially causing zipper noise on drive changes.

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
  pass: output finite at every drive value  (x5 — drive=0,1,50,99,100)

=== Results: 17/17 tests passed ===
```

### Measured RMS ratios

| Frequency | drive=0 | drive=100 | Ratio |
|-----------|---------|-----------|-------|
| 5kHz      | 0.114   | 0.016     | 7.1x (>17dB) |
| 500Hz     | ~0.199  | —         | baseline for RAT-02 |
| 10kHz     | ~0.071  | —         | 0.35x vs 500Hz |

The GBW pole produces strong drive-dependent darkening at high frequencies, as intended.

## Commits

| Task | Commit | Message |
|------|--------|---------|
| Task 1 | 0e8e85d | feat(03-02): implement RAT-02 slew-rate LP (1040Hz one-pole, fixed cutoff) |
| Task 2 | c0c405c | feat(03-02): implement RAT-03 GBW dominant pole LP (drive-dependent cutoff) |

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] RAT-02 test threshold assumed 500Hz passes near unity (>0.3 RMS)**
- **Found during:** Task 1 GREEN phase
- **Issue:** The upstream HPF2 at 1.5kHz attenuates 500Hz to ~0.20 RMS. The plan spec assumed 500Hz would be near unity, but the HPF chain (added in 03-01) sits upstream and rolls off signals below 1.5kHz. The `rms500 > 0.3f` threshold cannot be met.
- **Fix:** Updated RAT-02 thresholds to reflect the full chain: `rms500 > 0.1f`, `rms10k < rms500 * 0.5f` (still meaningfully verifies slew LP rolloff via relative ratio).
- **Files modified:** Tests/TurboRatTest.cpp
- **Commit:** 0e8e85d

**2. [Rule 1 - Bug] RAT-01 test threshold broke when slew LP was added (10kHz > 0.3 → 0.071)**
- **Found during:** Task 1 GREEN phase (post-implementation)
- **Issue:** Adding the 1040Hz slew LP attenuates 10kHz from ~0.47 (HPF-only) to ~0.071. The RAT-01 check `rms10k > 0.3f` was set for the HPF-only chain in 03-01.
- **Fix:** Updated to `rms10k > 0.05f` and `rms30 < rms10k * 0.15f`.
- **Files modified:** Tests/TurboRatTest.cpp
- **Commit:** 0e8e85d

**3. [Rule 1 - Bug] RAT-01 test broke again when GBW LP added at default drive=72 (10kHz → 0.006)**
- **Found during:** Task 2 GREEN phase
- **Issue:** At default drive=72, gbwHz = 600/0.72 ≈ 833Hz, which attenuates 10kHz to ~0.006 RMS — nearly the same as 30Hz. The HPF relative test (`rms30 < rms10k * 0.15`) failed since both frequencies were comparably attenuated by the GBW LP.
- **Fix:** RAT-01 test now explicitly sets `p.drive = 0.0f` to keep the GBW LP at its widest (8000Hz), isolating HPF chain characterization. Updated thresholds to `rms10k > 0.03f` and `rms30 < rms10k * 0.20f` (actual values: 0.044 and 0.007 respectively).
- **Files modified:** Tests/TurboRatTest.cpp
- **Commit:** c0c405c

## Known Stubs

- RAT-04..RAT-07 test stubs remain as `check(true, "pending — see 03-0X")` — intentional; implemented in plans 03-03 and 03-04.

## Threat Surface Scan

No new network endpoints, auth paths, file access patterns, or schema changes. Pure DSP signal processing. The only runtime risk (driveNorm NaN/negative from broken smoothing) is mitigated by `max(driveNorm, 0.01f)` + `jlimit(200, 8000)` as documented in the plan's threat register.

## Self-Check: PASSED
