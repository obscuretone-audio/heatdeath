#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include "../utils/BiquadFilter.h"
#include <vector>
#include <array>

class BurnIn
{
public:
    struct Parameters
    {
        float amount  = 0.f;   // 0–1 direct burn level — knob maps here directly
        bool  acetate = false;
        bool  msMode  = false;
    };

    void  prepare (double sampleRate, int samplesPerBlock);
    void  setParameters (const Parameters& p);
    void  process (juce::AudioBuffer<float>& buffer, int numSamples);
    void  reset();

private:
    double sr = 44100.0;
    Parameters params;

    // Effective burn value — set directly from params.amount each block
    float burn = 0.0f;

    // Macro-expanded parameters (updated each block in expandMacros())
    float drive, msSat, biasAmt, wowDepth, fltDepth;
    float hissLevel, aspNoise, bumpGainDb;

    // Jiles-Atherton state (per channel)
    float M_L = 0.0f, H_prev_L = 0.0f;
    float M_R = 0.0f, H_prev_R = 0.0f;

    // Head bump: peaking biquad at 90Hz, Q=1.5 (always on)
    BiquadFilter headBumpL, headBumpR;

    // HF loss: 2nd-order Butterworth LP at 10.5kHz
    BiquadFilter hfLossL, hfLossR;

    // Wow/flutter variable-delay buffer (stereo, allocated in prepare())
    std::vector<float> wowBufL, wowBufR;
    int wowWritePos = 0;
    int wowBufSize  = 0;

    // Wow: LP-filtered noise at ~0.7Hz
    float wowState = 0.0f;
    float wowAlpha = 0.0f;

    // Flutter: LFO at ~8Hz with noise perturbation
    double flutterPhase = 0.0;
    float  flutterNoise = 0.0f;

    // Pink noise — Paul Kellet 7-pole method (per channel)
    std::array<float, 7> pinkL{}, pinkR{};

    // Asperity noise envelope follower
    float aspEnvL = 0.0f, aspEnvR = 0.0f;

    juce::Random rng;

    // Helpers
    void  expandMacros() noexcept;
    float jaDeriv (float H, float M, float delta,
                   float Ms, float k, float a, float c, float alpha) const noexcept;
    float jaRK4   (float Hprev, float Hcurr, float Mprev, float delta,
                   float Ms, float k, float a, float c, float alpha) const noexcept;
    float pink    (std::array<float, 7>& b, float white) const noexcept;

    static void setPeakingEQ    (BiquadFilter& f, double sr, float freq, float Q, float gainDb) noexcept;
    static void setButterworthLP(BiquadFilter& f, double sr, float freq) noexcept;
};
