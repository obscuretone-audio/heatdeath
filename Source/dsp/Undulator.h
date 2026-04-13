#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include "../utils/BiquadFilter.h"
#include <vector>

class Undulator
{
public:
    struct Parameters
    {
        float rate     = 2.4f;    // Hz, primary LFO rate
        float depth    = 0.82f;   // 0–1, AM depth
        float phase    = 180.f;   // degrees, R channel LFO offset (180 = antiphase)
        float drift    = 0.40f;   // 0–1, Brownian phase wander amount
        float modRate  = 0.6f;    // Hz, secondary LFO rate
        float modDepth = 0.45f;   // 0–1, secondary → primary depth modulation
        float modSpeed = 0.35f;   // 0–1, secondary → primary rate modulation
        float spread   = 0.55f;   // 0–1, detuned delay depth (L=half, R=full)
        float feedback = 0.38f;   // 0–1, delay feedback
        float grit     = 0.62f;   // 0–1, TMS32010 Q15 wrap-around saturation
        float mix      = 1.0f;    // 0–1, wet/dry
        int   shape    = 0;       // 0=Sine 1=Tri 2=Peak 3=Random 4=Envelope
    };

    void prepare (double sampleRate, int samplesPerBlock);
    void setParameters (const Parameters& p);
    void process (juce::AudioBuffer<float>& buffer, int numSamples);
    void reset();

private:
    double sr = 44100.0;
    Parameters params;

    // Primary LFO phase accumulator [0, 1)
    double lfoPhase  = 0.0;

    // Secondary (modulation) LFO phase [0, 1)
    double modPhase  = 0.0;

    // Random shape (shape=3): two inharmonic sines at rate and rate*sqrt(2)
    double rndPhase1 = 0.0;
    double rndPhase2 = 0.0;

    // Brownian drift offsets [0, 1) — separate per channel
    double driftL = 0.0;
    double driftR = 0.0;

    // Envelope follower for shape=4
    float envL = 0.0f;
    float envR = 0.0f;

    // Circular delay buffers — allocated in prepare()
    std::vector<float> delayBufL, delayBufR;
    int delayWritePos = 0;
    int delayBufSize  = 0;

    // H3000 pre-emphasis (+6dB HF shelf) and de-emphasis (−6dB HF shelf) at 3kHz
    BiquadFilter preEmphL, preEmphR;
    BiquadFilter deEmphL,  deEmphR;

    juce::Random rng;

    // Compute high-shelf biquad coefficients (Audio EQ Cookbook, S=1)
    static void setHighShelf (BiquadFilter& f, double sampleRate,
                               float freqHz, float gainDb) noexcept;
};
