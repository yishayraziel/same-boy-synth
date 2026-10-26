# Same Boy — 4-Channel PSG Synthesizer

A VST3 / AU instrument that emulates the original Game Boy (DMG) sound chip.
Every note plays the full four-channel patch: **Pulse 1** (with hardware frequency sweep),
**Pulse 2**, **Wave** (32 × 4-bit drawable wave RAM) and **Noise** (15/7-bit LFSR).

## Sound engine
- DMG-accurate envelope: 4-bit volume, pace in 1/64 s steps, up/down direction
- Length counter (1/256 s units, `OFF` at max) and a stepped 4-bit release
- Pulse 1 sweep: `x ± x >> shift` every `pace/128 s`, with overflow cut-off like the hardware
- **11-BIT** pitch mode quantises every note to the real 11-bit frequency register
  (`131072/(2048-x)` for pulse, `65536/(2048-x)` for wave, the real noise clock table);
  **FREE** gives exact equal temperament
- Wave channel volume codes 100 / 50 / 25 %, hard L/C/R panning per channel (NR51 style)
- Band-limited (PolyBLEP + 4× oversampling) so it's clean in a mix, not aliased mush
- Output routing: **SPEAKER** (mono, tiny-speaker band-pass), **PHONES** (stereo, DMG coupling cap),
  **CLEAN** (DC block only)
- Voice modes: **POLY** (8 voices), **MONO** (legato + glide), **ARP** (UP / DN / U/D / played ORDer, 1–4 octaves)
- Vibrato with delay, mod wheel adds vibrato, pitch-bend range knob, sustain pedal

## Panel
- Drag knobs vertically, double-click to reset, mouse-wheel for fine moves. The bottom bar of the scope
  shows the value of whatever you hover.
- **P1 P2 WU NZ** (above scope) and the **CH** squares turn channels on/off. The footer letters light when a channel sounds.
- **Draw in the wave RAM** to make a custom wave (shape switches to RAM).
- Preset box: `<` / `>` or click the name for the list. **D-pad** steps presets, **A**/**B** audition C4/C3,
  **SELECT** = init patch, **START** = randomize.
- The window is resizable (drag the corner).

## Building
```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target SameBoy_VST3
```
JUCE 8 is fetched automatically. GitHub Actions (`.github/workflows/build.yml`) builds macOS
(universal VST3 + AU + .pkg installer) and Windows x64 VST3 on every push.
