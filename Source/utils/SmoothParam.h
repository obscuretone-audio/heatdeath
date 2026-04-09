#pragma once
#include <cmath>
#include <juce_core/juce_core.h>

// Per-sample one-pole IIR smoother.
// Used by DSP stages for per-sample parameter smoothing (Phases 3-6).
// For smoothing of processor-level floats, prefer juce::SmoothedValue<float>.
//
// Formula:
//   coeff = exp(-2pi / (ms * sampleRate / 1000))
//   tick:  current = coeff * current + (1 - coeff) * target
//
// coeff near 1.0 = slow smoother (long time constant)
// coeff near 0.0 = fast smoother / instant (coeff == 0 snaps to target)
struct SmoothParam
{
    float current = 0.0f;
    float target  = 0.0f;
    float coeff   = 0.0f;  // 0 = instant, approaches 1.0 = slow

    // Configure the smoothing time constant.
    // ms: time to reach ~63% of target (one time constant).
    // sampleRate: processor sample rate (Hz).
    // ms <= 0 or sampleRate <= 0 -> instant (coeff = 0).
    void setTimeMs (float ms, double sampleRate) noexcept
    {
        if (ms <= 0.0f || sampleRate <= 0.0)
        {
            coeff = 0.0f;
            return;
        }
        const double samples = static_cast<double>(ms) * sampleRate / 1000.0;
        coeff = static_cast<float>(
            std::exp(-juce::MathConstants<double>::twoPi / samples));
    }

    void setTarget (float v) noexcept { target = v; }

    // Advance one sample. Returns the new current value.
    float tick() noexcept
    {
        current = coeff * current + (1.0f - coeff) * target;
        return current;
    }

    // Jump current and target to v. Clears any pending ramp.
    void reset (float v) noexcept
    {
        current = target = v;
    }
};
