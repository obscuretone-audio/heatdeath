#include "MicroPitch.h"
#include <cmath>

// Fixed reference frequency for beat-rate calculation.
// The spec calibrates beating at A440 — -7¢ left gives ~1.78 Hz beat, +11¢ right ~2.80 Hz.
static constexpr double kRefFreq = 440.0;

void MicroPitch::prepare (double sampleRate, int /*samplesPerBlock*/)
{
    sr     = sampleRate;
    phaseL = 0.0;
    phaseR = 0.0;
}

void MicroPitch::setParameters (const Parameters& p)
{
    params = p;
}

void MicroPitch::process (juce::AudioBuffer<float>& buffer, int numSamples)
{
    // Frequency shift in Hz from cents — can be negative (detuneL is always negative)
    const double freqShiftL = kRefFreq * (std::pow (2.0, params.detuneL / 1200.0) - 1.0);
    const double freqShiftR = kRefFreq * (std::pow (2.0, params.detuneR / 1200.0) - 1.0);

    // Normalised phase increments (phase is kept in [0, 1) — cos is called as cos(2π*phase))
    const double incL = freqShiftL / sr;
    const double incR = freqShiftR / sr;

    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getWritePointer (1);

    const float mix   = params.mix;
    const float dry   = 1.0f - mix;
    const float width = params.width;

    for (int i = 0; i < numSamples; ++i)
    {
        const float in = L[i];   // Both channels are identical mono input at this point

        // AM at each channel's detuned frequency
        const float shiftedL = in * static_cast<float> (std::cos (6.283185307179586 * phaseL));
        const float shiftedR = in * static_cast<float> (std::cos (6.283185307179586 * phaseR));

        // Stereo width: 1.0 = hard L/R, 0.0 = both voices collapsed to centre
        const float mid  = (shiftedL + shiftedR) * 0.5f;
        const float wetL = shiftedL * width + mid * (1.0f - width);
        const float wetR = shiftedR * width + mid * (1.0f - width);

        // Wet/dry blend
        L[i] = in * dry + wetL * mix;
        R[i] = in * dry + wetR * mix;

        // Advance and wrap phases — floor() handles both positive and negative increments
        phaseL += incL;
        phaseL -= std::floor (phaseL);

        phaseR += incR;
        phaseR -= std::floor (phaseR);
    }
}

void MicroPitch::reset()
{
    phaseL = 0.0;
    phaseR = 0.0;
}
