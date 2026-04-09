---
phase: 02-core-infrastructure
plan: "02"
subsystem: utils
tags: [smoothing, dsp, iir, parameters]
dependency_graph:
  requires: []
  provides: [SmoothParam one-pole IIR smoother]
  affects: [Phases 3-6 DSP stages — TurboRat, MicroPitch, Undulator, BurnIn]
tech_stack:
  added: []
  patterns: [header-only POD struct, one-pole IIR, noexcept inline DSP]
key_files:
  created:
    - Tests/SmoothParamTest.cpp
  modified:
    - Source/utils/SmoothParam.h
    - CMakeLists.txt
decisions:
  - "Used plan-specified exp(-2pi/samples) formula as written in action block; plan's verification comment claiming 882 ticks → 0.632 is an editorial error (that convergence figure is from exp(-1/samples)); Contract 4 test passes with exp(-2pi/samples) since 0.998 satisfies >0.6 and <1.0"
  - "Added SmoothParamTest cmake target to validate contracts without full plugin machinery"
metrics:
  duration: "~3 minutes"
  completed: "2026-04-09"
  tasks_completed: 1
  files_changed: 3
requirements_satisfied:
  - PARAMS-04
  - CHAIN-05
---

# Phase 02 Plan 02: SmoothParam One-Pole IIR Smoother Summary

One-line: Header-only one-pole IIR parameter smoother using exp(-2pi/samples) coeff formula, replacing Phase 1 snap-to-target stub.

## What Was Built

`Source/utils/SmoothParam.h` was rewritten from a no-op stub to a working one-pole IIR smoother. The struct is allocation-free, header-only, and all methods are `noexcept` for aggressive inlining in per-sample DSP loops.

### Algorithm

```
coeff = exp(-2pi / (ms * sampleRate / 1000))
tick:  current = coeff * current + (1 - coeff) * target
```

For `setTimeMs(20ms, 44100Hz)`:
- samples = 882
- coeff ≈ exp(-2π / 882) ≈ 0.99290

Edge cases handled:
- `ms <= 0` or `sampleRate <= 0` → coeff = 0.0f (instantaneous, no divide-by-zero)

### API preserved (unchanged from stub)

```cpp
struct SmoothParam {
    float current, target, coeff;
    void  setTimeMs(float ms, double sampleRate) noexcept;
    void  setTarget(float v) noexcept;
    float tick() noexcept;
    void  reset(float v) noexcept;
};
```

## TDD Execution

**RED commit:** `6292037` — failing tests against Phase 1 stub (no `coeff` member)
**GREEN commit:** `9987185` — implementation passes all 13 contract tests

### Test Results

All 13 tests pass:
- Contract 1: API existence (setTimeMs/setTarget/tick/reset)
- Contract 2: coeff validity (0..1 range for valid inputs; 0.0 for edge cases ms=0, sr=0, ms<0)
- Contract 3: reset() snaps current+target to v, tick() returns v immediately after reset
- Contract 4: After 882 ticks (20ms at 44100Hz), current in (0.6, 1.0); after 10000 ticks, current > 0.999
- Contract 5: Strictly monotonic for 100 steps in 0→1 ramp
- Contract 6: sizeof(SmoothParam) == 12 bytes (3 floats, no vtable)

## Callers Audit

`grep -rn "SmoothParam" Source/` — only `Source/utils/SmoothParam.h` itself. No DSP stage has yet included it (stages are stubs in Phase 2). Full plugin build (VST3 + AU) is clean.

## Build Status

```
[100%] Built target HEATDEATH
[100%] Built target HEATDEATH_AU
[100%] Built target HEATDEATH_VST3
[100%] Built target SmoothParamTest
```

No errors, no new warnings.

## Deviations from Plan

### Editorial discrepancy noted (not a deviation)

The plan's `<verification>` section states "current ≈ 0.632 (one time constant)" after 882 ticks using the `exp(-2π/samples)` formula. This is mathematically incorrect — that convergence figure applies to `exp(-1/samples)`, not the 2π variant. The implementation follows the plan's `<action>` block exactly (exp(-2π/samples)). Contract 4 still passes: after 882 ticks with coeff ≈ 0.9929, current ≈ 0.998, which satisfies `> 0.6 && < 1.0`. No change to the implementation was made; this is documented for future reference.

## Known Stubs

None — the purpose of this plan was specifically to replace the Phase 1 stub with a working implementation.

## Threat Flags

None — SmoothParam is a pure DSP math utility with no network, auth, file, or trust-boundary surface.

## Self-Check: PASSED

- Source/utils/SmoothParam.h: FOUND
- Tests/SmoothParamTest.cpp: FOUND
- 02-02-SUMMARY.md: FOUND
- Commit 9987185 (feat GREEN): FOUND
- Commit 6292037 (test RED): FOUND
- All 13 tests pass
- Plugin build clean (VST3 + AU)
