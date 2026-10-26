#pragma once
#include <vector>
#include <utility>

namespace presets
{
struct Preset
{
    const char* name;
    std::vector<std::pair<const char*, float>> values;   // real (unnormalised) values; choices = index
    const char* customWave = nullptr;                     // 32 hex digits when w_shape = CUSTOM
};

inline const std::vector<Preset>& all()
{
    static const std::vector<Preset> list = {
        { "INIT SQUARE", {} },
        { "POCKET LEAD", { { "p1_duty", 1 }, { "p2_on", 1 }, { "p2_duty", 2 }, { "p2_detune", 7 }, { "p2_level", 0.45f },
                           { "vib_depth", 28 }, { "vib_rate", 6.2f }, { "vib_delay", 0.25f }, { "env_rel", 0.2f },
                           { "v_mode", 1 }, { "glide", 0.06f } } },
        { "OVERWORLD BASS", { { "p1_on", 0 }, { "w_on", 1 }, { "w_shape", 1 }, { "w_oct", -1 }, { "w_level", 0.95f },
                              { "p2_on", 1 }, { "p2_duty", 0 }, { "p2_oct", -1 }, { "p2_level", 0.25f },
                              { "env_vol", 12 }, { "env_pace", 6 }, { "env_rel", 0.08f } } },
        { "TWIN PULSE CHORUS", { { "p1_duty", 2 }, { "p1_pan", 0 }, { "p1_level", 0.75f },
                                 { "p2_on", 1 }, { "p2_duty", 1 }, { "p2_pan", 2 }, { "p2_detune", 12 }, { "p2_level", 0.75f },
                                 { "vib_depth", 14 }, { "vib_rate", 5.0f }, { "vib_delay", 0.3f },
                                 { "env_vol", 13 }, { "env_rel", 0.4f }, { "w_shape", 1 } } },
        { "COIN GET", { { "p1_duty", 2 }, { "env_pace", 3 }, { "v_mode", 2 }, { "arp_speed", 13 },
                        { "arp_oct", 2 }, { "out_route", 0 }, { "env_rel", 0.05f } } },
        { "WAVE BELL", { { "p1_duty", 0 }, { "env_pace", 3 }, { "p1_level", 0.6f }, { "w_on", 1 }, { "w_shape", 4 },
                         { "w_oct", 1 }, { "w_level", 0.45f }, { "w_vol", 1 }, { "env_rel", 0.7f }, { "vib_depth", 6 } } },
        { "NOISE HAT", { { "p1_on", 0 }, { "n_on", 1 }, { "n_pitch", 0.97f }, { "n_level", 0.8f },
                         { "env_pace", 1 }, { "env_len", 10 }, { "env_rel", 0.03f } } },
        { "SNARE CRUNCH", { { "p1_duty", 2 }, { "p1_sweep", 1 }, { "p1_sweepdir", 0 }, { "p1_pace", 1 }, { "p1_shift", 2 },
                            { "p1_level", 0.5f }, { "n_on", 1 }, { "n_pitch", 0.78f }, { "n_level", 0.85f },
                            { "env_pace", 2 }, { "env_rel", 0.05f } } },
        { "KICK SWEEP", { { "p1_duty", 2 }, { "p1_sweep", 1 }, { "p1_sweepdir", 0 }, { "p1_pace", 1 }, { "p1_shift", 1 },
                          { "p1_level", 1.0f }, { "n_on", 1 }, { "n_pitch", 0.55f }, { "n_level", 0.35f },
                          { "env_len", 40 }, { "env_pace", 3 }, { "env_rel", 0.0f }, { "out_pitch", 0 } } },
        { "ARP DUNGEON", { { "p1_duty", 1 }, { "v_mode", 2 }, { "arp_pattern", 2 }, { "arp_speed", 16 },
                           { "arp_oct", 2 }, { "env_vol", 12 }, { "p2_on", 1 }, { "p2_duty", 0 },
                           { "p2_detune", -10 }, { "p2_level", 0.35f }, { "env_rel", 0.1f } } },
        { "TRIANGLE SUB", { { "p1_on", 0 }, { "w_on", 1 }, { "w_shape", 1 }, { "w_oct", -1 }, { "w_level", 1.0f },
                            { "v_mode", 1 }, { "glide", 0.05f }, { "env_rel", 0.1f } } },
        { "LASER ZAP", { { "p1_duty", 0 }, { "p1_sweep", 1 }, { "p1_sweepdir", 0 }, { "p1_pace", 2 }, { "p1_shift", 2 },
                         { "env_pace", 3 }, { "env_rel", 0.0f }, { "out_pitch", 0 } } },
        { "CHIP ORGAN", { { "p1_on", 0 }, { "w_on", 1 }, { "w_shape", 6 }, { "w_level", 0.9f }, { "p2_on", 1 },
                          { "p2_duty", 2 }, { "p2_oct", 1 }, { "p2_level", 0.35f }, { "env_rel", 0.2f } } },
        { "BATTLE BRASS", { { "p1_duty", 1 }, { "p2_on", 1 }, { "p2_duty", 0 }, { "p2_detune", -6 }, { "p2_level", 0.55f },
                            { "env_vol", 6 }, { "env_dir", 1 }, { "env_pace", 1 }, { "vib_depth", 20 },
                            { "vib_delay", 0.45f }, { "env_rel", 0.25f } } },
        { "SAW BASS", { { "p1_on", 0 }, { "w_on", 1 }, { "w_shape", 2 }, { "w_oct", -1 }, { "w_level", 0.95f },
                        { "p2_on", 1 }, { "p2_duty", 0 }, { "p2_oct", -1 }, { "p2_level", 0.3f },
                        { "v_mode", 1 }, { "glide", 0.04f }, { "env_rel", 0.06f } } },
        { "LINK CABLE PAD", { { "p1_duty", 2 }, { "p1_pan", 0 }, { "p1_level", 0.6f }, { "p2_on", 1 }, { "p2_duty", 0 },
                              { "p2_pan", 2 }, { "p2_detune", 10 }, { "p2_level", 0.6f }, { "w_on", 1 }, { "w_shape", 4 },
                              { "w_oct", -1 }, { "w_level", 0.5f }, { "env_vol", 10 }, { "env_rel", 1.5f },
                              { "vib_depth", 15 }, { "vib_delay", 0.6f }, { "vib_rate", 4.5f } } },
        { "TINY SPEAKER", { { "p1_duty", 1 }, { "out_route", 0 }, { "vib_depth", 18 }, { "env_rel", 0.15f } } },
        { "METAL TONE", { { "p1_on", 0 }, { "n_on", 1 }, { "n_lfsr", 1 }, { "n_track", 1 }, { "n_pitch", 0.62f },
                          { "n_level", 0.8f }, { "env_pace", 4 }, { "env_rel", 0.2f } } },
        { "GHOST WAVE", { { "p1_on", 0 }, { "w_on", 1 }, { "w_shape", 0 }, { "w_level", 0.9f }, { "vib_depth", 22 },
                          { "vib_rate", 3.2f }, { "vib_delay", 0.2f }, { "env_rel", 0.8f } },
          "0123456789ABCDEFFEDCBA9876543210" },
    };
    return list;
}
} // namespace presets
