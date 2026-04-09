#pragma once
#include <juce_audio_basics/juce_audio_basics.h>

class BurnIn
{
public:
    struct Parameters
    {
        float heatRate     = 0.f;
        bool  freeze       = false;
        bool  acetate      = false;
        bool  msMode       = false;
        bool  timerActive  = false;
        float timerProgress = 0.f;
    };

    void prepare (double sampleRate, int samplesPerBlock);
    void setParameters (const Parameters& p);
    void process (juce::AudioBuffer<float>& buffer, int numSamples);
    void reset();

    // Thermal state persistence (BURN-08 — implemented Phase 6)
    void  setPersistedTemp (float temp, float tempPrev);
    float getCurrentTemp()  const { return 0.f; }
    float getPreviousTemp() const { return 0.f; }

private:
    double sr = 44100.0;
    Parameters params;
};
