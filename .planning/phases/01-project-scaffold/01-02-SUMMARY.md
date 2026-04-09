---
phase: 01-project-scaffold
plan: 02
subsystem: source-scaffold
tags: [cpp, juce, scaffold, dsp-stubs, ui-stubs]
dependency_graph:
  requires: [01-01]
  provides: [source-tree, dsp-headers, ui-headers, parameter-layout]
  affects: [02-core-infrastructure, 03-turbo-rat, 04-micropitch, 05-undulator, 06-tape-burnin, 07-ui-presets]
tech_stack:
  added: [JUCE AudioProcessorEditor, JUCE LookAndFeel_V4, JUCE AudioProcessorValueTreeState]
  patterns: [parameter-struct-per-stage, header-only-utils, verbatim-starter-copy]
key_files:
  created:
    - Source/Parameters.h
    - Source/Parameters.cpp
    - Source/PluginProcessor.h
    - Source/PluginProcessor.cpp
    - Source/PluginEditor.h
    - Source/PluginEditor.cpp
    - Source/dsp/TurboRat.h
    - Source/dsp/TurboRat.cpp
    - Source/dsp/MicroPitch.h
    - Source/dsp/MicroPitch.cpp
    - Source/dsp/Undulator.h
    - Source/dsp/H3000Undulator.cpp
    - Source/dsp/BurnIn.h
    - Source/dsp/TapeBurnIn.cpp
    - Source/dsp/Oversampler.h
    - Source/dsp/Oversampler.cpp
    - Source/ui/HeatdeathLookAndFeel.h
    - Source/ui/HeatdeathLookAndFeel.cpp
    - Source/ui/StagePanel.h
    - Source/ui/StagePanel.cpp
    - Source/utils/SmoothParam.h
    - Source/utils/BiquadFilter.h
  modified:
    - Source/PluginProcessor.cpp (include path fix + createEditor impl)
decisions:
  - DSP class names follow starter (TurboRat, MicroPitch, Undulator, BurnIn) not plan template names (H3000Undulator, TapeBurnIn)
  - H3000Undulator.cpp and TapeBurnIn.cpp reused as translation units for Undulator and BurnIn classes to match CMakeLists without changes
  - createEditor() added to PluginProcessor.cpp (declared in header but missing from starter .cpp)
  - DSP include paths changed from DSP/ (uppercase) to dsp/ (lowercase) to match filesystem
metrics:
  duration: 4 minutes
  completed: 2026-04-09
  tasks: 3
  files: 22
requirements: [SCAF-02]
---

# Phase 01 Plan 02: Source Tree Scaffold Summary

**One-liner:** Full Source/ tree with verbatim starter files (HeatDeathProcessor + 447-line APVTS layout) and empty-but-compilable DSP/UI/utils stubs matching actual processor method signatures.

## Tasks Completed

| # | Task | Commit | Files |
|---|------|--------|-------|
| 1 | Copy starter files into Source/ and create directory tree | 0727a41 | Parameters.h, Parameters.cpp, PluginProcessor.h, PluginProcessor.cpp |
| 2 | Stub DSP headers + .cpp files, Oversampler, and utils header-only files | c110ea1 | 12 DSP/utils files + PluginProcessor.cpp (include path fix) |
| 3 | Stub PluginEditor and UI headers/cpp | f262b2f | PluginEditor.h/cpp, HeatdeathLookAndFeel.h/cpp, StagePanel.h/cpp |

## DSP Class Contract (for downstream phases)

All DSP stage classes use a `Parameters` struct passed via `setParameters()` and a `process(AudioBuffer<float>&, int)` signature. No raw pointer `processBlock` patterns.

### TurboRat (Source/dsp/TurboRat.h)
```cpp
struct Parameters { float drive, filter, volume, slew, asym, sag; int clipMode; };
void prepare(double sampleRate, int samplesPerBlock);
void setParameters(const Parameters&);
void process(juce::AudioBuffer<float>& buffer, int numSamples);
void reset();
static constexpr float kDefaultSlew = 0.68f;
static constexpr float kDefaultAsym = 0.20f;
```

### MicroPitch (Source/dsp/MicroPitch.h)
```cpp
struct Parameters { float detuneL, detuneR, mix, width; };
void prepare(double sampleRate, int samplesPerBlock);
void setParameters(const Parameters&);
void process(juce::AudioBuffer<float>& buffer, int numSamples);
float getLastLfoValue() const;  // returns 0.f at Phase 1
void reset();
```

### Undulator (Source/dsp/Undulator.h, impl in H3000Undulator.cpp)
```cpp
struct Parameters { float rate, depth, phase, drift, modRate, modDepth, modSpeed,
                         spread, feedback, grit, mix; int shape; };
void prepare(double sampleRate, int samplesPerBlock);
void setParameters(const Parameters&);
void process(juce::AudioBuffer<float>& buffer, int numSamples);
void reset();
```

### BurnIn (Source/dsp/BurnIn.h, impl in TapeBurnIn.cpp)
```cpp
struct Parameters { float heatRate, timerProgress; bool freeze, acetate, msMode, timerActive; };
void prepare(double sampleRate, int samplesPerBlock);
void setParameters(const Parameters&);
void process(juce::AudioBuffer<float>& buffer, int numSamples);
void setPersistedTemp(float temp, float tempPrev);
float getCurrentTemp() const;   // returns 0.f at Phase 1
float getPreviousTemp() const;  // returns 0.f at Phase 1
void reset();
```

### Oversampler (Source/dsp/Oversampler.h)
```cpp
void prepare(double sampleRate, int samplesPerBlock);
void upsample(juce::AudioBuffer<float>&);
void downsample(juce::AudioBuffer<float>&);
int getOversampleFactor() const;  // returns 1 at Phase 1
void reset();
```

## Processor and Editor Class Names

- Processor: `HeatDeathProcessor` (in PluginProcessor.h/.cpp)
- Editor: `HeatDeathEditor` (in PluginEditor.h/.cpp)
- APVTS state ID: `"HEATDEATH_STATE"`
- Plugin entry point: `createPluginFilter()` → `new HeatDeathProcessor()`

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] DSP include paths used uppercase `DSP/` — filesystem has lowercase `dsp/`**
- **Found during:** Task 2 analysis of starter PluginProcessor.cpp
- **Issue:** Starter uses `#include "DSP/TurboRat.h"` but target directory is `Source/dsp/` (lowercase). Would fail to compile on case-sensitive filesystems and is ambiguous on macOS.
- **Fix:** Patched PluginProcessor.cpp includes to use lowercase `dsp/`
- **Files modified:** Source/PluginProcessor.cpp
- **Commit:** c110ea1

**2. [Rule 3 - Blocking] DSP class names differ from plan stub templates**
- **Found during:** Task 2 — cross-reading starter PluginProcessor.cpp vs plan interfaces block
- **Issue:** Plan template named stubs `H3000Undulator` and `TapeBurnIn`, but the starter processor uses `Undulator` and `BurnIn` (forward-declared in PluginProcessor.h). Created `Undulator.h` and `BurnIn.h` as the actual headers; reused `H3000Undulator.cpp` and `TapeBurnIn.cpp` as their translation units (CMakeLists unchanged).
- **Fix:** Created Undulator.h + BurnIn.h; impl placed in existing CMakeLists-referenced .cpp files
- **Files modified:** Source/dsp/Undulator.h (new), Source/dsp/BurnIn.h (new), Source/dsp/H3000Undulator.cpp, Source/dsp/TapeBurnIn.cpp
- **Commit:** c110ea1

**3. [Rule 2 - Missing] `createEditor()` declared in PluginProcessor.h but not implemented in starter .cpp**
- **Found during:** Task 3 — grep showed no createEditor() in starter PluginProcessor.cpp
- **Issue:** Pure virtual override from AudioProcessor — without an implementation the linker will fail
- **Fix:** Added `HeatDeathProcessor::createEditor()` returning `new HeatDeathEditor(*this)` at end of PluginProcessor.cpp
- **Files modified:** Source/PluginProcessor.cpp
- **Commit:** f262b2f

**4. [Rule 3 - Blocking] DSP `setParameters()` uses struct initializer syntax — required Parameters struct**
- **Found during:** Task 2 — starter processBlock uses `stageTurboRat->setParameters({.drive=..., ...})`
- **Issue:** Plan interface block showed individual positional args; starter uses designated initializers requiring a named struct
- **Fix:** Each DSP class header defines a `Parameters` struct with named fields matching the initializer keys
- **Files modified:** All DSP headers
- **Commit:** c110ea1

## Known Stubs

All DSP `process()` methods are intentional pass-throughs for Phase 1. These are tracked by design:

| Stub | File | Reason |
|------|------|--------|
| TurboRat::process() no-op | Source/dsp/TurboRat.cpp | Phase 3 implements LM308 model |
| MicroPitch::process() no-op | Source/dsp/MicroPitch.cpp | Phase 4 implements SSB pitch shift |
| Undulator::process() no-op | Source/dsp/H3000Undulator.cpp | Phase 5 implements dual-LFO AM/FM |
| BurnIn::process() no-op | Source/dsp/TapeBurnIn.cpp | Phase 6 implements Jiles-Atherton |
| BurnIn::getCurrentTemp() returns 0 | Source/dsp/BurnIn.h | Phase 6 wires thermal state |
| MicroPitch::getLastLfoValue() returns 0 | Source/dsp/MicroPitch.h | Phase 4/5 wires cross-feed |
| HeatDeathEditor::paint() placeholder text | Source/PluginEditor.cpp | Phase 7 wires real UI |

## Self-Check: PASSED

All files exist and all commits are in git log:

- 0727a41: feat(01-02): copy starter files verbatim into Source/
- c110ea1: feat(01-02): create DSP/utils stubs matching starter PluginProcessor.cpp method signatures
- f262b2f: feat(01-02): stub PluginEditor + UI classes, wire createEditor() to processor
