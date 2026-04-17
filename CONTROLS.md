# HEATDEATH — Control Reference

Four stages in series: RAT distortion → MicroPitch → Undulator tremolo → Burn-In tape saturation. Signal is mono in, stereo out. A global parallel mix blends the clean input against the fully processed output.

---

## Stage 1 — Turbo RAT

**Drive** (0–60, default 9)
Gain into the op-amp clipping stage. Low values (5–15) give gritty edge with clarity. Mid values (20–40) are compressed and aggressive. Above 45 the signal becomes dense and nearly square — good for textural mush, hard to cut through a mix.

**Filter** (0–100, default 1.5) — reverse-wired LPF
Counter-intuitive: 0 is bright (32kHz, almost no filtering), 100 is dark (475Hz, very muffled). At default it's near-flat. Around 30–40 you get classic RAT warmth. Above 70 it starts sounding underwater. Most useful range is 0–50.

**Volume** (0–75, default 24.5)
Output level after clipping. Not unity at 75 — more like a loud push. Use this to match perceived loudness to the dry signal or to drive the next stage harder.

**LM308 Slew** (0–100, default 68)
Models the LM308 op-amp's high-frequency rolloff. Higher = brighter and more open. Lower = the classic warm/fat RAT sound with HF squashed. At 0 there's a notable ~1kHz ceiling. At 100 it opens up to ~5kHz before the Filter takes over.

**Asymmetry** (0–100, default 20)
Introduces half-cycle clipping asymmetry — one side of the waveform clips differently. Low values (10–25) add subtle even-harmonic content, making distortion sound more "alive." Higher values get increasingly uneven and ragged, good for amp-like character.

**Sag** (0–100, default 25)
Power supply droop under transients. At 0 the supply is rigid — consistent response. As you raise it, loud transients cause momentary voltage drop, pulling back gain mid-attack. Creates that slightly squashed, vintage-amp breathing. Above 60 it becomes very pronounced, almost gated on peaks.

**Clip Mode** (LED / Silicon / Lift)
Selects the virtual diode type in the clipping circuit.
- **LED**: Harder threshold, more headroom before clipping — louder, more aggressive edge.
- **Silicon**: Softer threshold, gentler onset — the classic "round" RAT distortion.
- **Lift**: Very high threshold with makeup gain — almost clean at low drive, then hard-limits suddenly at high drive. Good for gating/transient shaping.

---

## Stage 2 — MicroPitch (H3000-style)

Pitch-shifts left and right channels by small amounts to create stereo width and chorusing through beating between channels.

**L Cents** (−25¢ to 0¢, default −25¢)
Left channel pitch shift. More negative = wider detune = faster beating rate. At −2¢ the beating is very slow and gentle. At −25¢ it's moving quickly and the stereo image is wide.

**R Cents** (0¢ to +25¢, default +25¢)
Right channel pitch shift upward. Mirrors L Cents. The asymmetry between L and R values is what creates the beating.

**Mix** (0–100%, default 100%)
Blends the pitch-shifted signal with dry mono. At 0% you hear just the unprocessed signal. At 100% it's fully shifted. Mid values (40–70%) give a subtle shimmer while keeping mono-compatibility.

**Rate** (20–2000Hz, default 440Hz)
Sets the reference frequency used to calculate beating rate from the cent values. Lower = slower beats for a given detune amount. At 100Hz with ±7¢ the beating is glacial. At 1kHz+ it's fast and fluttery. Think of it as "where in the frequency range is the pitch shifting calibrated."

---

## Stage 3 — Undulator (H3000-style tremolo/AM)

Amplitude modulation with a two-LFO system — a primary LFO sets the main tremolo rate and depth, a secondary LFO slowly modulates those parameters over time. A detuned stereo delay sits before the AM for width and shimmer.

**Depth** (0–100, default 68)
How deep the tremolo cuts. 0 = no AM, signal passes flat. 30 = gentle pulse. 68 = strong cut but signal never goes silent. Above 85 it approaches silence at the trough.

**Speed** (0.5–8.5Hz, default 2.4Hz)
Primary LFO rate. 0.5–1.5Hz is slow, hypnotic. 2–4Hz is classic tremolo range. Above 5Hz starts sounding like a vibrato artifact or ring-mod smear at high depth.

**Space** (0–100, default 55)
Detuned stereo delay spread. Controls the delay time difference between L and R channels before the AM stage. Low values = subtle width. High values = wider, comb-filtered, almost flanger-like. Interacts heavily with Feedback.

**Waver** (0–100, default 45)
Secondary LFO depth modulation. How much the secondary LFO varies the primary AM depth over time. At 0 the depth is fixed. Higher values make the tremolo swell and retreat in irregular waves — prevents the effect from feeling robotic.

**Mix** (0–100%, default 100%)
Wet/dry for the Undulator stage. Lower values blend processed and unprocessed for subtlety.

**LFO Shape** (SIN / TRI / PKK / RND / ENV)
- **SIN**: Smooth, even tremolo. The most musical and unobtrusive.
- **TRI**: Linear ramp up and down — slightly more mechanical, crisper edge.
- **PKK**: Fast attack, exponential decay — choppy, almost gated tremolo.
- **RND**: Two inharmonic sines mixed — irregular, never quite repeats. Good for organic textures.
- **ENV**: Tracks input RMS — the louder the input, the more the tremolo engages. Good for dynamic, responsive feel.

*In the Undulator section, Space and Waver knobs are visible but Speed/Waver/Mix refer to the three lower knobs (Space / Waver / Mix from left to right).*

---

## Stage 4 — Burn-In (tape saturation)

Thermal tape saturation model. Simulates oxide degradation, print-through, and sticky-slip playback artifacts.

**Burn** (0–100, default 70)
Heat rate — how aggressively the tape model accumulates distortion. At low values (10–25) it's a gentle, even-harmonic warmth and subtle HF compression. Mid values (40–60) add audible saturation on peaks and some LF density. Above 70 the nonlinear character becomes pronounced — transient softening, harmonic bloom, the signal "melts" a little. Above 85 it's deliberately degraded: pitch instability artifacts, oxide-shed texture.

**Mix** (0–100%, default 0%)
Wet/dry for Burn-In. Default is 0% so the stage is off until you dial it in. Blend to taste — a small amount (15–30%) of a well-set Burn amount is often more useful than full wet.

---

## Global

**Master** (0–100%, default 100%)
Global parallel wet/dry. The clean mono input is held at the plugin boundary and blended with the fully processed stereo output. At 0% you hear the dry signal. At 100% full processing. Mid values parallel-process — the dry mono anchors the low end and center while the stereo effects float on top. Very useful for keeping bass focused.

---

## Inter-Stage Trims

Three ±12dB gain controls applied between stages. Not exposed on the main UI — automation targets. Useful when one stage is significantly louder or quieter than the next, especially when RAT drive is high and you need to keep MicroPitch input sane.

- **Post-RAT Trim**: Between RAT output and MicroPitch input.
- **Post-Pitch Trim**: Between MicroPitch output and Undulator input.
- **Post-Undulator Trim**: Between Undulator output and Burn-In input.

---

## Expert / Hidden

**Global Feedback** (off by default)
Routes a fraction of the final output back to the RAT input. Even tiny amounts create sub-oscillation, resonance buildup, and instability. Keep the amount very low (under 10) — above that it self-oscillates quickly.

**M/S Mode**
Burn-In processes mid and side channels independently instead of L/R. Useful for applying tape character only to the stereo image while keeping the center cleaner, or vice versa.

**Acetate Mode** (hidden, no UI label)
Changes Burn-In physics from polyester to acetate tape character. Sharper oxide-shedding transitions, lower stick-slip threshold, occasional sudden pitch drop at high temperature, faster demagnetization falloff. Discoverable by accident.
