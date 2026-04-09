#include "PluginProcessor.h"
#include "PluginEditor.h"

// DSP stage headers — implemented separately
#include "dsp/TurboRat.h"
#include "dsp/MicroPitch.h"
#include "dsp/Undulator.h"
#include "dsp/BurnIn.h"

//==============================================================================
// Bus layout helper
//==============================================================================

static juce::AudioProcessor::BusesProperties getDefaultBuses()
{
    // Mono input, stereo output — the stereo field is created by MicroPitch.
    // Nothing upstream of MicroPitch generates stereo information.
    return juce::AudioProcessor::BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::mono(),   true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true);
}

//==============================================================================
// Constructor
//==============================================================================

HeatDeathProcessor::HeatDeathProcessor()
    : AudioProcessor (getDefaultBuses()),
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
    // Coefficient: a = 1 - (2π * fc / fs). fc = 5Hz.
    const float dcCoeff = 1.0f - (juce::MathConstants<float>::twoPi * 5.0f
                                  / static_cast<float> (sampleRate));
    const auto  dcCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass (
                               sampleRate, 5.0f);

    dcBlock1L.prepare (monoSpec); dcBlock1L.coefficients = dcCoeffs;
    dcBlock1R.prepare (monoSpec); dcBlock1R.coefficients = dcCoeffs;
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
    const float bypassRamp = static_cast<float> (samplesPerBlock) /
                             static_cast<float> (sampleRate * 0.01);
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
    dryBuffer.setSize (1, samplesPerBlock, false, true, false);
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

    dcBlock1L.reset(); dcBlock1R.reset();
    dcBlock2L.reset(); dcBlock2R.reset();
    dcBlock3L.reset(); dcBlock3R.reset();
    feedbackLpf.reset();
    feedbackSample = 0.0f;
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
    // 0. Update disintegration timer state (if active).
    //    Must happen before Stage 4 reads heat_rate, since the timer overrides it.
    //--------------------------------------------------------------------------
    updateTimerState (numSamples);

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
        const float fbSample = feedbackSample * fbAmount;

        for (int i = 0; i < numSamples; ++i)
            mono[i] += fbSample;  // same value per block — single-sample hold
                                  // is intentional at these frequencies (<100Hz)
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
    bypassSmoothRat.setTargetValue (pRatBypass->load() > 0.5f ? 0.0f : 1.0f);

    stageTurboRat->setParameters ({
        .drive    = pRatDrive->load(),
        .filter   = pRatFilter->load(),
        .volume   = pRatVolume->load(),
        .slew     = pRatSlew->load(),
        .asym     = pRatAsym->load(),
        .clipMode = static_cast<int> (pRatClipMode->load()),
        .sag      = pRatSag->load()
    });

    stageTurboRat->process (buffer, numSamples);

    // Bypass crossfade (mono)
    for (int i = 0; i < numSamples; ++i)
    {
        const float wet = bypassSmoothRat.getNextValue();
        mono[i] = mono[i] * wet + dryBuffer.getSample (0, i) * (1.0f - wet);
    }

    // DC block post-RAT (mono — only one channel exists here)
    for (int i = 0; i < numSamples; ++i)
        mono[i] = dcBlock1L.processSample (mono[i]);

    // Inter-stage trim post-RAT
    {
        const float trimGain = juce::Decibels::decibelsToGain (pTrimPostRat->load());
        buffer.applyGain (0, 0, numSamples, trimGain);
    }

    //--------------------------------------------------------------------------
    // 5. Stage 2 — MicroPitch (mono in, stereo out).
    //    Expands the buffer to stereo here. After this point all processing
    //    is stereo. The dry buffer remains mono for the global mix blend.
    //
    //    Runs at native sample rate — no oversampling needed (linear stage).
    //--------------------------------------------------------------------------
    bypassSmoothPitch.setTargetValue (pPitchBypass->load() > 0.5f ? 0.0f : 1.0f);

    // Expand buffer to stereo before passing to MicroPitch
    buffer.setSize (2, numSamples, true, false, true);
    buffer.copyFrom (1, 0, buffer, 0, 0, numSamples);  // R = L (mono copy)

    stageMicroPitch->setParameters ({
        .detuneL = pPitchDetuneL->load(),
        .detuneR = pPitchDetuneR->load(),
        .mix     = pPitchMix->load()     / 100.0f,
        .width   = pPitchWidth->load()   / 100.0f
    });

    stageMicroPitch->process (buffer, numSamples);

    // Bypass crossfade (stereo — mono dry expanded to stereo for blend)
    {
        auto* L = buffer.getWritePointer (0);
        auto* R = buffer.getWritePointer (1);
        const auto* dry = dryBuffer.getReadPointer (0);

        for (int i = 0; i < numSamples; ++i)
        {
            const float wet = bypassSmoothPitch.getNextValue();
            L[i] = L[i] * wet + dry[i] * (1.0f - wet);
            R[i] = R[i] * wet + dry[i] * (1.0f - wet);
        }
    }

    // DC block post-MicroPitch (stereo)
    {
        auto* L = buffer.getWritePointer (0);
        auto* R = buffer.getWritePointer (1);
        for (int i = 0; i < numSamples; ++i)
        {
            L[i] = dcBlock2L.processSample (L[i]);
            R[i] = dcBlock2R.processSample (R[i]);
        }
    }

    // Inter-stage trim post-MicroPitch
    {
        const float trimGain = juce::Decibels::decibelsToGain (pTrimPostPitch->load());
        buffer.applyGain (trimGain);
    }

    //--------------------------------------------------------------------------
    // 6. Stage 3 — H3000 Undulator (stereo in, stereo out).
    //    Chip grit runs at 4× oversampling internally.
    //    All LFO and delay stages run at native rate.
    //--------------------------------------------------------------------------
    bypassSmoothUnd.setTargetValue (pUndBypass->load() > 0.5f ? 0.0f : 1.0f);

    stageMicroPitch->getLastLfoValue();  // cross-feed to MicroPitch detune
                                         // (implemented inside MicroPitch::process)

    stageUndulator->setParameters ({
        .rate        = pUndRate->load(),
        .depth       = pUndDepth->load()     / 100.0f,
        .phase       = pUndPhase->load(),
        .drift       = pUndDrift->load()     / 100.0f,
        .modRate     = pUndModRate->load(),
        .modDepth    = pUndModDepth->load()  / 100.0f,
        .modSpeed    = pUndModSpeed->load()  / 100.0f,
        .spread      = pUndSpread->load()    / 100.0f,
        .feedback    = pUndFeedback->load()  / 100.0f,
        .grit        = pUndGrit->load()      / 100.0f,
        .mix         = pUndMix->load()       / 100.0f,
        .shape       = static_cast<int> (pUndShape->load())
    });

    // Hold pre-Undulator buffer for bypass blend
    juce::AudioBuffer<float> preUndBuffer (2, numSamples);
    preUndBuffer.copyFrom (0, 0, buffer, 0, 0, numSamples);
    preUndBuffer.copyFrom (1, 0, buffer, 1, 0, numSamples);

    stageUndulator->process (buffer, numSamples);

    // Bypass crossfade (stereo)
    {
        auto* L    = buffer.getWritePointer (0);
        auto* R    = buffer.getWritePointer (1);
        const auto* dryL = preUndBuffer.getReadPointer (0);
        const auto* dryR = preUndBuffer.getReadPointer (1);

        for (int i = 0; i < numSamples; ++i)
        {
            const float wet = bypassSmoothUnd.getNextValue();
            L[i] = L[i] * wet + dryL[i] * (1.0f - wet);
            R[i] = R[i] * wet + dryR[i] * (1.0f - wet);
        }
    }

    // DC block post-Undulator (stereo)
    {
        auto* L = buffer.getWritePointer (0);
        auto* R = buffer.getWritePointer (1);
        for (int i = 0; i < numSamples; ++i)
        {
            L[i] = dcBlock3L.processSample (L[i]);
            R[i] = dcBlock3R.processSample (R[i]);
        }
    }

    // Inter-stage trim post-Undulator
    {
        const float trimGain = juce::Decibels::decibelsToGain (pTrimPostUnd->load());
        buffer.applyGain (trimGain);
    }

    //--------------------------------------------------------------------------
    // 7. Stage 4 — Burn-In (stereo in, stereo out).
    //    Oxide saturation runs at 4× oversampling internally.
    //    All other processes (noise, flutter, print-through, etc.) native rate.
    //--------------------------------------------------------------------------
    bypassSmoothBurnin.setTargetValue (pBurninBypass->load() > 0.5f ? 0.0f : 1.0f);

    // Hold pre-Burn-In buffer for bypass blend
    juce::AudioBuffer<float> preBurninBuffer (2, numSamples);
    preBurninBuffer.copyFrom (0, 0, buffer, 0, 0, numSamples);
    preBurninBuffer.copyFrom (1, 0, buffer, 1, 0, numSamples);

    stageBurnIn->setParameters ({
        .heatRate  = pBurninAmount->load() / 100.0f,
        .freeze    = pBurninFreeze->load()  > 0.5f,
        .acetate   = pAcetateMode->load()   > 0.5f,
        .msMode    = pGlobalMsMode->load()  > 0.5f,
        .timerActive = pTimerActive->load() > 0.5f,
        .timerProgress = getTimerProgress()  // 0.0–1.0
    });

    stageBurnIn->process (buffer, numSamples);

    // Bypass crossfade (stereo)
    {
        auto* L    = buffer.getWritePointer (0);
        auto* R    = buffer.getWritePointer (1);
        const auto* dryL = preBurninBuffer.getReadPointer (0);
        const auto* dryR = preBurninBuffer.getReadPointer (1);

        for (int i = 0; i < numSamples; ++i)
        {
            const float wet = bypassSmoothBurnin.getNextValue();
            L[i] = L[i] * wet + dryL[i] * (1.0f - wet);
            R[i] = R[i] * wet + dryR[i] * (1.0f - wet);
        }
    }

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
    // Serialise APVTS parameters (all knob positions, choices, booleans)
    auto state = apvts.copyState();

    // If thermal persistence is enabled, write temp into the state tree
    // as additional properties alongside the parameter values.
    if (pBurninPersist->load() > 0.5f)
    {
        state.setProperty ("persistedTemp",     persistedTemp,     nullptr);
        state.setProperty ("persistedTempPrev", persistedTempPrev, nullptr);
    }
    else
    {
        // Explicitly write 0.0 so that toggling persist off clears any
        // previously saved temp rather than leaving stale data
        state.setProperty ("persistedTemp",     0.0f, nullptr);
        state.setProperty ("persistedTempPrev", 0.0f, nullptr);
    }

    // Serialise timer elapsed time so a running timer survives project reload
    state.setProperty ("timerElapsedSeconds",
                       static_cast<double> (timerElapsedSeconds), nullptr);

    // Write to memory block
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

    auto newState = juce::ValueTree::fromXml (*xml);

    // Restore APVTS parameters
    apvts.replaceState (newState);

    // Restore thermal state — only applied if persist was enabled when saved.
    // The actual injection into BurnIn happens in prepareToPlay() or on the
    // next processBlock() call via stageBurnIn->setPersistedTemp().
    persistedTemp     = static_cast<float> (
                            newState.getProperty ("persistedTemp",     0.0f));
    persistedTempPrev = static_cast<float> (
                            newState.getProperty ("persistedTempPrev", 0.0f));

    if (pBurninPersist->load() > 0.5f)
        stageBurnIn->setPersistedTemp (persistedTemp, persistedTempPrev);

    // Restore timer elapsed time
    timerElapsedSeconds = static_cast<double> (
        newState.getProperty ("timerElapsedSeconds", 0.0));
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
    pRatBypass     = apvts.getRawParameterValue (RAT_BYPASS);

    // Stage 2 — MicroPitch
    pPitchDetuneL  = apvts.getRawParameterValue (PITCH_DETUNE_L);
    pPitchDetuneR  = apvts.getRawParameterValue (PITCH_DETUNE_R);
    pPitchMix      = apvts.getRawParameterValue (PITCH_MIX);
    pPitchWidth    = apvts.getRawParameterValue (PITCH_WIDTH);
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
    pBurninFreeze  = apvts.getRawParameterValue (BURNIN_FREEZE);
    pBurninPersist = apvts.getRawParameterValue (BURNIN_PERSIST);
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

    // Timer
    pTimerDuration = apvts.getRawParameterValue (TIMER_DURATION);
    pTimerActive   = apvts.getRawParameterValue (TIMER_ACTIVE);

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
    jassert (pRatBypass   != nullptr);

    jassert (pPitchDetuneL != nullptr);
    jassert (pPitchDetuneR != nullptr);
    jassert (pPitchMix     != nullptr);
    jassert (pPitchWidth   != nullptr);
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
    jassert (pBurninFreeze  != nullptr);
    jassert (pBurninPersist != nullptr);
    jassert (pBurninBypass  != nullptr);

    jassert (pTrimPostRat   != nullptr);
    jassert (pTrimPostPitch != nullptr);
    jassert (pTrimPostUnd   != nullptr);

    jassert (pGlobalMix            != nullptr);
    jassert (pGlobalFeedbackAmount != nullptr);
    jassert (pGlobalFeedbackActive != nullptr);
    jassert (pGlobalMsMode         != nullptr);

    jassert (pTimerDuration != nullptr);
    jassert (pTimerActive   != nullptr);
    jassert (pAcetateMode   != nullptr);
}

//==============================================================================
// updateTimerState
// Called at the top of processBlock. Advances timerElapsedSeconds when active.
// The timer drives BurnIn's heat_rate toward 1.0 over the chosen duration —
// the stage itself reads this via the timerProgress value in its params struct.
//==============================================================================

void HeatDeathProcessor::updateTimerState (int numSamples)
{
    if (pTimerActive->load() < 0.5f)
        return;

    // Duration lookup in seconds
    static constexpr double durations[] = { 600.0, 1200.0, 2400.0, 4440.0 };
    const int durationIndex = juce::jlimit (0, 3,
                                  static_cast<int> (pTimerDuration->load()));
    const double totalDuration = durations[durationIndex];

    // Advance elapsed time
    const double blockDuration = static_cast<double> (numSamples) / currentSampleRate;
    timerElapsedSeconds += blockDuration;

    // Clamp — timer stops at full duration (does not reset automatically)
    timerElapsedSeconds = std::min (timerElapsedSeconds, totalDuration);

    // Update BurnIn persisted temp tracker (for serialisation)
    persistedTemp     = stageBurnIn->getCurrentTemp();
    persistedTempPrev = stageBurnIn->getPreviousTemp();
}

float HeatDeathProcessor::getTimerProgress() const
{
    if (pTimerActive->load() < 0.5f)
        return 0.0f;

    static constexpr double durations[] = { 600.0, 1200.0, 2400.0, 4440.0 };
    const int durationIndex = juce::jlimit (0, 3,
                                  static_cast<int> (pTimerDuration->load()));

    return static_cast<float> (timerElapsedSeconds / durations[durationIndex]);
}

//==============================================================================
// Plugin entry point (required by JUCE)
//==============================================================================

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HeatDeathProcessor();
}
