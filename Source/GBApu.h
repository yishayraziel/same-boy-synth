// Same Boy - Game Boy (DMG) APU emulation for a polyphonic instrument.
// Each voice owns a full set of the four DMG channels (pulse+sweep, pulse,
// wave RAM, LFSR noise) so a note sounds the whole layered patch.
#pragma once
#include <cmath>
#include <cstdint>
#include <array>
#include <algorithm>

namespace gb
{
constexpr double kClock = 4194304.0;
constexpr double kPi = 3.14159265358979323846;

// ---- 11-bit frequency register helpers (DMG) --------------------------------
inline double pulseFreqFromX (double x) { return 131072.0 / (2048.0 - x); }
inline double waveFreqFromX  (double x) { return 65536.0  / (2048.0 - x); }
inline double xFromPulseFreq (double f) { return 2048.0 - 131072.0 / f; }

inline double quantizePulse (double f)
{
    double x = std::round (2048.0 - 131072.0 / f);
    x = std::clamp (x, 0.0, 2047.0);
    return pulseFreqFromX (x);
}
inline double quantizeWave (double f)
{
    double x = std::round (2048.0 - 65536.0 / f);
    x = std::clamp (x, 0.0, 2047.0);
    return waveFreqFromX (x);
}

inline double noteToHz (double note) { return 440.0 * std::pow (2.0, (note - 69.0) / 12.0); }

// ---- noise clock table: 262144 / (r * 2^s) ---------------------------------
struct NoiseTable
{
    std::array<double, 112> f {};
    int n = 0;
    NoiseTable()
    {
        for (int s = 0; s < 14; ++s)
            for (int r = 0; r < 8; ++r)
            {
                const double div = (r == 0 ? 8.0 : 16.0 * r) * (double) (1 << s);
                double v = kClock / div;
                bool dup = false;
                for (int i = 0; i < n; ++i) if (std::abs (f[(size_t) i] - v) < 1e-6) dup = true;
                if (! dup && n < (int) f.size()) f[(size_t) n++] = v;
            }
        std::sort (f.begin(), f.begin() + n); // ascending
    }
    double nearest (double hz) const
    {
        double best = f[0], bd = 1e30;
        for (int i = 0; i < n; ++i)
        {
            double d = std::abs (std::log (f[(size_t) i] / hz));
            if (d < bd) { bd = d; best = f[(size_t) i]; }
        }
        return best;
    }
    double fromKnob (double k) const   // 0..1 -> table entry
    {
        int i = (int) std::lround (k * (n - 1));
        return f[(size_t) std::clamp (i, 0, n - 1)];
    }
};

// ---- PolyBLEP ---------------------------------------------------------------
inline double polyblep (double t, double dt)
{
    if (t < dt)       { t /= dt;             return t + t - t * t - 1.0; }
    if (t > 1.0 - dt) { t = (t - 1.0) / dt;  return t * t + t + t + 1.0; }
    return 0.0;
}

// ---- Pulse oscillator (duty 12.5 / 25 / 50 / 75 %) -------------------------
struct PulseOsc
{
    double phase = 0.0;
    double next (double freq, double sr, int duty)
    {
        static constexpr double duties[4] = { 0.125, 0.25, 0.5, 0.75 };
        const double d  = duties[duty & 3];
        const double dt = std::min (freq / sr, 0.49);
        double y = phase < d ? 1.0 : -1.0;
        y += polyblep (phase, dt);
        double t2 = phase + 1.0 - d; if (t2 >= 1.0) t2 -= 1.0;
        y -= polyblep (t2, dt);
        phase += dt; if (phase >= 1.0) phase -= 1.0;
        return y;
    }
};

// ---- Wave RAM oscillator: 32 x 4-bit samples -------------------------------
struct WaveOsc
{
    double phase = 0.0;
    // returns a DAC-style value in [-1, 1] (before volume shift)
    double next (double freq, double sr, const uint8_t* ram)
    {
        const double dt = std::min (freq / sr, 0.49);
        const double u  = phase * 32.0;
        const int idx   = (int) u & 31;
        double y = ram[idx] / 7.5 - 1.0;
        // BLEP the step edges (step-domain)
        const double du = dt * 32.0;
        if (du < 0.5)
        {
            const double frac = u - std::floor (u);
            const int prev = (idx + 31) & 31, nxt = (idx + 1) & 31;
            const double hPrev = (ram[idx] - ram[prev]) / 7.5;   // edge we just crossed
            const double hNext = (ram[nxt] - ram[idx]) / 7.5;    // edge coming up
            if (frac < du)         y += 0.5 * hPrev * polyblep (frac, du);
            else if (frac > 1 - du) y += 0.5 * hNext * polyblep (frac, du);
        }
        phase += dt; if (phase >= 1.0) phase -= 1.0;
        return y;
    }
};

// ---- LFSR noise (15 / 7 bit) with box-filtered output ----------------------
struct NoiseOsc
{
    uint16_t lfsr = 0x7fff;
    double phase = 0.0;
    void reset() { lfsr = 0x7fff; phase = 0.0; }
    inline double out() const { return (lfsr & 1) ? -1.0 : 1.0; }
    inline void clock (bool short7)
    {
        const uint16_t bit = (uint16_t) ((lfsr ^ (lfsr >> 1)) & 1);
        lfsr = (uint16_t) ((lfsr >> 1) | (bit << 14));
        if (short7) lfsr = (uint16_t) ((lfsr & ~(1 << 6)) | (bit << 6));
    }
    double next (double clockHz, double sr, bool short7)
    {
        const double step = clockHz / sr;   // LFSR clocks per output sample
        double remaining = 1.0, acc = 0.0;
        int guard = 0;
        while (guard++ < 64)
        {
            const double toNext = (1.0 - phase) / step;
            if (toNext >= remaining) { acc += out() * remaining; phase += remaining * step; break; }
            acc += out() * toNext; remaining -= toNext; phase = 0.0; clock (short7);
        }
        if (phase >= 1.0) phase -= std::floor (phase);
        return acc;
    }
};

// ---- Simple RBJ biquad -----------------------------------------------------
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void reset() { z1 = z2 = 0; }
    void setLP (double sr, double f, double q) { set (sr, f, q, 0); }
    void setHP (double sr, double f, double q) { set (sr, f, q, 1); }
    void setPeak (double sr, double f, double q, double dB)
    {
        const double A = std::pow (10.0, dB / 40.0), w = 2 * kPi * f / sr;
        const double al = std::sin (w) / (2 * q), c = std::cos (w);
        const double a0 = 1 + al / A;
        b0 = (1 + al * A) / a0; b1 = -2 * c / a0; b2 = (1 - al * A) / a0;
        a1 = -2 * c / a0; a2 = (1 - al / A) / a0;
    }
    void set (double sr, double f, double q, int type)
    {
        f = std::min (f, sr * 0.45);
        const double w = 2 * kPi * f / sr, al = std::sin (w) / (2 * q), c = std::cos (w);
        const double a0 = 1 + al;
        if (type == 0) { b0 = (1 - c) / 2 / a0; b1 = (1 - c) / a0; b2 = b0; }
        else           { b0 = (1 + c) / 2 / a0; b1 = -(1 + c) / a0; b2 = b0; }
        a1 = -2 * c / a0; a2 = (1 - al) / a0;
    }
    inline double process (double x)
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};
} // namespace gb
