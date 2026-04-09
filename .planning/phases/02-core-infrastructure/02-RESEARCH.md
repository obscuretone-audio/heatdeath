# Phase 2: Core Infrastructure - Research

**Researched:** 2026-04-09
**Domain:** JUCE AudioProcessor infrastructure — APVTS, parameter smoothing, processBlock skeleton, oversampling, DC blocking, bypass crossfade
**Confidence:** HIGH

---

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| PARAMS-01 | Full APVTS definition in `Parameters.cpp` using all ID constants from `Parameters.h`; no raw string IDs outside that file | Agent starter file `cpp files for agent/Parameters.cpp` provides the complete authoritative layout; Source/Parameters.cpp is identical — implement as written |
| PARAMS-02 | All parameter pointers cached in `prepareToPlay()` via `cacheParameterPointers()` — no APVTS hash lookups inside `processBlock` | Agent starter `PluginProcessor.cpp` shows the full `cacheParameterPointers()` implementation with all 30+ pointers and jassert sanity guards |
| PARAMS-03 | Parameter groups match four hardware stages (RAT / MicroPitch / Undulator / Burn-In / Global) | Agent starter `Parameters.cpp` defines six groups: rat, pitch, und, burnin, trim, global, timer, plus standalone acetate_mode |
| PARAMS-04 | All float params smoothed 20ms; `und_speed` smoothed 50ms; bypass toggles use 10ms crossfade | `juce::SmoothedValue<float>` with `reset(sampleRate, timeSeconds)` is the correct JUCE mechanism; used in PluginProcessor for bypass smoothers — extend to all float params read in processBlock |
| CHAIN-01 | `processBlock` sums stereo input to mono (L+R × 0.5) before Stage 1; stereo field created entirely by Stage 2 | Implemented in agent starter processBlock: uses `buffer.getWritePointer(0)` as mono working channel; R channel used for stereo from Stage 2 onward |
| CHAIN-02 | 4× polyphase FIR oversampling applied at plugin boundary; waveshaper stages run at 4× rate | `juce::dsp::Oversampling` class with `juce::dsp::Oversampling<float>(numChannels, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR)` is the permitted approach |
| CHAIN-03 | DC blocking (first-order HPF at ~5Hz) applied after each stage output | Agent starter uses `juce::dsp::IIR::Filter<float>` with `makeHighPass(sampleRate, 5.0f)` coefficients — 3 pairs (6 filters total) in PluginProcessor private section |
| CHAIN-04 | Input limiter (soft tanh clip at unity threshold) applied at plugin boundary before Stage 1 | Agent starter uses `juce::dsp::WaveShaper<float>` with `FastMathApproximations::tanh(x)` — correct pattern |
| CHAIN-05 | Per-stage bypass with 10ms dry/wet crossfade; global parallel wet/dry mix | Agent starter implements per-block `bypassSmooth*.setTargetValue()` + per-sample `getNextValue()` crossfade — exact pattern to follow |
| CHAIN-06 | Global feedback path — fraction of final output routed back to Stage 1 input (0–15%), heavy LP at 100Hz | Agent starter has stub implementation; WR-03 requires replacing block-rate injection with per-sample interpolation |
| CHAIN-07 | Inter-stage trim controls (±12dB) at each stage junction | Agent starter uses `juce::Decibels::decibelsToGain(pTrimPost*.load())` applied via `buffer.applyGain()` — correct pattern |

</phase_requirements>

---

## Summary

Phase 2 ports the authoritative agent starter files (`cpp files for agent/`) into `Source/`, implementing all APVTS parameter definitions, parameter pointer caching, processBlock skeleton, oversampling, DC blocking, bypass crossfades, parameter smoothing, and the global feedback path. The agent starter files are already complete, correct, and represent the target state — Phase 2's primary job is to transplant them into the live Source tree while fixing three WR-level issues found in Phase 1 review (WR-03, WR-04, WR-05) and two WR-level dead-code warnings (WR-01, WR-02).

The most important architectural decision in this phase: the agent starter processBlock places the `juce::dsp::Oversampling` boundary at the **plugin boundary** (before Stage 1), not inside each DSP stage. Individual stages (TurboRat, BurnIn) run their internal processing at the 4× rate using the oversampled buffer. This is different from having each stage manage its own oversampler — Phase 2 must implement it in `PluginProcessor`, not in the stage classes.

**Primary recommendation:** Implement the agent starter files as-is into Source/, apply the four WR fixes from 01-REVIEW.md, implement `juce::SmoothedValue` for all float parameters read in processBlock (the agent starter already does bypass and global mix — extend to all stage float params), and wire `juce::dsp::Oversampling` at the PluginProcessor level.

---

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| `juce::AudioProcessorValueTreeState` (APVTS) | JUCE 7.0.12 (pinned) | All parameter storage, serialization, undo | Required by spec; provides atomic float access, group/layout construction, XML state |
| `juce::SmoothedValue<float>` | JUCE 7.0.12 | Per-sample parameter smoothing, bypass crossfades | Built-in, zero-allocation, correct real-time use |
| `juce::dsp::Oversampling<float>` | JUCE 7.0.12 | 4× polyphase FIR upsampling/downsampling | Only third-party DSP component permitted by spec; JUCE-provided |
| `juce::dsp::IIR::Filter<float>` | JUCE 7.0.12 | DC blocking HPFs, feedback LPF | Built-in, correct for biquad/first-order IIR |
| `juce::dsp::WaveShaper<float>` | JUCE 7.0.12 | Input limiter (soft tanh) | Built-in waveshaper with function pointer |
| `juce::Decibels::decibelsToGain` | JUCE 7.0.12 | dB → linear for inter-stage trims | Correct conversion, header-only |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| `juce::dsp::FastMathApproximations::tanh` | JUCE 7.0.12 | Fast tanh for input limiter | Use inside processBlock where speed matters |
| `juce::AudioBuffer<float>` | JUCE 7.0.12 | Dry buffer, working buffers | Pre-allocated in prepareToPlay |
| `juce::dsp::ProcessSpec` | JUCE 7.0.12 | Prepare context (sr, blockSize, channels) | Required by all juce::dsp components |
| `juce::ScopedNoDenormals` | JUCE 7.0.12 | Suppress denormal CPU spikes | Always first line of processBlock |

**No additional installations required.** All dependencies are within JUCE 7.0.12, which is already pinned via CMakeLists.txt FetchContent. [VERIFIED: Source/Parameters.cpp, Source/PluginProcessor.h already compile with these includes]

---

## Architecture Patterns

### Recommended Project Structure

The agent starter defines the complete target. Phase 2 writes to these exact files:

```
Source/
├── Parameters.h         — namespace Params{} with all constexpr string IDs (already complete, matches agent starter)
├── Parameters.cpp       — createParameterLayout() with 6 groups + acetate (copy from agent starter)
├── PluginProcessor.h    — All atomic<float>* member pointers, SmoothedValue members, dryBuffer, feedbackSample + previousFeedbackSample (add WR-03 fix)
└── PluginProcessor.cpp  — prepareToPlay, processBlock, cacheParameterPointers, updateTimerState, getTimerProgress (copy from agent starter + apply WR fixes)
```

DSP stubs in `Source/dsp/` are NOT modified in Phase 2 — they pass through as-is. The Oversampler stub in `Source/dsp/Oversampler.h/cpp` will be replaced with real `juce::dsp::Oversampling` usage directly in PluginProcessor.

### Pattern 1: APVTS Parameter Group Construction

**What:** All parameters defined in `createParameterLayout()` using `std::make_unique<Group>` with variadic child parameters passed at construction.
**When to use:** Once, in Parameters.cpp. Never elsewhere.

```cpp
// Source: cpp files for agent/Parameters.cpp (authoritative)
auto ratGroup = std::make_unique<Group> ("rat", "RAT", "|",
    std::make_unique<Float> (pid(RAT_DRIVE), "Drive",
        skewed(0.0f, 100.0f, 0.1f, 0.7f), 72.0f, Attr().withLabel("%")),
    // ... more params
    std::make_unique<Bool> (pid(RAT_BYPASS), "RAT Bypass", false)
);
layout.add(std::move(ratGroup));
```

**Key detail:** `ParameterID` uses version stamp `{ id, 1 }` throughout. If an ID changes in a future release, bump the version and add a migration handler in `setStateInformation()`.

### Pattern 2: Parameter Pointer Caching

**What:** `apvts.getRawParameterValue(ID)` called once in constructor (via `cacheParameterPointers()`), result stored as `std::atomic<float>*`. In processBlock, read via `.load()`.
**When to use:** Every parameter that processBlock reads.

```cpp
// Source: cpp files for agent/PluginProcessor.cpp
void HeatDeathProcessor::cacheParameterPointers()
{
    using namespace Params;
    pRatDrive = apvts.getRawParameterValue(RAT_DRIVE);
    // ...
    jassert(pRatDrive != nullptr);  // catches ID typos at startup
}
```

**Critical:** `cacheParameterPointers()` is called in the **constructor**, not in `prepareToPlay()`. The agent starter does this correctly. APVTS parameter storage is stable for the object's lifetime — pointers are safe to cache once.

### Pattern 3: SmoothedValue for Bypass Crossfade

**What:** One `juce::SmoothedValue<float>` per stage. Target set to 0.0 (bypass) or 1.0 (active) at top of processBlock. Per-sample `getNextValue()` used for crossfade blend.
**When to use:** All per-stage bypass toggles. Global mix blend.

```cpp
// Source: cpp files for agent/PluginProcessor.cpp
// In prepareToPlay:
bypassSmoothRat.reset(sampleRate, 0.01);  // 10ms
bypassSmoothRat.setCurrentAndTargetValue(1.0f);

// In processBlock:
bypassSmoothRat.setTargetValue(pRatBypass->load() > 0.5f ? 0.0f : 1.0f);
for (int i = 0; i < numSamples; ++i)
{
    const float wet = bypassSmoothRat.getNextValue();
    mono[i] = mono[i] * wet + dryBuffer.getSample(0, i) * (1.0f - wet);
}
```

### Pattern 4: SmoothedValue for Float Parameter Smoothing (PARAMS-04)

**What:** For all float parameters consumed in processBlock (drive, depth, mix, etc.), wrap in `juce::SmoothedValue<float>`. Set target each block from the cached atomic. Advance per-sample.
**When to use:** Every float parameter that processBlock reads and passes to DSP stages.

This is the pattern PARAMS-04 requires. The agent starter implements it for bypass toggles and global mix but shows raw `.load()` calls for stage parameters (because the stage classes do their own internal smoothing in the agent design). For Phase 2 (stub stages), the processor-level smoothers serve as the smoothing infrastructure until real DSP stages implement per-sample smoothing in later phases.

**Decision:** Phase 2 should implement `juce::SmoothedValue<float>` members in PluginProcessor for all float params that processBlock reads directly (trims, global mix). For parameters passed to stage `setParameters()` structs, the stage classes in later phases will implement their own per-sample smoothing. The bypass SmoothedValues are already complete in the agent starter.

### Pattern 5: Oversampling at Plugin Boundary

**What:** `juce::dsp::Oversampling<float>` instantiated in PluginProcessor (not in stage classes). `processSamplesUp()` called before Stage 1; `processSamplesDown()` called after Stage 4.
**When to use:** Once, wrapping the entire DSP chain.

```cpp
// In PluginProcessor.h private section:
juce::dsp::Oversampling<float> oversampler {
    1,   // numChannels (mono at boundary)
    2,   // factor (2^2 = 4x)
    juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
    true // maximise quality
};

// In prepareToPlay:
oversampler.initProcessing(samplesPerBlock);

// In processBlock (simplified):
auto oversampledBlock = oversampler.processSamplesUp(monoBlock);
// ... run stages on oversampledBlock at 4x rate ...
oversampler.processSamplesDown(monoBlock);
```

**Critical detail:** The oversampler operates on a `juce::dsp::AudioBlock<float>`, not directly on an `AudioBuffer`. The agent starter processBlock uses `AudioBlock` wrappers for the input limiter — the oversampling integration should follow the same pattern. After downsampling, the buffer is back at host sample rate.

**Buffer channel count complexity:** The chain goes mono (input) → mono (RAT) → stereo (MicroPitch onward). The oversampler is applied at the mono boundary. WR-04 (see below) requires using a pre-allocated working buffer for the stereo expansion, not `buffer.setSize()` on the host buffer.

### Pattern 6: DC Blocking Filter

**What:** First-order highpass using `juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 5.0f)`. Applied per-sample via `filter.processSample(x)`.

```cpp
// In prepareToPlay:
const auto dcCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 5.0f);
dcBlock1L.prepare(monoSpec);
dcBlock1L.coefficients = dcCoeffs;

// In processBlock (mono, post-RAT):
for (int i = 0; i < numSamples; ++i)
    mono[i] = dcBlock1L.processSample(mono[i]);
```

**Filter count:** 3 post-RAT (mono — dcBlock1L only; dcBlock1R is dead weight per IN-02), post-MicroPitch (stereo — dcBlock2L, dcBlock2R), post-Undulator (stereo — dcBlock3L, dcBlock3R). Total: 5 live filters.

### Pattern 7: Per-Sample Feedback Interpolation (WR-03 Fix)

**What:** Replace block-rate constant injection with linear interpolation from `previousFeedbackSample` to `feedbackSample` across the block.

```cpp
// Source: 01-REVIEW.md WR-03 fix
// Add to PluginProcessor.h private section:
float previousFeedbackSample = 0.0f;

// In processBlock, feedback injection:
const float fbAmount = (pGlobalFeedbackAmount->load() / 100.0f) * 0.15f;
const float fbStart  = previousFeedbackSample * fbAmount;
const float fbEnd    = feedbackSample * fbAmount;
for (int i = 0; i < numSamples; ++i)
{
    const float alpha = static_cast<float>(i) / static_cast<float>(numSamples);
    mono[i] += fbStart + alpha * (fbEnd - fbStart);
}
previousFeedbackSample = feedbackSample;
```

### Anti-Patterns to Avoid

- **`apvts.getRawParameterValue()` inside `processBlock`:** String hash lookup on every call. Cache in constructor via `cacheParameterPointers()` instead.
- **`buffer.setSize()` in `processBlock`:** May touch host-owned memory; non-standard for expanding channel count. Use pre-allocated `workBuffer` (stereo, allocated in `prepareToPlay`) instead. [WR-04]
- **Constant block-rate feedback injection:** Injects a step at each block boundary at 86–344Hz (audible buzz at 128–512 sample blocks). Interpolate across the block. [WR-03]
- **Reading `persistedTemp` from state unconditionally:** Silently loads stale thermal state even when BURNIN_PERSIST is false. Gate behind persist check. [WR-05]
- **Per-sample filter coefficient updates:** Expensive and produces zippering. Update coefficients once per block from smoothed values; apply coefficients per-sample.
- **Any allocation in `processBlock`:** `std::vector`, `AudioBuffer` constructor, `new` — all forbidden. Pre-allocate in `prepareToPlay`.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Polyphase FIR oversampling | Custom FIR filter bank | `juce::dsp::Oversampling<float>` | Spec explicitly permits this one JUCE DSP module; correct anti-aliasing, tested |
| Parameter smoothing | One-pole IIR in processBlock | `juce::SmoothedValue<float>` | Thread-safe, handles ramp correctly, no allocation |
| First-order IIR HP/LP | Direct DF1 implementation | `juce::dsp::IIR::Filter` + `Coefficients::makeHighPass/makeLowPass` | Coefficient stability, denormal handling |
| dB to linear conversion | `pow(10.0f, dB/20.0f)` | `juce::Decibels::decibelsToGain()` | Correct, handles -∞ dB edge case |
| Input waveshaping | Custom tanh loop | `juce::dsp::WaveShaper<float>` | Handles AudioBlock correctly; use `FastMathApproximations::tanh` as the function |
| APVTS state XML | Custom XML serialization | `apvts.copyState()` + `createXml()` | Built-in, correct, survives APVTS internals changes |

**Key insight:** All DSP in Phase 2 is infrastructure, not signal processing. Every item in this table is solved by JUCE. The "from-scratch DSP" rule applies to the four stage algorithms (Phases 3–6), not to the plugin plumbing.

---

## Key Discrepancy: Source Files vs. Agent Starter Files

This is the most important research finding. There are two sets of source files:

1. **`cpp files for agent/`** — The authoritative implementation files provided as target. Contain complete, working implementations of Parameters.cpp, PluginProcessor.h, and PluginProcessor.cpp.

2. **`Source/`** — The current live source tree. `Source/Parameters.h` and `Source/Parameters.cpp` are **identical** to the agent starter versions. `Source/PluginProcessor.h` and `Source/PluginProcessor.cpp` are also **identical** to the agent starter versions.

**Implication:** Phase 1 already transplanted the agent starter files into Source/. The current live codebase IS the agent starter implementation. Phase 2 does not need to transplant files — it needs to:

1. Fix WR-01 through WR-05 identified in Phase 1 review
2. Implement `juce::dsp::Oversampling` to replace the pass-through `Oversampler` stub
3. Implement `SmoothParam` in `Source/utils/SmoothParam.h` (currently a no-op stub)
4. Implement the full `juce::SmoothedValue` infrastructure for all float params (bypass smoothers are done; stage float params are currently passed raw via `.load()`)

**The agent starter `PluginProcessor.cpp` calls `stageTurboRat->process(buffer, numSamples)` etc. — these DSP stages are pass-through stubs.** Phase 2 keeps them as stubs. The infrastructure (oversampling, DC block, bypass crossfade, smoothing) must work correctly around them.

---

## WR Fixes Required in Phase 2

All five WR findings from 01-REVIEW.md must be addressed:

### WR-01: Delete unused `bypassRamp` variable
**File:** `Source/PluginProcessor.cpp:93-94`
**Fix:** Delete the two lines computing `bypassRamp`. Smoothers are already initialized correctly via `reset(sampleRate, 0.01)`.

### WR-02: Delete unused `dcCoeff` variable
**File:** `Source/PluginProcessor.cpp:72-73`
**Fix:** Delete the two lines computing `dcCoeff`. DC filter uses `makeHighPass()` result, not the manual approximation.

### WR-03: Per-sample interpolation for global feedback injection
**File:** `Source/PluginProcessor.cpp:167-168`, `Source/PluginProcessor.h`
**Fix:** Add `float previousFeedbackSample = 0.0f;` to PluginProcessor.h private section. Replace the constant injection loop with linear interpolation across the block. Update `previousFeedbackSample` at end of injection.

### WR-04: Pre-allocated working buffer for stereo expansion
**File:** `Source/PluginProcessor.cpp:228`, `Source/PluginProcessor.h`
**Fix:** Add `juce::AudioBuffer<float> workBuffer;` to PluginProcessor.h private section (alongside `dryBuffer`). In `prepareToPlay`, allocate `workBuffer.setSize(2, samplesPerBlock, false, true, false)`. In `processBlock`, copy mono input into workBuffer channels 0 and 1, process in-place, then copy stereo result to `buffer` using `buffer.setSize(2, ...)` on a member-owned buffer rather than the host's buffer. **Alternatively** — and more correctly — keep `buffer.setSize(2, numSamples, true, false, true)` on the `workBuffer` member, not on the host-provided `buffer`. The mono→stereo expansion happens into `workBuffer`, and the final stereo output is written back into the host `buffer` (which must already be stereo from the host perspective given the bus layout declares stereo output).

**Note on WR-04:** The bus layout declares `withOutput("Output", juce::AudioChannelSet::stereo(), true)` — JUCE guarantees the host provides a stereo buffer. The `buffer.setSize(2, ...)` call in the existing code may work in practice because the host buffer is already stereo. But calling `setSize` to change channel count mid-processBlock on a host-owned buffer is still non-standard. The safe pattern is to allocate `workBuffer` as a stereo member, copy mono input into channel 0, process, let MicroPitch write stereo into workBuffer, then `buffer.copyFrom()` result back. This is the approach to implement.

### WR-05: Gate thermal state restoration behind persist flag
**File:** `Source/PluginProcessor.cpp:506-512`
**Fix:** In `setStateInformation`, read `persistedTemp`/`persistedTempPrev` only when `pBurninPersist->load() > 0.5f`. Otherwise set member fields to 0.0f.

---

## Oversampling Implementation Details

`juce::dsp::Oversampling<float>` setup in `PluginProcessor.h` private section:

```cpp
// 1 channel (mono at boundary), factor order 2 (2^2 = 4x), polyphase IIR
juce::dsp::Oversampling<float> oversampler {
    1u,
    2u,
    juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
    true,   // maximise quality
    false   // do not interleave channels
};
```

In `prepareToPlay`:
```cpp
oversampler.reset();
oversampler.initProcessing(static_cast<size_t>(samplesPerBlock));
```

In `processBlock`, the oversampling wraps the waveshaper stages (RAT, Undulator chip grit, BurnIn oxide). MicroPitch runs at native rate since it is a linear pitch-shift stage with no nonlinearity.

**Signal flow at oversampling boundary:**
1. Mono input → `processSamplesUp()` → 4× rate mono block
2. TurboRat processes at 4× rate
3. MicroPitch runs at native rate (or on the downsampled output — see note below)
4. Undulator chip grit runs at 4× rate (Undulator LFO/delay runs at native rate)
5. BurnIn oxide saturation runs at 4× rate
6. `processSamplesDown()` → back to host rate

**Practical simplification for Phase 2 (stub stages):** Since all four stage classes are pass-throughs in Phase 2, the oversampler can be applied at the plugin boundary (wrapping the whole chain). Stages 3 and 4 manage their own internal oversampling in Phases 5 and 6. For Phase 2, implement a single oversampler in PluginProcessor that wraps the entire processBlock body (excluding global mix blend which happens post-downsample). This is consistent with the agent starter's comment "Runs at 4× oversampling internally" on each stage — the architecture intends each stage to manage its own 4× context, but for the Phase 2 skeleton, a single PluginProcessor-level oversampler is acceptable.

---

## SmoothParam Implementation

`Source/utils/SmoothParam.h` is currently a Phase 1 no-op stub. Phase 2 needs it as a real smoother for use in DSP stage classes (Phases 3–6 will use it). For Phase 2, the processor-level `juce::SmoothedValue<float>` members handle the smoothing that PARAMS-04 requires.

Phase 2 should implement `SmoothParam` properly so it is ready for Phase 3:

```cpp
struct SmoothParam
{
    float current = 0.f;
    float target  = 0.f;
    float coeff   = 0.f;  // one-pole IIR coefficient

    void setTimeMs(float ms, double sampleRate)
    {
        // one-pole LP: coeff = exp(-2π / (ms * sr / 1000))
        coeff = std::exp(-juce::MathConstants<float>::twoPi
                         / (static_cast<float>(ms * sampleRate / 1000.0)));
    }

    void setTarget(float v) { target = v; }

    float tick()
    {
        current = coeff * current + (1.f - coeff) * target;
        return current;
    }

    void reset(float v) { current = target = v; }
};
```

This is a standard one-pole lowpass smoother. [ASSUMED — the spec does not specify SmoothParam's algorithm, but one-pole LP at the specified time constant is the correct approach and consistent with what `juce::SmoothedValue` does internally]

---

## Common Pitfalls

### Pitfall 1: APVTS Group Separators in Constructor
**What goes wrong:** `std::make_unique<Group>("rat", "RAT", "|", ...)` — the separator character (third argument) is a display hint for the DAW. If omitted or passed wrong type, group construction fails at runtime.
**Why it happens:** Group constructor is variadic; separator must come before parameter unique_ptrs.
**How to avoid:** Copy the agent starter pattern exactly. Separator is `"|"` throughout.

### Pitfall 2: `SmoothedValue` Not Initialized Before First processBlock
**What goes wrong:** If `setCurrentAndTargetValue()` is not called in `prepareToPlay()`, the smoother starts at 0.0 and ramps up, causing a fade-in artifact on the first audio output.
**How to avoid:** After `reset(sampleRate, timeSeconds)`, always call `setCurrentAndTargetValue(desiredInitialValue)`.

### Pitfall 3: Host Buffer Channel Count Assumption
**What goes wrong:** `buffer.getNumChannels()` is 1 at Stage 1 (mono input bus) but 2 at Stage 2 onward (stereo output bus). JUCE may provide the buffer already as stereo (output bus drives the channel count). Calling `buffer.setSize(2, ...)` on the host buffer to "expand" it is undefined in hosts that provide a fixed-size buffer.
**How to avoid:** [WR-04] Use pre-allocated `workBuffer` for stereo processing. Write mono RAT output to `workBuffer.channel(0)`, let MicroPitch expand to stereo in `workBuffer`, then copy back to `buffer`.

### Pitfall 4: `juce::dsp::IIR::Filter` Not Reset Between prepareToPlay Calls
**What goes wrong:** If `prepareToPlay` is called multiple times (sample rate change), old filter state from the previous session persists. First audio output will contain stale filter state.
**How to avoid:** Call `dcBlock*.reset()` in `releaseResources()` (already done in agent starter) AND in `prepareToPlay()` after `prepare()` and coefficient assignment.

### Pitfall 5: `juce::dsp::Oversampling` Latency Not Reported
**What goes wrong:** `juce::dsp::Oversampling` adds latency (phase shift from the anti-aliasing filter). Hosts that support latency compensation need `getLatencySamples()` to return the correct value, or playback will be misaligned relative to dry signal.
**How to avoid:** Override `AudioProcessor::getLatencySamples()` to return `static_cast<int>(oversampler.getLatencyInSamples())`. Call `setLatencySamples()` in `prepareToPlay()`.
**Warning sign:** Dry/wet alignment sounds wrong in hosts with delay compensation enabled.

### Pitfall 6: `juce::dsp::Oversampling` Channel Count Mismatch
**What goes wrong:** `Oversampling<float>` is constructed with a fixed channel count. If the number of channels in the processed `AudioBlock` does not match, `processSamplesUp` may assert or silently process wrong data.
**How to avoid:** Construct oversampler with channel count matching the mono signal being upsampled (1 channel). Upsample before MicroPitch stereo expansion; downsample after. Alternatively, construct for 2 channels and pad, but 1-channel construction is cleaner.

### Pitfall 7: Feedback Scalar Applied Before Input Limiter Creates Headroom Issue
**What goes wrong:** The agent starter adds feedback to the mono input BEFORE the tanh input limiter. This is intentional (feedback is clipped by the limiter). But if `previousFeedbackSample` is not initialized to 0.0f, the first block injects garbage.
**How to avoid:** Initialize `feedbackSample = 0.0f` and `previousFeedbackSample = 0.0f` in `prepareToPlay()`.

---

## Code Examples

### APVTS with Parameter Groups
```cpp
// Source: cpp files for agent/Parameters.cpp (verified against Source/Parameters.cpp — identical)
Layout createParameterLayout()
{
    Layout layout;

    auto ratGroup = std::make_unique<Group> ("rat", "RAT", "|",
        std::make_unique<Float> (
            pid(RAT_DRIVE), "Drive",
            skewed(0.0f, 100.0f, 0.1f, 0.7f), 72.0f,
            Attr().withLabel("%")),
        // ... more params
    );
    layout.add(std::move(ratGroup));

    // Acetate mode — standalone bool, not in a group
    layout.add(std::make_unique<Bool>(pid(ACETATE_MODE), "Acetate Mode", false));

    return layout;
}
```

### juce::dsp::Oversampling Wiring
```cpp
// In PluginProcessor.h private section:
juce::dsp::Oversampling<float> oversampler {
    1u,  // numChannels
    2u,  // factor (2^2 = 4x)
    juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
    true, false
};

// In prepareToPlay:
oversampler.reset();
oversampler.initProcessing(static_cast<size_t>(samplesPerBlock));
setLatencySamples(static_cast<int>(oversampler.getLatencyInSamples()));

// In processBlock (mono phase):
juce::dsp::AudioBlock<float> monoBlock (buffer.getArrayOfWritePointers(), 1, numSamples);
auto oversampledBlock = oversampler.processSamplesUp(monoBlock);
// ... process oversampledBlock at 4x rate ...
oversampler.processSamplesDown(monoBlock);
```

### WR-05 Fix in setStateInformation
```cpp
// Source: 01-REVIEW.md WR-05 fix
const bool persistEnabled = (pBurninPersist->load() > 0.5f);
persistedTemp     = persistEnabled
    ? static_cast<float>(newState.getProperty("persistedTemp",     0.0f))
    : 0.0f;
persistedTempPrev = persistEnabled
    ? static_cast<float>(newState.getProperty("persistedTempPrev", 0.0f))
    : 0.0f;

if (persistEnabled)
    stageBurnIn->setPersistedTemp(persistedTemp, persistedTempPrev);
```

---

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Manual per-sample one-pole LP for smoothing | `juce::SmoothedValue<float>` | JUCE 5+ | Thread-safe, correct ramp semantics |
| Separate `juce::dsp::Oversampling` for each DSP stage | Single oversampler at plugin boundary (per agent starter architecture) | Project decision | Simpler, stages focus on DSP not infrastructure |
| `apvts.getParameter(id)->getValue()` in processBlock | `apvts.getRawParameterValue(id)` cached as atomic<float>* | JUCE 5+ | Hash lookup eliminated from realtime path |
| `buffer.setSize()` to expand mono→stereo | Pre-allocated stereo workBuffer copied in/out | WR-04 fix | Avoids host buffer mutation |

---

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | SmoothParam should use one-pole IIR with `exp(-2π / (ms * sr / 1000))` | SmoothParam Implementation | If the spec intended a different algorithm (e.g., linear ramp), the phase and frequency response of smoothing will differ — but audible impact is negligible at 20ms |
| A2 | Oversampler can be implemented at the PluginProcessor level for Phase 2, with each stage managing its own oversampling in later phases | Oversampling Implementation Details | If the architecture requires stage-level oversamplers from Phase 2, the processBlock structure is more complex; but the agent starter comments ("Runs at 4× oversampling internally") support stage-level management starting from Phase 3 |

---

## Open Questions

1. **Oversampler latency compensation scope**
   - What we know: `juce::dsp::Oversampling` adds measurable latency; `getLatencyInSamples()` returns it
   - What's unclear: Phase 2 ROADMAP does not mention latency reporting; Phase 9 (DAW integration) success criteria do not mention it
   - Recommendation: Implement `setLatencySamples(oversampler.getLatencyInSamples())` in `prepareToPlay()` in Phase 2 — it is a one-liner and required for correct DAW dry/wet alignment

2. **WorkBuffer channel count for WR-04**
   - What we know: Host provides stereo output buffer; mono processing through Stage 1; MicroPitch expands to stereo
   - What's unclear: Whether to pre-allocate `workBuffer` as 1-channel then resize per block, or always 2-channel
   - Recommendation: Pre-allocate `workBuffer` as 2-channel stereo in `prepareToPlay`. Copy mono input into channel 0 for RAT processing. After MicroPitch, channel 0 and 1 are populated. Copy workBuffer back to host `buffer`. This avoids any resize during processBlock.

3. **`getLastLfoValue()` dead call in processBlock (IN-01)**
   - What we know: `stageMicroPitch->getLastLfoValue()` is called before Undulator but return value is discarded
   - What's unclear: Whether Phase 2 should remove this call or leave it as a placeholder for Phase 4's cross-feed design
   - Recommendation: Replace with a comment describing the intended cross-feed design. Remove the actual call — it reads `0.f` from the stub and does nothing.

---

## Environment Availability

Step 2.6: SKIPPED — Phase 2 is purely C++ source code changes. No external CLI tools, databases, or services required beyond the existing JUCE CMake build that was verified passing in Phase 1.

---

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | Manual build + auval + DAW load (no automated unit test framework in project) |
| Config file | None |
| Quick run command | `cmake --build build --config Release 2>&1 | tail -20` |
| Full suite command | `cmake --build build --config Release && auval -v aufx HDTH TKHA` |

### Phase Requirements → Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| PARAMS-01 | All parameters appear in DAW automation lane with correct IDs | smoke | `auval -v aufx HDTH TKHA` | N/A — auval |
| PARAMS-02 | No APVTS string lookup in processBlock | manual | Code review: grep for `getRawParameterValue` in processBlock | N/A |
| PARAMS-03 | Parameter groups show as RAT/MicroPitch/Undulator/Burn-In/Global in DAW | smoke | DAW load + inspect lanes | N/A |
| PARAMS-04 | No clicks on parameter change | manual | Play audio, sweep knobs, listen | N/A |
| CHAIN-01 | Plugin accepts mono input | smoke | `auval -v aufx HDTH TKHA` | N/A |
| CHAIN-02 | Build compiles with Oversampling wired | build | `cmake --build build --config Release` | N/A |
| CHAIN-03 | No DC offset accumulation after 10s sine | manual | Sine → plugin → measure output DC | N/A |
| CHAIN-04 | Input limiter clips at ~0dBFS | manual | Feed +12dBFS tone, verify limited | N/A |
| CHAIN-05 | Bypass toggle produces no click | manual | Toggle bypass while playing, listen | N/A |
| CHAIN-06 | Feedback path active flag gates injection | manual | Enable feedback, hear sub-bass injection | N/A |
| CHAIN-07 | ±12dB trim controls change level | manual | Set trim +/-12, measure output | N/A |
| BUILD-04 | No allocation in processBlock | manual | Code review + AddressSanitizer | N/A |

### Phase Gate
Build green + auval pass + no audible clicks on parameter changes + no DC offset on sine test.

### Wave 0 Gaps
None — no automated test framework is used for this project at this stage.

---

## Security Domain

No network calls, no file I/O outside of JUCE state serialization (handled by JUCE's own XML APIs), no user-provided string parsing outside of APVTS (handled internally by JUCE). No ASVS categories apply to a local audio plugin with no network surface.

---

## Sources

### Primary (HIGH confidence)
- `cpp files for agent/Parameters.cpp` — authoritative parameter layout (complete implementation, verified identical to Source/Parameters.cpp)
- `cpp files for agent/PluginProcessor.h` — authoritative processor class (complete member declarations, verified identical to Source/PluginProcessor.h)
- `cpp files for agent/PluginProcessor.cpp` — authoritative processBlock, prepareToPlay, cacheParameterPointers (complete implementation, verified identical to Source/PluginProcessor.cpp)
- `.planning/phases/01-project-scaffold/01-REVIEW.md` — WR-01 through WR-05 findings with exact fix descriptions

### Secondary (MEDIUM confidence)
- `heatdeath_agent_prompt.md` — signal chain architecture, stage interfaces, oversampling placement comments
- `heatdeath_vst_spec.md` — parameter ranges, DSP algorithm requirements
- `.planning/REQUIREMENTS.md` — PARAMS-01 through PARAMS-04, CHAIN-01 through CHAIN-07 requirements

### Tertiary (LOW confidence — training knowledge)
- `juce::dsp::Oversampling` API and `getLatencyInSamples()` method availability [ASSUMED — confirmed by pattern in agent starter but not verified against JUCE 7.0.12 docs this session]
- `SmoothParam` one-pole IIR algorithm [ASSUMED]

---

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — all libraries are JUCE built-ins already used in Phase 1 build
- Architecture: HIGH — agent starter files provide complete authoritative implementation; Source/ files confirmed identical
- Pitfalls: HIGH — WR findings are concrete with exact file:line locations; oversampling pitfalls from training knowledge (MEDIUM for those items)
- WR fixes: HIGH — 01-REVIEW.md provides exact code for each fix

**Research date:** 2026-04-09
**Valid until:** Phase 2 completion — no external dependencies with version churn risk
