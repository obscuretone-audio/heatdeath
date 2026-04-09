---
phase: 01-project-scaffold
reviewed: 2026-04-09T00:00:00Z
depth: standard
files_reviewed: 24
files_reviewed_list:
  - .gitignore
  - CMakeLists.txt
  - Source/Parameters.cpp
  - Source/Parameters.h
  - Source/PluginEditor.cpp
  - Source/PluginEditor.h
  - Source/PluginProcessor.cpp
  - Source/PluginProcessor.h
  - Source/dsp/BurnIn.h
  - Source/dsp/H3000Undulator.cpp
  - Source/dsp/MicroPitch.cpp
  - Source/dsp/MicroPitch.h
  - Source/dsp/Oversampler.cpp
  - Source/dsp/Oversampler.h
  - Source/dsp/TapeBurnIn.cpp
  - Source/dsp/TurboRat.cpp
  - Source/dsp/TurboRat.h
  - Source/dsp/Undulator.h
  - Source/ui/HeatdeathLookAndFeel.cpp
  - Source/ui/HeatdeathLookAndFeel.h
  - Source/ui/StagePanel.cpp
  - Source/ui/StagePanel.h
  - Source/utils/BiquadFilter.h
  - Source/utils/SmoothParam.h
findings:
  critical: 0
  warning: 5
  info: 6
  total: 11
status: issues_found
---

# Phase 01: Code Review Report

**Reviewed:** 2026-04-09
**Depth:** standard
**Files Reviewed:** 24
**Status:** issues_found

## Summary

This is the Phase 1 scaffold for the HEATDEATH JUCE audio plugin. The architecture
is well-structured: a clean parameter namespace, forward-declared DSP stage objects,
and a clearly annotated processBlock pipeline. DSP stubs are intentionally empty
pass-throughs and are not penalised here.

Five warnings require attention before DSP phases begin — three of them are correctness
bugs in the live processBlock path that will cause audible problems or incorrect
behaviour even at this stub stage. Six info-level items are flagged for clean-up.

No security vulnerabilities were found. No hardcoded credentials or unsafe APIs.

---

## Warnings

### WR-01: Unused variable `bypassRamp` in `prepareToPlay`

**File:** `Source/PluginProcessor.cpp:93`
**Issue:** `bypassRamp` is computed but never used. The bypass smoothers are configured
via `reset(sampleRate, 0.01)` on the lines immediately below, making `bypassRamp`
entirely dead. If the intent was to pass `bypassRamp` to `reset()` as a
samples-based ramp, the call is wrong — but either way the variable is unused and
the compiler will warn at `-Wall`.
**Fix:** Delete lines 93-94. The smoothers are already initialised correctly via
`reset(sampleRate, 0.01)`.
```cpp
// Delete this:
const float bypassRamp = static_cast<float> (samplesPerBlock) /
                         static_cast<float> (sampleRate * 0.01);
```

---

### WR-02: Unused variable `dcCoeff` in `prepareToPlay`

**File:** `Source/PluginProcessor.cpp:72`
**Issue:** `dcCoeff` is computed (a manual first-order coefficient approximation) but
then never used — the DC blocking filters are actually initialised using the separate
`dcCoeffs` returned by `makeHighPass()` on line 74. This dead variable creates
confusion about which formula is actually used, and will produce a compiler warning.
**Fix:** Delete lines 72-73.
```cpp
// Delete this:
const float dcCoeff = 1.0f - (juce::MathConstants<float>::twoPi * 5.0f
                              / static_cast<float> (sampleRate));
```

---

### WR-03: Global feedback injects single held sample rather than per-sample signal

**File:** `Source/PluginProcessor.cpp:167-168`
**Issue:** The feedback injection loop adds `fbSample` — a constant computed once
before the loop — to every sample in the block. The comment acknowledges this is
"single-sample hold — intentional at these frequencies (<100Hz)." However, the
signal is already lowpass-filtered to 100Hz and averaged to a single scalar value
at step 9 (lines 403-407). Injecting that same scalar uniformly across every sample
of the *next* block is functionally correct for DC/sub-bass feedback, but creates a
block-sized DC step at every block boundary. At typical block sizes (128-512 samples)
this step occurs at 86-344Hz — well above the stated intent of "<100Hz." At 44100Hz
/ 128 samples this is ~344Hz steps, which will be audible as buzz at high feedback
amounts. The design comment is inaccurate and the implementation will produce
artefacts when real DSP stages are running.
**Fix:** At a minimum, linearly interpolate from `previousFeedbackSample` to
`feedbackSample` across the block. Alternatively, feed the per-sample LPF output
rather than averaging to a scalar. At Phase 1 stub stage this won't be audible, but
the architecture should be corrected before real signal flows through it.
```cpp
// Replace the loop with interpolated injection:
const float fbStart = previousFeedbackSample;
const float fbEnd   = feedbackSample;
for (int i = 0; i < numSamples; ++i)
{
    const float alpha = static_cast<float>(i) / static_cast<float>(numSamples);
    mono[i] += (fbStart + alpha * (fbEnd - fbStart)) * fbAmount;
}
previousFeedbackSample = feedbackSample;
// Add `float previousFeedbackSample = 0.0f;` as a member in PluginProcessor.h
```

---

### WR-04: `buffer.setSize` in `processBlock` may reallocate on the audio thread

**File:** `Source/PluginProcessor.cpp:228`
**Issue:** `buffer.setSize(2, numSamples, true, false, true)` is called in
`processBlock` on every block. JUCE's `AudioBuffer::setSize` documentation states
that it only reallocates if the requested size exceeds the current allocation. The
`retainExistingContent=true, clearExtraSpace=false, avoidReallocating=true` flags
suppress reallocation — however, the passed buffer is owned by the host and passed
by reference. Calling `setSize` on a host-owned buffer to change its channel count
is non-standard and host-dependent; some hosts pass buffers that are not
user-resizable. A safer approach is to pre-allocate a separate stereo working buffer
in `prepareToPlay` and copy results out.
**Fix:** Allocate a `juce::AudioBuffer<float> workBuffer` (stereo) as a member in
`prepareToPlay` with the expected max block size. In `processBlock`, copy the mono
input into `workBuffer` channel 0 and 1, process in place, then copy stereo result
back to `buffer`. This avoids mutating the host's buffer layout.

---

### WR-05: `persistedTemp`/`persistedTempPrev` restored unconditionally from state

**File:** `Source/PluginProcessor.cpp:506-512`
**Issue:** In `setStateInformation`, `persistedTemp` and `persistedTempPrev` are
always read from the state tree (lines 506-509) regardless of whether
`BURNIN_PERSIST` is currently enabled. The guard on line 511
(`if (pBurninPersist->load() > 0.5f)`) only controls whether the values are pushed
into the BurnIn stage — but `persistedTemp` and `persistedTempPrev` member variables
are still silently loaded with whatever values the XML contains. A saved state with
`persistedTemp = 0.9` and `BURNIN_PERSIST = false` will populate the member fields
with 0.9, which could then be pushed into BurnIn if the user later toggles Persist
on without reloading the project. This is a subtle state leak.
**Fix:** Only read the persisted temps from state when persist is also enabled in
the restored state, or always read them but document clearly that the `> 0.5f` guard
on the call to `setPersistedTemp` is the only protection against stale injection.
```cpp
// In setStateInformation — make the conditional explicit:
const bool persistEnabled = (pBurninPersist->load() > 0.5f);
persistedTemp     = persistEnabled ? static_cast<float>(
                        newState.getProperty("persistedTemp", 0.0f)) : 0.0f;
persistedTempPrev = persistEnabled ? static_cast<float>(
                        newState.getProperty("persistedTempPrev", 0.0f)) : 0.0f;

if (persistEnabled)
    stageBurnIn->setPersistedTemp (persistedTemp, persistedTempPrev);
```

---

## Info

### IN-01: `getLastLfoValue()` return value discarded by caller

**File:** `Source/PluginProcessor.cpp:278`
**Issue:** `stageMicroPitch->getLastLfoValue()` is called and its return value
(`0.f` in the stub) is thrown away. The comment says "cross-feed to MicroPitch
detune (implemented inside MicroPitch::process)" — this is contradictory. If
cross-feeding is done inside `process`, there is nothing to read here. The dead
call should either be removed or the cross-feed architecture clarified before Phase
4 implements it.
**Fix:** Remove the call or replace it with a comment describing the intended
cross-feed design for Phase 4 to implement.

---

### IN-02: `dcBlock1R` initialised but never used in mono stage

**File:** `Source/PluginProcessor.cpp:78`
**Issue:** `dcBlock1R` is prepared and assigned coefficients in `prepareToPlay`
(line 78), and reset in `releaseResources` (line 127). However, after Stage 1 only
channel 0 exists (mono) — the DC block for channel 1 is only applied at and after
Stage 2 (post-MicroPitch). `dcBlock1R` is dead weight until the code is restructured.
This is a clarity issue rather than a bug, but it could mislead future implementors
into thinking there is already a stereo signal at the post-RAT boundary.
**Fix:** Remove `dcBlock1R` member from the processor and only initialise
`dcBlock2R`/`dcBlock3R` where stereo processing begins. Or rename to clarify
that it is reserved for a future stereo-RAT path.

---

### IN-03: `HeatdeathLookAndFeel` is defined but never instantiated or attached

**File:** `Source/ui/HeatdeathLookAndFeel.h`, `Source/PluginEditor.h`
**Issue:** The LookAndFeel class is compiled but not used anywhere. No member exists
in `HeatDeathEditor` to hold it, and no `setLookAndFeel` call exists. When Phase 7
wires the UI, forgetting to call `setLookAndFeel(&laf)` is a common JUCE bug — it
must also be paired with `setLookAndFeel(nullptr)` in the destructor to avoid
dangling pointer UB. Flagging now while the scaffold is small.
**Fix:** When Phase 7 attaches it, add:
```cpp
// In HeatDeathEditor private section:
HeatdeathLookAndFeel laf;
// In constructor:
setLookAndFeel (&laf);
// In destructor:
setLookAndFeel (nullptr);
```

---

### IN-04: `StagePanel` compiled as a .cpp/.h pair with the .cpp empty

**File:** `Source/ui/StagePanel.cpp`
**Issue:** `StagePanel.cpp` contains only a comment and includes the header. All
implementation is inline in the header (empty `paint`/`resized` bodies). Having an
empty .cpp file adds a compilation unit and a redundant include for no benefit at
this stage.
**Fix:** Either move the class entirely into the header (header-only stub), or keep
the .cpp stub and remove the inline definitions from the header (declare methods in
header, define in .cpp). Pick one pattern and be consistent with `HeatdeathLookAndFeel`
which uses the declare-in-header / define-in-cpp approach.

---

### IN-05: `Oversampler` is not used by any DSP stage in Phase 1

**File:** `Source/dsp/Oversampler.h`, `Source/dsp/Oversampler.cpp`
**Issue:** The `Oversampler` class is compiled but no DSP stage holds or calls it.
`PluginProcessor.cpp` does not include or instantiate it. This is expected for a
Phase 1 stub, but the class is not forward-declared or referenced anywhere, so it
will silently rot if its interface changes. Worth noting so Phase 2/3 integration
has a checklist item.
**Fix:** Add a `TODO(Phase-2): Instantiate Oversampler inside TurboRat, Undulator,
BurnIn` comment in each relevant DSP header.

---

### IN-06: `SmoothParam::setTimeMs` is a no-op — silently ignores its arguments

**File:** `Source/utils/SmoothParam.h:8`
**Issue:** `setTimeMs(float, double)` accepts a time and sample rate but immediately
discards both (empty body). Any code that calls `setTimeMs` trusting that smoothing
will happen will get a surprise — `tick()` instantly snaps to target with no
interpolation. The stub comment on `tick()` makes this clear for `tick`, but
`setTimeMs` has no stub comment and its no-op is silent.
**Fix:** Add a comment to `setTimeMs` matching the style of `tick()`:
```cpp
void setTimeMs (float /*ms*/, double /*sampleRate*/) {}  // Phase 1: no smoothing
```
And/or add a `jassertfalse` or static_assert in the body to make calling code
fail fast during development if it mistakenly relies on smoothing before Phase 2.

---

_Reviewed: 2026-04-09_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
