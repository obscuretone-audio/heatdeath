---
phase: 02-core-infrastructure
plan: "04"
subsystem: plugin-processor
tags: [oversampling, feedback-interpolation, trim-smoothing, WR-03, CHAIN-02, CHAIN-06, CHAIN-07, PARAMS-04]
dependency_graph:
  requires: [02-03]
  provides: [juce-dsp-oversampling-wired, WR-03-fix, per-sample-trim-smoothers, oversampler-stub-deleted]
  affects: [Source/PluginProcessor.h, Source/PluginProcessor.cpp, CMakeLists.txt]
tech_stack:
  added: [juce::dsp::Oversampling<float>]
  patterns: [4x-polyphase-IIR-oversampling, per-sample-linear-interpolation, SmoothedValue-per-sample-trim]
key_files:
  created: []
  modified:
    - Source/PluginProcessor.h
    - Source/PluginProcessor.cpp
    - CMakeLists.txt
  deleted:
    - Source/dsp/Oversampler.h
    - Source/dsp/Oversampler.cpp
decisions:
  - "Approach A chosen for Oversampler stub: full deletion (not header-only wrapper) — zero references found in Source/, clean deletion confirmed"
  - "Oversampler up/down placed before Stage 1 setTargetValue call (not after), so anti-alias filter latency is registered before the stage stub runs"
  - "previousFeedbackSample placed in header private section alongside feedbackSample for clear pairing"
metrics:
  duration_seconds: 233
  completed_date: "2026-04-09"
  tasks_completed: 2
  files_modified: 3
  files_deleted: 2
requirements_satisfied: [CHAIN-02, CHAIN-06, CHAIN-07, PARAMS-04]
---

# Phase 02 Plan 04: Oversampling, WR-03 Feedback Fix, Trim Smoothers Summary

**One-liner:** juce::dsp::Oversampling<float> (4x polyphase IIR) wired at Stage 1 boundary with latency reporting, WR-03 per-sample feedback interpolation, three SmoothedValue inter-stage trim controls, and Phase 1 Oversampler stub deleted.

## Tasks Completed

| Task | Name | Commit | Files |
|------|------|--------|-------|
| 1 | Wire oversampler, previousFeedbackSample, three trim SmoothedValues; fix WR-03 in processBlock | 009fa63 | Source/PluginProcessor.h, Source/PluginProcessor.cpp |
| 2 | Delete legacy Phase 1 Oversampler stub (Approach A) | a42c861 | Source/dsp/Oversampler.h, Source/dsp/Oversampler.cpp, CMakeLists.txt |

## What Was Built

### Task 1: Oversampler wiring, WR-03 fix, trim smoothers

**CHAIN-02: juce::dsp::Oversampling<float> member (PluginProcessor.h):**

```cpp
juce::dsp::Oversampling<float> oversampler {
    1u,                                                                  // numChannels (mono at boundary)
    2u,                                                                  // factor order = 4x
    juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
    true,                                                                // max quality
    false                                                                // do not interleave
};
```

**prepareToPlay additions:**
```cpp
oversampler.reset();
oversampler.initProcessing (static_cast<size_t> (samplesPerBlock));
setLatencySamples (static_cast<int> (oversampler.getLatencyInSamples()));
previousFeedbackSample = 0.0f;
// three trim smoothers reset and seeded with current param values
```

**processBlock — CHAIN-02 wrap around Stage 1:**
```cpp
{
    juce::dsp::AudioBlock<float> monoBlock (buffer.getArrayOfWritePointers(),
                                            1, static_cast<size_t> (numSamples));
    auto osBlock = oversampler.processSamplesUp (monoBlock);
    juce::ignoreUnused (osBlock);   // Phase 3 will process osBlock at 4x
    oversampler.processSamplesDown (monoBlock);
}
```

For Phase 2 the stage is a pass-through stub, so the up/down round-trip is transparent but establishes correct latency reporting and exercises the anti-alias filters.

**WR-03: Per-sample linear interpolation of feedback injection:**

Before (step artefact at block boundaries):
```cpp
const float fbSample = feedbackSample * fbAmount;
for (int i = 0; i < numSamples; ++i)
    mono[i] += fbSample;  // same value per block
```

After (smooth ramp from previous to current feedback):
```cpp
const float fbStart = previousFeedbackSample * fbAmount;
const float fbEnd   = feedbackSample         * fbAmount;
const float invN    = (numSamples > 0) ? 1.0f / static_cast<float>(numSamples) : 0.0f;
for (int i = 0; i < numSamples; ++i)
{
    const float alpha = static_cast<float>(i) * invN;
    mono[i] += fbStart + alpha * (fbEnd - fbStart);
}
previousFeedbackSample = feedbackSample;
// else branch: previousFeedbackSample = 0.0f;
```

**CHAIN-07 + PARAMS-04: Per-sample smoothed trim controls (replacing block-rate applyGain):**

All three sites follow the same pattern:
```cpp
trimPostRatSmooth.setTargetValue(juce::Decibels::decibelsToGain(pTrimPostRat->load()));
{
    auto* m = buffer.getWritePointer(0);
    for (int i = 0; i < numSamples; ++i)
        m[i] *= trimPostRatSmooth.getNextValue();
}
```

The post-MicroPitch and post-Undulator trims apply the same per-sample gain to both L and R channels of workBuffer.

### Task 2: Oversampler stub deletion (Approach A)

The Phase 1 stub `Source/dsp/Oversampler.{h,cpp}` was a no-op pass-through (all methods empty or return 1). It had zero callers outside its own files. Approach A was applied:

1. Deleted `Source/dsp/Oversampler.h`
2. Deleted `Source/dsp/Oversampler.cpp`
3. Removed `Source/dsp/Oversampler.cpp` from `CMakeLists.txt` `target_sources`
4. Confirmed `grep -rn "dsp/Oversampler" Source/` returns zero hits
5. Build clean

## Verification Results

```
1. Oversampling wired:
   grep -n "processSamplesUp|processSamplesDown|initProcessing" Source/PluginProcessor.cpp
   Line 116: initProcessing in prepareToPlay     PASS
   Line 225: processSamplesUp in processBlock    PASS
   Line 227: processSamplesDown in processBlock  PASS

2. Latency reported:
   grep -n "setLatencySamples" Source/PluginProcessor.cpp
   Line 117: in prepareToPlay                    PASS

3. WR-03 fix present:
   grep -n "previousFeedbackSample" Source/PluginProcessor.cpp
   Lines 119, 120, 184, 194, 198 — init, fbStart read, update, else-branch reset  PASS

4. Trim smoothers per-sample:
   grep -n "trimPost.*Smooth\." Source/PluginProcessor.cpp
   setTargetValue + getNextValue at all three sites  PASS

5. No applyGain for trims:
   grep -n "applyGain.*trimGain" Source/PluginProcessor.cpp
   ZERO hits  PASS

6. Oversampler stub retired:
   grep -rn "dsp/Oversampler" Source/
   ZERO hits (Approach A — full deletion)  PASS

7. Build:
   cmake --build build --config Release
   No errors, no new warnings (pre-existing processorRef warning unrelated)  PASS
```

## Deviations from Plan

None — plan executed exactly as written. Approach A chosen for the stub deletion as specified by the plan's default preference (zero CMake complications found, no hidden deps).

## Known Stubs

None introduced by this plan. The oversampler up/down round-trip around the pass-through Stage 1 is intentional and documented — Phase 3 will move the TurboRat DSP onto the oversampled block. This is not a stub; it is the correct architecture for Phase 2.

## Threat Flags

None — no new network endpoints, auth paths, file access patterns, or schema changes introduced.

## Self-Check: PASSED

- Source/PluginProcessor.h: FOUND (oversampler, previousFeedbackSample, three trim SmoothedValues present)
- Source/PluginProcessor.cpp: FOUND (initProcessing, processSamplesUp/Down, setLatencySamples, WR-03 interp, per-sample trims)
- Source/dsp/Oversampler.h: CONFIRMED DELETED
- Source/dsp/Oversampler.cpp: CONFIRMED DELETED
- CMakeLists.txt: FOUND (Oversampler.cpp removed from target_sources)
- Commit 009fa63: Task 1
- Commit a42c861: Task 2
- All 6 post-plan verification checks: PASSED
- Build: PASSED (no errors, no new warnings)
