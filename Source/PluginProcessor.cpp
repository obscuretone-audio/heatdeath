#include "PluginProcessor.h"
#include "PluginEditor.h"

// DSP stage headers — implemented separately
#include "dsp/TurboRat.h"
#include "dsp/MicroPitch.h"
#include "dsp/Undulator.h"
#include "dsp/BurnIn.h"

//==============================================================================
// Constructor
//==============================================================================

HeatDeathProcessor::HeatDeathProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::mono(),   true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "HEATDEATH_STATE", Params::createParameterLayout())
{
    // Construct DSP stages. They are initialised (sample rate, buffer size) in
    // prepareToPlay() — not here. Constructors must not allocate audio buffers.
    stageTurboRat   = std::make_unique<TurboRat>();
    stageMicroPitch = std::make_unique<MicroPitch>();
    stageUndulator  = std::make_unique<Undulator>();
    stageBurnIn     = std::make_unique<BurnIn>();

    // Cache raw parameter pointers immediately after APVTS construction.
    // These are stable for the lifetime of the processor — APVTS never
    // reallocates its parameter storage.
    cacheParameterPointers();

    // Input limiter waveshaper — soft clip at unity threshold.
    // Protects Stage 1 from pathological inputs. Initialised here since it
    // has no sample-rate dependency.
    inputLimiter.functionToUse = [] (float x)
    {
        // fast_tanh approximation — identical to what the DSP stages use
        return juce::dsp::FastMathApproximations::tanh (x);
    };
}

HeatDeathProcessor::~HeatDeathProcessor() = default;

//==============================================================================
// prepareToPlay
//==============================================================================

void HeatDeathProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize  = samplesPerBlock;

    juce::dsp::ProcessSpec spec { sampleRate,
                                  static_cast<juce::uint32> (samplesPerBlock),
                                  2u };  // stereo spec for stereo stages

    juce::dsp::ProcessSpec monoSpec { sampleRate,
                                      static_cast<juce::uint32> (samplesPerBlock),
                                      1u };

    // Input limiter
    inputLimiter.prepare (monoSpec);

    // DSP stages — each stage is responsible for its own oversampling setup
    stageTurboRat->prepare   (sampleRate, samplesPerBlock);
    stageMicroPitch->prepare (sampleRate, samplesPerBlock);
    stageUndulator->prepare  (sampleRate, samplesPerBlock);
    stageBurnIn->prepare     (sampleRate, samplesPerBlock);

    // DC blocking filters — ~5Hz first-order highpass at each stage boundary.
    const auto  dcCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass (
                               sampleRate, 5.0f);

    dcBlock1L.prepare (monoSpec); dcBlock1L.coefficients = dcCoeffs;
    dcBlock2L.prepare (monoSpec); dcBlock2L.coefficients = dcCoeffs;
    dcBlock2R.prepare (monoSpec); dcBlock2R.coefficients = dcCoeffs;
    dcBlock3L.prepare (monoSpec); dcBlock3L.coefficients = dcCoeffs;
    dcBlock3R.prepare (monoSpec); dcBlock3R.coefficients = dcCoeffs;

    // Global feedback LPF — heavy lowpass at 100Hz. Prevents RF oscillation in
    // the feedback path. Mono — feedback is summed to mono before injection.
    const auto fbCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass (
                              sampleRate, 100.0f);
    feedbackLpf.prepare (monoSpec);
    feedbackLpf.coefficients = fbCoeffs;
    feedbackSample = 0.0f;

    // Bypass smoothers — 10ms window.
    bypassSmoothRat.reset   (sampleRate, 0.01);
    bypassSmoothPitch.reset (sampleRate, 0.01);
    bypassSmoothUnd.reset   (sampleRate, 0.01);
    bypassSmoothBurnin.reset(sampleRate, 0.01);

    bypassSmoothRat.setCurrentAndTargetValue    (1.0f);
    bypassSmoothPitch.setCurrentAndTargetValue  (1.0f);
    bypassSmoothUnd.setCurrentAndTargetValue    (1.0f);
    bypassSmoothBurnin.setCurrentAndTargetValue (1.0f);

    // Global mix smoother — 20ms window.
    globalMixSmooth.reset (sampleRate, 0.02);
    globalMixSmooth.setCurrentAndTargetValue (
        pGlobalMix->load() / 100.0f);

    // Dry buffer — pre-allocated to avoid real-time heap activity.
    // Holds mono input copy for global wet/dry blend.
    dryBuffer.setSize      (1, samplesPerBlock, false, true, false);

    // WR-04: Pre-allocate stereo working and bypass-snapshot buffers.
    // Sized once here; never resized in processBlock.
    workBuffer      .setSize (2, samplesPerBlock, false, true, false);
    preUndBuffer    .setSize (2, samplesPerBlock, false, true, false);
    preBurninBuffer .setSize (2, samplesPerBlock, false, true, false);

    // CHAIN-02: prepare 4x oversampler and report its anti-alias filter latency.
    oversampler.reset();
    oversampler.initProcessing (static_cast<size_t> (samplesPerBlock));
    setLatencySamples (static_cast<int> (oversampler.getLatencyInSamples()));

    // WR-03: reset previousFeedbackSample on every prepare.
    previousFeedbackSample = 0.0f;

    // CHAIN-07 + PARAMS-04: reset and seed the three trim smoothers (20ms window).
    trimPostRatSmooth  .reset (sampleRate, 0.02);
    trimPostPitchSmooth.reset (sampleRate, 0.02);
    trimPostUndSmooth  .reset (sampleRate, 0.02);

    trimPostRatSmooth  .setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pTrimPostRat  ->load()));
    trimPostPitchSmooth.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pTrimPostPitch->load()));
    trimPostUndSmooth  .setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pTrimPostUnd  ->load()));
}

//==============================================================================
// releaseResources
//==============================================================================

void HeatDeathProcessor::releaseResources()
{
    stageTurboRat->reset();
    stageMicroPitch->reset();
    stageUndulator->reset();
    stageBurnIn->reset();

    dcBlock1L.reset();
    dcBlock2L.reset(); dcBlock2R.reset();
    dcBlock3L.reset(); dcBlock3R.reset();
    feedbackLpf.reset();
    feedbackSample = 0.0f;

    oversampler.reset();
}

//==============================================================================
// processBlock
//==============================================================================

void HeatDeathProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                       juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    auto* mono = buffer.getWritePointer (0);

    //--------------------------------------------------------------------------
    // 1. Hold a dry copy of the mono input for global wet/dry blend.
    //--------------------------------------------------------------------------
    dryBuffer.copyFrom (0, 0, buffer, 0, 0, numSamples);

    //--------------------------------------------------------------------------
    // 2. Inject global feedback from previous block output into mono input.
    //    Only active when pGlobalFeedbackActive is true.
    //    Feedback signal is mono, lowpass-filtered at 100Hz, hard-limited to
    //    0.15 of full scale. Added to input before the input limiter.
    //--------------------------------------------------------------------------
    if (pGlobalFeedbackActive->load() > 0.5f)
    {
        const float fbAmount = (pGlobalFeedbackAmount->load() / 100.0f) * 0.15f;
        const float fbStart  = previousFeedbackSample * fbAmount;
        const float fbEnd    = feedbackSample         * fbAmount;
        const float invN     = (numSamples > 0)
                               ? 1.0f / static_cast<float> (numSamples)
                               : 0.0f;
        for (int i = 0; i < numSamples; ++i)
        {
            const float alpha = static_cast<float> (i) * invN;
            mono[i] += fbStart + alpha * (fbEnd - fbStart);
        }
        previousFeedbackSample = feedbackSample;
    }
    else
    {
        previousFeedbackSample = 0.0f;
    }

    //--------------------------------------------------------------------------
    // 3. Input limiter — soft tanh clip at unity.
    //    Applied after feedback injection, before Stage 1.
    //--------------------------------------------------------------------------
    {
        juce::dsp::AudioBlock<float>        block (buffer.getArrayOfWritePointers(),
                                                   1, numSamples);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        inputLimiter.process (ctx);
    }

    //--------------------------------------------------------------------------
    // 4. Stage 1 — Turbo RAT (mono in, mono out).
    //    Runs at 4× oversampling internally.
    //--------------------------------------------------------------------------

    // CHAIN-02: exercise 4x oversampler around Stage 1 RAT.
    // For Phase 2 the stage is a pass-through stub, so we run up/down
    // purely to register anti-alias filter latency with the host and
    // keep the signal path bit-identical. Phase 3 (TurboRat DSP) will
    // move the stage processing onto the oversampled block.
    // Parameters must be set BEFORE processOS so the RAT runs on the current
    // block's knob values, not the previous block's.
    stageTurboRat->setParameters ({
        .drive    = pRatDrive->load(),
        .filter   = pRatFilter->load(),
        .volume   = pRatVolume->load(),
        .slew     = pRatSlew->load(),
        .asym     = pRatAsym->load(),
        .clipMode = static_cast<int> (pRatClipMode->load()),
        .sag      = pRatSag->load()
    });

    bypassSmoothRat.setTargetValue (pRatBypass->load() > 0.5f ? 0.0f : 1.0f);

    {
        juce::dsp::AudioBlock<float> monoBlock (buffer.getArrayOfWritePointers(),
                                                1, static_cast<size_t> (numSamples));
        auto osBlock = oversampler.processSamplesUp (monoBlock);
        stageTurboRat->processOS (osBlock);    // Stage 1 processed on oversampled block (03-01)
        oversampler.processSamplesDown (monoBlock);
    }

    // Stage 1 processed on oversampled block above (03-01).

    // Bypass crossfade (mono)
    for (int i = 0; i < numSamples; ++i)
    {
        const float wet = bypassSmoothRat.getNextValue();
        mono[i] = mono[i] * wet + dryBuffer.getSample (0, i) * (1.0f - wet);
    }

    // RAT wet/dry mix — blend processed with pre-RAT dry signal
    {
        const float ratMix = pRatMix->load() / 100.0f;
        if (ratMix < 0.9999f)
            for (int i = 0; i < numSamples; ++i)
                mono[i] = mono[i] * ratMix + dryBuffer.getSample (0, i) * (1.0f - ratMix);
    }

    // DC block post-RAT (mono — only one channel exists here)
    for (int i = 0; i < numSamples; ++i)
        mono[i] = dcBlock1L.processSample (mono[i]);

    // CHAIN-07 + PARAMS-04: per-sample smoothed trim post-RAT (mono)
    trimPostRatSmooth.setTargetValue (
        juce::Decibels::decibelsToGain (pTrimPostRat->load()));
    {
        auto* m = buffer.getWritePointer (0);
        for (int i = 0; i < numSamples; ++i)
            m[i] *= trimPostRatSmooth.getNextValue();
    }

    //--------------------------------------------------------------------------
    // 5. Stage 2 — MicroPitch (mono in, stereo out).
    //    WR-04 fix: mono→stereo expansion happens into the pre-allocated
    //    workBuffer, not by resizing the host buffer.
    //    After this point all processing is stereo in workBuffer.
    //    The dry buffer remains mono for the global mix blend.
    //
    //    Runs at native sample rate — no oversampling needed (linear stage).
    //--------------------------------------------------------------------------
    bypassSmoothPitch.setTargetValue (pPitchBypass->load() > 0.5f ? 0.0f : 1.0f);

    // Seed workBuffer channels 0 and 1 from the mono input (buffer ch 0).
    workBuffer.copyFrom (0, 0, buffer, 0, 0, numSamples);
    workBuffer.copyFrom (1, 0, buffer, 0, 0, numSamples);

    stageMicroPitch->setParameters ({
        .detuneL = pPitchDetuneL->load(),
        .detuneR = pPitchDetuneR->load(),
        .mix     = pPitchMix->load()     / 100.0f,
        .width   = pPitchWidth->load()   / 100.0f,
        .focus   = pPitchFocus->load()
    });

    stageMicroPitch->process (workBuffer, numSamples);

    // Bypass crossfade (stereo) — dry is post-RAT mono (buffer ch 0),
    // NOT dryBuffer (which is pre-RAT and would discard RAT processing).
    {
        auto* L = workBuffer.getWritePointer (0);
        auto* R = workBuffer.getWritePointer (1);
        const auto* postRat = buffer.getReadPointer (0);

        for (int i = 0; i < numSamples; ++i)
        {
            const float wet = bypassSmoothPitch.getNextValue();
            L[i] = L[i] * wet + postRat[i] * (1.0f - wet);
            R[i] = R[i] * wet + postRat[i] * (1.0f - wet);
        }
    }

    // DC block post-MicroPitch (stereo), operates on workBuffer
    {
        auto* L = workBuffer.getWritePointer (0);
        auto* R = workBuffer.getWritePointer (1);
        for (int i = 0; i < numSamples; ++i)
        {
            L[i] = dcBlock2L.processSample (L[i]);
            R[i] = dcBlock2R.processSample (R[i]);
        }
    }

    // CHAIN-07 + PARAMS-04: per-sample smoothed trim post-MicroPitch (stereo)
    trimPostPitchSmooth.setTargetValue (
        juce::Decibels::decibelsToGain (pTrimPostPitch->load()));
    {
        auto* L = workBuffer.getWritePointer (0);
        auto* R = workBuffer.getWritePointer (1);
        for (int i = 0; i < numSamples; ++i)
        {
            const float g = trimPostPitchSmooth.getNextValue();
            L[i] *= g;
            R[i] *= g;
        }
    }

    //--------------------------------------------------------------------------
    // 6. Stage 3 — H3000 Undulator (stereo in, stereo out).
    //    Chip grit runs at 4× oversampling internally.
    //    All LFO and delay stages run at native rate.
    //--------------------------------------------------------------------------
    // IN-01: Undulator cross-feed into MicroPitch detune will be wired
    // inside Undulator::process in Phase 5. The Phase 1 dead call on
    // stageMicroPitch->getLastLfoValue() has been removed.

    bypassSmoothUnd.setTargetValue (pUndBypass->load() > 0.5f ? 0.0f : 1.0f);

    // Waver [0,1] expands into four secondary-LFO parameters simultaneously.
    // Space [0,1] couples spread and feedback at a fixed ratio.
    // This matches the spec: single-knob macros rather than exposed internals.
    const float waver = pUndModDepth->load() / 100.0f;
    const float space = pUndSpread->load()   / 100.0f;

    stageUndulator->setParameters ({
        .rate        = pUndRate->load(),
        .depth       = pUndDepth->load()     / 100.0f,
        .phase       = pUndPhase->load(),
        .drift       = waver * 0.55f,
        .modRate     = 0.25f + waver * 0.85f,
        .modDepth    = waver * 0.70f,
        .modSpeed    = waver * 0.50f,
        .spread      = space,
        .feedback    = space * 0.65f,
        .grit        = pUndGrit->load()      / 100.0f,
        .mix         = pUndMix->load()       / 100.0f,
        .shape       = static_cast<int> (pUndShape->load())
    });

    // Snapshot pre-Undulator state for bypass crossfade, into pre-allocated member
    preUndBuffer.copyFrom (0, 0, workBuffer, 0, 0, numSamples);
    preUndBuffer.copyFrom (1, 0, workBuffer, 1, 0, numSamples);

    stageUndulator->process (workBuffer, numSamples);

    // Bypass crossfade (stereo) against preUndBuffer
    {
        auto* L    = workBuffer.getWritePointer (0);
        auto* R    = workBuffer.getWritePointer (1);
        const auto* dryL = preUndBuffer.getReadPointer (0);
        const auto* dryR = preUndBuffer.getReadPointer (1);

        for (int i = 0; i < numSamples; ++i)
        {
            const float wet = bypassSmoothUnd.getNextValue();
            L[i] = L[i] * wet + dryL[i] * (1.0f - wet);
            R[i] = R[i] * wet + dryR[i] * (1.0f - wet);
        }
    }

    // DC block post-Undulator (stereo) on workBuffer
    {
        auto* L = workBuffer.getWritePointer (0);
        auto* R = workBuffer.getWritePointer (1);
        for (int i = 0; i < numSamples; ++i)
        {
            L[i] = dcBlock3L.processSample (L[i]);
            R[i] = dcBlock3R.processSample (R[i]);
        }
    }

    // CHAIN-07 + PARAMS-04: per-sample smoothed trim post-Undulator (stereo)
    trimPostUndSmooth.setTargetValue (
        juce::Decibels::decibelsToGain (pTrimPostUnd->load()));
    {
        auto* L = workBuffer.getWritePointer (0);
        auto* R = workBuffer.getWritePointer (1);
        for (int i = 0; i < numSamples; ++i)
        {
            const float g = trimPostUndSmooth.getNextValue();
            L[i] *= g;
            R[i] *= g;
        }
    }

    //--------------------------------------------------------------------------
    // 7. Stage 4 — Burn-In (stereo in, stereo out).
    //    Oxide saturation runs at 4× oversampling internally.
    //    All other processes (noise, flutter, print-through, etc.) native rate.
    //--------------------------------------------------------------------------
    bypassSmoothBurnin.setTargetValue (pBurninBypass->load() > 0.5f ? 0.0f : 1.0f);

    // Snapshot pre-BurnIn state for bypass crossfade, into pre-allocated member
    preBurninBuffer.copyFrom (0, 0, workBuffer, 0, 0, numSamples);
    preBurninBuffer.copyFrom (1, 0, workBuffer, 1, 0, numSamples);

    stageBurnIn->setParameters ({
        .amount  = pBurninAmount->load() / 100.0f,
        .acetate = pAcetateMode->load()  > 0.5f,
        .msMode  = pGlobalMsMode->load() > 0.5f
    });

    stageBurnIn->process (workBuffer, numSamples);

    // Burn-In wet/dry mix — blend processed output with pre-BurnIn signal.
    // preBurninBuffer already holds the snapshot taken above.
    {
        const float burninMix = pBurninMix->load() / 100.0f;
        if (burninMix < 0.9999f)
        {
            auto* L          = workBuffer.getWritePointer (0);
            auto* R          = workBuffer.getWritePointer (1);
            const auto* dryL = preBurninBuffer.getReadPointer (0);
            const auto* dryR = preBurninBuffer.getReadPointer (1);
            const float dry  = 1.0f - burninMix;
            for (int i = 0; i < numSamples; ++i)
            {
                L[i] = L[i] * burninMix + dryL[i] * dry;
                R[i] = R[i] * burninMix + dryR[i] * dry;
            }
        }
    }

    // Bypass crossfade (stereo) against preBurninBuffer
    {
        auto* L    = workBuffer.getWritePointer (0);
        auto* R    = workBuffer.getWritePointer (1);
        const auto* dryL = preBurninBuffer.getReadPointer (0);
        const auto* dryR = preBurninBuffer.getReadPointer (1);

        for (int i = 0; i < numSamples; ++i)
        {
            const float wet = bypassSmoothBurnin.getNextValue();
            L[i] = L[i] * wet + dryL[i] * (1.0f - wet);
            R[i] = R[i] * wet + dryR[i] * (1.0f - wet);
        }
    }

    // Copy fully processed stereo from workBuffer into host output buffer.
    buffer.copyFrom (0, 0, workBuffer, 0, 0, numSamples);
    buffer.copyFrom (1, 0, workBuffer, 1, 0, numSamples);

    //--------------------------------------------------------------------------
    // 8. Global wet/dry blend.
    //    Clean mono input (dryBuffer) blended with fully processed stereo output.
    //    Phase relationship between dry and wet is intentional — do not compensate.
    //--------------------------------------------------------------------------
    globalMixSmooth.setTargetValue (pGlobalMix->load() / 100.0f);

    {
        auto* L         = buffer.getWritePointer (0);
        auto* R         = buffer.getWritePointer (1);
        const auto* dry = dryBuffer.getReadPointer (0);

        for (int i = 0; i < numSamples; ++i)
        {
            const float mix = globalMixSmooth.getNextValue();
            L[i] = L[i] * mix + dry[i] * (1.0f - mix);
            R[i] = R[i] * mix + dry[i] * (1.0f - mix);
        }
    }

    //--------------------------------------------------------------------------
    // 9. Update global feedback sample for next block.
    //    Sum stereo output to mono, apply LPF, hold last sample.
    //    Hard-limited to ±1.0 — the scaling to 0.0–0.15 happens at injection (step 2).
    //--------------------------------------------------------------------------
    if (pGlobalFeedbackActive->load() > 0.5f)
    {
        const auto* L = buffer.getReadPointer (0);
        const auto* R = buffer.getReadPointer (1);

        float fbSum = 0.0f;
        for (int i = 0; i < numSamples; ++i)
            fbSum += (L[i] + R[i]) * 0.5f;

        fbSum /= static_cast<float> (numSamples);

        // LPF
        feedbackSample = feedbackLpf.processSample (fbSum);

        // Hard limit — should never be needed if rest of chain is correct,
        // but feedback paths can produce unexpected spikes on parameter changes
        feedbackSample = juce::jlimit (-1.0f, 1.0f, feedbackSample);
    }
    else
    {
        feedbackSample = 0.0f;
    }

    //--------------------------------------------------------------------------
    // 10. Output hard safety clip — should never trigger in normal use.
    //     If it does, something upstream produced NaN or a runaway value.
    //--------------------------------------------------------------------------
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i)
        {
            // Replace NaN/inf with silence
            if (! std::isfinite (data[i]))
                data[i] = 0.0f;

            data[i] = juce::jlimit (-2.0f, 2.0f, data[i]);
        }
    }
}

//==============================================================================
// Bus layout
//==============================================================================

bool HeatDeathProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Input must be mono. Output must be stereo.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::mono())
        return false;

    return true;
}

//==============================================================================
// State serialisation
//==============================================================================

void HeatDeathProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void HeatDeathProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));

    if (xml == nullptr)
        return;

    if (! xml->hasTagName (apvts.state.getType()))
        return;

    apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
// cacheParameterPointers
// Called once from the constructor. After this, parameter values are read via
// the cached atomic<float>* pointers — never via string lookups in the process loop.
//==============================================================================

void HeatDeathProcessor::cacheParameterPointers()
{
    using namespace Params;

    // Stage 1 — RAT
    pRatDrive      = apvts.getRawParameterValue (RAT_DRIVE);
    pRatFilter     = apvts.getRawParameterValue (RAT_FILTER);
    pRatVolume     = apvts.getRawParameterValue (RAT_VOLUME);
    pRatSlew       = apvts.getRawParameterValue (RAT_SLEW);
    pRatAsym       = apvts.getRawParameterValue (RAT_ASYM);
    pRatClipMode   = apvts.getRawParameterValue (RAT_CLIP_MODE);
    pRatSag        = apvts.getRawParameterValue (RAT_SAG);
    pRatMix        = apvts.getRawParameterValue (RAT_MIX);
    pRatBypass     = apvts.getRawParameterValue (RAT_BYPASS);

    // Stage 2 — MicroPitch
    pPitchDetuneL  = apvts.getRawParameterValue (PITCH_DETUNE_L);
    pPitchDetuneR  = apvts.getRawParameterValue (PITCH_DETUNE_R);
    pPitchMix      = apvts.getRawParameterValue (PITCH_MIX);
    pPitchWidth    = apvts.getRawParameterValue (PITCH_WIDTH);
    pPitchFocus    = apvts.getRawParameterValue (PITCH_FOCUS);
    pPitchBypass   = apvts.getRawParameterValue (PITCH_BYPASS);

    // Stage 3 — Undulator
    pUndRate       = apvts.getRawParameterValue (UND_RATE);
    pUndDepth      = apvts.getRawParameterValue (UND_DEPTH);
    pUndPhase      = apvts.getRawParameterValue (UND_PHASE);
    pUndDrift      = apvts.getRawParameterValue (UND_DRIFT);
    pUndModRate    = apvts.getRawParameterValue (UND_MOD_RATE);
    pUndModDepth   = apvts.getRawParameterValue (UND_MOD_DEPTH);
    pUndModSpeed   = apvts.getRawParameterValue (UND_MOD_SPEED);
    pUndSpread     = apvts.getRawParameterValue (UND_SPREAD);
    pUndFeedback   = apvts.getRawParameterValue (UND_FEEDBACK);
    pUndGrit       = apvts.getRawParameterValue (UND_GRIT);
    pUndMix        = apvts.getRawParameterValue (UND_MIX);
    pUndShape      = apvts.getRawParameterValue (UND_SHAPE);
    pUndBypass     = apvts.getRawParameterValue (UND_BYPASS);

    // Stage 4 — Burn-In
    pBurninAmount  = apvts.getRawParameterValue (BURNIN_AMOUNT);
    pBurninMix     = apvts.getRawParameterValue (BURNIN_MIX);
    pBurninBypass  = apvts.getRawParameterValue (BURNIN_BYPASS);

    // Trims
    pTrimPostRat   = apvts.getRawParameterValue (TRIM_POST_RAT);
    pTrimPostPitch = apvts.getRawParameterValue (TRIM_POST_PITCH);
    pTrimPostUnd   = apvts.getRawParameterValue (TRIM_POST_UND);

    // Global
    pGlobalMix            = apvts.getRawParameterValue (GLOBAL_MIX);
    pGlobalFeedbackAmount = apvts.getRawParameterValue (GLOBAL_FEEDBACK_AMOUNT);
    pGlobalFeedbackActive = apvts.getRawParameterValue (GLOBAL_FEEDBACK_ACTIVE);
    pGlobalMsMode         = apvts.getRawParameterValue (GLOBAL_MS_MODE);

    // Hidden features
    pAcetateMode   = apvts.getRawParameterValue (ACETATE_MODE);

    // Sanity — assert all pointers were resolved.
    // If any are null, the parameter ID string in Parameters.h does not match
    // the ID used in createParameterLayout(). This catches typos at startup.
    jassert (pRatDrive    != nullptr);
    jassert (pRatFilter   != nullptr);
    jassert (pRatVolume   != nullptr);
    jassert (pRatSlew     != nullptr);
    jassert (pRatAsym     != nullptr);
    jassert (pRatClipMode != nullptr);
    jassert (pRatSag      != nullptr);
    jassert (pRatMix      != nullptr);
    jassert (pRatBypass   != nullptr);

    jassert (pPitchDetuneL != nullptr);
    jassert (pPitchDetuneR != nullptr);
    jassert (pPitchMix     != nullptr);
    jassert (pPitchWidth   != nullptr);
    jassert (pPitchFocus   != nullptr);
    jassert (pPitchBypass  != nullptr);

    jassert (pUndRate      != nullptr);
    jassert (pUndDepth     != nullptr);
    jassert (pUndPhase     != nullptr);
    jassert (pUndDrift     != nullptr);
    jassert (pUndModRate   != nullptr);
    jassert (pUndModDepth  != nullptr);
    jassert (pUndModSpeed  != nullptr);
    jassert (pUndSpread    != nullptr);
    jassert (pUndFeedback  != nullptr);
    jassert (pUndGrit      != nullptr);
    jassert (pUndMix       != nullptr);
    jassert (pUndShape     != nullptr);
    jassert (pUndBypass    != nullptr);

    jassert (pBurninAmount  != nullptr);
    jassert (pBurninMix     != nullptr);
    jassert (pBurninBypass  != nullptr);

    jassert (pTrimPostRat   != nullptr);
    jassert (pTrimPostPitch != nullptr);
    jassert (pTrimPostUnd   != nullptr);

    jassert (pGlobalMix            != nullptr);
    jassert (pGlobalFeedbackAmount != nullptr);
    jassert (pGlobalFeedbackActive != nullptr);
    jassert (pGlobalMsMode         != nullptr);

    jassert (pAcetateMode   != nullptr);
}


//==============================================================================
// Editor
//==============================================================================

juce::AudioProcessorEditor* HeatDeathProcessor::createEditor()
{
    return new HeatDeathEditor (*this);
}

//==============================================================================
// Plugin entry point (required by JUCE)
//==============================================================================

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HeatDeathProcessor();
}
