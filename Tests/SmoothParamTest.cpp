// SmoothParam unit tests — Phase 02-02
// Tests for one-pole IIR smoother contracts.
// Run: cmake --build build --target SmoothParamTest && ./build/Tests/SmoothParamTest

#include "../Source/utils/SmoothParam.h"
#include <cstdio>
#include <cmath>
#include <cstdlib>

static int g_tests = 0;
static int g_failures = 0;

static void check(bool cond, const char* desc) {
    ++g_tests;
    if (!cond) {
        ++g_failures;
        std::printf("  FAIL: %s\n", desc);
    } else {
        std::printf("  pass: %s\n", desc);
    }
}

int main()
{
    std::printf("=== SmoothParam Tests ===\n\n");

    // -----------------------------------------------------------------------
    // Contract 1: API compiles (setTimeMs, setTarget, tick, reset exist)
    // -----------------------------------------------------------------------
    {
        std::printf("[Contract 1] API existence\n");
        SmoothParam sp;
        sp.setTimeMs(20.0f, 44100.0);
        sp.setTarget(1.0f);
        float v = sp.tick();
        sp.reset(0.0f);
        check(true, "API compiles: setTimeMs/setTarget/tick/reset all present");
        (void)v;
    }

    // -----------------------------------------------------------------------
    // Contract 2: setTimeMs coeff validity
    // -----------------------------------------------------------------------
    {
        std::printf("\n[Contract 2] setTimeMs coeff validity\n");
        SmoothParam sp;

        sp.setTimeMs(20.0f, 44100.0);
        check(sp.coeff > 0.0f && sp.coeff < 1.0f,
              "setTimeMs(20ms, 44100Hz) -> coeff in (0, 1)");

        // Expected: coeff ≈ exp(-2π / 882) ≈ 0.99290
        check(sp.coeff > 0.99f && sp.coeff < 0.9999f,
              "setTimeMs(20ms, 44100Hz) -> coeff ≈ 0.9929 (between 0.99 and 0.9999)");

        sp.setTimeMs(0.0f, 44100.0);
        check(sp.coeff == 0.0f, "setTimeMs(0ms) -> coeff = 0 (instant)");

        // ms <= 0 or sampleRate <= 0 → coeff = 0, no divide-by-zero
        SmoothParam sp2;
        sp2.setTimeMs(20.0f, 0.0);
        check(sp2.coeff == 0.0f, "setTimeMs(20ms, sr=0) -> coeff = 0 (no crash)");

        SmoothParam sp3;
        sp3.setTimeMs(-5.0f, 44100.0);
        check(sp3.coeff == 0.0f, "setTimeMs(ms<0) -> coeff = 0");
    }

    // -----------------------------------------------------------------------
    // Contract 3: reset()
    // -----------------------------------------------------------------------
    {
        std::printf("\n[Contract 3] reset()\n");
        SmoothParam sp;
        sp.setTimeMs(20.0f, 44100.0);
        sp.setTarget(0.8f);
        // advance a few ticks to build up current
        sp.tick(); sp.tick(); sp.tick();

        sp.reset(0.5f);
        check(sp.current == 0.5f, "reset(0.5f) -> current == 0.5f");
        check(sp.target  == 0.5f, "reset(0.5f) -> target  == 0.5f");

        float v = sp.tick();
        check(v == 0.5f, "tick() after reset(0.5f) returns 0.5f (no ramp)");
    }

    // -----------------------------------------------------------------------
    // Contract 4: tick convergence (one time-constant ~63%)
    // -----------------------------------------------------------------------
    {
        std::printf("\n[Contract 4] tick convergence\n");
        SmoothParam sp;
        sp.reset(0.0f);
        sp.setTimeMs(20.0f, 44100.0);
        sp.setTarget(1.0f);

        // After 882 ticks (== 20ms at 44100Hz), current should be ≈63%
        for (int i = 0; i < 882; ++i)
            sp.tick();

        check(sp.current > 0.6f && sp.current < 1.0f,
              "After 882 ticks (20ms), current > 0.6 and < 1.0 (~63% at one tau)");

        // After 10000 ticks, should be very close to 1.0
        for (int i = 882; i < 10000; ++i)
            sp.tick();

        check(sp.current > 0.999f,
              "After 10000 ticks, current > 0.999 (converged)");
    }

    // -----------------------------------------------------------------------
    // Contract 5: tick is strictly monotonic (no snapping)
    // -----------------------------------------------------------------------
    {
        std::printf("\n[Contract 5] strict monotonicity\n");
        SmoothParam sp;
        sp.reset(0.0f);
        sp.setTimeMs(20.0f, 44100.0);
        sp.setTarget(1.0f);

        float prev = sp.current;
        bool monotonic = true;
        for (int i = 0; i < 100; ++i) {
            float v = sp.tick();
            if (v <= prev) {
                monotonic = false;
                std::printf("    non-monotonic at tick %d: prev=%f, cur=%f\n", i, prev, v);
                break;
            }
            prev = v;
        }
        check(monotonic, "tick() is strictly monotonic for first 100 steps (0→1)");
    }

    // -----------------------------------------------------------------------
    // Contract 6: no allocation — compile-time check via sizeof (POD style)
    // -----------------------------------------------------------------------
    {
        std::printf("\n[Contract 6] POD-style struct size\n");
        // SmoothParam should have exactly 3 floats (current, target, coeff)
        // In a real POD struct with no vtable that's 12 bytes.
        constexpr std::size_t expected = sizeof(float) * 3;
        check(sizeof(SmoothParam) == expected,
              "sizeof(SmoothParam) == 12 bytes (3 floats, no vtable)");
    }

    std::printf("\n=== Results: %d/%d tests passed ===\n",
                g_tests - g_failures, g_tests);

    return g_failures == 0 ? 0 : 1;
}
