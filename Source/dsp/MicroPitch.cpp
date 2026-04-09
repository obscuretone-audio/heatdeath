#include "MicroPitch.h"

void MicroPitch::prepare (double sampleRate, int /*samplesPerBlock*/)
{
    sr = sampleRate;
}

void MicroPitch::setParameters (const Parameters& p)
{
    params = p;
}

void MicroPitch::process (juce::AudioBuffer<float>& /*buffer*/, int /*numSamples*/)
{
    // Phase 1 stub: pass-through. Real SSB/delay pitch shift in Phase 4.
}

void MicroPitch::reset() {}
