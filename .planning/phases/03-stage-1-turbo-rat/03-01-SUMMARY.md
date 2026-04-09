---
phase: 03-stage-1-turbo-rat
plan: 01
subsystem: dsp/TurboRat
tags: [dsp, hpf, oversampling, test-harness, tdd]
dependency_graph:
  requires: []
  provides: [TurboRatTest-executable, processOS-API, HPF-chain-RAT-01]
  affects: [Source/PluginProcessor.cpp, Source/dsp/TurboRat.h, Source/dsp/TurboRat.cpp]
tech_stack:
  added: [juce::dsp::AudioBlock<float>, SmoothParam (from Phase 02-02)]
  patterns: [one-pole-HPF, per-block-coefficient-update, host-rate-smoothing]
key_files:
  created: [Tests/TurboRatTest.cpp]
  modified: [CMakeLists.txt, Source/dsp/TurboRat.h, Source/dsp/TurboRat.cpp, Source/PluginProcessor.cpp]
decisions:
  - TurboRat.h/cpp extended in same commit as Wave 0 harness since processOS API required for test compilation
  - SmoothParam configured at HOST rate (44100 Hz) not OS rate (176400 Hz) per Research open question #4
  - Parameters defaults corrected: filter=50, volume=65, sag=25 (prior stub had filter=35, volume=60, sag=30)
metrics:
  duration_minutes: 10
  completed: "2026-04-09T22:01:22Z"
  tasks_completed: 2
  files_modified: 5
---

# Phase 03 Plan 01: TurboRatTest Harness + RAT-01 HPF Chain Summary

**One-liner:** TurboRatTest executable + cascaded one-pole HPF chain (60Hz + 1.5kHz) using rc/(rc+dt) alpha formula at 4x oversampled rate, with PluginProcessor wired to call processOS on the upsampled block.

## What Was Built

### Task 1: Wave 0 — TurboRatTest harness and CMake target

- `Tests/TurboRatTest.cpp` created following `SmoothParamTest.cpp` pattern exactly: same `g_tests`/`g_failures` counters, same `check()` helper, `int main`, same `printf` format.
- Stubs for all seven requirements (RAT-01..RAT-07), with RAT-02..RAT-07 marked `check(true, "pending — see 03-0X")` so the suite stays green until those plans implement them.
- Real RAT-01 tests: 30Hz attenuation >20dB vs 10kHz, 10kHz near unity (>0.3 rms), reset+silence→silence.
- `CMakeLists.txt` extended with `add_executable(TurboRatTest ...)` block linking `juce_core`, `juce_audio_basics`, `juce_dsp`, `juce_audio_formats`, `juce_audio_processors`.

### Task 2: RAT-01 HPF chain + processOS API + PluginProcessor wiring

#### TurboRat.h additions (member layout)

```cpp
// Public methods added:
void processOS(juce::dsp::AudioBlock<float>& osBlock);
void updateCoefficients(double osSampleRate);

// Private members added:
double osSr = 176400.0;
int    osSamplesPerBlock = 2048;
SmoothParam smoothDrive, smoothFilter, smoothVolume, smoothAsym;
float hpf1PrevIn  = 0.0f, hpf1PrevOut = 0.0f;  // 60Hz HPF state
float hpf2PrevIn  = 0.0f, hpf2PrevOut = 0.0f;  // 1.5kHz HPF state
float hpf1Alpha   = 0.0f;  // per-block coefficient
float hpf2Alpha   = 0.0f;  // per-block coefficient
static float computeHPFAlpha(float cutoffHz, double sampleRate) noexcept;
```

#### PluginProcessor.cpp diff (lines replaced)

Before (Phase 2 stub, lines 225-227):
```cpp
auto osBlock = oversampler.processSamplesUp (monoBlock);
juce::ignoreUnused (osBlock);          // Phase 3 will process osBlock at 4x
oversampler.processSamplesDown (monoBlock);
```

After (03-01):
```cpp
auto osBlock = oversampler.processSamplesUp (monoBlock);
stageTurboRat->processOS (osBlock);    // Stage 1 processed on oversampled block (03-01)
oversampler.processSamplesDown (monoBlock);
```

Also removed (line 242): `stageTurboRat->process (buffer, numSamples);`
Replaced with comment: `// Stage 1 processed on oversampled block above (03-01).`

#### How computeHPFAlpha maps to the one-pole HPF difference equation

The one-pole HPF difference equation is:
```
y[n] = alpha * (y[n-1] + x[n] - x[n-1])
```

The alpha coefficient comes from the RC high-pass filter continuous-time model discretized via the backward Euler (impulse invariant) approximation:

```
alpha = RC / (RC + T)
     = rc / (rc + dt)
```

where `rc = 1 / (2π × fc)` and `dt = 1 / sampleRate`. As `fc → 0`, `alpha → 1` (all-pass). As `fc → sampleRate/2`, `alpha → 0` (pure differentiator). At 60Hz and osSr=176400Hz, `alpha ≈ 0.99786`. At 1500Hz and osSr=176400Hz, `alpha ≈ 0.94680`. These values are in `(0, 1)` as required.

#### SmoothParam at HOST rate (not OS rate)

`smoothDrive`, `smoothFilter`, `smoothVolume`, `smoothAsym` are all configured with:
```cpp
smoothDrive.setTimeMs(20.0f, sampleRate);  // sampleRate = HOST rate (e.g. 44100)
```

This is correct per Research open question #4: smoothers track parameter changes at the host buffer rate. The smoothed values are read once per block (before the OS sample loop) to derive per-block IIR coefficients. Running smoothers at OS rate (176400 Hz) would make the 20ms time constant 4x too short.

## Verification Results

```
=== TurboRat Tests ===

[RAT-01] Pre-clip HPF chain
  pass: 10kHz passes through HPF chain near unity (rms > 0.3)
  pass: 30Hz attenuated by >20dB relative to 10kHz (rms30 < 0.1 * rms10k)
  pass: reset() + silence in -> silence out (no DC, no NaN)

[RAT-02..RAT-07] — all pending stubs pass

=== Results: 9/9 tests passed ===
```

Both `TurboRatTest` and `HEATDEATH_VST3` build cleanly with no errors.

## Commits

| Task | Commit | Message |
|------|--------|---------|
| Task 1 | 51d70c3 | feat(03-01): add TurboRatTest harness and CMake target (Wave 0) |
| Task 2 | f63b5c0 | feat(03-01): implement RAT-01 HPF chain + processOS API + PluginProcessor wiring |

## Deviations from Plan

### Auto-fixed Issues

None — plan executed exactly as written.

### Notes

- Task 1 and Task 2 TurboRat.h/cpp changes were technically interleaved (the header needed processOS to compile TurboRatTest.cpp), but committed in proper task order.
- The plan referenced `./build/Tests/TurboRatTest` as the binary path. The actual build output location is `./build/TurboRatTest` (CMake places executables in the build root, not a Tests/ subdirectory). This matches the SmoothParamTest pattern (`./build/SmoothParamTest`). No functional impact — binary builds and runs correctly.

## Discrepancies from 03-RESEARCH.md Assumptions

None significant. All Architecture Patterns and Standard Stack recommendations were followed exactly. The `computeHPFAlpha` formula matches the Research doc verbatim. The SmoothParam HOST-rate configuration matches Research open question #4 recommendation.

## Known Stubs

- RAT-02..RAT-07 test stubs: `check(true, "pending — see 03-0X")` — intentional; each will be replaced by real tests in plans 03-02, 03-03, 03-04.
- `TurboRat::process(buffer, numSamples)` kept as empty no-op for build compatibility; PluginProcessor no longer calls it (03-01 removed the call).

## Self-Check: PASSED

- `Tests/TurboRatTest.cpp` — FOUND
- `Source/dsp/TurboRat.h` — FOUND (processOS + updateCoefficients declared)
- `Source/dsp/TurboRat.cpp` — FOUND (updateCoefficients + processOS implemented)
- `Source/PluginProcessor.cpp` — FOUND (stageTurboRat->processOS wired)
- Commit 51d70c3 — FOUND
- Commit f63b5c0 — FOUND
- TurboRatTest binary at `./build/TurboRatTest` — FOUND, exits 0
- HEATDEATH_VST3 builds cleanly — CONFIRMED
