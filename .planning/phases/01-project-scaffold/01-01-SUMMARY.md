---
phase: 01-project-scaffold
plan: "01"
subsystem: infra
tags: [cmake, juce, vst3, au, fetchcontent, c++17]

# Dependency graph
requires: []
provides:
  - JUCE 7.0.12 integrated via FetchContent (not vendored)
  - CMake project configured for VST3 and AU plugin targets on macOS
  - Plugin identity HEATDEATH / TKHA / HDTH declared in build system
  - .gitignore covering all JUCE/CMake build artifacts
  - Stub Source/ file tree for cmake configure step to pass
affects: [02-core-infrastructure, all downstream phases]

# Tech tracking
tech-stack:
  added: [JUCE 7.0.12 via FetchContent, CMake 3.22+, C++17, Homebrew LLVM (workaround)]
  patterns:
    - FetchContent pulls JUCE at configure time; no vendored JUCE files in repo
    - Plugin declared with juce_add_plugin; sources listed in target_sources
    - macOS universal binary via CMAKE_OSX_ARCHITECTURES arm64;x86_64
    - C++17 enforced with extensions OFF

key-files:
  created:
    - CMakeLists.txt
    - .gitignore
    - Source/PluginProcessor.cpp (stub)
    - Source/PluginEditor.cpp (stub)
    - Source/Parameters.cpp (stub)
    - Source/dsp/TurboRat.cpp (stub)
    - Source/dsp/MicroPitch.cpp (stub)
    - Source/dsp/H3000Undulator.cpp (stub)
    - Source/dsp/TapeBurnIn.cpp (stub)
    - Source/dsp/Oversampler.cpp (stub)
    - Source/ui/HeatdeathLookAndFeel.cpp (stub)
    - Source/ui/StagePanel.cpp (stub)
  modified: []

key-decisions:
  - "JUCE 7.0.12 pinned via GIT_TAG (not 'main') for reproducible builds"
  - "Homebrew LLVM required on this machine: Apple CommandLineTools clang lacks C++ stdlib headers"
  - "Stub source files added to allow cmake generate step to pass before Plan 01-02 writes real code"

patterns-established:
  - "CMake FetchContent for JUCE: all downstream phases must not vendor JUCE files"
  - "Plugin identity constants: PLUGIN_CODE HDTH, PLUGIN_MANUFACTURER_CODE TKHA — do not change"

requirements-completed: [SCAF-01, SCAF-03, SCAF-04]

# Metrics
duration: 5min
completed: 2026-04-09
---

# Phase 01 Plan 01: Project Scaffold — CMake / JUCE Setup Summary

**CMake project with JUCE 7.0.12 via FetchContent, VST3+AU targets (PLUGIN_CODE HDTH / TKHA), macOS universal binary, and stub Source/ tree to allow configure step to pass**

## Performance

- **Duration:** ~5 min
- **Started:** 2026-04-09T13:56:03Z
- **Completed:** 2026-04-09T14:01:00Z
- **Tasks:** 2 (+ 1 deviation fix)
- **Files modified:** 13

## Accomplishments

- CMakeLists.txt at project root: JUCE 7.0.12 pulled via FetchContent, no vendored JUCE files
- juce_add_plugin target with exact identity: PRODUCT_NAME "HEATDEATH", PLUGIN_CODE HDTH, PLUGIN_MANUFACTURER_CODE TKHA, FORMATS VST3 AU, no MIDI, no Standalone
- All 10 Source/ .cpp files referenced in target_sources; stub files created so cmake generate step passes
- .gitignore covers build/, _deps/, *.vst3, *.component, JuceLibraryCode/, .DS_Store

## Task Commits

1. **Task 1: Write CMakeLists.txt** - `a7423eb` (feat)
2. **Task 2: Write .gitignore** - `7f50c98` (chore)
3. **Deviation: Add stub source files** - `92b2b91` (chore)

## Files Created/Modified

- `CMakeLists.txt` — JUCE project: cmake_minimum_required 3.22, FetchContent JUCE 7.0.12, juce_add_plugin HEATDEATH, target_sources, target_compile_definitions, target_link_libraries
- `.gitignore` — Excludes build/, _deps/, *.vst3, *.component, JuceLibraryCode/, .DS_Store, IDE dirs
- `Source/PluginProcessor.cpp` — Single-line stub (Plan 01-02 will implement)
- `Source/PluginEditor.cpp` — Single-line stub
- `Source/Parameters.cpp` — Single-line stub
- `Source/dsp/TurboRat.cpp` — Single-line stub
- `Source/dsp/MicroPitch.cpp` — Single-line stub
- `Source/dsp/H3000Undulator.cpp` — Single-line stub
- `Source/dsp/TapeBurnIn.cpp` — Single-line stub
- `Source/dsp/Oversampler.cpp` — Single-line stub
- `Source/ui/HeatdeathLookAndFeel.cpp` — Single-line stub
- `Source/ui/StagePanel.cpp` — Single-line stub

## Decisions Made

- Pinned JUCE to 7.0.12 rather than a branch tag for reproducible builds
- Used Homebrew LLVM (`/opt/homebrew/opt/llvm/bin/clang++`) for cmake configure: the system Apple CommandLineTools clang 16.0.0 on this machine lacks C++ stdlib headers (`<algorithm>` not found), which is a broken local toolchain install — not a project issue. Future users on machines with full Xcode.app should use the default toolchain.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Added stub source files to unblock cmake generate step**
- **Found during:** Overall verification (cmake -B build -S .)
- **Issue:** CMake's `target_sources` requires all listed files to exist on disk at generate time. The plan listed 10 source files in `target_sources` but described them as "placeholder for sources Plan 01-02 will create." Without the files, cmake errors: "Cannot find source file: Source/PluginProcessor.cpp" and configure fails.
- **Fix:** Created 10 single-line stub .cpp files (one `// Stub — implementation in Plan 01-02` comment each) across Source/, Source/dsp/, and Source/ui/. Plan 01-02 will overwrite these with real implementations.
- **Files modified:** Source/PluginProcessor.cpp, Source/PluginEditor.cpp, Source/Parameters.cpp, Source/dsp/TurboRat.cpp, Source/dsp/MicroPitch.cpp, Source/dsp/H3000Undulator.cpp, Source/dsp/TapeBurnIn.cpp, Source/dsp/Oversampler.cpp, Source/ui/HeatdeathLookAndFeel.cpp, Source/ui/StagePanel.cpp
- **Verification:** cmake configure step completes with "Configuring done / Generating done / Build files have been written to: build/"
- **Committed in:** `92b2b91`

---

**Total deviations:** 1 auto-fixed (Rule 3 - blocking)
**Impact on plan:** Necessary to meet acceptance criteria; stub files carry no DSP code and will be fully replaced in Plan 01-02. No scope creep.

## Known Stubs

All 10 Source/ .cpp files are stubs. They are intentional placeholders — Plan 01-02 (`01-02-PLAN.md`) will replace them with real implementations.

| File | Stub content | Resolved by |
|------|-------------|-------------|
| Source/PluginProcessor.cpp | Comment only | Plan 01-02 |
| Source/PluginEditor.cpp | Comment only | Plan 01-02 |
| Source/Parameters.cpp | Comment only | Plan 01-02 |
| Source/dsp/TurboRat.cpp | Comment only | Plan 01-02 |
| Source/dsp/MicroPitch.cpp | Comment only | Plan 01-02 |
| Source/dsp/H3000Undulator.cpp | Comment only | Plan 01-02 |
| Source/dsp/TapeBurnIn.cpp | Comment only | Plan 01-02 |
| Source/dsp/Oversampler.cpp | Comment only | Plan 01-02 |
| Source/ui/HeatdeathLookAndFeel.cpp | Comment only | Plan 01-02 |
| Source/ui/StagePanel.cpp | Comment only | Plan 01-02 |

## Issues Encountered

- **Apple CommandLineTools clang missing C++ stdlib headers:** On this machine, `clang++` (CommandLineTools 16.0.0) cannot find `<algorithm>` or any C++ stdlib headers. This is a broken local install (the `c++/v1/` directory has only 3 legacy files). Workaround: use Homebrew LLVM (`/opt/homebrew/opt/llvm/bin/clang++`). cmake configure passes cleanly with this compiler. Full Xcode.app install would also resolve this. This is a machine-level environment issue, not a project issue.

## Next Phase Readiness

- CMake project is configured; cmake generates build files successfully
- Plan 01-02 can immediately start writing Source/ files — all stubs are in place to overwrite
- The `.gitignore` ensures JUCE FetchContent cache (`build/_deps/juce-src/`) is never committed
- No blockers for Plan 01-02

---
*Phase: 01-project-scaffold*
*Completed: 2026-04-09*
