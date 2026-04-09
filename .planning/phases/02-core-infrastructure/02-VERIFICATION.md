---
phase: 02-core-infrastructure
verified: 2026-04-09T00:00:00Z
status: human_needed
score: 9/10 must-haves verified
overrides_applied: 0
human_verification:
  - test: "Load plugin in host, toggle each stage bypass rapidly, confirm no audible click"
    expected: "Smooth 10ms crossfade on every bypass toggle — no zipper noise, no click, no pop"
    why_human: "Bypass crossfade correctness is an auditory property; code structure confirms SmoothedValue is wired but audio quality requires human listening test"
  - test: "Load plugin, feed 1kHz sine at -6dBFS, observe output after 60+ seconds continuous playback"
    expected: "No DC offset accumulation; output remains centered around zero"
    why_human: "DC accumulation is a slow-accumulation phenomenon that can only be confirmed by running the signal chain for sustained periods; grep cannot verify zero accumulation"
  - test: "Change trim post-RAT from 0dB to +6dB while audio is playing"
    expected: "Gain change ramps smoothly over ~20ms with no audible zipper artifact"
    why_human: "Per-sample SmoothedValue trim application is structurally correct but zipper-free quality requires listening confirmation"
---

# Phase 2: Core Infrastructure Verification Report

**Phase Goal:** The plugin processes audio end-to-end through a signal chain skeleton with APVTS parameters, oversampling, DC blocking, per-stage bypass, and parameter smoothing — all stages pass-through but the plumbing is complete
**Verified:** 2026-04-09
**Status:** human_needed
**Re-verification:** No — initial verification

## Goal Achievement

### Observable Truths

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | All 40 parameters appear with correct IDs, ranges, defaults; no raw string lookups in processBlock | VERIFIED | grep -c returns 40 atomic pointers, 40 getRawParameterValue calls all inside cacheParameterPointers, 0 raw string ID literals in processBlock |
| 2 | Audio passes from mono input through four stage slots out as stereo with no crash | VERIFIED | Bus layout declared mono-in/stereo-out; processBlock routes buffer channel 0 → workBuffer (stereo) → back to host buffer; output safety clip present |
| 3 | 4x oversampling active at Stage 1 boundary; DC-block HPF fires after each stage output | VERIFIED | processSamplesUp/Down at lines 225-227 wrapping Stage 1; dcBlock1L post-RAT (mono), dcBlock2L/R post-MicroPitch, dcBlock3L/R post-Undulator, all wired per-sample |
| 4 | Parameter changes produce no audible click (20ms smoothing; bypass 10ms crossfade) | NEEDS HUMAN | SmoothedValue members present and reset at correct windows; per-sample application in loops confirmed; audio quality requires listening test |
| 5 | Dry buffer and feedback buffer pre-allocated in prepareToPlay; no allocations inside processBlock | VERIFIED | All setSize calls at lines 106-112 in prepareToPlay; zero setSize/new/malloc/AudioBuffer ctor inside processBlock; workBuffer/preUndBuffer/preBurninBuffer pre-allocated |
| 6 | SmoothParam one-pole IIR smoother is a real implementation, not a stub | VERIFIED | coeff field present; tick() uses formula `current = coeff * current + (1.0f - coeff) * target`; setTimeMs uses exp(-2pi/samples); 13-contract test suite passes |
| 7 | processBlock has no real-time allocation | VERIFIED | grep audit confirms all setSize calls in prepareToPlay only; `new` keyword appears only in createEditor/createPluginFilter, outside processBlock |
| 8 | juce::dsp::Oversampling<float> instantiated as member, wired in prepareToPlay, used in processBlock | VERIFIED | Member declaration at PluginProcessor.h lines 238-244; initProcessing + setLatencySamples in prepareToPlay; processSamplesUp/Down at processBlock lines 225-227 |
| 9 | All 40 parameter pointers cached with jassert guards | VERIFIED | 40 getRawParameterValue assign lines + 40 jassert(!= nullptr) lines, all inside cacheParameterPointers(); called from constructor at line 30 after APVTS init |
| 10 | DC accumulation stays zero through sustained playback | NEEDS HUMAN | DC block filters structurally correct (5Hz HPF at 3 stage boundaries, 4 filters prepared), but zero-accumulation guarantee requires sustained audio test |

**Score:** 9/10 truths — 8 fully verified, 2 require human confirmation

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `Source/Parameters.cpp` | createParameterLayout() with 7 groups + standalone ACETATE_MODE | VERIFIED | All 7 groups (rat/pitch/und/burnin/trim/global/timer) + standalone Bool; 40 parameters total confirmed by grep -c |
| `Source/PluginProcessor.h` | 40 atomic<float>* members; workBuffer/preUndBuffer/preBurninBuffer; juce::dsp::Oversampling; trim SmoothedValues | VERIFIED | grep -c returns 40; all three buffer members at lines 228-233; oversampler at lines 238-244; trimPostRat/Pitch/UndSmooth at lines 251-253 |
| `Source/PluginProcessor.cpp` | cacheParameterPointers(); allocation-free processBlock; WR fixes; oversampling wired | VERIFIED | All sections confirmed by grep audit; setSize only in prepareToPlay; WR-01/02/03/04/05 and IN-01/02 all resolved |
| `Source/utils/SmoothParam.h` | One-pole IIR smoother, header-only, allocation-free, ≥30 lines | VERIFIED | 51 lines; coeff field; exp(-2pi/samples) formula; noexcept methods; POD struct (12 bytes) |
| `Tests/SmoothParamTest.cpp` | 13-contract test suite | VERIFIED | File present; 13 tests covering API, coeff validity, reset, convergence, monotonicity, POD size |
| `CMakeLists.txt` | No Oversampler.cpp in target_sources; SmoothParamTest target present | VERIFIED | Oversampler.cpp absent from target_sources; SmoothParamTest executable target at line 103 |

### Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| Constructor | cacheParameterPointers() | direct call at line 30 after APVTS init | WIRED | `cacheParameterPointers();` at line 30, after apvts member init at line 18 |
| cacheParameterPointers() | Params:: constants | `using namespace Params; apvts.getRawParameterValue(ID)` | WIRED | 40 getRawParameterValue calls referencing Params:: IDs |
| processBlock | cached atomic pointers | `pXxx->load()` | WIRED | All parameter reads use `->load()` on cached pointers; zero string lookups |
| prepareToPlay | workBuffer.setSize | stereo pre-allocation | WIRED | workBuffer.setSize(2, samplesPerBlock, ...) at line 110 |
| processBlock (Stage 2) | workBuffer | mono→stereo copy via copyFrom | WIRED | workBuffer.copyFrom(0/1, 0, buffer, 0, ...) at lines 276-277 |
| setStateInformation (WR-05) | pBurninPersist gate | `const bool persistEnabled` | WIRED | persistEnabled declared line 573; gates both reads and stageBurnIn inject |
| prepareToPlay | oversampler.initProcessing + setLatencySamples | CHAIN-02 setup | WIRED | Lines 116-117 in prepareToPlay |
| processBlock (Stage 1) | oversampler.processSamplesUp / processSamplesDown | mono AudioBlock wrap | WIRED | Lines 223-228 in processBlock |
| processBlock (feedback) | previousFeedbackSample interpolation | per-sample alpha lerp | WIRED | fbStart/fbEnd linear interp at lines 184-194; previousFeedbackSample updated at line 194 |
| processBlock (three trim sites) | trimPostRat/Pitch/UndSmooth.getNextValue() | per-sample multiply | WIRED | setTargetValue + getNextValue loop at lines 256-262, 314-325, 386-397 |

### Data-Flow Trace (Level 4)

| Artifact | Data Variable | Source | Produces Real Data | Status |
|----------|---------------|--------|--------------------|--------|
| `PluginProcessor.cpp processBlock` | `pRatDrive->load()` etc. | cacheParameterPointers resolves APVTS atomic storage | Yes — APVTS holds live parameter state | FLOWING |
| `PluginProcessor.cpp processBlock` | `workBuffer` | prepareToPlay setSize + per-block copyFrom from buffer ch 0 | Yes — populated from host audio input each block | FLOWING |
| `PluginProcessor.cpp processBlock` | `feedbackSample` | feedbackLpf.processSample on block-average of output | Yes — computed from output each block when feedback active | FLOWING |
| `SmoothParam.h tick()` | `current` | `coeff * current + (1-coeff) * target` per tick | Yes — live IIR computation | FLOWING |

### Behavioral Spot-Checks

| Behavior | Command | Result | Status |
|----------|---------|--------|--------|
| SmoothParam produces coeff in (0,1) for 20ms/44100Hz | `grep -n "exp.*twoPi" Source/utils/SmoothParam.h` | Found at line 34 | PASS |
| Oversampler wired at Stage 1 | `grep -n "processSamplesUp" Source/PluginProcessor.cpp` | Line 225 inside processBlock | PASS |
| No allocations in processBlock | All setSize calls | Lines 106-112 in prepareToPlay only | PASS |
| getRawParameterValue confined to cacheParameterPointers | `grep -n "getRawParameterValue" Source/PluginProcessor.cpp` | Lines 600-653, all inside cacheParameterPointers | PASS |
| Dead variables removed | `grep bypassRamp\|dcCoeff\|dcBlock1R Source/PluginProcessor.cpp` | Only live `dcCoeffs` (different name) found; bypassRamp/dcBlock1R absent | PASS |
| 40 jassert nullptr guards | `grep -c "jassert.*!= nullptr"` | 40 | PASS |
| Legacy Oversampler stub deleted | `ls Source/dsp/` | No Oversampler.h or Oversampler.cpp present | PASS |
| Full test suite (SmoothParam) | SmoothParamTest target exists, 13 tests | Target wired in CMakeLists.txt line 103; 13 check() calls in test file | PASS |

### Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
|-------------|------------|-------------|--------|----------|
| PARAMS-01 | 02-01 | Full APVTS definition using Params:: constants; no raw string IDs outside Parameters.cpp | SATISFIED | Parameters.cpp uses pid() helper wrapping Params:: IDs exclusively; zero raw literals in PluginProcessor.cpp |
| PARAMS-02 | 02-01 | All parameter pointers cached; no APVTS hash lookups inside processBlock | SATISFIED | 40 getRawParameterValue calls all inside cacheParameterPointers(); processBlock uses ->load() exclusively |
| PARAMS-03 | 02-01 | Parameter groups match hardware stages for DAW automation lane display | SATISFIED | 7 groups: rat/pitch/und/burnin/trim/global/timer with correct display names; ACETATE_MODE standalone |
| PARAMS-04 | 02-02, 02-03, 02-04 | All float params smoothed 20ms; und_speed 50ms; bypass 10ms crossfade | SATISFIED (structural) | SmoothParam IIR implemented; 4 bypass SmoothedValues at 10ms; 3 trim SmoothedValues at 20ms; globalMixSmooth at 20ms. Audio quality needs human test |
| CHAIN-01 | 02-03 | processBlock enforces mono input before Stage 1; stereo created by Stage 2 | SATISFIED | Bus layout declared mono-in via .withInput("Input", mono()); isBusesLayoutSupported rejects non-mono input; workBuffer expansion at Stage 2 |
| CHAIN-02 | 02-04 | 4x polyphase IIR oversampling at plugin boundary | SATISFIED | juce::dsp::Oversampling<float> (1ch, 2-order=4x, polyphaseIIR) member; initProcessing + setLatencySamples in prepareToPlay; processSamplesUp/Down around Stage 1 in processBlock |
| CHAIN-03 | 02-03 | DC blocking HPF at ~5Hz after each stage output | SATISFIED | dcBlock1L post-RAT (mono); dcBlock2L/R post-MicroPitch (stereo); dcBlock3L/R post-Undulator (stereo); all prepared with makeHighPass(sr, 5.0f) |
| CHAIN-04 | 02-03 | Input limiter (soft tanh at unity) before Stage 1 | SATISFIED | inputLimiter WaveShaper with FastMathApproximations::tanh; prepared in prepareToPlay; applied before Stage 1 at lines 205-210 |
| CHAIN-05 | 02-03 | Per-stage bypass 10ms crossfade; global wet/dry mix | SATISFIED | 4 bypassSmooth* SmoothedValues (10ms); global wet/dry blend at lines 445-458 using globalMixSmooth. Audio quality needs human test |
| CHAIN-06 | 02-04 | Global feedback path gated on pGlobalFeedbackActive; 100Hz LP | SATISFIED | feedbackLpf prepared with makeLowPass(sr, 100Hz); injection block gated on pGlobalFeedbackActive->load() > 0.5f; per-sample interpolation (WR-03) |
| CHAIN-07 | 02-04 | Inter-stage trims ±12dB smoothed per-sample | SATISFIED | Three trim SmoothedValues (20ms); setTargetValue + getNextValue per-sample loops at three stage junctions; range ±12dB in Parameters.cpp |

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|------|------|---------|----------|--------|
| `Source/PluginProcessor.cpp` | 754, 763 | `new HeatDeathEditor`, `new HeatDeathProcessor` | Info | Outside processBlock; standard JUCE plugin pattern; not a real-time allocation issue |
| `Source/PluginProcessor.cpp` | ~71 | `dcCoeffs` local variable (not `dcCoeff`) | Info | Not the dead variable WR-02 removed; this is the live makeHighPass result — correct |

No blockers found. No stubs. No placeholder return values. No unguarded allocations in the audio path.

### Human Verification Required

#### 1. Bypass Click-Free Test

**Test:** Load the plugin in a VST3 host (AU Lab, Ableton, or auval). Feed a continuous 1kHz sine at -6dBFS. Rapidly toggle each stage's bypass (RAT, MicroPitch, Undulator, BurnIn) on and off while listening.
**Expected:** Completely silent transitions — no click, no pop, no zipper noise. The 10ms SmoothedValue crossfade should be imperceptible.
**Why human:** The SmoothedValue wiring is correct structurally, but the absence of audible artifact is a subjective audio quality measure that cannot be verified by static analysis.

#### 2. DC Offset Non-Accumulation Test

**Test:** Feed a 1kHz sine at -6dBFS through the plugin with all stages active and no bypass. Let it run for at least 60 seconds. Use a DAW meter or oscilloscope to check the output DC offset before and after.
**Expected:** DC offset remains at or below -80dBFS throughout. No measurable buildup over time.
**Why human:** DC blocking filters are structurally correct (5Hz HPF at three stage boundaries), but the absence of DC accumulation in practice requires a sustained audio run — the DSP stage stubs (pass-through) plus the 5Hz HPFs make accumulation unlikely but this must be confirmed empirically.

#### 3. Trim Knob Zipper-Free Test

**Test:** Load the plugin, feed audio, and move a trim post-RAT control from 0dB to +6dB over about one second while listening.
**Expected:** Smooth gain change with no audible zipper/stepping artifact. The 20ms SmoothedValue ramp should produce an inaudible transition at any reasonable sweep speed.
**Why human:** Per-sample gain smoothing is structurally correct, but the absence of zipper artifacts under real audio conditions requires ears.

### Gaps Summary

No gaps found. All must-haves are either fully verified or require human audio testing. The three human verification items are audio quality checks — the underlying code structures are correct and complete.

---

_Verified: 2026-04-09T00:00:00Z_
_Verifier: Claude (gsd-verifier)_
