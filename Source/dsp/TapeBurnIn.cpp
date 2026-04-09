#include "BurnIn.h"

void BurnIn::prepare (double sampleRate, int /*samplesPerBlock*/)
{
    sr = sampleRate;
}

void BurnIn::setParameters (const Parameters& p)
{
    params = p;
}

void BurnIn::process (juce::AudioBuffer<float>& /*buffer*/, int /*numSamples*/)
{
    // Phase 1 stub: pass-through. Real Jiles-Atherton tape hysteresis in Phase 6.
}

void BurnIn::reset() {}

void BurnIn::setPersistedTemp (float /*temp*/, float /*tempPrev*/) {}
