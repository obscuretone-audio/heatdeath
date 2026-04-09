# Phase 3: Stage 1 — Turbo RAT - Research

**Researched:** 2026-04-09
**Domain:** Audio DSP — analog circuit emulation (op-amp gain stage, diode waveshaper, filter chains, JFET buffer) in C++/JUCE
**Confidence:** HIGH

---

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| RAT-01 | Two cascaded first-order HPFs before clipping: 60Hz (40dB/decade) and 1.5kHz (20dB/decade) | First-order HPF coefficients derivable from bilinear transform: `alpha = 1/(1 + 2*pi*fc/sr)`; cascade is two independent single-pole filters applied sequentially |
| RAT-02 | Gain-dependent LM308 slew-rate LP: baked constant `kDefaultSlew=0.68f` (~1040Hz bandwidth); not user-adjustable; applied before waveshaper | Fixed-cutoff first-order LP; `kDefaultSlew` in TurboRat.h already defined as `0.68f`; maps to ~1040Hz using `cutoffHz = sr * kDefaultSlew / (2*pi)` at 44.1kHz |
| RAT-03 | GBW dominant pole LP: `gbwHz=600/max(drive,0.01)` clamped [200, 8000Hz]; peaks ~600Hz at max drive | Same first-order LP structure as RAT-02 but cutoff recalculated per-block from smoothed drive; `juce::jlimit(200.f, 8000.f, 600.f / max(driveNorm, 0.01f))` |
| RAT-04 | Asymmetric tanh diode waveshaper: LED threshold 1.7V, Silicon 0.65V, Lift soft knee; `kDefaultAsym=0.20f`; normalized to 0dBFS | Spec provides exact formula; asymmetry applies different denominator to positive vs negative half-cycles; normalize by dividing output by threshold |
| RAT-05 | Post-clip reverse-wired tone LPF: filter=0 → 32kHz (bright), filter=1 → 475Hz (dark); matches real RAT counterintuitive wiring | First-order LP; cutoff maps INVERSELY from filter knob: `fc = 32000 - (32000 - 475) * filterNorm` or log-spaced interpolation; per-block coefficient update |
| RAT-06 | JFET output buffer: first-order LP at 18kHz + `volume` gain scalar (0–2.0) | Fixed-cutoff 18kHz LP + linear gain; `volumeGain = volume/100.0f * 2.0f` maps 0–100 → 0.0–2.0 |
| RAT-07 | All float params smoothed per-sample (20ms); filter coefficients updated per-block from smoothed values, never per-sample | Use `SmoothParam` struct (already in `Source/utils/SmoothParam.h`) for drive, filter, volume, slew, asym, sag; read `.current` at block start to derive coefficients |

</phase_requirements>

---

## Summary

Phase 3 replaces the pass-through stub in `Source/dsp/TurboRat.cpp` with a complete LM308 circuit emulation. The implementation runs entirely on the oversampled block produced by `oversampler.processSamplesUp()` in PluginProcessor — the processBlock skeleton already wires `osBlock` around Stage 1 and marks it for Phase 3. The stage operates on a mono signal throughout and returns a mono signal.

The signal chain inside TurboRat follows the real hardware order: two cascaded HPFs (60Hz, 1.5kHz) → slew-rate LP (fixed ~1040Hz) → GBW dominant-pole LP (drive-dependent) → asymmetric tanh diode waveshaper → post-clip tone LPF (reverse-wired 475Hz–32kHz) → JFET output buffer (18kHz LP + volume scalar). All five filters are first-order IIR (one-pole), implemented using the same `alpha * x + (1-alpha) * prevOutput` structure. Coefficients are computed once per block from smoothed parameter values; the sample loop reads only pre-computed coefficients, never calls `std::exp` or `std::cos`.

The waveshaper formula is specified verbatim in the spec and uses `std::tanh` (or `juce::dsp::FastMathApproximations::tanh` for the hot path at 4x oversampling). Normalization divides the output by the threshold value so that 0dBFS in → 0dBFS out at unity gain settings.

**Primary recommendation:** Expand `TurboRat.h` to add filter state variables and per-stage `SmoothParam` instances; implement `TurboRat::process()` to receive the oversampled `AudioBlock` from PluginProcessor and apply the chain in spec order; update coefficients once at block entry from smoothed values.

---

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| `SmoothParam` (project util) | Phase 2 | Per-sample 20ms parameter smoothing inside DSP stage | Already built and tested (Phase 02-02); exp(-2pi/samples) IIR; zero allocation |
| `juce::dsp::FastMathApproximations::tanh` | JUCE 7.0.12 | Waveshaper hot path at 4x oversampling | ~4x faster than `std::tanh`; sufficient accuracy for audio; already used by input limiter in PluginProcessor |
| `juce::dsp::Oversampling<float>` | JUCE 7.0.12 (pinned) | 4x oversampling boundary in PluginProcessor | Already wired in PluginProcessor; TurboRat receives the upsampled block directly |
| C++ `<cmath>` | C++17 | `std::exp`, `std::atan`, `std::max` for coefficient computation | Used in coefficient update (per-block, not per-sample) |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| `juce::jlimit<float>` | JUCE 7.0.12 | Clamp GBW cutoff to [200, 8000Hz] and other guard rails | Preferred over manual `std::clamp` in JUCE codebase |
| `juce::MathConstants<float>::pi` | JUCE 7.0.12 | pi constant for coefficient formulas | Prefer over raw 3.14159 literal |

**No additional installations required.** All dependencies are within JUCE 7.0.12 (already pinned) and the project's own `Source/utils/SmoothParam.h`. [VERIFIED: Source/utils/SmoothParam.h exists and all 13 contract tests pass per 02-02-SUMMARY.md]

---

## Architecture Patterns

### Where TurboRat Fits in the Existing processBlock

The Phase 2 processBlock stub (verified in 02-04-SUMMARY.md) already wraps Stage 1 with the oversampler:

```cpp
// From PluginProcessor.cpp (Phase 2 state)
{
    juce::dsp::AudioBlock<float> monoBlock (buffer.getArrayOfWritePointers(),
                                            1, static_cast<size_t> (numSamples));
    auto osBlock = oversampler.processSamplesUp (monoBlock);
    juce::ignoreUnused (osBlock);          // Phase 3 will process osBlock at 4x
    oversampler.processSamplesDown (monoBlock);
}
```

Phase 3 replaces `juce::ignoreUnused(osBlock)` with a call into `stageTurboRat->processOS(osBlock)` (or equivalent). The stage receives a `juce::dsp::AudioBlock<float>` at 4x the host sample rate (e.g., 176400 samples/sec at 44.1kHz host).

**Important:** `stageTurboRat->setParameters()` and `stageTurboRat->process()` are already called in PluginProcessor after the oversampler block. Phase 3 must decide whether:
- Option A: Split into `processOS(osBlock)` for the waveshaper path + existing `process()` for pass-through cleanup, OR
- Option B: Have `stageTurboRat->process()` receive the osBlock directly from PluginProcessor

Option B is simpler. PluginProcessor passes `osBlock` to `stageTurboRat->processOS(osBlock)`, then the existing `stageTurboRat->process(buffer, numSamples)` call can be removed or repurposed. The planner must choose the exact API shape.

### Recommended TurboRat Class Structure

```cpp
// TurboRat.h additions for Phase 3
class TurboRat {
public:
    // ... existing API ...

    // Phase 3: Called with the 4x oversampled mono block from PluginProcessor.
    // Contains: HPF chain + slew LP + GBW LP + waveshaper + tone LPF + JFET buffer.
    // Called BEFORE oversampler.processSamplesDown().
    void processOS (juce::dsp::AudioBlock<float>& osBlock);

    // updateCoefficients() — call once per block BEFORE processOS().
    // Reads smoothed parameter values, derives IIR alpha coefficients.
    // Never called per-sample.
    void updateCoefficients (double osSampleRate);

private:
    double     sr      = 44100.0;
    double     osSr    = 176400.0;   // 4x host rate
    Parameters params;

    // Per-sample smoothers (SmoothParam from Source/utils/SmoothParam.h)
    SmoothParam smoothDrive, smoothFilter, smoothVolume, smoothSlew, smoothAsym, smoothSag;

    // Filter state (one-pole IIR — two state variables per filter: prevIn, prevOut)
    float hpf1State = 0.0f;   // 60Hz HPF
    float hpf2State = 0.0f;   // 1.5kHz HPF
    float slewState = 0.0f;   // slew-rate LP (~1040Hz)
    float gbwState  = 0.0f;   // GBW dominant pole LP
    float toneState = 0.0f;   // post-clip tone LPF
    float jfetState = 0.0f;   // JFET output LP (18kHz)

    // Pre-computed coefficients (updated once per block in updateCoefficients)
    float hpf1Alpha = 0.0f;
    float hpf2Alpha = 0.0f;
    float slewAlpha = 0.0f;
    float gbwAlpha  = 0.0f;
    float toneAlpha = 0.0f;
    float jfetAlpha = 0.0f;
    float volumeGain = 0.0f;
};
```

### First-Order Filter Coefficient Computation

**HPF alpha (from bilinear transform / one-pole HPF):**
```cpp
// Source: standard DSP textbook bilinear transform; ASSUMED from training knowledge
// For a first-order HPF at cutoffHz:
float computeHPFAlpha(float cutoffHz, double sampleRate) {
    const double rc = 1.0 / (juce::MathConstants<double>::twoPi * cutoffHz);
    const double dt = 1.0 / sampleRate;
    return static_cast<float>(rc / (rc + dt));   // alpha for HPF difference equation
}

// HPF difference equation per sample:
//   y[n] = alpha * (y[n-1] + x[n] - x[n-1])
// This requires storing both prevIn and prevOut.
```

**LPF alpha (simple one-pole RC):**
```cpp
// For a first-order LPF at cutoffHz:
float computeLPFAlpha(float cutoffHz, double sampleRate) {
    // alpha = exp(-2*pi*fc/sr)  — same formula as SmoothParam coefficient
    return static_cast<float>(
        std::exp(-juce::MathConstants<double>::twoPi * cutoffHz / sampleRate));
}

// LPF difference equation per sample:
//   y[n] = alpha * y[n-1] + (1 - alpha) * x[n]
// State: only prevOut needed.
```

**Note:** The SmoothParam utility uses the same `exp(-2pi*fc/sr)` formula for its IIR coefficient. The LPF filters in TurboRat can reuse this math directly. [ASSUMED: standard DSP formulation — verified to be consistent with SmoothParam.h implementation which uses exp(-2pi/samples)]

### Asymmetric Tanh Waveshaper

Verbatim from `heatdeath_vst_spec.md` (HIGH confidence — project spec document):

```cpp
// Source: heatdeath_vst_spec.md, Section 2
// threshold: 1.7f (LED), 0.65f (Silicon), 12.0f (Lift)
// kAsym: kDefaultAsym = 0.20f (baked constant, not user-adjustable)

float applyWaveshaper(float v, float threshold, float kAsym) {
    float out;
    if (v >= 0.0f) {
        out = threshold * juce::dsp::FastMathApproximations::tanh(10.0f * v / threshold);
    } else {
        out = -threshold * juce::dsp::FastMathApproximations::tanh(
                  10.0f * -v / (threshold * (1.0f + kAsym * 0.4f)));
    }
    return out / threshold;   // normalize
}
```

**Clip mode → threshold mapping:**
| clipMode (int) | Threshold |
|----------------|-----------|
| 0 (LED)        | 1.7f      |
| 1 (Silicon)    | 0.65f     |
| 2 (Lift)       | 12.0f     |
| 3 (Ruetz)      | TBD — spec says "Ruetz is hidden mode, no label in UI"; treat as Silicon with reversed asymmetry or use spec value if found |

**Ruetz mode note:** The spec does not define a threshold for Ruetz mode. This is a hidden/undocumented mode. [ASSUMED] Treat as a variation of Silicon (0.65V threshold) with modified asymmetry until further spec is found.

### Reverse-Wired Tone LPF (RAT-05)

The RAT hardware's Filter/Tone control is reverse-wired: turning the pot UP reduces high frequencies. Spec confirms: "filter=0 → 32kHz (bright), filter=1 → 475Hz (dark)."

```cpp
// Source: REQUIREMENTS.md RAT-05, heatdeath_vst_spec.md Section 2
// filterNorm = filter/100.0f (0.0 = bright, 1.0 = dark)
// The cutoff sweeps from 32kHz (open) to 475Hz (closed) as filter increases.

// Log-spaced interpolation (perceptually correct for frequency):
float computeToneCutoff(float filterNorm) {
    const float logHigh = std::log(32000.0f);
    const float logLow  = std::log(475.0f);
    // filterNorm=0 → fc=32000Hz, filterNorm=1 → fc=475Hz
    return std::exp(logHigh + filterNorm * (logLow - logHigh));
}
```

### GBW Dominant Pole (RAT-03)

```cpp
// Source: REQUIREMENTS.md RAT-03
// driveNorm = drive/100.0f (0.0–1.0)
// gbwHz = 600 / max(driveNorm, 0.01)
// clamped to [200, 8000Hz]
float gbwHz = juce::jlimit(200.0f, 8000.0f, 600.0f / std::max(driveNorm, 0.01f));
```

At drive=0 (norm=0), gbwHz = 600/0.01 = 60000, clamped to 8000Hz — brightest.
At drive=100 (norm=1.0), gbwHz = 600/1.0 = 600Hz — darkest, most slew-limiting.
At drive=50 (norm=0.5), gbwHz = 1200Hz.

### Slew-Rate LP (RAT-02)

The slew-rate constant `kDefaultSlew = 0.68f` is defined in TurboRat.h as a `static constexpr float`. The spec says it produces ~1040Hz bandwidth. The mapping is:

```cpp
// Source: heatdeath_vst_spec.md Section 2; kDefaultSlew defined in TurboRat.h
// "baked constant kDefaultSlew=0.68f → ~1040Hz bandwidth"
// This is a FIXED cutoff — not user-adjustable.
// Use computeLPFAlpha(1040.0f, osSampleRate) OR derive from kDefaultSlew directly.
// [ASSUMED] The 1040Hz figure comes from treating kDefaultSlew as a one-pole
// LP coefficient: alpha = kDefaultSlew, from which fc = -sr*ln(alpha)/(2*pi)
// At 44100Hz: fc = -44100 * ln(0.68) / (2*pi) ≈ 44100 * 0.3857 / 6.283 ≈ 2707Hz
// This does NOT match ~1040Hz. At 4x oversampled rate (176400Hz):
// fc = -176400 * ln(0.68) / (2*pi) ≈ 10826Hz — also doesn't match.
// Resolution: treat kDefaultSlew as a drive-independent fixed target of 1040Hz,
// and compute alpha normally: computeLPFAlpha(1040.0f, osSampleRate).
// The constant kDefaultSlew (0.68) may be an opacity/amount that SCALES into 1040Hz
// via a different formula. Planner should use 1040Hz as the target cutoff,
// not kDefaultSlew directly as an IIR alpha value.
```

[ASSUMED] The relationship between `kDefaultSlew=0.68f` and `~1040Hz` is ambiguous — the two values are inconsistent if kDefaultSlew is used directly as an alpha coefficient at either 44.1kHz or 176.4kHz. The safest interpretation is: the slew LP has a fixed cutoff of 1040Hz; kDefaultSlew is a calibration constant the spec mentions for documentation but not necessarily as the literal IIR alpha. The planner should implement the 1040Hz LP and document this assumption.

### JFET Output Buffer (RAT-06)

```cpp
// Source: heatdeath_vst_spec.md Section 2; REQUIREMENTS.md RAT-06
// Two operations in order:
// 1. First-order LP at 18kHz (fixed cutoff — JFET input capacitance roll-off)
// 2. Volume scalar: volumeGain = (volume/100.0f) * 2.0f  → maps [0,100] to [0.0, 2.0]
```

The spec also says "adds subtle second-harmonic bloom — approximate with a very mild asymmetric tanh at unity gain." The requirements spec (RAT-06) does not mention this bloom, only the LP and volume scalar. Planner should implement exactly what RAT-06 specifies (LP + scalar) and defer the second-harmonic bloom to a note or future enhancement.

### Parameter Smoothing Integration

TurboRat uses `SmoothParam` (from `Source/utils/SmoothParam.h`, Phase 02-02) for per-sample smoothing of all six float parameters: drive, filter, volume, slew, asym, sag.

```cpp
// In TurboRat::prepare():
smoothDrive .setTimeMs(20.0f, osSampleRate);
smoothFilter.setTimeMs(20.0f, osSampleRate);
smoothVolume.setTimeMs(20.0f, osSampleRate);
smoothSlew  .setTimeMs(20.0f, osSampleRate);
smoothAsym  .setTimeMs(20.0f, osSampleRate);
smoothSag   .setTimeMs(20.0f, osSampleRate);

// In TurboRat::processOS() at the TOP of the block (before sample loop):
smoothDrive .setTarget(params.drive / 100.0f);
smoothFilter.setTarget(params.filter / 100.0f);
// ... etc.

// Advance smoothers to get block-start values for coefficient computation:
const float driveNorm   = smoothDrive .tick();
const float filterNorm  = smoothFilter.tick();
// ... then compute coefficients from these values ...

// In the sample loop, advance per sample and use smoothed values for
// any per-sample-varying gains (volume, sag), while keeping coefficients
// fixed for that block (RAT-07 requirement: "coefficients updated per-block, never per-sample").
```

**RAT-07 compliance pattern:** Advance smoothers once-per-block at block entry to get the current smoothed value for coefficient computation. Use those computed coefficients for the entire block. The smoother still advances per-sample inside the loop for the volume/sag scalars (which don't require coefficient recalculation), but the filter alphas are block-stable.

### Anti-Pattern to Avoid

```cpp
// WRONG — per-sample coefficient recalculation (violates RAT-07)
for (int i = 0; i < numSamples; ++i) {
    const float drive = smoothDrive.tick();
    const float alpha = computeLPFAlpha(600.0f / drive, osSampleRate);  // expensive per-sample
    output[i] = alpha * state + (1-alpha) * input[i];
    state = output[i];
}

// CORRECT — compute coefficient once per block, use across all samples
const float driveAtBlockStart = smoothDrive.tick();
const float gbwAlpha = computeLPFAlpha(600.0f / std::max(driveAtBlockStart, 0.01f), osSampleRate);
for (int i = 0; i < numSamples; ++i) {
    output[i] = gbwAlpha * state + (1.0f - gbwAlpha) * input[i];
    state = output[i];
}
```

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Fast tanh approximation | Custom polynomial | `juce::dsp::FastMathApproximations::tanh` | Already in JUCE 7, tested, consistent with input limiter in PluginProcessor |
| Parameter smoothing | Custom ramp or second IIR | `SmoothParam` (Source/utils/SmoothParam.h) | Built and tested Phase 02-02; 13 contract tests pass; consistent with rest of project |
| Range clamping | Manual `if (x < min) x = min` | `juce::jlimit<float>(min, max, x)` | JUCE idiom; consistent with rest of codebase |
| dB to linear conversion | `std::pow(10.0f, db/20.0f)` | `juce::Decibels::decibelsToGain(db)` | Already used for trim smoothers in PluginProcessor |
| Oversampling | Custom interpolation | Existing `oversampler` in PluginProcessor | Already wired; Phase 3 just processes the `osBlock` |

---

## Common Pitfalls

### Pitfall 1: Processing at the Wrong Sample Rate
**What goes wrong:** TurboRat filter coefficients computed with host sample rate (e.g., 44100Hz) instead of oversampled rate (176400Hz). Cutoff frequencies will be 4x too high — the 1040Hz slew LP will behave as ~4160Hz, producing wrong character.
**Why it happens:** `prepare(sampleRate, samplesPerBlock)` receives the host rate. The oversampled rate is `sampleRate * 4`.
**How to avoid:** Store `osSr = sampleRate * 4.0` in `prepare()`. Use `osSr` for ALL coefficient computation. Use `sr` only for `SmoothParam::setTimeMs()` — smoothers operate at HOST rate because they are advanced once per sample of the NATIVE block.
**Warning signs:** Filter sounds too bright; tone control range seems compressed; character doesn't change convincingly with Drive.

**Critical distinction:** SmoothParam smoothers are advanced once per HOST sample (the outer block loop). Filter state is advanced once per OVERSAMPLED sample (4x inner block). Both use `tick()` but at different rates. SmoothParam should be configured with HOST sample rate for its time constant (20ms = 882 host samples), but filter coefficients must use osSr.

### Pitfall 2: Coefficient Recomputation Inside the Sample Loop (RAT-07 Violation)
**What goes wrong:** `std::exp()` called per-sample inside the hot loop at 4x oversampling. At 176400 calls/sec this is ~3.5x the cost at native rate and will cause audible dropouts.
**Why it happens:** Natural coding pattern puts all per-sample processing in one loop.
**How to avoid:** Call `updateCoefficients()` once before the sample loop. The loop only does multiply-add arithmetic.
**Warning signs:** CPU spike when TurboRat is engaged; profiler shows `exp()` in hot path.

### Pitfall 3: HPF Difference Equation Error
**What goes wrong:** Using LPF difference equation `y[n] = alpha*y[n-1] + (1-alpha)*x[n]` for the HPF instead of the correct HPF form.
**Why it happens:** The two equations look similar but produce opposite frequency responses.
**How to avoid:** The correct one-pole HPF difference equation is:
```
y[n] = alpha * (y[n-1] + x[n] - x[n-1])
```
This requires storing both `prevIn` and `prevOut` per filter instance.
**Warning signs:** Spectrum analysis shows the signal is LOW-PASS filtered (lows preserved, highs removed) after the "HPF chain" — opposite of correct behavior.

### Pitfall 4: Normalization Direction for Filter Knob (RAT-05)
**What goes wrong:** Filter knob maps intuitively (0 = dark/LPF at 475Hz, 100 = bright) but spec requires reverse wiring (0 = bright/32kHz, 100 = dark/475Hz).
**Why it happens:** Intuitive mapping is opposite to hardware.
**How to avoid:** Verify with REQUIREMENTS.md RAT-05: "filter=0 → 475Hz (dark), filter=1 → 32kHz (bright)" — note this is the requirement, but the ROADMAP plan 03-04 says "reverse-wired, 475Hz–32kHz". Read Parameters.cpp: RAT_FILTER is labeled "Filter — reverse-wired LPF. 0 = bright (32kHz), 100 = dark (475Hz)." The knob physical position 0 = bright, 100 = dark. Implement accordingly.
**Warning signs:** Filter swept from 0→100 produces brightening instead of darkening.

### Pitfall 5: Missing `reset()` for Filter State on prepareToPlay
**What goes wrong:** TurboRat filter state variables (hpf1State, hpf2State, etc.) contain stale values from previous session or previous call, causing audible click or DC on first audio block.
**Why it happens:** `reset()` is already declared in TurboRat.h but the stub implementation is empty.
**How to avoid:** Phase 3 must implement `reset()` to zero all filter state variables: hpf1State, hpf2State, slewState, gbwState, toneState, jfetState. Also reset all SmoothParam instances.
**Warning signs:** Audible click on first process call; DC offset at output when plugin first loaded.

### Pitfall 6: Parameters Struct Defaults Don't Match Parameters.cpp
**What goes wrong:** TurboRat.h Parameters struct defaults (filter=35.f, volume=60.f) are stale values from Phase 1 (before 02-01 corrected them to 50.0 and 65.0). The APVTS has correct defaults but if TurboRat::Parameters is ever default-constructed, stale values are used.
**Why it happens:** The struct in TurboRat.h was defined in Phase 1 before the parameter audit in 02-01.
**How to avoid:** Update TurboRat.h Parameter struct defaults to match corrected values from 02-01-SUMMARY.md: `filter=50.f`, `volume=65.f`, `sag=25.f`. The sag default was also wrong (30.f → 25.f).
**Warning signs:** Default behavior doesn't match preset values; debugging session finds discrepancy between APVTS atomic and struct default.

---

## Code Examples

### Complete per-sample loop template (4x oversampled block)

```cpp
// Source: derived from REQUIREMENTS.md + SmoothParam.h + heatdeath_vst_spec.md
// Called from processOS(osBlock) after updateCoefficients() has run.
void TurboRat::processOS (juce::dsp::AudioBlock<float>& osBlock) {
    const int numOsSamples = static_cast<int>(osBlock.getNumSamples());
    float* data = osBlock.getChannelPointer(0);  // mono — channel 0 only

    // Advance smoothers once at block start to get target values
    // (coefficients already derived in updateCoefficients())
    const float asymNorm = smoothAsym.tick();     // per-sample smooth for waveshaper
    const float volGain  = smoothVolume.tick() * 2.0f;  // 0–100 → 0.0–2.0

    for (int i = 0; i < numOsSamples; ++i) {
        float x = data[i];

        // 1. HPF 60Hz
        const float hpf1Out = hpf1Alpha * (hpf1State + x - hpf1PrevIn);
        hpf1PrevIn = x;
        hpf1State  = hpf1Out;
        x = hpf1Out;

        // 2. HPF 1.5kHz
        const float hpf2Out = hpf2Alpha * (hpf2State + x - hpf2PrevIn);
        hpf2PrevIn = x;
        hpf2State  = hpf2Out;
        x = hpf2Out;

        // 3. Slew-rate LP (~1040Hz, fixed)
        slewState = slewAlpha * slewState + (1.0f - slewAlpha) * x;
        x = slewState;

        // 4. GBW dominant pole LP (drive-dependent cutoff, block-stable)
        gbwState = gbwAlpha * gbwState + (1.0f - gbwAlpha) * x;
        x = gbwState;

        // 5. Asymmetric tanh waveshaper (runs at 4x rate — hot path)
        {
            const float kAsym = asymNorm * 0.20f;  // or use baked kDefaultAsym
            if (x >= 0.0f) {
                x = threshold * juce::dsp::FastMathApproximations::tanh(10.0f * x / threshold);
            } else {
                x = -threshold * juce::dsp::FastMathApproximations::tanh(
                        10.0f * -x / (threshold * (1.0f + kAsym * 0.4f)));
            }
            x /= threshold;   // normalize
        }

        // 6. Post-clip tone LPF (reverse-wired, block-stable cutoff)
        toneState = toneAlpha * toneState + (1.0f - toneAlpha) * x;
        x = toneState;

        // 7. JFET output LP (18kHz, fixed) + volume scalar
        jfetState = jfetAlpha * jfetState + (1.0f - jfetAlpha) * x;
        x = jfetState * volGain;

        data[i] = x;
    }
}
```

### Coefficient update (per-block)

```cpp
// Source: derived from spec formulas
void TurboRat::updateCoefficients (double osSampleRate) {
    // Read current smoothed values (tick advances smoothers)
    const float driveNorm  = smoothDrive.tick();
    const float filterNorm = smoothFilter.tick();

    // HPF 60Hz — fixed cutoff, recomputed at prepare() only; but safe to recompute
    hpf1Alpha = computeHPFAlpha(60.0f,   static_cast<float>(osSampleRate));
    hpf2Alpha = computeHPFAlpha(1500.0f, static_cast<float>(osSampleRate));

    // Slew-rate LP — fixed 1040Hz
    slewAlpha = computeLPFAlpha(1040.0f, static_cast<float>(osSampleRate));

    // GBW dominant pole — drive-dependent
    const float gbwHz = juce::jlimit(200.0f, 8000.0f,
                            600.0f / std::max(driveNorm, 0.01f));
    gbwAlpha = computeLPFAlpha(gbwHz, static_cast<float>(osSampleRate));

    // Tone LPF — reverse-wired (filterNorm=0 → 32kHz, filterNorm=1 → 475Hz)
    const float logHigh = std::log(32000.0f);
    const float logLow  = std::log(475.0f);
    const float toneCutoff = std::exp(logHigh + filterNorm * (logLow - logHigh));
    toneAlpha = computeLPFAlpha(toneCutoff, static_cast<float>(osSampleRate));

    // JFET LP — fixed 18kHz
    jfetAlpha = computeLPFAlpha(18000.0f, static_cast<float>(osSampleRate));

    // Volume — not a filter coefficient, computed from smoothed value
    volumeGain = smoothVolume.tick() * 2.0f;  // 0–100 → 0.0–2.0

    // Clip mode → threshold (read from params, not smoothed — it's a choice param)
    switch (params.clipMode) {
        case 0: threshold = 1.7f;  break;  // LED
        case 1: threshold = 0.65f; break;  // Silicon
        case 2: threshold = 12.0f; break;  // Lift
        default: threshold = 0.65f; break; // Ruetz (fallback)
    }
}
```

---

## Runtime State Inventory

Step 2.5 SKIPPED — Phase 3 is a greenfield DSP implementation (filling in a stub), not a rename/refactor/migration phase. No runtime state inventory is applicable.

---

## Environment Availability

Step 2.6 SKIPPED — Phase 3 is purely a code change (C++ DSP implementation). No external tools, services, databases, or CLIs beyond the already-verified CMake/JUCE build chain are required.

Build chain verified in Phase 2 (02-04-SUMMARY.md): cmake --build PASS, no errors. JUCE 7.0.12 pinned via FetchContent and already in build cache.

---

## Validation Architecture

`workflow.nyquist_validation` is `true` in `.planning/config.json` — this section is required.

### Test Framework

| Property | Value |
|----------|-------|
| Framework | Custom C++ test harness (no external test framework — project uses standalone `int main()` test executables per 02-02-SUMMARY.md) |
| Config file | `CMakeLists.txt` — `add_executable(SmoothParamTest ...)` pattern established in Phase 2 |
| Quick run command | `cmake --build build --target TurboRatTest && ./build/Tests/TurboRatTest` |
| Full suite command | `cmake --build build --config Release && ./build/Tests/SmoothParamTest && ./build/Tests/TurboRatTest` |

### Phase Requirements → Test Map

| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| RAT-01 | HPF chain passes 10kHz, attenuates 30Hz by >20dB | unit | `./build/Tests/TurboRatTest` | No — Wave 0 |
| RAT-02 | Slew LP rolls off above ~1040Hz at osSampleRate | unit | `./build/Tests/TurboRatTest` | No — Wave 0 |
| RAT-03 | GBW cutoff inversely tracks drive; ~600Hz at drive=1.0 | unit | `./build/Tests/TurboRatTest` | No — Wave 0 |
| RAT-04 | LED mode (1.7V) clips later than Si (0.65V); normalized output | unit | `./build/Tests/TurboRatTest` | No — Wave 0 |
| RAT-05 | filter=0 → 32kHz passthrough; filter=100 → 475Hz rolloff | unit | `./build/Tests/TurboRatTest` | No — Wave 0 |
| RAT-06 | volume=100 → 2.0 gain scalar; JFET LP at 18kHz | unit | `./build/Tests/TurboRatTest` | No — Wave 0 |
| RAT-07 | No `std::exp` call inside sample loop (static analysis or timing) | unit + smoke | `./build/Tests/TurboRatTest` (timing test) | No — Wave 0 |

### Sampling Rate

- **Per task commit:** `cmake --build build --target TurboRatTest && ./build/Tests/TurboRatTest`
- **Per wave merge:** Full suite: `cmake --build build --config Release && ./build/Tests/SmoothParamTest && ./build/Tests/TurboRatTest`
- **Phase gate:** Full suite green before `/gsd-verify-work`

### Wave 0 Gaps

- `Tests/TurboRatTest.cpp` — covers all 7 RAT requirements above; pattern from `Tests/SmoothParamTest.cpp`
- `CMakeLists.txt` — add `TurboRatTest` executable target mirroring `SmoothParamTest` target

---

## Security Domain

This is a DSP implementation phase with no network, file I/O, authentication, or external trust boundaries. No ASVS categories apply. Security domain: NOT APPLICABLE for this phase.

---

## Open Questions

1. **kDefaultSlew → 1040Hz mapping ambiguity**
   - What we know: Spec says `kDefaultSlew=0.68f` produces "~1040Hz bandwidth"
   - What's unclear: Using 0.68 directly as an IIR alpha at any standard sample rate does not produce 1040Hz. At 44100Hz: `fc = -44100*ln(0.68)/(2*pi) ≈ 2707Hz`. At 176400Hz: ~10826Hz. Neither matches.
   - Recommendation: Implement as a fixed 1040Hz LP cutoff computed with `computeLPFAlpha(1040.0f, osSampleRate)`. Document kDefaultSlew as a spec annotation, not a literal alpha value. If exact hardware character depends on using 0.68 directly as alpha, this should be revisited with a listening test.

2. **Ruetz clip mode threshold**
   - What we know: Parameters.cpp includes "Ruetz" as index 3; it is a "hidden mode, no label in UI"
   - What's unclear: No threshold value is given in the spec for Ruetz mode
   - Recommendation: Implement as Silicon (0.65V) fallback. The planner can add a TODO comment. This will not affect RAT-01 through RAT-07 which all specify LED/Si/Lift only.

3. **API shape for processOS vs process (PluginProcessor integration)**
   - What we know: PluginProcessor currently calls `stageTurboRat->process(buffer, numSamples)` as a pass-through; oversampler produces `osBlock`
   - What's unclear: Whether to add `processOS(osBlock)` as a new method OR replace the existing `process()` signature to accept an AudioBlock
   - Recommendation: Add `processOS(juce::dsp::AudioBlock<float>&)` as a new method. PluginProcessor removes the `juce::ignoreUnused(osBlock)` line and calls `stageTurboRat->processOS(osBlock)` instead. The existing `process(buffer, numSamples)` call in PluginProcessor after the oversampler block should be removed or no-op'd. This keeps TurboRat self-contained and testable without PluginProcessor.

4. **Whether to advance SmoothParam at host rate or OS rate**
   - What we know: SmoothParam configured with 20ms time constant; RAT-07 says "per-sample" smoothing
   - What's unclear: "per-sample" = per host sample (882 samples for 20ms at 44.1kHz) or per OS sample (3528 samples at 4x)?
   - Recommendation: Per-HOST sample, not per-OS sample. The smoothers should be ticked once per native sample, not once per oversampled sample. This means advancing smoothers in PluginProcessor BEFORE the oversample-up step, or using a rate-matched approach. Simplest: advance smoothers per native block (once total, as the block-start snapshot for coefficient computation), consistent with current trim smoother pattern in PluginProcessor.

---

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | `kDefaultSlew=0.68f` should be implemented as a fixed 1040Hz LP cutoff, not used directly as an IIR alpha | Code Examples, Common Pitfalls | If 0.68 IS meant as direct alpha at a non-standard rate, the slew character will be wrong; audible but not catastrophic |
| A2 | Ruetz clip mode threshold = 0.65V (Silicon fallback) | Code Examples | Ruetz is hidden/undocumented; incorrect value only affects hidden feature, not primary RAT-04 requirement |
| A3 | Log-spaced interpolation for tone control cutoff sweep | Code Examples | Linear interpolation would produce perceptually uneven sweep; log is standard for frequency controls but not explicitly mandated by spec |
| A4 | SmoothParam smoothers advanced at HOST rate (not OS rate) for coefficient computation | Architecture Patterns | Wrong rate would mean 20ms constant is actually 5ms at 4x; audible click potential on drive changes |

---

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| `process(buffer, numSamples)` pass-through stub | `processOS(osBlock)` on 4x oversampled block | Phase 3 | TurboRat now processes at 4x rate; PluginProcessor integration point changes |
| Parameters struct defaults: filter=35, volume=60, sag=30 | Corrected: filter=50, volume=65, sag=25 | Phase 02-01 | Must update TurboRat.h Parameters struct to match |

**Stub behavior to replace:**
- `TurboRat::process()` — currently empty (pass-through); Phase 3 replaces this
- `TurboRat::reset()` — currently empty; Phase 3 must zero all filter state

---

## Sources

### Primary (HIGH confidence)
- `heatdeath_vst_spec.md` — Verbatim waveshaper formula, filter topology, signal chain order, parameter table
- `Source/dsp/TurboRat.h` — Existing class structure, kDefaultSlew/kDefaultAsym constants, Parameters struct
- `Source/dsp/TurboRat.cpp` — Current stub implementation to be replaced
- `Source/PluginProcessor.cpp` — Oversampler integration point; processBlock structure
- `.planning/REQUIREMENTS.md` — RAT-01 through RAT-07 exact specifications
- `Source/utils/SmoothParam.h` — Smoother implementation (exp(-2pi/samples), setTimeMs/tick API)
- `.planning/phases/02-core-infrastructure/02-02-SUMMARY.md` — SmoothParam contract tests confirmed passing
- `.planning/phases/02-core-infrastructure/02-04-SUMMARY.md` — Oversampler wiring confirmed

### Secondary (MEDIUM confidence)
- `Source/Parameters.cpp` — RAT_FILTER default corrected to 50 (Phase 02-01); validates TurboRat.h struct update needed
- `heatdeath_agent_prompt.md` — Coding brief references signal chain and spec values

### Tertiary (LOW confidence)
- [ASSUMED] One-pole HPF difference equation form `y[n] = alpha * (y[n-1] + x[n] - x[n-1])` — standard DSP formulation, not verified against an authoritative external source in this session
- [ASSUMED] Log-spaced tone cutoff interpolation — standard audio engineering practice for frequency knobs
- [ASSUMED] kDefaultSlew = 0.68 is a calibration annotation, not a direct IIR alpha coefficient

---

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — all dependencies are within JUCE 7.0.12 (already pinned) and project utilities (Phase 2 complete)
- Architecture: HIGH — processBlock integration point exactly specified in Phase 2 code; waveshaper formula verbatim from spec
- Pitfalls: HIGH — coefficient recalculation, wrong sample rate, HPF equation error are all well-understood DSP failure modes
- Open questions: MEDIUM — kDefaultSlew mapping and processOS API shape need planner decision

**Research date:** 2026-04-09
**Valid until:** 2026-05-09 (30 days — stack is stable; JUCE 7.0.12 is pinned)
