# Heatdeath

A four-stage audio effects plugin (VST3 / AU) built in C++ with JUCE. Mono in, stereo out.

```
RAT distortion → MicroPitch stereo detune → Undulator tremolo → Tape burn-in saturation
```

Each stage is modeled from scratch against documented hardware behavior — no generic
waveshapers, no third-party DSP libraries. A global parallel-mix control blends the dry
input against the fully processed signal.

## Stages

- **Turbo RAT** — op-amp clipping distortion with a reverse-wired tone filter (0 is bright,
  100 is dark) and a slew-rate control modeling a classic op-amp's high-frequency rolloff.
- **MicroPitch** — dual-voice stereo pitch detune (default −7¢ / +11¢) via delay/SSB
  modulation, independently panned.
- **Undulator** — dual-LFO amplitude/frequency modulation with nine LFO shapes, a feedback
  delay ("Space"), and a chip-saturation stage ("Grit").
- **Burn-In** — tape saturation modeled on Jiles-Atherton hysteresis, with a wet trim.

Full parameter reference: [`CONTROLS.md`](./CONTROLS.md).

## Building

Requires CMake and a C++17 toolchain. JUCE is pulled in via CMake's `FetchContent` /
`GIT_TAG`, pinned to JUCE 7.0.12 for reproducible builds.

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Produces VST3 and AU (macOS) plugin targets. On macOS, Homebrew LLVM is required — the
Apple Command Line Tools clang does not ship the C++ standard library headers this project
needs.

## Testing

`Tests/` holds unit test harnesses for the DSP stages (e.g. `TurboRatTest.cpp`,
`SmoothParamTest.cpp`), written before the corresponding implementation. The AU build has
been validated with Apple's `auval` tool and manually verified across six conditions in a
DAW.

## How this was built

This project was built by directing Claude Code through a structured, phase-gated
development process rather than freeform prompting: a written project brief and roadmap,
then one plan per unit of work, each with its own research notes, a written summary of
what was done and why, and a verification pass before moving on. The `.planning/`
directory in this repo is that trail in full — not a description of the process, the
actual planning and verification documents themselves.

Test harnesses were written ahead of the implementation they test, and each DSP stage was
validated against its documented hardware behavior before being considered done.

## Status

Personal project. Working, and in regular use by its author. Not distributed or
published to a plugin marketplace.

## License

TBD.
