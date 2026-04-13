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
        float heatRate      = 0.f;
        bool  freeze        = false;
        bool  acetate       = false;
        bool  msMode        = false;
        bool  timerActive   = false;
        float timerProgress = 0.f;
    };

    void  prepare (double sampleRate, int samplesPerBlock);
    void  setParameters (const Parameters& p);
    void  process (juce::AudioBuffer<float>& buffer, int numSamples);
    void  reset();

    void  setPersistedTemp (float t, float tPrev) { temp = t; tempPrev = tPrev; burn = t; }
    float getCurrentTemp()  const { return temp; }
    float getPreviousTemp() const { return tempPrev; }

private:
    double sr = 44100.0;
    Parameters params;

    // Thermal accumulator — temp in [0, 1]
    float temp     = 0.0f;
    float tempPrev = 0.0f;
    // At heatRate=1.0, temp reaches 1.0 after kThermalTimeSec seconds
    static constexpr float kThermalTimeSec = 60.0f;

    // Effective burn value after thermal + timer resolution
    float burn = 0.0f;

    // Macro-expanded parameters (updated each block in expandMacros())
    float drive, msSat, biasAmt, wowDepth, fltDepth;
    float hissLevel, aspNoise, bumpGainDb;

    // Jiles-Atherton state (per channel)
    float M_L = 0.0f, H_prev_L = 0.0f;
    float M_R = 0.0f, H_prev_R = 0.0f;
    double biasPhase = 0.0;  // 55kHz bias oscillator

    // Head bump: peaking biquad at 90Hz, Q=1.5 (always on)
    BiquadFilter headBumpL, headBumpR;

    // HF loss: 2nd-order Butterworth LP at 10.5kHz
    BiquadFilter hfLossL, hfLossR;

    // Wow/flutter variable-delay buffer (stereo, allocated in prepare())
    std::vector<float> wowBufL, wowBufR;
    int wowWritePos = 0;
    int wowBufSize  = 0;

    // Wow: LP-filtered noise at ~0.7Hz
    float wowState = 0.0f;   // first-order LP state
    float wowAlpha = 0.0f;   // LP coefficient (set in prepare())

    // Flutter: LFO at ~8Hz with noise perturbation
    double flutterPhase = 0.0;
    float  flutterNoise = 0.0f;  // smoothed noise added to flutter phase

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
