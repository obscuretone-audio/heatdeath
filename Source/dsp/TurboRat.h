#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "../utils/SmoothParam.h"

class TurboRat
{
public:
    struct Parameters
    {
        float drive    = 72.f;
        float filter   = 50.f;  // corrected default per 03-RESEARCH.md Pitfall 6
        float volume   = 65.f;  // corrected default per 03-RESEARCH.md Pitfall 6
        float slew     = 68.f;
        float asym     = 20.f;
        int   clipMode = 0;
        float sag      = 25.f;  // corrected default per 03-RESEARCH.md Pitfall 6
    };

    TurboRat()  = default;
    ~TurboRat() = default;

    void prepare (double sampleRate, int samplesPerBlock);
    void setParameters (const Parameters& p);

    // Phase 2 pass-through — kept for build compatibility; body is a no-op.
    void process (juce::AudioBuffer<float>& buffer, int numSamples);

    // Phase 3: called with the 4x oversampled mono block from PluginProcessor.
    // Contains: HPF chain (03-01) + slew LP (03-02) + GBW LP (03-02)
    //           + waveshaper (03-03) + tone LPF (03-04) + JFET buffer (03-04).
    void processOS (juce::dsp::AudioBlock<float>& osBlock);

    // updateCoefficients — called once per block inside processOS.
    // Derives per-block IIR alpha coefficients from smoothed param values.
    // Never called per-sample.
    void updateCoefficients (double osSampleRate);

    void reset();

    // Internal hardware constants — not exposed as parameters
    static constexpr float kDefaultSlew = 0.68f;
    static constexpr float kDefaultAsym = 0.20f;

private:
    double sr               = 44100.0;
    double osSr             = 176400.0;
    int    osSamplesPerBlock = 2048;

    Parameters params;

    // SmoothParam instances (configured at HOST rate in prepare())
    SmoothParam smoothDrive, smoothFilter, smoothVolume, smoothAsym;

    // HPF state — one-pole HPF needs prevIn + prevOut per filter
    float hpf1PrevIn  = 0.0f, hpf1PrevOut = 0.0f;  // 60Hz
    float hpf2PrevIn  = 0.0f, hpf2PrevOut = 0.0f;  // 1.5kHz

    // Pre-computed per-block coefficients
    float hpf1Alpha = 0.0f;
    float hpf2Alpha = 0.0f;

    // Helper: compute one-pole HPF alpha from cutoff frequency and sample rate
    static float computeHPFAlpha (float cutoffHz, double sampleRate) noexcept;
};
