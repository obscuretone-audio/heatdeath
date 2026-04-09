#include "Oversampler.h"

void Oversampler::prepare (double sampleRate, int /*samplesPerBlock*/)
{
    sr = sampleRate;
}

void Oversampler::upsample   (juce::AudioBuffer<float>&) {}
void Oversampler::downsample (juce::AudioBuffer<float>&) {}
void Oversampler::reset() {}
