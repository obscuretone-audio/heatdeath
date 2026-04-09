---
phase: 02-core-infrastructure
reviewed: 2026-04-09T00:00:00Z
depth: standard
files_reviewed: 6
files_reviewed_list:
  - CMakeLists.txt
  - Source/Parameters.cpp
  - Source/PluginProcessor.cpp
  - Source/PluginProcessor.h
  - Source/utils/SmoothParam.h
  - Tests/SmoothParamTest.cpp
findings:
  critical: 0
  warning: 2
  info: 4
  total: 6
status: issues_found
---

# Phase 02: Code Review Report

**Reviewed:** 2026-04-09
**Depth:** standard
**Files Reviewed:** 6
**Status:** issues_found

## Summary

Reviewed the core infrastructure layer: build system, parameter layout, plugin processor
(construction, prepare, process chain, state serialisation), the `SmoothParam` utility, and
its unit tests.

The parameter layout (`Parameters.cpp`) is clean and well-structured. `SmoothParam.h` is
correct — the one-pole IIR formula, edge-case guards (zero ms, zero sample rate), and the
`reset` path all behave as documented. The unit tests are sound and will catch regressions.

Two functional bugs were found in `PluginProcessor.cpp`, both in the global feedback path.
Neither causes a crash or data loss, but the feedback filter's behaviour does not match its
stated design intent (100 Hz lowpass). The remaining four findings are code-quality /
encapsulation issues that carry a risk of introducing threading bugs in later phases when
editor interaction is wired up.

---

## Warnings

### WR-01: feedbackLpf driven with block average instead of per-sample data

**File:** `Source/PluginProcessor.cpp:471-477`

**Issue:** The IIR lowpass filter (`feedbackLpf`, 100 Hz cutoff) is intended to smooth the
feedback signal at sample resolution. However, the code first sums the entire block to a
scalar average and then calls `processSample` exactly once per block:

```cpp
float fbSum = 0.0f;
for (int i = 0; i < numSamples; ++i)
    fbSum += (L[i] + R[i]) * 0.5f;

fbSum /= static_cast<float> (numSamples);
feedbackSample = feedbackLpf.processSample (fbSum);  // one tick per block
```

`processSample` advances the filter's internal state by one sample. Because it is called once
per block (not once per sample), the filter is effectively clocked at the block rate
(e.g., ~86 Hz at 512 samples / 44100 Hz) rather than at the sample rate. The filter's actual
pole position does not correspond to the 100 Hz coefficient computed at sample rate. At typical
block sizes the effective cutoff is far below 100 Hz and varies with block size.

**Fix:** Capture only the last sample of the block (or run the filter per-sample across the
block) so it is clocked at the sample rate as the coefficient expects:

```cpp
// Option A — hold last sample (simplest; the LPF smooths between blocks):
{
    const auto* L = buffer.getReadPointer (0);
    const auto* R = buffer.getReadPointer (1);
    for (int i = 0; i < numSamples; ++i)
        feedbackSample = feedbackLpf.processSample ((L[i] + R[i]) * 0.5f);
    feedbackSample = juce::jlimit (-1.0f, 1.0f, feedbackSample);
}

// Option B — keep the average but bypass the per-sample IIR entirely and
// use a block-rate one-pole smoother with a coefficient recomputed at block rate.
```

Option A matches the existing design intent (100 Hz pole at sample rate) with the least
change.

---

### WR-02: Feedback interpolation never reaches fbEnd (off-by-one at block boundary)

**File:** `Source/PluginProcessor.cpp:186-194`

**Issue:** The ramp from `fbStart` to `fbEnd` across the block uses:

```cpp
const float alpha = static_cast<float> (i) * invN;   // i in [0, numSamples-1]
mono[i] += fbStart + alpha * (fbEnd - fbStart);
```

`alpha` spans `[0, (numSamples-1)/numSamples]` — it never reaches `1.0`. The final sample
receives `fbStart + (N-1)/N * (fbEnd - fbStart)` rather than `fbEnd`. This leaves a residual
step of `(fbEnd - fbStart) / numSamples` at the block boundary, partially defeating the
interpolation's purpose of eliminating the step artefact.

**Fix:** Use `(i + 1) * invN` so the last sample lands exactly at `fbEnd`:

```cpp
const float alpha = static_cast<float> (i + 1) * invN;  // reaches 1.0 on last sample
mono[i] += fbStart + alpha * (fbEnd - fbStart);
```

Alternatively, store `fbEnd` as `previousFeedbackSample` after the loop so the next block's
`fbStart` equals the actual last-injected value — this compensates without changing the loop
arithmetic.

---

## Info

### IN-01: Public mutable audio-thread state on the processor

**File:** `Source/PluginProcessor.h:142-151`

**Issue:** `persistedTemp`, `persistedTempPrev`, and `timerElapsedSeconds` are `public`
non-atomic members. They are written from the audio thread (`updateTimerState`, lines 732-733)
and are intended to be read in `getStateInformation` (message thread). Any future editor code
that reads these to drive a display will have an unsynchronised data race.

```cpp
// currently public
float  persistedTemp     = 0.0f;
float  persistedTempPrev = 0.0f;
double timerElapsedSeconds = 0.0;
```

**Fix:** Move to `private`. Provide `const` accessors that return a snapshot via
`std::atomic` load or a lock, and document the threading contract. At minimum:

```cpp
private:
    std::atomic<float>  persistedTemp     { 0.0f };
    std::atomic<float>  persistedTempPrev { 0.0f };
    std::atomic<double> timerElapsedSeconds { 0.0 };

public:
    float  getPersistedTemp()     const noexcept { return persistedTemp.load(); }
    float  getPersistedTempPrev() const noexcept { return persistedTempPrev.load(); }
    double getTimerElapsed()      const noexcept { return timerElapsedSeconds.load(); }
```

---

### IN-02: All raw parameter pointers are public

**File:** `Source/PluginProcessor.h:81-134`

**Issue:** All 30+ `std::atomic<float>*` parameter pointers (e.g., `pRatDrive`, `pUndRate`)
are `public`. An editor component can call `pRatDrive->store(value)` directly, writing to the
raw atomic without going through the APVTS. This bypasses automation recording, undo history,
and parameter-change listeners. In the current phase the editor is a stub so there is no
immediate bug, but the public surface invites incorrect usage in future phases.

**Fix:** Move the parameter pointer members to `private`. The editor already has access to
`apvts` (which is intentionally public) and should use APVTS attachments or
`apvts.getParameter(id)->setValue(...)` for all writes. Read access from DSP stages that
legitimately need it (e.g., `StageTurboRat` reading from the processor) should go through a
narrow accessor or the `setParameters` struct pattern already used in `processBlock`.

---

### IN-03: CMakeLists.txt — SmoothParamTest JUCE defines are fragile

**File:** `CMakeLists.txt:106-109`

**Issue:** The standalone `SmoothParamTest` target manually defines
`JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1` and `JUCE_MODULE_AVAILABLE_juce_core=1` to suppress
JUCE header guard warnings when linking only `juce::juce_core`. These are internal JUCE
preprocessor guards that are not part of its public API and may change between JUCE versions.
If they are removed or renamed in a future JUCE release the test target will break with cryptic
compile errors.

**Fix:** If the test target needs JUCE only for `juce::MathConstants`, consider replacing the
`MathConstants<double>::twoPi` usage in `SmoothParam.h` with `2.0 * M_PI` (or a local
`constexpr`) and removing the JUCE dependency from the test entirely. This also makes
`SmoothParam.h` usable without any JUCE include at all:

```cpp
// In SmoothParam.h — replace:
std::exp(-juce::MathConstants<double>::twoPi / samples)
// with:
std::exp(-6.283185307179586 / samples)
// or:
static constexpr double kTwoPi = 6.283185307179586;
std::exp(-kTwoPi / samples)
```

The test target then needs no JUCE link at all and no workaround defines.

---

### IN-04: Test Contract 3 comment misleads about why the assertion holds

**File:** `Tests/SmoothParamTest.cpp:85`

**Issue:** The test:

```cpp
float v = sp.tick();
check(v == 0.5f, "tick() after reset(0.5f) returns 0.5f (no ramp)");
```

passes because `reset(0.5f)` sets `current = target = 0.5f`, and `tick()` computes
`coeff * 0.5f + (1 - coeff) * 0.5f = 0.5f` — which equals `0.5f` regardless of `coeff`.
The comment "no ramp" implies `coeff == 0`, but the smoother still has `coeff ≈ 0.9929` from
the earlier `setTimeMs(20ms)` call. The test is correct but the assertion would also pass for
a broken implementation that never zeroes `coeff` on `reset`. A stronger check would be:

```cpp
sp.reset(0.5f);
check(sp.coeff > 0.0f, "reset() does not zero coeff");
sp.setTarget(1.0f);      // now ramp should resume
float v = sp.tick();
check(v > 0.5f && v < 1.0f, "tick() after reset+setTarget advances toward target");
```

This also verifies that `reset` does not accidentally zero `coeff` and that the ramp resumes
from the reset value.

---

_Reviewed: 2026-04-09_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
