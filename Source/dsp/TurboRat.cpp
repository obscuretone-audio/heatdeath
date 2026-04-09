#include "TurboRat.h"

void TurboRat::prepare (double sampleRate, int samplesPerBlock)
{
    sr               = sampleRate;
    osSr             = sampleRate * 4.0;
    osSamplesPerBlock = samplesPerBlock * 4;

    // SmoothParam configured at HOST rate per Research open question #4
    smoothDrive .setTimeMs (20.0f, sampleRate);
    smoothFilter.setTimeMs (20.0f, sampleRate);
    smoothVolume.setTimeMs (20.0f, sampleRate);
    smoothAsym  .setTimeMs (20.0f, sampleRate);

    reset();
}

void TurboRat::setParameters (const Parameters& p)
{
    params = p;
}

void TurboRat::reset()
{
    hpf1PrevIn  = 0.0f;
    hpf1PrevOut = 0.0f;
    hpf2PrevIn  = 0.0f;
    hpf2PrevOut = 0.0f;

    slewState = 0.0f;

    smoothDrive .reset (params.drive   / 100.0f);
    smoothFilter.reset (params.filter  / 100.0f);
    smoothVolume.reset (params.volume  / 100.0f);
    smoothAsym  .reset (params.asym    / 100.0f);
}

float TurboRat::computeHPFAlpha (float cutoffHz, double sampleRate) noexcept
{
    const double rc = 1.0 / (juce::MathConstants<double>::twoPi
                             * static_cast<double> (cutoffHz));
    const double dt = 1.0 / sampleRate;
    return static_cast<float> (rc / (rc + dt));
}

float TurboRat::computeLPFAlpha (float cutoffHz, double sampleRate) noexcept
{
    return static_cast<float> (
        std::exp (-juce::MathConstants<double>::twoPi
                  * static_cast<double> (cutoffHz) / sampleRate));
}

void TurboRat::updateCoefficients (double osSampleRate)
{
    hpf1Alpha = computeHPFAlpha (60.0f,   osSampleRate);
    hpf2Alpha = computeHPFAlpha (1500.0f, osSampleRate);

    // RAT-02: Slew-rate LP — fixed cutoff at ~1040Hz per heatdeath_vst_spec.md §2.
    // Note: kDefaultSlew=0.68f is a spec annotation; the 1040Hz target is used
    // directly per 03-RESEARCH.md open question #1 (literal alpha=0.68 at
    // osSr=176400 would give ~10.8kHz, not 1040Hz).
    slewAlpha = computeLPFAlpha (1040.0f, osSampleRate);
}

void TurboRat::processOS (juce::dsp::AudioBlock<float>& osBlock)
{
    // Set smoother targets from current params
    smoothDrive .setTarget (params.drive   / 100.0f);
    smoothFilter.setTarget (params.filter  / 100.0f);
    smoothVolume.setTarget (params.volume  / 100.0f);
    smoothAsym  .setTarget (params.asym    / 100.0f);

    // Advance smoothers ONCE at block start — values used for per-block coefficients
    (void) smoothDrive .tick();
    (void) smoothFilter.tick();
    (void) smoothVolume.tick();
    (void) smoothAsym  .tick();

    updateCoefficients (osSr);

    const int numOsSamples = static_cast<int> (osBlock.getNumSamples());
    float* data = osBlock.getChannelPointer (0);

    for (int i = 0; i < numOsSamples; ++i)
    {
        float x = data[i];

        // HPF 60Hz: y[n] = alpha * (y[n-1] + x[n] - x[n-1])
        const float y1 = hpf1Alpha * (hpf1PrevOut + x - hpf1PrevIn);
        hpf1PrevIn  = x;
        hpf1PrevOut = y1;
        x = y1;

        // HPF 1.5kHz
        const float y2 = hpf2Alpha * (hpf2PrevOut + x - hpf2PrevIn);
        hpf2PrevIn  = x;
        hpf2PrevOut = y2;
        x = y2;

        // 3. Slew-rate LP (~1040Hz, fixed)
        slewState = slewAlpha * slewState + (1.0f - slewAlpha) * x;
        x = slewState;

        data[i] = x;   // 03-03..03-04 will insert further stages here
    }
}

void TurboRat::process (juce::AudioBuffer<float>& /*buffer*/, int /*numSamples*/)
{
    // Stage 1 processed on oversampled block via processOS (03-01).
    // This method is kept for build compatibility; body is intentionally empty.
}
