#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Parameters.h"
#include "Preset_Manager/PresetManager.h"

// Forward declarations — DSP stage classes defined in their own headers
class TurboRat;
class MicroPitch;
class Undulator;
class BurnIn;

//==============================================================================

class HeatDeathProcessor final : public juce::AudioProcessor
{
public:

    //==========================================================================
    HeatDeathProcessor();
    ~HeatDeathProcessor() override;

    //==========================================================================
    // AudioProcessor overrides
    //==========================================================================

    void prepareToPlay  (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock   (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    bool isBusesLayoutSupported (const BusesLayout&) const override;

    //==========================================================================
    // State
    //==========================================================================

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    // Plugin identity
    //==========================================================================

    const juce::String getName() const override     { return "HEATDEATH"; }
    bool  acceptsMidi()  const override             { return false; }
    bool  producesMidi() const override             { return false; }
    bool  isMidiEffect() const override             { return false; }
    double getTailLengthSeconds() const override    { return 4.0; } // print-through tail

    int  getNumPrograms()    override               { return 1; }
    int  getCurrentProgram() override               { return 0; }
    void setCurrentProgram (int) override           {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    //==========================================================================
    // Editor
    //==========================================================================

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    //==========================================================================
    // APVTS — public so the editor can attach its controls directly
    //==========================================================================

    juce::AudioProcessorValueTreeState apvts;
    std::unique_ptr<PresetManager> presetManager;

    //==========================================================================
    // Parameter accessors
    //
    // Call these from the process loop — they return a raw pointer to the
    // SmoothedValue or atomic value inside the APVTS. Do not call
    // apvts.getRawParameterValue() in the process loop — this involves a
    // string hash lookup on every call.
    //
    // Pattern: cache the raw pointer in prepareToPlay(), read it in processBlock().
    //==========================================================================

    // --- Stage 1: RAT ---
    std::atomic<float>* pRatDrive      = nullptr;
    std::atomic<float>* pRatFilter     = nullptr;
    std::atomic<float>* pRatVolume     = nullptr;
    std::atomic<float>* pRatSlew       = nullptr;
    std::atomic<float>* pRatAsym       = nullptr;
    std::atomic<float>* pRatClipMode   = nullptr;  // cast to int at use site
    std::atomic<float>* pRatSag        = nullptr;
    std::atomic<float>* pRatMix        = nullptr;
    std::atomic<float>* pRatBypass     = nullptr;  // cast to bool at use site

    // --- Stage 2: MicroPitch ---
    std::atomic<float>* pPitchDetuneL  = nullptr;
    std::atomic<float>* pPitchDetuneR  = nullptr;
    std::atomic<float>* pPitchMix      = nullptr;
    std::atomic<float>* pPitchWidth    = nullptr;
    std::atomic<float>* pPitchFocus      = nullptr;
    std::atomic<float>* pPitchCrossover  = nullptr;
    std::atomic<float>* pPitchBypass     = nullptr;

    // --- Stage 3: Undulator ---
    std::atomic<float>* pUndRate       = nullptr;
    std::atomic<float>* pUndDepth      = nullptr;
    std::atomic<float>* pUndPhase      = nullptr;
    std::atomic<float>* pUndDrift      = nullptr;
    std::atomic<float>* pUndModRate    = nullptr;
    std::atomic<float>* pUndModDepth   = nullptr;
    std::atomic<float>* pUndModSpeed   = nullptr;
    std::atomic<float>* pUndSpread     = nullptr;
    std::atomic<float>* pUndFeedback   = nullptr;
    std::atomic<float>* pUndGrit       = nullptr;
    std::atomic<float>* pUndMix        = nullptr;
    std::atomic<float>* pUndShape      = nullptr;  // cast to int at use site
    std::atomic<float>* pUndBypass     = nullptr;

    // --- Stage 4: Burn-In ---
    std::atomic<float>* pBurninAmount  = nullptr;
    std::atomic<float>* pBurninMix     = nullptr;
    std::atomic<float>* pBurninVol     = nullptr;
    std::atomic<float>* pBurninBypass  = nullptr;

    // --- Trims ---
    std::atomic<float>* pTrimPostRat   = nullptr;
    std::atomic<float>* pTrimPostPitch = nullptr;
    std::atomic<float>* pTrimPostUnd   = nullptr;

    // --- Global ---
    std::atomic<float>* pGlobalMix             = nullptr;
    std::atomic<float>* pGlobalFeedbackAmount  = nullptr;
    std::atomic<float>* pGlobalFeedbackActive  = nullptr;
    std::atomic<float>* pGlobalMsMode          = nullptr;

    // --- Hidden features ---
    std::atomic<float>* pAcetateMode   = nullptr;

private:

    //==========================================================================
    // DSP stage objects
    //==========================================================================

    std::unique_ptr<TurboRat>   stageTurboRat;
    std::unique_ptr<MicroPitch> stageMicroPitch;
    std::unique_ptr<Undulator>  stageUndulator;
    std::unique_ptr<BurnIn>     stageBurnIn;

    //==========================================================================
    // Input limiter — applied at plugin boundary before Stage 1.
    // Soft tanh clip at unity threshold. Prevents waveshaper from receiving
    // pathological inputs (0dBFS+, DC offset, inter-sample peaks).
    //==========================================================================

    juce::dsp::WaveShaper<float> inputLimiter;

    //==========================================================================
    // DC blocking filters — one per stage boundary (three total).
    // First-order highpass at ~5Hz. Applied after each stage output.
    // Prevents asymmetric waveshapers from accumulating DC offset across chain.
    //==========================================================================

    juce::dsp::IIR::Filter<float> dcBlock1L;              // post-RAT (mono)
    juce::dsp::IIR::Filter<float> dcBlock2L, dcBlock2R;  // post-MicroPitch
    juce::dsp::IIR::Filter<float> dcBlock3L, dcBlock3R;  // post-Undulator

    //==========================================================================
    // Per-stage bypass smoothing.
    // One SmoothedValue per stage. Prevents clicks on bypass toggle.
    // 10ms smoothing window applied as a wet/dry crossfade in processBlock.
    //==========================================================================

    juce::SmoothedValue<float> bypassSmoothRat;
    juce::SmoothedValue<float> bypassSmoothPitch;
    juce::SmoothedValue<float> bypassSmoothUnd;
    juce::SmoothedValue<float> bypassSmoothBurnin;

    //==========================================================================
    // Global mix smoothing.
    // 20ms window — global mix changes need slightly longer smoothing than
    // stage bypasses since the dry/wet phase relationship can cause audible
    // transients if changed abruptly.
    //==========================================================================

    juce::SmoothedValue<float> globalMixSmooth;

    //==========================================================================
    // Global feedback path.
    // Holds the previous block's output (L+R summed to mono, lowpass filtered)
    // for injection into Stage 1 input on the next block.
    // Only active when pGlobalFeedbackActive is true.
    //==========================================================================

    float                         feedbackSample = 0.0f;
    juce::dsp::IIR::Filter<float> feedbackLpf;   // 100Hz cutoff — heavy LP

    //==========================================================================
    // Sample rate and block size — cached in prepareToPlay for use elsewhere
    //==========================================================================

    double currentSampleRate  = 44100.0;
    int    currentBlockSize   = 512;

    //==========================================================================
    // Dry buffer — holds a copy of the input for global wet/dry blend.
    // Allocated in prepareToPlay to avoid real-time allocation.
    //==========================================================================

    juce::AudioBuffer<float> dryBuffer;

    // WR-04: Pre-allocated stereo working buffer for mono→stereo expansion.
    // Sized to max block size in prepareToPlay. Never resized in processBlock.
    juce::AudioBuffer<float> workBuffer;

    // Pre-allocated buffers for bypass crossfade snapshots. Phase 1 allocated
    // these inline inside processBlock (AudioBuffer ctor on stack = heap alloc).
    juce::AudioBuffer<float> preUndBuffer;     // bypass crossfade snapshot (pre-Undulator)
    juce::AudioBuffer<float> preBurninBuffer;  // bypass crossfade snapshot (pre-BurnIn)

    // CHAIN-02: 4x polyphase IIR oversampler at plugin boundary (mono at Stage 1).
    // Stages 3 and 4 will add stage-internal oversampling in Phases 5 and 6;
    // for Phase 2 the single plugin-level oversampler is sufficient.
    juce::dsp::Oversampling<float> oversampler {
        1u,                                                                  // numChannels (mono at boundary)
        2u,                                                                  // factor order = 4x
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
        true,                                                                // max quality
        false                                                                // do not interleave
    };

    // WR-03: Per-sample linear interpolation of feedback injection across the block
    // avoids the step artefact at block boundaries. Holds previous block's feedback.
    float previousFeedbackSample = 0.0f;

    // CHAIN-07 + PARAMS-04: smoothed inter-stage trims (20ms window).
    juce::SmoothedValue<float> trimPostRatSmooth;
    juce::SmoothedValue<float> trimPostPitchSmooth;
    juce::SmoothedValue<float> trimPostUndSmooth;

    //==========================================================================

    void  cacheParameterPointers();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HeatDeathProcessor)
};
