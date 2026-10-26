#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>

namespace params
{
using APVTS = juce::AudioProcessorValueTreeState;

inline const juce::StringArray kPan    { "L", "C", "R" };
inline const juce::StringArray kDuty   { "12.5%", "25%", "50%", "75%" };
inline const juce::StringArray kShapes { "CUSTOM", "TRIANGLE", "SAW", "SQUARE", "SINE", "SOFT SAW", "ORGAN", "STAIRS" };

// Fill a 32-step, 4-bit wave RAM for a factory shape
inline void fillShape (int shape, uint8_t* ram)
{
    for (int i = 0; i < 32; ++i)
    {
        const double t = i / 32.0;
        double v = 0; // -1..1
        switch (shape)
        {
            case 1: v = t < 0.5 ? (4 * t - 1) : (3 - 4 * t); break;
            case 2: v = 2 * t - 1; break;
            case 3: v = t < 0.5 ? 1 : -1; break;
            case 4: v = std::sin (2 * 3.14159265358979 * t); break;
            case 5: v = std::sin (2 * 3.14159265358979 * t) * 0.6 + (2 * t - 1) * 0.5; break;
            case 6: v = 0.55 * std::sin (2 * 3.14159265358979 * t) + 0.3 * std::sin (4 * 3.14159265358979 * t)
                      + 0.2 * std::sin (6 * 3.14159265358979 * t); break;
            case 7: v = std::floor (t * 4) / 1.5 - 1; break;
            default: v = t < 0.5 ? (4 * t - 1) : (3 - 4 * t); break;
        }
        int s = (int) std::lround ((juce::jlimit (-1.0, 1.0, v) + 1.0) * 7.5);
        ram[i] = (uint8_t) juce::jlimit (0, 15, s);
    }
}

inline APVTS::ParameterLayout createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    auto I = [&] (const char* id, const char* name, int lo, int hi, int def)
    { p.push_back (std::make_unique<AudioParameterInt> (ParameterID { id, 1 }, name, lo, hi, def)); };
    auto F = [&] (const char* id, const char* name, float lo, float hi, float def, float skewMid = -1.0f,
                  const char* unit = "")
    {
        NormalisableRange<float> r (lo, hi);
        if (skewMid > 0) r.setSkewForCentre (skewMid);
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def,
                     AudioParameterFloatAttributes().withLabel (unit)));
    };
    auto C = [&] (const char* id, const char* name, StringArray ch, int def)
    { p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, ch, def)); };
    auto B = [&] (const char* id, const char* name, bool def)
    { p.push_back (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def)); };

    // Envelope (DMG NRx2) + extras
    I ("env_vol",  "Env Volume", 0, 15, 15);
    I ("env_pace", "Env Pace",   0, 7, 0);
    I ("env_len",  "Length",     1, 64, 64);           // 64 = off
    C ("env_dir",  "Env Direction", { "DOWN", "UP" }, 0);
    F ("env_rel",  "Release", 0.0f, 3.0f, 0.15f, 0.5f, "s");

    // Pitch / vibrato
    F ("glide",     "Glide", 0.0f, 1.0f, 0.0f, 0.15f, "s");
    I ("bend",      "Bend Range", 0, 24, 2);
    F ("vib_rate",  "Vibrato Rate", 0.1f, 15.0f, 5.5f, 4.0f, "Hz");
    F ("vib_depth", "Vibrato Depth", 0.0f, 100.0f, 0.0f, -1.0f, "ct");
    F ("vib_delay", "Vibrato Delay", 0.0f, 2.0f, 0.3f, 0.5f, "s");

    // CH1 pulse + sweep
    B ("p1_on", "P1 On", true);
    F ("p1_level", "P1 Level", 0.0f, 1.0f, 0.8f);
    C ("p1_pan", "P1 Pan", kPan, 1);
    C ("p1_duty", "P1 Duty", kDuty, 2);
    I ("p1_pace", "P1 Sweep Pace", 0, 7, 0);
    I ("p1_shift", "P1 Sweep Shift", 0, 7, 0);
    C ("p1_sweepdir", "P1 Sweep Dir", { "DN", "UP" }, 1);
    B ("p1_sweep", "P1 Sweep On", false);

    // CH2 pulse
    B ("p2_on", "P2 On", false);
    F ("p2_level", "P2 Level", 0.0f, 1.0f, 0.8f);
    C ("p2_pan", "P2 Pan", kPan, 1);
    C ("p2_duty", "P2 Duty", kDuty, 1);
    F ("p2_detune", "P2 Detune", -50.0f, 50.0f, 0.0f, -1.0f, "ct");
    I ("p2_oct", "P2 Octave", -2, 2, 0);

    // CH3 wave
    B ("w_on", "Wave On", false);
    F ("w_level", "Wave Level", 0.0f, 1.0f, 0.8f);
    C ("w_pan", "Wave Pan", kPan, 1);
    C ("w_shape", "Wave Shape", kShapes, 1);
    I ("w_oct", "Wave Octave", -2, 2, 0);
    C ("w_vol", "Wave Volume", { "100", "50", "25" }, 0);

    // CH4 noise
    B ("n_on", "Noise On", false);
    F ("n_level", "Noise Level", 0.0f, 1.0f, 0.6f);
    C ("n_pan", "Noise Pan", kPan, 1);
    C ("n_lfsr", "LFSR Mode", { "15-BIT", "7-BIT" }, 0);
    F ("n_pitch", "Noise Pitch", 0.0f, 1.0f, 0.75f);
    C ("n_track", "Noise Tracking", { "FIXED", "KEY" }, 0);

    // Voice / arp
    C ("v_mode", "Voice Mode", { "POLY", "MONO", "ARP" }, 0);
    F ("arp_speed", "Arp Speed", 1.0f, 60.0f, 12.0f, 12.0f, "Hz");
    C ("arp_pattern", "Arp Pattern", { "UP", "DN", "U/D", "ORD" }, 0);
    I ("arp_oct", "Arp Octaves", 1, 4, 1);

    // Output
    C ("out_route", "Routing", { "SPEAKER", "PHONES", "CLEAN" }, 1);
    C ("out_pitch", "Pitch Mode", { "FREE", "11-BIT" }, 1);
    F ("master", "Master", 0.0f, 1.0f, 0.7f);

    return { p.begin(), p.end() };
}
} // namespace params
