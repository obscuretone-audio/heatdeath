#pragma once
#include <juce_audio_basics/juce_audio_basics.h>

class MicroPitch
{
public:
    struct Parameters
    {
        float detuneL = -7.f;
        float detuneR = 11.f;
        float mix     = 0.85f;
        float width   = 1.0f;
        float focus   = 440.f;  // Hz — reference frequency for beat-rate calculation
    };

    void prepare (double sampleRate, int samplesPerBlock);
    void setParameters (const Parameters& p);
    void process (juce::AudioBuffer<float>& buffer, int numSamples);
    float getLastLfoValue() const { return 0.f; }  // Phase 1: always returns 0
    void reset();

private:
    double sr      = 44100.0;
    double phaseL  = 0.0;   // normalised [0, 1) — avoids kTwoPi multiply drift
    double phaseR  = 0.0;
    Parameters params;
};
