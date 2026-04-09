#pragma once

struct SmoothParam
{
    float current = 0.f;
    float target  = 0.f;

    void  setTimeMs (float /*ms*/, double /*sampleRate*/) {}
    void  setTarget (float v) { target = v; }
    float tick()     { current = target; return current; }  // Phase 1: no smoothing
    void  reset (float v) { current = target = v; }
};
