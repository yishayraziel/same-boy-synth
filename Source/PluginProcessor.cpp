#include "PluginProcessor.h"
#include "PluginEditor.h"

struct SameBoyProcessor::Snapshot
{
    int envVol, envPace, envLen, envDir; double envRel;
    double glide; int bend; double vibRate, vibDepth, vibDelay;
    bool p1On; double p1Level; int p1Pan, p1Duty, p1Pace, p1Shift, p1SweepDir; bool p1Sweep;
    bool p2On; double p2Level; int p2Pan, p2Duty; double p2Detune; int p2Oct;
    bool wOn; double wLevel; int wPan, wOct, wVol;
    bool nOn; double nLevel; int nPan; bool nShort; double nPitch; bool nKey;
    int mode; double arpSpeed; int arpPattern, arpOct;
    int route; bool quant; double master;
};

static float rawf (juce::AudioProcessorValueTreeState& a, const char* id) { return a.getRawParameterValue (id)->load(); }
static int   rawi (juce::AudioProcessorValueTreeState& a, const char* id) { return (int) std::lround (rawf (a, id)); }

SameBoyProcessor::SameBoyProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "SameBoy", params::createLayout())
{
    for (auto& a : activity) a.store (0.0f);
    uint8_t tmp[32]; params::fillShape (1, tmp);
    for (int i = 0; i < 32; ++i) waveRam[(size_t) i].store (tmp[i]);
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            apvts.addParameterListener (rp->getParameterID(), this);
    loadPreset (3);
}

SameBoyProcessor::~SameBoyProcessor()
{
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            apvts.removeParameterListener (rp->getParameterID(), this);
}

void SameBoyProcessor::parameterChanged (const juce::String& id, float value)
{
    if (! loadingPreset.load()) presetDirty.store (true);
    if (id == "w_shape")
    {
        const int shape = (int) std::lround (value);
        if (shape > 0)
        {
            uint8_t tmp[32]; params::fillShape (shape, tmp);
            for (int i = 0; i < 32; ++i) waveRam[(size_t) i].store (tmp[i]);
        }
    }
}

bool SameBoyProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void SameBoyProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
        2, kOsOrder, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
    oversampler->initProcessing ((size_t) juce::jmax (1, samplesPerBlock));
    setLatencySamples ((int) std::ceil (oversampler->getLatencyInSamples()));
    for (auto& v : voices) v = Voice();
    held.clear(); pedalHeld.clear();
    lastRoute = -1;
}

const juce::String SameBoyProcessor::getProgramName (int index)
{
    const auto& l = presets::all();
    if (index < 0 || index >= (int) l.size()) return {};
    return juce::String (index + 1).paddedLeft ('0', 2) + " " + l[(size_t) index].name;
}

juce::String SameBoyProcessor::presetDisplayName() const
{
    auto name = const_cast<SameBoyProcessor*> (this)->getProgramName (presetIndex.load());
    return presetDirty.load() ? name + "*" : name;
}

void SameBoyProcessor::loadPreset (int index)
{
    const auto& l = presets::all();
    index = juce::jlimit (0, (int) l.size() - 1, index);
    const auto& pr = l[(size_t) index];
    loadingPreset.store (true);
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            rp->setValueNotifyingHost (rp->getDefaultValue());
    for (auto& [id, val] : pr.values)
        if (auto* rp = apvts.getParameter (id))
            rp->setValueNotifyingHost (rp->convertTo0to1 (val));
    // wave RAM: shape listener has filled it; custom waves come from the preset
    const int shape = rawi (apvts, "w_shape");
    if (shape == 0 && pr.customWave != nullptr)
        for (int i = 0; i < 32; ++i)
            waveRam[(size_t) i].store ((uint8_t) juce::CharacterFunctions::getHexDigitValue ((juce::juce_wchar) pr.customWave[i]));
    else
    {
        uint8_t tmp[32]; params::fillShape (shape == 0 ? 1 : shape, tmp);
        for (int i = 0; i < 32; ++i) waveRam[(size_t) i].store (tmp[i]);
    }
    presetIndex.store (index);
    presetDirty.store (false);
    loadingPreset.store (false);
}

void SameBoyProcessor::randomize()
{
    juce::Random r;
    auto set = [&] (const char* id, float v) { if (auto* p = apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (v)); };
    loadPreset (0);
    loadingPreset.store (true);
    set ("p1_duty", (float) r.nextInt (4));
    set ("p2_on", (float) r.nextBool()); set ("p2_duty", (float) r.nextInt (4));
    set ("p2_detune", (float) r.nextInt (25) - 12); set ("p2_oct", (float) r.nextInt (3) - 1);
    set ("p2_pan", (float) r.nextInt (3)); set ("p1_pan", (float) r.nextInt (3));
    const bool wave = r.nextInt (3) == 0;
    set ("w_on", wave); set ("w_shape", (float) (1 + r.nextInt (7))); set ("w_oct", (float) (r.nextInt (2) - 1));
    set ("n_on", r.nextInt (5) == 0 ? 1.0f : 0.0f); set ("n_pitch", 0.4f + 0.6f * r.nextFloat()); set ("n_level", 0.3f);
    set ("env_vol", (float) (9 + r.nextInt (7))); set ("env_pace", (float) (r.nextBool() ? 0 : r.nextInt (6)));
    set ("env_rel", r.nextFloat() * 0.6f);
    set ("vib_depth", r.nextBool() ? 0.0f : r.nextFloat() * 30.0f);
    set ("p1_sweep", r.nextInt (6) == 0 ? 1.0f : 0.0f); set ("p1_pace", (float) (1 + r.nextInt (4)));
    set ("p1_shift", (float) (2 + r.nextInt (5))); set ("p1_sweepdir", (float) r.nextInt (2));
    set ("v_mode", (float) r.nextInt (3)); set ("arp_pattern", (float) r.nextInt (4));
    set ("arp_speed", 6.0f + r.nextFloat() * 20.0f); set ("arp_oct", (float) (1 + r.nextInt (2)));
    loadingPreset.store (false);
    presetDirty.store (true);
}

// ---------------------------------------------------------------------------
void SameBoyProcessor::triggerVoice (Voice& v, int note, float vel, bool legato, const Snapshot& s)
{
    const bool wasActive = v.active;
    v.active = true; v.gate = true; v.released = false; v.sustained = false;
    v.note = note; v.vel = vel; v.age = ++ageCounter;
    v.targetNote = note;
    if (! (legato && wasActive && s.glide > 0.0005)) v.curNote = note;
    if (legato && wasActive) return;    // legato: keep envelope running

    v.envVol = juce::jlimit (0, 15, (int) std::lround (s.envVol * (0.6 + 0.4 * vel)));
    v.envTimer = 0; v.relStep = 15; v.relTimer = 0; v.lenTimer = 0; v.vibTime = 0;
    v.sweepX0 = gb::xFromPulseFreq (gb::noteToHz (note));
    if (s.quant) v.sweepX0 = std::round (juce::jlimit (0.0, 2047.0, v.sweepX0));
    v.sweepX = v.sweepX0; v.sweepTimer = 0; v.sweepDead = false;
    if (! wasActive) { v.p1.phase = v.p2.phase = v.w.phase = 0.0; }
    v.n.reset();
}

void SameBoyProcessor::releaseVoice (Voice& v, const Snapshot& s)
{
    if (! v.active || v.released) return;
    v.gate = false; v.released = true;
    v.relStep = 15; v.relTimer = 0; v.volAtRelease = v.envVol;
    if (s.envRel <= 0.0005) v.active = false;
}

void SameBoyProcessor::noteOn (int note, float vel, const Snapshot& s)
{
    held.erase (std::remove (held.begin(), held.end(), note), held.end());
    held.push_back (note);
    pedalHeld.erase (std::remove (pedalHeld.begin(), pedalHeld.end(), note), pedalHeld.end());

    if (s.mode == 0) // POLY
    {
        Voice* target = nullptr;
        for (auto& v : voices) if (v.active && v.note == note) { target = &v; break; }
        if (! target) for (auto& v : voices) if (! v.active) { target = &v; break; }
        if (! target)
        {   // steal: oldest released first, then oldest
            for (auto& v : voices) if (v.released && (! target || v.age < target->age)) target = &v;
            if (! target) for (auto& v : voices) if (! target || v.age < target->age) target = &v;
        }
        triggerVoice (*target, note, vel, false, s);
    }
    else if (s.mode == 1) // MONO legato
    {
        auto& v = voices[0];
        triggerVoice (v, note, vel, v.active && v.gate, s);
    }
    else // ARP: (re)start on first note
    {
        if (held.size() == 1) { arpPos = 0; arpDir = 1; arpTimer = 0; voices[0].vel = vel; arpStep (s); }
    }
}

void SameBoyProcessor::noteOff (int note, const Snapshot& s)
{
    if (sustainPedal)
    {
        if (std::find (pedalHeld.begin(), pedalHeld.end(), note) == pedalHeld.end()) pedalHeld.push_back (note);
        return;
    }
    held.erase (std::remove (held.begin(), held.end(), note), held.end());

    if (s.mode == 0)
    {
        for (auto& v : voices) if (v.active && v.gate && v.note == note) releaseVoice (v, s);
    }
    else if (s.mode == 1)
    {
        auto& v = voices[0];
        if (! held.empty()) { if (v.note != held.back()) triggerVoice (v, held.back(), v.vel, true, s); }
        else releaseVoice (v, s);
    }
    else
    {
        if (held.empty()) releaseVoice (voices[0], s);
    }
}

void SameBoyProcessor::arpStep (const Snapshot& s)
{
    if (held.empty()) return;
    std::vector<int> base = held;
    if (s.arpPattern != 3) std::sort (base.begin(), base.end());
    std::vector<int> seq;
    for (int o = 0; o < s.arpOct; ++o) for (int n : base) seq.push_back (n + 12 * o);
    const int n = (int) seq.size();
    int idx = 0;
    switch (s.arpPattern)
    {
        case 1: idx = n - 1 - (arpPos % n); ++arpPos; break;
        case 2:
        {
            if (n == 1) { idx = 0; break; }
            const int period = 2 * n - 2, p = arpPos % period;
            idx = p < n ? p : period - p; ++arpPos; break;
        }
        default: idx = arpPos % n; ++arpPos; break;
    }
    triggerVoice (voices[0], seq[(size_t) idx], voices[0].vel, false, s);
}

void SameBoyProcessor::allNotesOff()
{
    for (auto& v : voices) v = Voice();
    held.clear(); pedalHeld.clear();
}

// ---------------------------------------------------------------------------
void SameBoyProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    buffer.clear();
    if (! oversampler || numSamples == 0) return;

    auto& a = apvts;
    Snapshot s;
    s.envVol = rawi (a, "env_vol"); s.envPace = rawi (a, "env_pace"); s.envLen = rawi (a, "env_len");
    s.envDir = rawi (a, "env_dir"); s.envRel = rawf (a, "env_rel");
    s.glide = rawf (a, "glide"); s.bend = rawi (a, "bend");
    s.vibRate = rawf (a, "vib_rate"); s.vibDepth = rawf (a, "vib_depth"); s.vibDelay = rawf (a, "vib_delay");
    s.p1On = rawf (a, "p1_on") > 0.5f; s.p1Level = rawf (a, "p1_level"); s.p1Pan = rawi (a, "p1_pan");
    s.p1Duty = rawi (a, "p1_duty"); s.p1Pace = rawi (a, "p1_pace"); s.p1Shift = rawi (a, "p1_shift");
    s.p1SweepDir = rawi (a, "p1_sweepdir"); s.p1Sweep = rawf (a, "p1_sweep") > 0.5f;
    s.p2On = rawf (a, "p2_on") > 0.5f; s.p2Level = rawf (a, "p2_level"); s.p2Pan = rawi (a, "p2_pan");
    s.p2Duty = rawi (a, "p2_duty"); s.p2Detune = rawf (a, "p2_detune"); s.p2Oct = rawi (a, "p2_oct");
    s.wOn = rawf (a, "w_on") > 0.5f; s.wLevel = rawf (a, "w_level"); s.wPan = rawi (a, "w_pan");
    s.wOct = rawi (a, "w_oct"); s.wVol = rawi (a, "w_vol");
    s.nOn = rawf (a, "n_on") > 0.5f; s.nLevel = rawf (a, "n_level"); s.nPan = rawi (a, "n_pan");
    s.nShort = rawi (a, "n_lfsr") == 1; s.nPitch = rawf (a, "n_pitch"); s.nKey = rawi (a, "n_track") == 1;
    s.mode = rawi (a, "v_mode"); s.arpSpeed = rawf (a, "arp_speed");
    s.arpPattern = rawi (a, "arp_pattern"); s.arpOct = rawi (a, "arp_oct");
    s.route = rawi (a, "out_route"); s.quant = rawi (a, "out_pitch") == 1; s.master = rawf (a, "master");

    if (s.mode != lastMode) { allNotesOff(); lastMode = s.mode; }
    for (int i = 0; i < 32; ++i) ramLocal[i] = waveRam[(size_t) i].load (std::memory_order_relaxed);

    // output filters per routing
    if (s.route != lastRoute)
    {
        for (int c = 0; c < 2; ++c)
        {
            hp[(size_t) c].reset(); lp[(size_t) c].reset(); pk[(size_t) c].reset();
            if (s.route == 0)      { hp[(size_t) c].setHP (sr, 320, 0.7); lp[(size_t) c].setLP (sr, 4800, 0.9); pk[(size_t) c].setPeak (sr, 2300, 1.2, 5.0); }
            else if (s.route == 1) { hp[(size_t) c].setHP (sr, 22, 0.5);  lp[(size_t) c].setLP (sr, 15000, 0.6); pk[(size_t) c].setPeak (sr, 1000, 1.0, 0.0); }
            else                   { hp[(size_t) c].setHP (sr, 6, 0.5);   lp[(size_t) c].setLP (sr, 22000, 0.5); pk[(size_t) c].setPeak (sr, 1000, 1.0, 0.0); }
        }
        lastRoute = s.route;
    }

    // UI audition (A / B buttons)
    {
        const int an = auditionNote.load();
        if (an != lastAudition)
        {
            if (lastAudition >= 0) midi.addEvent (juce::MidiMessage::noteOff (1, lastAudition), 0);
            if (an >= 0) midi.addEvent (juce::MidiMessage::noteOn (1, an, (juce::uint8) 110), 0);
            lastAudition = an;
        }
    }

    juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), 2, (size_t) numSamples);
    if (buffer.getNumChannels() < 2)
        block = juce::dsp::AudioBlock<float> (buffer.getArrayOfWritePointers(), 1, (size_t) numSamples);
    juce::AudioBuffer<float> work (2, numSamples);
    work.clear();
    juce::dsp::AudioBlock<float> wblock (work);
    auto up = oversampler->processSamplesUp (wblock);
    float* upL = up.getChannelPointer (0);
    float* upR = up.getChannelPointer (1);

    const double dt = 1.0 / sr, osSr = sr * kOs;
    const double bendSemis = pitchBend * s.bend;
    const double vibDepthCents = s.vibDepth + modWheel * 60.0;
    const double envPeriod = s.envPace / 64.0;
    const double sweepPeriod = s.p1Pace / 128.0;
    const double lenTime = s.envLen >= 64 ? -1.0 : s.envLen / 256.0;
    const double relPeriod = s.envRel / 15.0;
    const double glideCoef = s.glide > 0.0005 ? 1.0 - std::exp (-dt / (s.glide / 3.0)) : 1.0;
    const double noiseBase = noiseTable.fromKnob (s.nPitch);
    static constexpr double panL[3] = { 1, 1, 0 }, panR[3] = { 0, 1, 1 };
    static constexpr double waveShift[3] = { 1.0, 0.5, 0.25 };
    float act[4] = { 0, 0, 0, 0 };

    auto midiIt = midi.cbegin();
    for (int i = 0; i < numSamples; ++i)
    {
        for (; midiIt != midi.cend() && (*midiIt).samplePosition <= i; ++midiIt)
        {
            const auto m = (*midiIt).getMessage();
            if (m.isNoteOn()) noteOn (m.getNoteNumber(), m.getFloatVelocity(), s);
            else if (m.isNoteOff()) noteOff (m.getNoteNumber(), s);
            else if (m.isPitchWheel()) pitchBend = (m.getPitchWheelValue() - 8192) / 8192.0;
            else if (m.isController())
            {
                if (m.getControllerNumber() == 1) modWheel = m.getControllerValue() / 127.0;
                else if (m.getControllerNumber() == 64)
                {
                    sustainPedal = m.getControllerValue() >= 64;
                    if (! sustainPedal) { auto ph = pedalHeld; pedalHeld.clear(); for (int n : ph) noteOff (n, s); }
                }
                else if (m.getControllerNumber() == 123 || m.getControllerNumber() == 120) allNotesOff();
            }
            else if (m.isAllNotesOff() || m.isAllSoundOff()) allNotesOff();
        }

        if (s.mode == 2 && ! held.empty())
        {
            arpTimer += dt * s.arpSpeed;
            if (arpTimer >= 1.0) { arpTimer -= 1.0; arpStep (s); }
        }

        for (auto& v : voices)
        {
            if (! v.active) continue;

            // ---- control rate (per host sample) ----
            v.curNote += (v.targetNote - v.curNote) * glideCoef;
            if (envPeriod > 0)
            {
                v.envTimer += dt;
                while (v.envTimer >= envPeriod)
                {
                    v.envTimer -= envPeriod;
                    v.envVol = juce::jlimit (0, 15, v.envVol + (s.envDir == 1 ? 1 : -1));
                }
            }
            if (lenTime > 0)
            {
                v.lenTimer += dt;
                if (v.lenTimer >= lenTime) { v.active = false; continue; }
            }
            if (v.released)
            {
                v.relTimer += dt;
                while (relPeriod > 0 && v.relTimer >= relPeriod && v.relStep > 0) { v.relTimer -= relPeriod; --v.relStep; }
                if (v.relStep <= 0) { v.active = false; continue; }
            }
            double sweepRatio = 1.0;
            if (s.p1Sweep && s.p1Pace > 0 && s.p1Shift > 0 && ! v.sweepDead)
            {
                v.sweepTimer += dt;
                while (v.sweepTimer >= sweepPeriod)
                {
                    v.sweepTimer -= sweepPeriod;
                    double delta = s.quant ? (double) (((int) v.sweepX) >> s.p1Shift) : v.sweepX / (double) (1 << s.p1Shift);
                    double nx = v.sweepX + (s.p1SweepDir == 1 ? delta : -delta);
                    if (nx > 2047.0) { v.sweepDead = true; break; }
                    v.sweepX = nx;
                }
                if (! v.sweepDead) sweepRatio = gb::pulseFreqFromX (v.sweepX) / gb::pulseFreqFromX (v.sweepX0);
            }
            v.vibTime += dt;
            double vib = 0.0;
            if (vibDepthCents > 0.0 && v.vibTime > s.vibDelay)
            {
                const double tt = v.vibTime - s.vibDelay;
                const double fade = juce::jmin (1.0, tt / 0.15);
                double ph = std::fmod (tt * s.vibRate, 1.0);
                const double tri = ph < 0.25 ? ph * 4 : ph < 0.75 ? 2 - ph * 4 : ph * 4 - 4;
                vib = tri * vibDepthCents / 100.0 * fade;
            }
            const double baseNote = v.curNote + bendSemis + vib;

            double f1 = gb::noteToHz (baseNote) * sweepRatio;
            double f2 = gb::noteToHz (baseNote + 12.0 * s.p2Oct + s.p2Detune / 100.0);
            double fw = gb::noteToHz (baseNote + 12.0 * s.wOct);
            double fn = s.nKey ? noiseBase * std::pow (2.0, (baseNote - 60.0) / 12.0) : noiseBase;
            if (s.quant)
            {
                f1 = gb::quantizePulse (f1); f2 = gb::quantizePulse (f2); fw = gb::quantizeWave (fw);
                if (s.nKey) fn = noiseTable.nearest (fn);
            }
            fn = juce::jlimit (2.0, 524288.0, fn);

            // effective 4-bit volumes
            int vol = v.envVol;
            double waveGate = 1.0;
            if (v.released)
            {
                vol = juce::jmin (vol, (int) std::ceil (v.volAtRelease * v.relStep / 15.0));
                const double fr = v.relStep / 15.0;
                waveGate = fr > 0.75 ? 1.0 : fr > 0.375 ? 0.5 : fr > 0.125 ? 0.25 : 0.0;
            }
            const double amp = vol / 15.0;
            const double a1 = (s.p1On && ! v.sweepDead) ? amp * s.p1Level : 0.0;
            const double a2 = s.p2On ? amp * s.p2Level : 0.0;
            const double aw = s.wOn ? waveShift[s.wVol] * waveGate * s.wLevel : 0.0;
            const double an = s.nOn ? amp * s.nLevel : 0.0;
            act[0] = juce::jmax (act[0], (float) a1); act[1] = juce::jmax (act[1], (float) a2);
            act[2] = juce::jmax (act[2], (float) aw); act[3] = juce::jmax (act[3], (float) an);

            // ---- audio rate (oversampled) ----
            for (int k = 0; k < kOs; ++k)
            {
                double l = 0, r = 0;
                if (a1 > 0) { const double y = v.p1.next (f1, osSr, s.p1Duty) * a1; l += y * panL[s.p1Pan]; r += y * panR[s.p1Pan]; }
                else v.p1.next (f1, osSr, s.p1Duty);
                if (a2 > 0) { const double y = v.p2.next (f2, osSr, s.p2Duty) * a2; l += y * panL[s.p2Pan]; r += y * panR[s.p2Pan]; }
                else v.p2.next (f2, osSr, s.p2Duty);
                if (aw > 0) { const double y = v.w.next (fw, osSr, ramLocal) * aw; l += y * panL[s.wPan]; r += y * panR[s.wPan]; }
                if (an > 0) { const double y = v.n.next (fn, osSr, s.nShort) * an; l += y * panL[s.nPan]; r += y * panR[s.nPan]; }
                upL[i * kOs + k] += (float) (l * 0.25);
                upR[i * kOs + k] += (float) (r * 0.25);
            }
        }
    }

    oversampler->processSamplesDown (wblock);

    // ---- output stage ----
    const float gain = (float) (2.4 * s.master * s.master);
    auto* L = work.getWritePointer (0);
    auto* R = work.getWritePointer (1);
    auto clip = [] (double x)
    {
        const double ax = std::abs (x);
        if (ax < 0.8) return x;
        return std::copysign (0.8 + 0.2 * std::tanh ((ax - 0.8) / 0.2), x);
    };
    int wpos = scopeWrite.load (std::memory_order_relaxed);
    for (int i = 0; i < numSamples; ++i)
    {
        double l = L[i], r = R[i];
        if (s.route == 0) { l = r = 0.5 * (l + r) * 1.2; }
        l = lp[0].process (pk[0].process (hp[0].process (l)));
        r = lp[1].process (pk[1].process (hp[1].process (r)));
        l = clip (l * gain); r = clip (r * gain);
        L[i] = (float) l; R[i] = (float) r;
        scope[(size_t) wpos] = (float) (0.5 * (l + r));
        wpos = (wpos + 1) & (scopeSize - 1);
    }
    scopeWrite.store (wpos, std::memory_order_release);
    for (int c = 0; c < 4; ++c) activity[(size_t) c].store (act[c]);

    if (buffer.getNumChannels() >= 2)
    {
        buffer.copyFrom (0, 0, work, 0, 0, numSamples);
        buffer.copyFrom (1, 0, work, 1, 0, numSamples);
    }
    else
    {
        buffer.copyFrom (0, 0, work, 0, 0, numSamples);
        buffer.addFrom (0, 0, work, 1, 0, numSamples);
        buffer.applyGain (0.5f);
    }
    midi.clear();
}

// ---------------------------------------------------------------------------
void SameBoyProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    juce::String hex;
    for (int i = 0; i < 32; ++i) hex << juce::String::toHexString ((int) waveRam[(size_t) i].load()).toUpperCase();
    state.setProperty ("wave", hex, nullptr);
    state.setProperty ("preset", presetIndex.load(), nullptr);
    state.setProperty ("dirty", presetDirty.load(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void SameBoyProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        auto tree = juce::ValueTree::fromXml (*xml);
        if (! tree.hasType (apvts.state.getType())) return;
        loadingPreset.store (true);
        apvts.replaceState (tree);
        loadingPreset.store (false);
        const auto hex = tree.getProperty ("wave").toString();
        if (hex.length() == 32)
            for (int i = 0; i < 32; ++i)
                waveRam[(size_t) i].store ((uint8_t) juce::CharacterFunctions::getHexDigitValue (hex[i]));
        presetIndex.store ((int) tree.getProperty ("preset", 3));
        presetDirty.store ((bool) tree.getProperty ("dirty", false));
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SameBoyProcessor(); }
