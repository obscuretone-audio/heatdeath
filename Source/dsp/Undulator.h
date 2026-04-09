#pragma once
#include <juce_audio_basics/juce_audio_basics.h>

class Undulator
{
public:
    struct Parameters
    {
        float rate     = 2.4f;
        float depth    = 0.82f;
        float phase    = 180.f;
        float drift    = 0.40f;
        float modRate  = 0.6f;
        float modDepth = 0.45f;
        float modSpeed = 0.35f;
        float spread   = 0.55f;
        float feedback = 0.38f;
        float grit     = 0.62f;
        float mix      = 1.0f;
        int   shape    = 0;
    };

    void prepare (double sampleRate, int samplesPerBlock);
    void setParameters (const Parameters& p);
    void process (juce::AudioBuffer<float>& buffer, int numSamples);
    void reset();

private:
    double sr = 44100.0;
    Parameters params;
};
