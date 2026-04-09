#pragma once

struct BiquadFilter
{
    float b0 = 1.f, b1 = 0.f, b2 = 0.f, a1 = 0.f, a2 = 0.f;
    float z1 = 0.f, z2 = 0.f;

    void reset() { z1 = z2 = 0.f; }

    float process (float x)
    {
        // DF1 — Phase 1: coefficients default to pass-through (b0=1, rest=0)
        float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};
