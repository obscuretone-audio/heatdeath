#include "Undulator.h"
#include <cmath>
#include <algorithm>

static constexpr double kTwoPi = 6.283185307179586;

// Q15 two's-complement wrap-around: folds signal at ±1 like 16-bit fixed-point overflow.
// x = 1.2 → -0.8; x = -1.3 → 0.7  (hard fold, not clip — the TMS32010 character)
static inline float q15Wrap (float x) noexcept
{
    return x - 2.0f * std::floor ((x + 1.0f) * 0.5f);
}

//==============================================================================
// High-shelf biquad — Audio EQ Cookbook, S=1 (shelf slope = 1)
// With S=1: alpha = sin(w0) * sqrt(2) / 2
//==============================================================================
void Undulator::setHighShelf (BiquadFilter& f, double sr,
                               float freqHz, float gainDb) noexcept
{
    const double A     = std::pow (10.0, gainDb / 40.0);
    const double w0    = kTwoPi * freqHz / sr;
    const double cosw0 = std::cos (w0);
    const double sinw0 = std::sin (w0);
    const double alpha = sinw0 * std::sqrt (2.0) / 2.0;  // S=1
    const double sqA   = std::sqrt (A);

    const double b0 =    A * ((A + 1.0) + (A - 1.0) * cosw0 + 2.0 * sqA * alpha);
    const double b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cosw0);
    const double b2 =    A * ((A + 1.0) + (A - 1.0) * cosw0 - 2.0 * sqA * alpha);
    const double a0 =        (A + 1.0) - (A - 1.0) * cosw0 + 2.0 * sqA * alpha;
    const double a1 =  2.0 * ((A - 1.0) - (A + 1.0) * cosw0);
    const double a2 =        (A + 1.0) - (A - 1.0) * cosw0 - 2.0 * sqA * alpha;

    f.b0 = static_cast<float> (b0 / a0);
    f.b1 = static_cast<float> (b1 / a0);
    f.b2 = static_cast<float> (b2 / a0);
    f.a1 = static_cast<float> (a1 / a0);
    f.a2 = static_cast<float> (a2 / a0);
}

//==============================================================================
void Undulator::prepare (double sampleRate, int /*samplesPerBlock*/)
{
    sr = sampleRate;

    // Max delay: right channel at spread=1.0 is 100ms; +2 guard samples.
    delayBufSize = static_cast<int> (0.100 * sr) + 2;
    delayBufL.assign (static_cast<size_t> (delayBufSize), 0.0f);
    delayBufR.assign (static_cast<size_t> (delayBufSize), 0.0f);
    delayWritePos = 0;

    // Pre/de-emphasis: +6dB / −6dB high shelf at 3kHz (H3000 ADC/DAC character).
    setHighShelf (preEmphL, sr, 3000.0f, +6.0f);
    setHighShelf (preEmphR, sr, 3000.0f, +6.0f);
    setHighShelf (deEmphL,  sr, 3000.0f, -6.0f);
    setHighShelf (deEmphR,  sr, 3000.0f, -6.0f);

    reset();
}

//==============================================================================
void Undulator::setParameters (const Parameters& p)
{
    params = p;
}

//==============================================================================
void Undulator::process (juce::AudioBuffer<float>& buffer, int numSamples)
{
    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getWritePointer (1);

    // Delay times are constant within a block (params set once per block).
    // L = half of spread, R = full — creates asymmetric stereo spread.
    const int delaySamplesL = std::max (1, std::min (
        static_cast<int> (params.spread * 0.050 * sr), delayBufSize - 1));
    const int delaySamplesR = std::max (1, std::min (
        static_cast<int> (params.spread * 0.100 * sr), delayBufSize - 1));

    // R channel LFO phase offset, normalised [0, 1)
    const double phaseOffR = params.phase / 360.0;

    // Grit LP cutoff: 16kHz at grit=0, 4kHz at grit=1 — computed once per block
    const float gritLPCutoff = 16000.0f - params.grit * 12000.0f;
    const float gritLPAlpha  = static_cast<float> (
        std::exp (-kTwoPi * static_cast<double> (gritLPCutoff) / sr));

    for (int i = 0; i < numSamples; ++i)
    {
        const float inL = L[i];
        const float inR = R[i];

        // ------------------------------------------------------------------
        // Envelope follower — used only for shape=4, but always updated.
        // Attack ~5ms, release ~100ms.
        // ------------------------------------------------------------------
        {
            const float absIn = std::abs (inL + inR) * 0.5f;
            const float kA = 0.9989f;   // ~5ms attack  @ 44100
            const float kR = 0.9998f;   // ~100ms release
            envL = absIn > envL ? kA * envL + (1.0f - kA) * absIn : kR * envL;
            envR = envL;  // mono follower broadcast to both channels
        }

        // ------------------------------------------------------------------
        // Secondary (modulation) LFO — always Sine
        // Modulates primary rate (modSpeed) and depth (modDepth)
        // ------------------------------------------------------------------
        const float modLfo    = static_cast<float> (std::sin (kTwoPi * modPhase));
        modPhase += params.modRate / sr;
        modPhase -= std::floor (modPhase);

        const float currentRate  = params.rate *
            (1.0f + modLfo * params.modSpeed * 0.5f);
        const float currentDepth = std::clamp (
            params.depth + modLfo * params.modDepth * 0.3f, 0.0f, 1.0f);

        // ------------------------------------------------------------------
        // Brownian drift — tiny random phase perturbation per channel.
        // Scale chosen so drift=1.0 produces noticeable but slow wander.
        // ------------------------------------------------------------------
        {
            const float scale = static_cast<float> (params.drift) * 3.0e-5f;
            driftL += (rng.nextFloat() - 0.5f) * scale;
            driftL -= std::floor (driftL);
            driftR += (rng.nextFloat() - 0.5f) * scale;
            driftR -= std::floor (driftR);
        }

        // ------------------------------------------------------------------
        // Primary LFO value per channel
        // phL/phR are not explicitly wrapped — sin/cos are periodic and the
        // triangle/peak lambdas use fmod internally.
        // ------------------------------------------------------------------
        const double phL = lfoPhase + driftL;
        const double phR = lfoPhase + phaseOffR + driftR;

        float lfoL, lfoR;

        switch (params.shape)
        {
            case 1:  // Triangle: linear rise then fall
            {
                auto tri = [] (double ph) -> float
                {
                    const double p = ph - std::floor (ph);   // wrap to [0,1)
                    return static_cast<float> (p < 0.5 ? p * 4.0 - 1.0
                                                        : 3.0 - p * 4.0);
                };
                lfoL = tri (phL);
                lfoR = tri (phR);
                break;
            }
            case 2:  // Peak: fast rise (1/4 cycle), slow fall (3/4 cycle)
            {
                auto peak = [] (double ph) -> float
                {
                    const double p = ph - std::floor (ph);
                    if (p < 0.25)
                        return static_cast<float> (p * 4.0);
                    return static_cast<float> (1.0 - (p - 0.25) * (4.0 / 3.0));
                };
                lfoL = peak (phL);
                lfoR = peak (phR);
                break;
            }
            case 3:  // Random: two inharmonic sines (ratio sqrt(2))
            {
                lfoL = 0.5f * (static_cast<float> (std::sin (kTwoPi * rndPhase1))
                             + static_cast<float> (std::sin (kTwoPi * rndPhase2)));
                // R offset applied as additive phase to both inharmonic sines
                lfoR = 0.5f * (static_cast<float> (std::sin (kTwoPi * rndPhase1 + kTwoPi * phaseOffR))
                             + static_cast<float> (std::sin (kTwoPi * rndPhase2 + kTwoPi * phaseOffR)));
                rndPhase1 += currentRate / sr;
                rndPhase1 -= std::floor (rndPhase1);
                rndPhase2 += currentRate * 1.41421356237 / sr;
                rndPhase2 -= std::floor (rndPhase2);
                break;
            }
            case 4:  // Envelope: amplitude follower maps [0,1] → [−1,1]
                lfoL = std::clamp (envL * 2.0f - 1.0f, -1.0f, 1.0f);
                lfoR = lfoL;
                break;
            default: // 0: Sine
                lfoL = static_cast<float> (std::sin (kTwoPi * phL));
                lfoR = static_cast<float> (std::sin (kTwoPi * phR));
                break;
        }

        // ------------------------------------------------------------------
        // Detuned feedback delay
        // L delay = spread*50ms (half), R delay = spread*100ms (full)
        // ------------------------------------------------------------------
        const int readPosL = (delayWritePos - delaySamplesL + delayBufSize) % delayBufSize;
        const int readPosR = (delayWritePos - delaySamplesR + delayBufSize) % delayBufSize;

        const float delayedL = delayBufL[static_cast<size_t> (readPosL)];
        const float delayedR = delayBufR[static_cast<size_t> (readPosR)];

        delayBufL[static_cast<size_t> (delayWritePos)] = inL + delayedL * params.feedback;
        delayBufR[static_cast<size_t> (delayWritePos)] = inR + delayedR * params.feedback;
        if (++delayWritePos >= delayBufSize)
            delayWritePos = 0;

        // ------------------------------------------------------------------
        // Pre-emphasis: +6dB HF shelf at 3kHz (H3000 ADC character)
        // ------------------------------------------------------------------
        float procL = preEmphL.process (delayedL);
        float procR = preEmphR.process (delayedR);

        // ------------------------------------------------------------------
        // Grit: TMS32010 Q15 two's-complement wrap-around saturation.
        // Quadratic drive curve (1×–7×) gives gradual onset at low grit.
        // Post-wrap LP (16kHz→4kHz as grit increases) tames harsh harmonics.
        // ------------------------------------------------------------------
        if (params.grit > 0.001f)
        {
            const float drive = 1.0f + params.grit * params.grit * 6.0f;
            procL = q15Wrap (procL * drive) / drive;
            procR = q15Wrap (procR * drive) / drive;

            gritLPL = gritLPAlpha * gritLPL + (1.0f - gritLPAlpha) * procL;
            gritLPR = gritLPAlpha * gritLPR + (1.0f - gritLPAlpha) * procR;
            procL = gritLPL;
            procR = gritLPR;
        }

        // ------------------------------------------------------------------
        // AM envelope: never clips, never fully silences at depth < 1.0.
        // Range is [1−depth, 1] as lfo sweeps [−1, +1].
        // ------------------------------------------------------------------
        procL *= (1.0f - currentDepth) + currentDepth * (lfoL * 0.5f + 0.5f);
        procR *= (1.0f - currentDepth) + currentDepth * (lfoR * 0.5f + 0.5f);

        // ------------------------------------------------------------------
        // De-emphasis: −6dB HF shelf at 3kHz (H3000 DAC character).
        // Mirrors pre-emphasis — clipping harmonics are softened in HF.
        // ------------------------------------------------------------------
        procL = deEmphL.process (procL);
        procR = deEmphR.process (procR);

        // ------------------------------------------------------------------
        // Wet/dry blend
        // ------------------------------------------------------------------
        L[i] = inL * (1.0f - params.mix) + procL * params.mix;
        R[i] = inR * (1.0f - params.mix) + procR * params.mix;

        // Advance primary LFO (after value computed, before next sample)
        lfoPhase += currentRate / sr;
        lfoPhase -= std::floor (lfoPhase);
    }
}

//==============================================================================
void Undulator::reset()
{
    lfoPhase  = 0.0;
    modPhase  = 0.0;
    rndPhase1 = 0.0;
    rndPhase2 = 0.0;
    driftL    = 0.0;
    driftR    = 0.0;
    envL      = 0.0f;
    envR      = 0.0f;
    gritLPL   = 0.0f;
    gritLPR   = 0.0f;
    delayWritePos = 0;

    std::fill (delayBufL.begin(), delayBufL.end(), 0.0f);
    std::fill (delayBufR.begin(), delayBufR.end(), 0.0f);

    preEmphL.reset(); preEmphR.reset();
    deEmphL.reset();  deEmphR.reset();
}
