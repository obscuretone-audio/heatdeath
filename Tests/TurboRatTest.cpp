// TurboRat unit tests — Phase 03-01
// Tests for the TurboRat LM308 circuit emulation (RAT-01..RAT-07).
// Run: cmake --build build --target TurboRatTest && ./build/TurboRatTest

#define TURBORAT_TEST_ACCESS 1
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
        // Use drive=0 so GBW LP is at max cutoff (8000Hz), keeping HPF chain
        // test independent of the drive-dependent GBW pole (added in 03-02).
        TurboRat::Parameters p{};
        p.drive = 0.0f;
        rat.setParameters(p);
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

        // Note: full chain = HPF60 + HPF1500 + slewLP(1040Hz) + gbwLP(8000Hz at drive=0).
        // 10kHz reaches ~0.044 RMS after the combined chain; 30Hz ~0.007.
        check(rms10k > 0.03f,
              "10kHz passes through chain with nonzero amplitude (rms > 0.03)");
        check(rms30 < rms10k * 0.20f,
              "30Hz attenuated by >14dB relative to 10kHz (HPF chain dominates at low end)");

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
    // [RAT-03] GBW dominant pole LP (drive-dependent)
    // -----------------------------------------------------------------------
    {
        std::printf("\n[RAT-03] GBW dominant pole LP\n");
        TurboRat rat;
        rat.prepare(44100.0, 512);

        const double osSr = 176400.0;
        const int    N    = 16384;

        auto rmsAtDrive = [&](float drivePct, double freqHz) -> float {
            TurboRat::Parameters p{};
            p.drive = drivePct;
            rat.setParameters(p);
            rat.reset();
            std::vector<float> buf(N);
            for (int i = 0; i < N; ++i)
                buf[i] = static_cast<float>(std::sin(2.0 * M_PI * freqHz * i / osSr));
            float* ch[1] = { buf.data() };
            juce::dsp::AudioBlock<float> block(ch, 1, (size_t)N);
            rat.processOS(block);
            double sumSq = 0.0;
            for (int i = N/2; i < N; ++i) sumSq += buf[i] * buf[i];
            return static_cast<float>(std::sqrt(sumSq / (N/2)));
        };

        // At 5kHz: drive=100 should darken more than drive=0 (GBW pole at ~600Hz vs 8000Hz).
        const float rmsLo = rmsAtDrive(0.0f,   5000.0);
        const float rmsHi = rmsAtDrive(100.0f, 5000.0);

        check(rmsLo > rmsHi,
              "drive=0 passes more 5kHz energy than drive=100 (GBW pole darkens with drive)");
        check(rmsHi < rmsLo * 0.6f,
              "drive=100 reduces 5kHz energy by >4dB vs drive=0 (cutoff moved from ~8kHz to ~600Hz)");

        // Smoke: no NaN/Inf across extreme drive values
        for (float d : { 0.0f, 1.0f, 50.0f, 99.0f, 100.0f }) {
            const float r = rmsAtDrive(d, 1000.0);
            check(std::isfinite(r), "output finite at every drive value");
            (void)r;
        }
    }

    // -----------------------------------------------------------------------
    // [RAT-04] Asymmetric tanh diode waveshaper (LED/Si/Lift)
    // -----------------------------------------------------------------------
    {
        std::printf("\n[RAT-04] Asymmetric tanh diode waveshaper\n");
        TurboRat rat;
        rat.prepare(44100.0, 512);

        const double osSr = 176400.0;
        const int    N    = 8192;

        auto runSineAt = [&](int clipMode) -> std::vector<float> {
            TurboRat::Parameters p{};
            p.drive    = 72.0f;
            p.filter   = 50.0f;
            p.volume   = 65.0f;
            p.asym     = 20.0f;
            p.sag      = 25.0f;
            p.clipMode = clipMode;
            rat.setParameters(p);
            rat.reset();
            std::vector<float> buf(N);
            for (int i = 0; i < N; ++i)
                buf[i] = static_cast<float>(std::sin(2.0 * M_PI * 1000.0 * i / osSr));
            float* ch[1] = { buf.data() };
            juce::dsp::AudioBlock<float> block(ch, 1, (size_t)N);
            rat.processOS(block);
            return buf;
        };

        auto mav = [](const std::vector<float>& v) -> float {
            double s = 0.0;
            for (size_t i = v.size()/2; i < v.size(); ++i) s += std::abs(v[i]);
            return static_cast<float>(s / (v.size()/2));
        };
        auto peak = [](const std::vector<float>& v) -> float {
            float p = 0.0f;
            for (size_t i = v.size()/2; i < v.size(); ++i) p = std::max(p, std::abs(v[i]));
            return p;
        };
        auto dcOffset = [](const std::vector<float>& v) -> float {
            double s = 0.0;
            for (size_t i = v.size()/2; i < v.size(); ++i) s += v[i];
            return static_cast<float>(s / (v.size()/2));
        };

        const auto ledBuf = runSineAt(0);
        const auto siBuf  = runSineAt(1);
        const auto liftBuf= runSineAt(2);

        // Normalization: peak <= ~1.1 (some overshoot allowed for transient region)
        check(peak(ledBuf)  < 1.1f, "LED mode: normalized peak < 1.1 (normalization works)");
        check(peak(siBuf)   < 1.1f, "Silicon mode: normalized peak < 1.1");
        check(peak(liftBuf) < 1.1f, "Lift mode: normalized peak < 1.1");

        // Silicon (0.65V) clips harder than LED (1.7V) — MAV closer to peak.
        const float mavLed = mav(ledBuf);
        const float mavSi  = mav(siBuf);
        check(mavSi > mavLed,
              "Silicon flattens toward unity faster than LED (MAV_si > MAV_led)");

        // Asymmetry: DC offset is non-zero (formula is not symmetric)
        const float dcLed = dcOffset(ledBuf);
        check(std::abs(dcLed) > 1e-4f,
              "Asymmetric waveshaper produces non-zero DC offset on pure sine");

        // No NaN / Inf in any output sample
        bool finiteOk = true;
        for (float v : ledBuf) if (!std::isfinite(v)) { finiteOk = false; break; }
        for (float v : siBuf)  if (!std::isfinite(v)) { finiteOk = false; break; }
        for (float v : liftBuf)if (!std::isfinite(v)) { finiteOk = false; break; }
        check(finiteOk, "All waveshaper outputs are finite (no NaN/Inf)");

        // Lift mode (threshold=12) is near-linear: the waveshaper gain is tanh(10*x/12)
        // which for small x gives 10x/12 = 0.83x — attenuates slightly and never saturates.
        // Silicon (threshold=0.65) saturates hard: gain=10/0.65=15.4x, output clips to ~1.0.
        // Correct check: Silicon's peak is much closer to saturation (1.0) than Lift.
        // Silicon clips hard -> peak_si/mav_si ~= 1.0 (flat top).
        // Lift is near-linear -> peak_lift/mav_lift ~= sqrt(2) (unclipped sine shape).
        const float siCrestFactor   = peak(siBuf)   / (mavSi   > 0.0f ? mavSi   : 1.0f);
        const float mavLift = mav(liftBuf);
        const float liftCrestFactor = peak(liftBuf) / (mavLift > 0.0f ? mavLift : 1.0f);
        check(liftCrestFactor > siCrestFactor,
              "Lift is near-linear (crest factor > Silicon which clips flat)");
    }

    // -----------------------------------------------------------------------
    // [RAT-05] Reverse-wired tone LPF (475Hz–32kHz)
    // -----------------------------------------------------------------------
    // [RAT-05] Reverse-wired tone LPF
    {
        std::printf("\n[RAT-05] Reverse-wired tone LPF\n");
        TurboRat rat;
        rat.prepare(44100.0, 512);

        const double osSr = 176400.0;
        const int    N    = 16384;

        auto rmsAtFilter = [&](float filterPct, double freqHz) -> float {
            TurboRat::Parameters p{};
            p.drive    = 20.0f;        // low drive: waveshaper near linear
            p.filter   = filterPct;
            p.volume   = 50.0f;        // 1.0x gain
            p.clipMode = 2;            // Lift (near-linear) so the test isolates the tone LPF
            p.asym     = 20.0f;
            p.sag      = 25.0f;
            rat.setParameters(p);
            rat.reset();
            // Prime the smoothers by processing a short silence block first
            std::vector<float> prime(2048, 0.0f);
            float* pch[1] = { prime.data() };
            juce::dsp::AudioBlock<float> pblock(pch, 1, (size_t)2048);
            for (int k = 0; k < 20; ++k) rat.processOS(pblock);   // let smoothers converge

            std::vector<float> buf(N);
            for (int i = 0; i < N; ++i)
                buf[i] = static_cast<float>(0.5 * std::sin(2.0 * M_PI * freqHz * i / osSr));
            float* ch[1] = { buf.data() };
            juce::dsp::AudioBlock<float> block(ch, 1, (size_t)N);
            rat.processOS(block);
            double sumSq = 0.0;
            for (int i = N/2; i < N; ++i) sumSq += buf[i] * buf[i];
            return static_cast<float>(std::sqrt(sumSq / (N/2)));
        };

        const float r0   = rmsAtFilter(0.0f,   5000.0);
        const float r25  = rmsAtFilter(25.0f,  5000.0);
        const float r50  = rmsAtFilter(50.0f,  5000.0);
        const float r75  = rmsAtFilter(75.0f,  5000.0);
        const float r100 = rmsAtFilter(100.0f, 5000.0);

        check(r0 > r100,
              "filter=0 passes more 5kHz than filter=100 (reverse-wired: 0=bright)");
        check(r100 < r0 * 0.25f,
              "filter=100 attenuates 5kHz by >12dB vs filter=0");
        check(r0 >= r25 && r25 >= r50 && r50 >= r75 && r75 >= r100,
              "Filter sweep 0->100 monotonically darkens 5kHz");
    }

    // -----------------------------------------------------------------------
    // [RAT-06] JFET output buffer (18kHz LP + volume scalar)
    // -----------------------------------------------------------------------
    // [RAT-06] JFET output buffer (18kHz LP + volume scalar)
    {
        std::printf("\n[RAT-06] JFET output buffer\n");
        TurboRat rat;
        rat.prepare(44100.0, 512);

        const double osSr = 176400.0;
        const int    N    = 16384;

        auto rmsAtVolume = [&](float volumePct, double freqHz) -> float {
            TurboRat::Parameters p{};
            p.drive    = 20.0f;
            p.filter   = 0.0f;         // bright — tone LPF out of the way
            p.volume   = volumePct;
            p.clipMode = 2;            // Lift
            p.asym     = 20.0f;
            p.sag      = 25.0f;
            rat.setParameters(p);
            rat.reset();
            std::vector<float> prime(2048, 0.0f);
            float* pch[1] = { prime.data() };
            juce::dsp::AudioBlock<float> pblock(pch, 1, (size_t)2048);
            for (int k = 0; k < 20; ++k) rat.processOS(pblock);

            std::vector<float> buf(N);
            for (int i = 0; i < N; ++i)
                buf[i] = static_cast<float>(0.3 * std::sin(2.0 * M_PI * freqHz * i / osSr));
            float* ch[1] = { buf.data() };
            juce::dsp::AudioBlock<float> block(ch, 1, (size_t)N);
            rat.processOS(block);
            double sumSq = 0.0;
            for (int i = N/2; i < N; ++i) sumSq += buf[i] * buf[i];
            return static_cast<float>(std::sqrt(sumSq / (N/2)));
        };

        const float rV0   = rmsAtVolume(0.0f,   1000.0);
        const float rV50  = rmsAtVolume(50.0f,  1000.0);
        const float rV100 = rmsAtVolume(100.0f, 1000.0);

        check(rV0 < 1e-3f,  "volume=0 -> silence");
        check(rV100 > rV50 * 1.7f,
              "volume=100 (2.0x) produces ~2x more RMS than volume=50 (1.0x)");

        // JFET 18kHz LP: 20kHz sine is attenuated relative to 1kHz at full volume, filter=0
        const float r1k  = rmsAtVolume(100.0f, 1000.0);
        const float r20k = rmsAtVolume(100.0f, 20000.0);
        check(r20k < r1k * 0.8f,
              "20kHz attenuated by JFET LP (18kHz) vs 1kHz");
    }

    // -----------------------------------------------------------------------
    // [RAT-07] Per-block coefficient update (no per-sample std::exp)
    // -----------------------------------------------------------------------
    // [RAT-07] Per-block coefficient invariant + parameter smoothing
    {
        std::printf("\n[RAT-07] Coefficient stability + smoothing\n");
        TurboRat rat;
        rat.prepare(44100.0, 512);
        TurboRat::Parameters p{};
        p.drive = 50.0f; p.filter = 50.0f; p.volume = 65.0f;
        p.asym = 20.0f; p.sag = 25.0f; p.clipMode = 0;
        rat.setParameters(p);
        rat.reset();

        // Prime smoothers to steady state
        std::vector<float> prime(2048, 0.0f);
        float* pch[1] = { prime.data() };
        juce::dsp::AudioBlock<float> pblock(pch, 1, (size_t)2048);
        for (int k = 0; k < 20; ++k) rat.processOS(pblock);

        auto c1 = rat.getCoefficientsForTest();
        // Second call with identical params: every coefficient must match exactly.
        // (No per-sample mutation of alphas allowed — only updateCoefficients writes them.)
        rat.processOS(pblock);
        auto c2 = rat.getCoefficientsForTest();

        check(c1.hpf1 == c2.hpf1, "hpf1Alpha stable across blocks at steady-state");
        check(c1.hpf2 == c2.hpf2, "hpf2Alpha stable across blocks at steady-state");
        check(c1.slew == c2.slew, "slewAlpha stable across blocks at steady-state");
        check(c1.gbw  == c2.gbw,  "gbwAlpha  stable across blocks at steady-state");
        check(c1.tone == c2.tone, "toneAlpha stable across blocks at steady-state");
        check(c1.jfet == c2.jfet, "jfetAlpha stable across blocks at steady-state");

        // Smoothing: changing drive target produces a RAMP in gbwAlpha across blocks,
        // not an instant jump.
        TurboRat::Parameters p2 = p;
        p2.drive = 100.0f;
        rat.setParameters(p2);
        rat.processOS(pblock);               // block 1: smoothers advance ~512 host samples
        auto c3 = rat.getCoefficientsForTest();
        rat.processOS(pblock);               // block 2: further advance
        auto c4 = rat.getCoefficientsForTest();

        check(c3.gbw != c1.gbw,
              "Changing drive target moves gbwAlpha after one block");
        check(c3.gbw != c4.gbw,
              "gbwAlpha still moving after block 2 (smoothing active, not snapping)");

        // Click-free: process 1kHz sine across a drive flip; adjacent-sample deltas bounded
        const double osSr = 176400.0;
        const int    Nblk = 2048;
        std::vector<float> sine(Nblk);
        for (int i = 0; i < Nblk; ++i)
            sine[i] = static_cast<float>(0.5 * std::sin(2.0 * M_PI * 1000.0 * i / osSr));

        rat.setParameters(p);  rat.reset();
        for (int k = 0; k < 10; ++k) {
            std::vector<float> buf = sine;
            float* ch[1] = { buf.data() };
            juce::dsp::AudioBlock<float> block(ch, 1, (size_t)Nblk);
            rat.processOS(block);
        }
        // Flip drive and measure first-block max delta
        rat.setParameters(p2);
        std::vector<float> buf = sine;
        float* ch[1] = { buf.data() };
        juce::dsp::AudioBlock<float> block(ch, 1, (size_t)Nblk);
        rat.processOS(block);
        float maxDelta = 0.0f;
        for (int i = 1; i < Nblk; ++i)
            maxDelta = std::max(maxDelta, std::abs(buf[i] - buf[i-1]));
        check(maxDelta < 0.5f,
              "No click on drive flip: adjacent-sample delta < 0.5");

        // Silence in -> silence out after reset (regression guard)
        rat.setParameters(p);
        rat.reset();
        std::vector<float> sil(512, 0.0f);
        float* sch[1] = { sil.data() };
        juce::dsp::AudioBlock<float> sblock(sch, 1, (size_t)512);
        rat.processOS(sblock);
        bool allSilent = true;
        for (float v : sil) if (std::abs(v) > 1e-6f) { allSilent = false; break; }
        check(allSilent, "Full chain: silence in -> silence out after reset()");
    }

    std::printf("\n=== Results: %d/%d tests passed ===\n",
                g_tests - g_failures, g_tests);

    return g_failures == 0 ? 0 : 1;
}
