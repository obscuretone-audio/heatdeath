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

        // Unipolar AM at each channel's beat frequency — (1+cos)/2 keeps signal
        // in phase at all times, giving clean beating without phase inversions.
        const float amL = 0.5f + 0.5f * static_cast<float> (std::cos (6.283185307179586 * phaseL));
        const float amR = 0.5f + 0.5f * static_cast<float> (std::cos (6.283185307179586 * phaseR));
        const float shiftedL = in * amL;
        const float shiftedR = in * amR;

        // Stereo width: 1.0 = hard L/R, 0.0 = both voices collapsed to centre
        const float mid  = (shiftedL + shiftedR) * 0.5f;
        const float wetL = shiftedL * width + mid * (1.0f - width);
        const float wetR = shiftedR * width + mid * (1.0f - width);

        // Wet/dry blend — wet × 2 compensates the 0.5 average of the (1+cos)/2 envelope.
        // Without this the stage averages −6 dB; the correction restores parity.
        L[i] = in * dry + wetL * mix * 2.0f;
        R[i] = in * dry + wetR * mix * 2.0f;

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
