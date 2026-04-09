#pragma once
#include <juce_audio_basics/juce_audio_basics.h>

class TurboRat
{
public:
    struct Parameters
    {
        float drive    = 72.f;
        float filter   = 35.f;
        float volume   = 60.f;
        float slew     = 68.f;
        float asym     = 20.f;
        int   clipMode = 0;
        float sag      = 30.f;
    };

    TurboRat()  = default;
    ~TurboRat() = default;

    void prepare (double sampleRate, int samplesPerBlock);
    void setParameters (const Parameters& p);
    void process (juce::AudioBuffer<float>& buffer, int numSamples);
    void reset();

    // Internal hardware constants — not exposed as parameters
    static constexpr float kDefaultSlew = 0.68f;
    static constexpr float kDefaultAsym = 0.20f;

private:
    double sr = 44100.0;
    Parameters params;
};
