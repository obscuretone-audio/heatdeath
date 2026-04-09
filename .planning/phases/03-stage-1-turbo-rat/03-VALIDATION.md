---
phase: 03
slug: stage-1-turbo-rat
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-04-09
---

# Phase 03 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | Custom C++ test harness (standalone `int main()` executables — pattern from Phase 2 SmoothParamTest) |
| **Config file** | `CMakeLists.txt` — add `TurboRatTest` target mirroring `SmoothParamTest` |
| **Quick run command** | `cmake --build build --target TurboRatTest && ./build/Tests/TurboRatTest` |
| **Full suite command** | `cmake --build build --config Release && ./build/Tests/SmoothParamTest && ./build/Tests/TurboRatTest` |
| **Estimated runtime** | ~10 seconds |

---

## Sampling Rate

- **After every task commit:** Run `cmake --build build --target TurboRatTest && ./build/Tests/TurboRatTest`
- **After every plan wave:** Run full suite (SmoothParamTest + TurboRatTest)
- **Before `/gsd-verify-work`:** Full suite must be green
- **Max feedback latency:** ~10 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------------|-----------|-------------------|-------------|--------|
| 03-01-01 | 01 | 0 | — | N/A | setup | `cmake --build build --target TurboRatTest` | ❌ W0 | ⬜ pending |
| 03-01-02 | 01 | 1 | RAT-01 | N/A | unit | `./build/Tests/TurboRatTest` | ❌ W0 | ⬜ pending |
| 03-02-01 | 02 | 1 | RAT-02 | N/A | unit | `./build/Tests/TurboRatTest` | ❌ W0 | ⬜ pending |
| 03-02-02 | 02 | 1 | RAT-03 | N/A | unit | `./build/Tests/TurboRatTest` | ❌ W0 | ⬜ pending |
| 03-03-01 | 03 | 2 | RAT-04 | N/A | unit | `./build/Tests/TurboRatTest` | ❌ W0 | ⬜ pending |
| 03-04-01 | 04 | 3 | RAT-05 | N/A | unit | `./build/Tests/TurboRatTest` | ❌ W0 | ⬜ pending |
| 03-04-02 | 04 | 3 | RAT-06 | N/A | unit | `./build/Tests/TurboRatTest` | ❌ W0 | ⬜ pending |
| 03-04-03 | 04 | 3 | RAT-07 | N/A | unit + smoke | `./build/Tests/TurboRatTest` | ❌ W0 | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `Tests/TurboRatTest.cpp` — stubs for RAT-01 through RAT-07; pattern from `Tests/SmoothParamTest.cpp`
- [ ] `CMakeLists.txt` — add `TurboRatTest` executable target mirroring existing `SmoothParamTest` target

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| RAT distortion character with Drive/Filter/Clip — audible harmonic content | RAT-01–07 | Audio quality is subjective; automated tests verify frequency response and clipping thresholds, not perceived character | Load plugin, feed 1kHz sine, sweep Drive/Filter/Clip, confirm distortion character changes as expected |
| kDefaultSlew 1040Hz bandwidth — authentic LM308 slew character | RAT-02 | Mapping of 0.68 literal alpha to 1040Hz is ambiguous; listening test determines if 1040Hz implementation matches hardware reference | Compare output against hardware RAT recordings at matching drive settings |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 15s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
