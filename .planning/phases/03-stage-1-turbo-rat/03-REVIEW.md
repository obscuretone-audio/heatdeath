---
phase: 03-stage-1-turbo-rat
reviewed: 2026-04-09T00:00:00Z
depth: standard
files_reviewed: 5
files_reviewed_list:
  - CMakeLists.txt
  - Source/PluginProcessor.cpp
  - Source/dsp/TurboRat.cpp
  - Source/dsp/TurboRat.h
  - Tests/TurboRatTest.cpp
findings:
  critical: 0
  warning: 4
  info: 3
  total: 7
status: issues_found
---

# Phase 03: Code Review Report

**Reviewed:** 2026-04-09
**Depth:** standard
**Files Reviewed:** 5
**Status:** issues_found

## Summary

Five files were reviewed covering the TurboRat DSP implementation (Stage 1), its integration into the main plugin processor, the CMake build configuration, and unit tests. The DSP signal chain in `TurboRat.cpp` is structurally sound and well-commented, but there is one clear logic ordering bug in `PluginProcessor.cpp` where parameters are set *after* the stage has already processed the current block, meaning TurboRat always runs one block behind. Additionally, `params.asym` and `params.sag` fields are wired into the parameters struct and read from the APVTS but have no effect on the DSP — `asym` is silently overridden by a compile-time constant and `sag` has no implementation at all. The test binary is also missing JUCE module compile definitions that the `SmoothParamTest` target correctly includes.

---

## Warnings

### WR-01: TurboRat processes audio with stale parameters — `setParameters` called after `processOS`

**File:** `Source/PluginProcessor.cpp:226-240`
**Issue:** `stageTurboRat->processOS(osBlock)` is called at line 226, then `stageTurboRat->setParameters({...})` is called at line 232. This means every call to `processOS` uses the parameters that were set during the *previous* `processBlock` invocation. All knob changes (drive, filter, volume, clip mode, etc.) are one block late. The bypass crossfade target is also set after the audio has been processed (line 230), so bypass transitions are one block behind as well.

**Fix:** Move the `setParameters` and `bypassSmoothRat.setTargetValue` calls to *before* the oversampler/processOS block:

```cpp
// Set parameters and bypass target BEFORE processing
stageTurboRat->setParameters ({
    .drive    = pRatDrive->load(),
    .filter   = pRatFilter->load(),
    .volume   = pRatVolume->load(),
    .slew     = pRatSlew->load(),
    .asym     = pRatAsym->load(),
    .clipMode = static_cast<int> (pRatClipMode->load()),
    .sag      = pRatSag->load()
});
bypassSmoothRat.setTargetValue (pRatBypass->load() > 0.5f ? 0.0f : 1.0f);

{
    juce::dsp::AudioBlock<float> monoBlock (...);
    auto osBlock = oversampler.processSamplesUp (monoBlock);
    stageTurboRat->processOS (osBlock);
    oversampler.processSamplesDown (monoBlock);
}
```

---

### WR-02: `params.asym` is smoothed but never used — waveshaper reads a compile-time constant instead

**File:** `Source/dsp/TurboRat.cpp:107-159`
**Issue:** `smoothAsym` is ticked each block (line 110: `(void) smoothAsym.tick()`), but the waveshaper block (lines 145-159) reads `kAsym` from `TurboRat::kDefaultAsym` (a `constexpr float` equal to `0.20f`), not from `smoothAsym.current`. The `asym` parameter passed via `setParameters` and stored in `params.asym` has zero effect on the output. Any host automation or user interaction with the `asym` knob is silently ignored.

**Fix:** Replace the compile-time constant in the waveshaper with the smoothed value:

```cpp
// In processOS, after ticking smoothers:
const float asymNorm = smoothAsym.current;   // 0.0..1.0

// In the waveshaper block, replace:
//   constexpr float kAsym = TurboRat::kDefaultAsym;
// with:
const float kAsym = asymNorm;
```

If the intended range is 0–1 mapped from the 0–100 parameter, no further scaling is needed (smoothAsym already stores `params.asym / 100.0f` per lines 36 and 104).

---

### WR-03: `params.sag` is accepted and stored but has no DSP implementation

**File:** `Source/dsp/TurboRat.h:17`, `Source/dsp/TurboRat.cpp` (entire file)
**Issue:** `Parameters::sag` is declared with a default of `25.f` and is populated from the APVTS (`pRatSag->load()`) every block in `PluginProcessor.cpp:239`. However, `sag` is never read anywhere in `TurboRat.cpp` — no state variables, no coefficient computation, no sample-level effect. The parameter is fully wired up to the UI and host but silently does nothing. This is an invisible no-op that could mislead users or cause confusion during debugging.

**Fix:** Either implement the SAG behaviour as specified, or add a clear `// TODO: sag not yet implemented` comment in `processOS` near the other parameter reads, and consider whether the APVTS parameter should be hidden from the UI until it is wired.

---

### WR-04: `TurboRatTest` target is missing JUCE module compile definitions

**File:** `CMakeLists.txt:118-128`
**Issue:** The `SmoothParamTest` target (lines 103-112) correctly defines `JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1` and `JUCE_MODULE_AVAILABLE_juce_core=1` to satisfy JUCE header preconditions. The `TurboRatTest` target (lines 118-128) links against `juce::juce_dsp`, `juce::juce_audio_basics`, and `juce::juce_audio_processors` but has no `target_compile_definitions` call at all. Depending on how JUCE propagates these macros through its CMake targets, this can cause build failures or undefined behaviour when JUCE module headers assert their availability macros.

**Fix:** Add compile definitions analogous to `SmoothParamTest`:

```cmake
target_compile_definitions(TurboRatTest PRIVATE
    JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1
    JUCE_MODULE_AVAILABLE_juce_core=1
    JUCE_MODULE_AVAILABLE_juce_audio_basics=1
    JUCE_MODULE_AVAILABLE_juce_dsp=1
    JUCE_MODULE_AVAILABLE_juce_audio_formats=1
    JUCE_MODULE_AVAILABLE_juce_audio_processors=1
)
```

---

## Info

### IN-01: `runTurboRat` helper is defined but never called — dead code

**File:** `Tests/TurboRatTest.cpp:38-48`
**Issue:** The `runTurboRat` static helper function is defined at the top of the test file but is never invoked. All test cases use inline lambdas instead. The unused function will generate a compiler warning (`-Wunused-function`) with `juce_recommended_warning_flags` enabled.

**Fix:** Remove the `runTurboRat` function, or replace one of the inline measurement lambdas with a call to it for consistency.

---

### IN-02: Duplicate comment headers for RAT-02 and RAT-05 test sections

**File:** `Tests/TurboRatTest.cpp:119-121`, `Tests/TurboRatTest.cpp:291-292`
**Issue:** The section header comment for `[RAT-02]` appears twice consecutively (lines 119 and 120-121). Similarly `[RAT-05]` appears on lines 291 and 292. While harmless, it looks like a copy-paste artefact.

**Fix:** Remove the duplicate comment lines.

---

### IN-03: `params.slew` parameter field is accepted but its value is not used in the slew LP

**File:** `Source/dsp/TurboRat.h:14`, `Source/dsp/TurboRat.cpp:65`
**Issue:** `Parameters::slew` (default `68.f`) is populated from the APVTS, but `slewAlpha` is always derived from a fixed 1040Hz cutoff (line 65). The code comment at line 42 of the header notes `kDefaultSlew=0.68f` is a "spec annotation", implying this is a known design decision, but no `// slew not user-controllable` note exists at the APVTS read site in `PluginProcessor.cpp:236`. This is lower severity than `sag` (WR-03) because the spec explicitly pins the slew frequency, but it can still mislead.

**Fix:** Add a comment at the `pRatSlew->load()` call site in `PluginProcessor.cpp` explaining that the slew parameter drives no DSP currently (slew is spec-pinned to 1040Hz), or remove the `slew` field from the `Parameters` struct if it is intentionally not user-controllable.

---

_Reviewed: 2026-04-09_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
