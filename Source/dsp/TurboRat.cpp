#include "TurboRat.h"

void TurboRat::prepare (double sampleRate, int /*samplesPerBlock*/)
{
    sr = sampleRate;
}

void TurboRat::setParameters (const Parameters& p)
{
    params = p;
}

void TurboRat::process (juce::AudioBuffer<float>& /*buffer*/, int /*numSamples*/)
{
    // Phase 1 stub: pass-through. Real LM308 model implemented in Phase 3.
}

void TurboRat::reset() {}
