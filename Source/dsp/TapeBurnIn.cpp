#include "BurnIn.h"
#include <cmath>
#include <algorithm>

static constexpr double kTwoPi = 6.283185307179586;

//==============================================================================
// Jiles-Atherton ODE derivative: dM/dH
// H, M        — current field and magnetization
// delta       — sign of dH/dt (+1 or -1)
// Ms,k,a,c,alpha — JA material parameters
//==============================================================================
float BurnIn::jaDeriv (float H, float M, float delta,
                        float Ms, float k, float a, float c, float alpha) const noexcept
{
    const float He = H + alpha * M;
    const float x  = He / a;

    float Man, dManDHe;
    if (std::abs (x) < 1.0e-4f)
    {
        // Taylor series avoids coth singularity at x→0
        Man      = Ms * x / 3.0f;
        dManDHe  = Ms / (3.0f * a);
    }
    else
    {
        const float cothX = 1.0f / std::tanh (x);
        Man     = Ms * (cothX - 1.0f / x);
        dManDHe = (Ms / a) * (1.0f / (x * x) - cothX * cothX + 1.0f);
    }

    const float denom_irr = delta * k - alpha * (Man - M);
    if (std::abs (denom_irr) < 1.0e-8f) return 0.0f;

    const float dMirr_dHe = (Man - M) / denom_irr;
    const float num        = (1.0f - c) * dMirr_dHe + c * dManDHe;
    const float denom      = 1.0f - alpha * num;
    if (std::abs (denom) < 1.0e-8f) return 0.0f;

    return num / denom;
}

//==============================================================================
// 4th-order Runge-Kutta step over one H interval
//==============================================================================
float BurnIn::jaRK4 (float Hprev, float Hcurr, float Mprev, float delta,
                      float Ms, float k, float a, float c, float alpha) const noexcept
{
    const float dH  = Hcurr - Hprev;
    const float dH2 = dH * 0.5f;

    const float k1 = jaDeriv (Hprev,        Mprev,              delta, Ms, k, a, c, alpha);
    const float k2 = jaDeriv (Hprev + dH2,  Mprev + dH2 * k1,  delta, Ms, k, a, c, alpha);
    const float k3 = jaDeriv (Hprev + dH2,  Mprev + dH2 * k2,  delta, Ms, k, a, c, alpha);
    const float k4 = jaDeriv (Hcurr,        Mprev + dH  * k3,  delta, Ms, k, a, c, alpha);

    return Mprev + (dH / 6.0f) * (k1 + 2.0f * k2 + 2.0f * k3 + k4);
}

//==============================================================================
// Paul Kellet pink noise (7-pole approximation)
//==============================================================================
float BurnIn::pink (std::array<float, 7>& b, float w) const noexcept
{
    b[0] = 0.99886f * b[0] + w * 0.0555179f;
    b[1] = 0.99332f * b[1] + w * 0.0750759f;
    b[2] = 0.96900f * b[2] + w * 0.1538520f;
    b[3] = 0.86650f * b[3] + w * 0.3104856f;
    b[4] = 0.55000f * b[4] + w * 0.5329522f;
    b[5] = -0.7616f * b[5] - w * 0.0168980f;
    const float out = b[0]+b[1]+b[2]+b[3]+b[4]+b[5]+b[6] + w * 0.5362f;
    b[6] = w * 0.115926f;
    return out * 0.11f;  // scale to roughly ±1
}

//==============================================================================
// Peaking EQ biquad — Audio EQ Cookbook
//==============================================================================
void BurnIn::setPeakingEQ (BiquadFilter& f, double sr,
                            float freq, float Q, float gainDb) noexcept
{
    const double A     = std::pow (10.0, gainDb / 40.0);
    const double w0    = kTwoPi * freq / sr;
    const double alpha = std::sin (w0) / (2.0 * Q);
    const double cosw0 = std::cos (w0);

    const double a0 = 1.0 + alpha / A;
    f.b0 = static_cast<float> ((1.0 + alpha * A) / a0);
    f.b1 = static_cast<float> ((-2.0 * cosw0)    / a0);
    f.b2 = static_cast<float> ((1.0 - alpha * A) / a0);
    f.a1 = static_cast<float> ((-2.0 * cosw0)    / a0);
    f.a2 = static_cast<float> ((1.0 - alpha / A) / a0);
}

//==============================================================================
// 2nd-order Butterworth LP biquad
//==============================================================================
void BurnIn::setButterworthLP (BiquadFilter& f, double sr, float freq) noexcept
{
    const double w0    = kTwoPi * freq / sr;
    const double cosw0 = std::cos (w0);
    const double sinw0 = std::sin (w0);
    const double alpha = sinw0 / (2.0 * std::sqrt (2.0));  // Butterworth Q = 1/sqrt(2)
    const double a0    = 1.0 + alpha;

    f.b0 = static_cast<float> ((1.0 - cosw0) * 0.5 / a0);
    f.b1 = static_cast<float> ((1.0 - cosw0)       / a0);
    f.b2 = f.b0;
    f.a1 = static_cast<float> ((-2.0 * cosw0)      / a0);
    f.a2 = static_cast<float> ((1.0 - alpha)        / a0);
}

//==============================================================================
// Macro expansion: derive all internal parameters from burn [0, 1]
//==============================================================================
void BurnIn::expandMacros() noexcept
{
    const float b = burn;
    drive      = b * 0.85f;          // was 0.55 — push harder into JA saturation
    msSat      = 0.15f + b * 0.75f;
    biasAmt    = 0.75f - b * 0.35f;
    wowDepth   = b * b * 0.007f;
    fltDepth   = b * b * 0.0008f;   // was 0.0025 — flutter was too strong at max
    hissLevel  = b * 0.005f;        // was 0.010 — further reduce noise floor
    aspNoise   = b * b * 0.010f;    // was 0.020 — halved
    bumpGainDb = 1.5f + b * 3.5f;

    // Recompute head bump gain (only gain changes, freq/Q are fixed)
    setPeakingEQ (headBumpL, sr, 90.0f, 1.5f, bumpGainDb);
    setPeakingEQ (headBumpR, sr, 90.0f, 1.5f, bumpGainDb);

    // HF loss cutoff tracks burn: 10.5kHz (cold) → 5kHz (full burn)
    const float hfCutoff = 10500.0f - b * 5500.0f;
    setButterworthLP (hfLossL, sr, hfCutoff);
    setButterworthLP (hfLossR, sr, hfCutoff);
}

//==============================================================================
// prepare
//==============================================================================
void BurnIn::prepare (double sampleRate, int /*samplesPerBlock*/)
{
    sr = sampleRate;

    // Wow/flutter delay buffer: max ~20ms at any sample rate
    wowBufSize = static_cast<int> (0.020 * sr) + 4;
    wowBufL.assign (static_cast<size_t> (wowBufSize), 0.0f);
    wowBufR.assign (static_cast<size_t> (wowBufSize), 0.0f);
    wowWritePos = 0;

    // Wow LP: one-pole at 0.7Hz
    wowAlpha = static_cast<float> (
        std::exp (-kTwoPi * 0.7 / sr));

    // HF loss (fixed, 15ips tape speed)
    setButterworthLP (hfLossL, sr, 10500.0f);
    setButterworthLP (hfLossR, sr, 10500.0f);

    // Head bump — gain recomputed in expandMacros(), set initial here
    setPeakingEQ (headBumpL, sr, 90.0f, 1.5f, 1.5f);
    setPeakingEQ (headBumpR, sr, 90.0f, 1.5f, 1.5f);

    reset();
}

//==============================================================================
void BurnIn::setParameters (const Parameters& p)
{
    params = p;
}

//==============================================================================
// process
//==============================================================================
void BurnIn::process (juce::AudioBuffer<float>& buffer, int numSamples)
{
    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getWritePointer (1);

    //--------------------------------------------------------------------------
    // 1. Set burn directly from knob — no thermal accumulation
    //--------------------------------------------------------------------------
    burn = params.amount;
    expandMacros();

    //--------------------------------------------------------------------------
    // 2. JA material parameters (Ferric Oxide vs Acetate)
    // Ms scaled by msSat macro; Acetate raises k and lowers c for sharper transitions.
    //--------------------------------------------------------------------------
    const float Ms    = 3.5e5f * msSat;
    const float k_ja  = params.acetate ? 35.0e3f : 27.0e3f;
    const float a_ja  = params.acetate ? 18.0e3f : 22.0e3f;
    const float c_ja  = params.acetate ?  0.08f  :  0.17f;
    const float al_ja = params.acetate ?  1.8e-3f :  1.6e-3f;

    // Input scale: maps audio ±1.0 to H ≈ ±5*a (moderate saturation at drive=0.55)
    const float H_scale = 5.0f * a_ja;

    // Drive factor — minimum 0.05 ensures the JA model is always exercised even at burn=0
    const float driveFactor = 0.05f + drive;

    // Output normalisation: makes model transparent in the linear regime at any drive.
    // In linear region M/Ms ≈ H / (3·a), so we divide back by the same ratio.
    // This gives unity gain at low burn and graceful compression as saturation kicks in.
    const float jaOutGain = 3.0f * a_ja / (H_scale * driveFactor);

    // Bias note: the spec calls for a 55kHz ultrasonic signal to linearise the recording.
    // At 44.1kHz native SR, 55kHz = 1.25 cycles/sample — can't be resolved without
    // 192kHz+. Instead, biasAmt modulates the reversibility parameter (c) to achieve
    // the same linearisation effect: high bias → more reversible (linear); low bias
    // (high burn) → more hysteretic with subtle zero-crossing deadzone character.
    const float c_eff = c_ja * (0.4f + biasAmt * 1.5f);  // range: c*0.4 – c*1.9

    //--------------------------------------------------------------------------
    // Per-sample loop
    //--------------------------------------------------------------------------
    for (int i = 0; i < numSamples; ++i)
    {
        float inL = L[i];
        float inR = R[i];

        // -- M/S decode --
        float mid = 0.0f, side = 0.0f;
        if (params.msMode)
        {
            mid  = (inL + inR) * 0.5f;
            side = (inL - inR) * 0.5f;
            inL  = mid;
            inR  = side;
        }

        // -- Record head drive --
        const float H_L = inL * H_scale * driveFactor;
        const float H_R = inR * H_scale * driveFactor;

        // -- Jiles-Atherton hysteresis (4 substeps ≈ 4× oversampling) --
        //    Each substep linearly interpolates H and advances the ODE.
        {
            const float dH4_L = (H_L - H_prev_L) * 0.25f;
            const float dH4_R = (H_R - H_prev_R) * 0.25f;
            const float delta_L = (H_L >= H_prev_L) ? 1.0f : -1.0f;
            const float delta_R = (H_R >= H_prev_R) ? 1.0f : -1.0f;

            for (int sub = 0; sub < 4; ++sub)
            {
                const float Hs_L = H_prev_L + dH4_L * (sub + 1.0f);
                const float Hs_R = H_prev_R + dH4_R * (sub + 1.0f);

                M_L = jaRK4 (H_prev_L + dH4_L * static_cast<float>(sub),
                              Hs_L, M_L, delta_L, Ms, k_ja, a_ja, c_eff, al_ja);
                M_R = jaRK4 (H_prev_R + dH4_R * static_cast<float>(sub),
                              Hs_R, M_R, delta_R, Ms, k_ja, a_ja, c_eff, al_ja);
            }
        }
        H_prev_L = H_L;
        H_prev_R = H_R;

        // Normalise magnetization to audio range — jaOutGain restores unity gain
        // in the linear regime so burn=0 is transparent
        float outL = (M_L / Ms) * jaOutGain;
        float outR = (M_R / Ms) * jaOutGain;

        // -- Wow/flutter variable delay --
        // Update wow: LP-filtered white noise at 0.7Hz
        {
            const float wn = rng.nextFloat() * 2.0f - 1.0f;
            wowState = wowAlpha * wowState + (1.0f - wowAlpha) * wn;
        }
        // Update flutter: ~8Hz LFO + noise perturbation
        {
            flutterPhase += 8.0 / sr;
            flutterPhase -= std::floor (flutterPhase);
            const float fn = rng.nextFloat() * 2.0f - 1.0f;
            flutterNoise = 0.995f * flutterNoise + 0.005f * fn;
        }

        const float wowDel  = wowDepth   * wowState;
        const float fltDel  = fltDepth   * (static_cast<float>(std::sin(kTwoPi * flutterPhase)) + flutterNoise * 0.3f);
        const float delSec  = std::max (0.0f, wowDel + fltDel);
        const float delSamp = delSec * static_cast<float> (sr);
        const int   delInt  = std::min (static_cast<int> (delSamp), wowBufSize - 2);
        const float delFrac = delSamp - static_cast<float> (delInt);

        // Write to delay buffer
        wowBufL[static_cast<size_t> (wowWritePos)] = outL;
        wowBufR[static_cast<size_t> (wowWritePos)] = outR;

        // Linear interpolation readback
        const int r0 = (wowWritePos - delInt + wowBufSize)     % wowBufSize;
        const int r1 = (wowWritePos - delInt - 1 + wowBufSize) % wowBufSize;
        outL = wowBufL[static_cast<size_t>(r0)] * (1.0f - delFrac)
             + wowBufL[static_cast<size_t>(r1)] * delFrac;
        outR = wowBufR[static_cast<size_t>(r0)] * (1.0f - delFrac)
             + wowBufR[static_cast<size_t>(r1)] * delFrac;

        if (++wowWritePos >= wowBufSize) wowWritePos = 0;

        // -- Head bump --
        outL = headBumpL.process (outL);
        outR = headBumpR.process (outR);

        // -- HF loss (playback head gap at 15ips) --
        outL = hfLossL.process (outL);
        outR = hfLossR.process (outR);

        // -- Noise: static hiss (pink) + asperity (signal-correlated) --
        {
            const float wL = rng.nextFloat() * 2.0f - 1.0f;
            const float wR = rng.nextFloat() * 2.0f - 1.0f;

            // Pink hiss
            outL += pink (pinkL, wL) * hissLevel;
            outR += pink (pinkR, wR) * hissLevel;

            // Asperity: envelope-tracked noise — louder when signal is louder
            const float aspK = 0.9995f;
            aspEnvL = aspK * aspEnvL + (1.0f - aspK) * std::abs (outL);
            aspEnvR = aspK * aspEnvR + (1.0f - aspK) * std::abs (outR);
            outL += wL * aspEnvL * aspNoise;
            outR += wR * aspEnvR * aspNoise;
        }

        // -- M/S re-encode --
        if (params.msMode)
        {
            L[i] = outL + outR;   // L = M + S
            R[i] = outL - outR;   // R = M - S
        }
        else
        {
            L[i] = outL;
            R[i] = outR;
        }
    }
}

//==============================================================================
void BurnIn::reset()
{
    burn = 0.0f;
    M_L = M_R = H_prev_L = H_prev_R = 0.0f;
    flutterPhase = 0.0;
    wowState = flutterNoise = 0.0f;
    aspEnvL = aspEnvR = 0.0f;
    wowWritePos = 0;
    pinkL.fill (0.0f); pinkR.fill (0.0f);

    std::fill (wowBufL.begin(), wowBufL.end(), 0.0f);
    std::fill (wowBufR.begin(), wowBufR.end(), 0.0f);

    headBumpL.reset(); headBumpR.reset();
    hfLossL.reset();   hfLossR.reset();
}

