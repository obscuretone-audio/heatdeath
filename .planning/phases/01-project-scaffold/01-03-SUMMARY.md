---
phase: 01-project-scaffold
plan: 03
subsystem: build-system
tags: [cmake, build, auval, validation, juce, universal-binary]
dependency_graph:
  requires: [01-01, 01-02]
  provides: [built-vst3-bundle, built-au-component, auval-pass]
  affects: []
tech_stack:
  added: []
  patterns: [Apple clang + MacOSX15.2.sdk + CXXFLAGS include path workaround]
key_files:
  created:
    - .planning/phases/01-project-scaffold/build-artifacts.log
  modified:
    - Source/PluginProcessor.cpp
    - Source/PluginProcessor.h
decisions:
  - "Build with Apple clang (not Homebrew LLVM 22) to avoid object format mismatch with Apple linker/ranlib/lipo"
  - "Use MacOSX15.2.sdk via CXXFLAGS=-I...sdk/usr/include/c++/v1 — Apple clang 16 lacks stdlib headers without explicit SDK path"
  - "Patch juce_AudioPluginInstance.h: change array-pointer to array-ref in template constructor (JUCE 7.0.12 + Clang 22 compat bug)"
  - "BusesProperties protected in JUCE 7 — moved bus config inline into AudioProcessor() initializer list"
  - "getTimerProgress() was defined in .cpp but not declared in .h private section — added declaration"
metrics:
  duration: "~3 hours (multiple toolchain troubleshooting iterations)"
  completed: "2026-04-09"
  tasks_completed: 2
  tasks_total: 3
  files_modified: 3
---

# Phase 01 Plan 03: Build + Validate Summary

**One-liner:** cmake Release build succeeds, VST3+AU universal binary produced, auval PASS for aufx HDTH TKHA with mono-in/stereo-out confirmed.

## Build Command and Result

```
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
  -DCMAKE_C_COMPILER=/usr/bin/clang \
  -DCMAKE_OSX_SYSROOT=/Library/Developer/CommandLineTools/SDKs/MacOSX15.2.sdk
(with CXXFLAGS=-I/Library/Developer/CommandLineTools/SDKs/MacOSX15.2.sdk/usr/include/c++/v1)

cmake --build build --config Release -j
```

Result: SUCCESS — no errors, warnings only (JUCE internal warnings, designated initializer C++20 extension notes).

## auval Result

```
AU VALIDATION SUCCEEDED.
```

Full command: `auval -v aufx HDTH TKHA`

Bus layout confirmed:
- Input/Output Channel Handling: `1-2` marked `X` (mono in, stereo out)
- Input: 1 ch, 44100 Hz, Float32
- Output: 2 ch, 44100 Hz, Float32, deinterleaved

## pluginval Result

NOT INSTALLED — skipped per plan instructions.

## Exact Bundle Paths

- `build/HEATDEATH_artefacts/Release/VST3/HEATDEATH.vst3`
- `build/HEATDEATH_artefacts/Release/AU/HEATDEATH.component`
- Installed to: `~/Library/Audio/Plug-Ins/VST3/HEATDEATH.vst3`
- Installed to: `~/Library/Audio/Plug-Ins/Components/HEATDEATH.component`

## Architecture

```
Architectures in the fat file: ... are: x86_64 arm64
```

Universal binary confirmed (arm64 + x86_64).

## DAW Verification (Task 3 — PENDING)

Awaiting human verification — Task 3 checkpoint not yet passed.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Fixed BusesProperties protected member access**
- **Found during:** Task 1 — first build attempt
- **Issue:** `Source/PluginProcessor.cpp` used a file-scope static helper returning `juce::AudioProcessor::BusesProperties` — this struct is `protected` in JUCE 7.0.12, not accessible from outside a subclass
- **Fix:** Removed static `getDefaultBuses()` helper; moved bus configuration inline into `AudioProcessor(BusesProperties()...)` call in `HeatDeathProcessor` constructor initializer list
- **Files modified:** `Source/PluginProcessor.cpp`
- **Commit:** 072f1b6

**2. [Rule 1 - Bug] Added missing `getTimerProgress()` declaration to PluginProcessor.h**
- **Found during:** Task 1 — first build attempt
- **Issue:** `getTimerProgress()` is defined in `PluginProcessor.cpp` and called in `processBlock`, but was never declared in `PluginProcessor.h`'s private section — Clang error: "use of undeclared identifier" and "out-of-line definition does not match any declaration"
- **Fix:** Added `float getTimerProgress() const;` to the `private:` section of `PluginProcessor.h`
- **Files modified:** `Source/PluginProcessor.h`
- **Commit:** 072f1b6

**3. [Rule 3 - Blocking] Patched JUCE 7.0.12 template constructor array-pointer decay bug (Clang 22)**
- **Found during:** Task 1 — build with Homebrew LLVM 22
- **Issue:** `juce_AudioPluginInstance.h` line 164 had `const short channelLayoutList[numLayouts][2]` (pointer decay) as template constructor argument; Clang 22 could not match this to `AudioProcessor(const short (*)[2])` — "no matching constructor" error
- **Fix:** Changed to array-reference syntax: `const short (&channelLayoutList)[numLayouts][2]`
- **Files modified:** `build/_deps/juce-src/modules/juce_audio_processors/processors/juce_AudioPluginInstance.h` (in-tree, regenerated on each clean build)
- **Commit:** 072f1b6
- **Note:** This patch must be re-applied after any `rm -rf build` + reconfigure since FetchContent clones a fresh JUCE copy. CMakeLists.txt should add a cmake patch step to make this durable (tracked below).

**4. [Rule 3 - Blocking] Toolchain: Homebrew LLVM 22 incompatible with Apple system tools**
- **Found during:** Task 1 — initial build attempt using Homebrew LLVM 22 as directed by STATE.md
- **Issue:** Homebrew LLVM 22 emits object files in a format that Apple CommandLineTools `lipo`, `ranlib`, and `ar` (from CLT 16/macOS 14) cannot read. Errors: "Invalid attribute group entry", "archive member not a mach-o file". The `lld` linker is not bundled with Homebrew LLVM on macOS.
- **Fix:** Switched to Apple clang 16 (`/usr/bin/clang++`) with `CMAKE_OSX_SYSROOT=/Library/Developer/CommandLineTools/SDKs/MacOSX15.2.sdk` and `CXXFLAGS=-I.../MacOSX15.2.sdk/usr/include/c++/v1`. The MacOSX15.2.sdk (present on this machine) contains complete C++ stdlib headers that Apple clang 16 can use.
- **Impact:** Universal binary (arm64 + x86_64) still produced correctly — Apple clang + lipo handles fat binaries natively.
- **Commit:** 072f1b6

### Deferred Items

- The JUCE `juce_AudioPluginInstance.h` patch is not durable across clean builds. A cmake `patch` command or ExternalProject mechanism should be added to CMakeLists.txt to apply it automatically. Tracked in deferred-items.md.
- STATE.md notes "Homebrew LLVM required" — this decision should be updated to document the Apple clang workaround.

## Known Stubs

Phase 1 is a pure scaffold — all DSP stages (`TurboRat`, `MicroPitch`, `Undulator`, `BurnIn`) are empty pass-throughs by design. This is intentional — phases 3-6 implement each stage. The plugin produces silence through the chain (no audio modification), which auval verified is stable.

## Threat Flags

None — no new network endpoints, auth paths, or file access patterns introduced beyond what the plan's threat model covers.

## Self-Check: PENDING

(Full self-check runs after Task 3 checkpoint is resolved.)
