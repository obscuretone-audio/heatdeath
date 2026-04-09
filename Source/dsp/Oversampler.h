#pragma once
#include <juce_audio_basics/juce_audio_basics.h>

class Oversampler
{
public:
    void prepare (double sampleRate, int samplesPerBlock);
    // Phase 1: returns input buffer unchanged. Real 4x polyphase FIR in Phase 2.
    void upsample   (juce::AudioBuffer<float>& buffer);
    void downsample (juce::AudioBuffer<float>& buffer);
    int  getOversampleFactor() const { return 1; }  // 1 = pass-through at Phase 1
    void reset();

private:
    double sr = 44100.0;
};
