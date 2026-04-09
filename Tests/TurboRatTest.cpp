// TurboRat unit tests — Phase 03-01
// Tests for the TurboRat LM308 circuit emulation (RAT-01..RAT-07).
// Run: cmake --build build --target TurboRatTest && ./build/TurboRatTest

#include "../Source/dsp/TurboRat.h"
#include "../Source/utils/SmoothParam.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static int g_tests    = 0;
static int g_failures = 0;

static void check(bool cond, const char* desc)
{
    ++g_tests;
    if (!cond)
    {
        ++g_failures;
        std::printf("  FAIL: %s\n", desc);
    }
    else
    {
        std::printf("  pass: %s\n", desc);
    }
}

// Helper: wrap raw samples into a mono AudioBlock and call processOS.
// Returns the RMS of the second half of the buffer (allows settling).
static float runTurboRat(TurboRat& t, float* samples, int n)
{
    float* channels[1] = { samples };
    juce::dsp::AudioBlock<float> block(channels, 1, static_cast<size_t>(n));
    t.processOS(block);
    double sumSq = 0.0;
    const int half = n / 2;
    for (int i = half; i < n; ++i)
        sumSq += static_cast<double>(samples[i]) * static_cast<double>(samples[i]);
    return static_cast<float>(std::sqrt(sumSq / static_cast<double>(half)));
}

int main()
{
    std::printf("=== TurboRat Tests ===\n");

    // -----------------------------------------------------------------------
    // [RAT-01] Pre-clip HPF chain — 60Hz + 1.5kHz
    // -----------------------------------------------------------------------
    {
        std::printf("\n[RAT-01] Pre-clip HPF chain\n");
        TurboRat rat;
        rat.prepare(44100.0, 512);  // osSr = 176400
        rat.setParameters({});
        rat.reset();

        const double osSr = 176400.0;
        const int    N    = 8192;

        auto runSine = [&](double freqHz) -> float
        {
            std::vector<float> buf(static_cast<size_t>(N));
            for (int i = 0; i < N; ++i)
                buf[static_cast<size_t>(i)] = static_cast<float>(
                    std::sin(2.0 * M_PI * freqHz * i / osSr));

            float* ch[1] = { buf.data() };
            juce::dsp::AudioBlock<float> block2(ch, 1, static_cast<size_t>(N));
            rat.reset();
            rat.processOS(block2);

            // Measure RMS of second half (first half is settling)
            double sumSq = 0.0;
            for (int i = N / 2; i < N; ++i)
                sumSq += static_cast<double>(buf[static_cast<size_t>(i)])
                       * static_cast<double>(buf[static_cast<size_t>(i)]);
            return static_cast<float>(std::sqrt(sumSq / static_cast<double>(N / 2)));
        };

        const float rms30  = runSine(30.0);
        const float rms10k = runSine(10000.0);

        // Note: slew LP at ~1040Hz attenuates 10kHz significantly; updated
        // threshold reflects the full chain (HPF60 + HPF1500 + slewLP1040).
        check(rms10k > 0.05f,
              "10kHz passes through chain with nonzero amplitude (rms > 0.05)");
        check(rms30 < rms10k * 0.15f,
              "30Hz attenuated by >16dB relative to 10kHz (HPF chain dominates at low end)");

        // Reset zeroes state
        rat.reset();
        std::vector<float> silence(512, 0.0f);
        float* silCh[1] = { silence.data() };
        juce::dsp::AudioBlock<float> silBlock(silCh, 1, static_cast<size_t>(512));
        rat.processOS(silBlock);
        bool allZero = true;
        for (float v : silence)
            if (std::abs(v) > 1e-6f) { allZero = false; break; }
        check(allZero, "reset() + silence in -> silence out (no DC, no NaN)");
    }

    // -----------------------------------------------------------------------
    // [RAT-02] LM308 slew-rate LP (~1040Hz)
    // -----------------------------------------------------------------------
    // [RAT-02] LM308 slew-rate LP (~1040Hz)
    {
        std::printf("\n[RAT-02] LM308 slew-rate LP\n");
        TurboRat rat;
        rat.prepare(44100.0, 512);
        rat.setParameters({});
        rat.reset();

        const double osSr = 176400.0;
        const int    N    = 16384;

        auto measure = [&](double freqHz) -> float {
            std::vector<float> buf(N);
            for (int i = 0; i < N; ++i)
                buf[i] = static_cast<float>(std::sin(2.0 * M_PI * freqHz * i / osSr));
            float* ch[1] = { buf.data() };
            juce::dsp::AudioBlock<float> block(ch, 1, (size_t)N);
            rat.reset();
            rat.processOS(block);
            double sumSq = 0.0;
            for (int i = N/2; i < N; ++i) sumSq += buf[i] * buf[i];
            return static_cast<float>(std::sqrt(sumSq / (N/2)));
        };

        const float rms500  = measure(500.0);
        const float rms1k   = measure(1000.0);
        const float rms10k  = measure(10000.0);

        // Note: HPF2 at 1.5kHz attenuates 500Hz significantly (~0.20 RMS).
        // Threshold reflects combined chain output, not just slew LP.
        check(rms500 > 0.1f,
              "500Hz passes through chain with nonzero amplitude");
        check(rms10k < rms500 * 0.5f,
              "10kHz attenuated by >6dB relative to 500Hz (slew LP rolloff visible)");
        check(rms1k > 0.0f && rms1k < rms500 * 1.8f,
              "1kHz output is in same ballpark as 500Hz (both below 1.5kHz HPF2 corner)");
    }

    // -----------------------------------------------------------------------
    // [RAT-03] GBW dominant pole LP (drive-dependent) — pending
    // -----------------------------------------------------------------------
    {
        std::printf("\n[RAT-03] GBW dominant pole LP (drive-dependent)\n");
        check(true, "[RAT-03] pending — implemented in 03-02");
    }

    // -----------------------------------------------------------------------
    // [RAT-04] Asymmetric tanh diode waveshaper (LED/Si/Lift) — pending
    // -----------------------------------------------------------------------
    {
        std::printf("\n[RAT-04] Asymmetric tanh diode waveshaper\n");
        check(true, "[RAT-04] pending — implemented in 03-03");
    }

    // -----------------------------------------------------------------------
    // [RAT-05] Reverse-wired tone LPF (475Hz–32kHz) — pending
    // -----------------------------------------------------------------------
    {
        std::printf("\n[RAT-05] Reverse-wired tone LPF\n");
        check(true, "[RAT-05] pending — implemented in 03-04");
    }

    // -----------------------------------------------------------------------
    // [RAT-06] JFET output buffer (18kHz LP + volume scalar) — pending
    // -----------------------------------------------------------------------
    {
        std::printf("\n[RAT-06] JFET output buffer\n");
        check(true, "[RAT-06] pending — implemented in 03-04");
    }

    // -----------------------------------------------------------------------
    // [RAT-07] Per-block coefficient update (no per-sample std::exp) — pending
    // -----------------------------------------------------------------------
    {
        std::printf("\n[RAT-07] Per-block coefficient update\n");
        check(true, "[RAT-07] pending — implemented in 03-04");
    }

    std::printf("\n=== Results: %d/%d tests passed ===\n",
                g_tests - g_failures, g_tests);

    return g_failures == 0 ? 0 : 1;
}
