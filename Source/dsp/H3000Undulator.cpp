#include "Undulator.h"

void Undulator::prepare (double sampleRate, int /*samplesPerBlock*/)
{
    sr = sampleRate;
}

void Undulator::setParameters (const Parameters& p)
{
    params = p;
}

void Undulator::process (juce::AudioBuffer<float>& /*buffer*/, int /*numSamples*/)
{
    // Phase 1 stub: pass-through. Real dual-LFO AM/FM tremolo in Phase 5.
}

void Undulator::reset() {}
