#include "PluginEditor.h"
#include "PixelFont.h"
#include "BinaryData.h"

using namespace ui;

// ============================================================================
void KnobLnF::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                float start, float end, juce::Slider& s)
{
    const float r  = (float) (int) s.getProperties()["ring"];
    const auto c   = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).getCentre();
    // tick dots
    g.setColour (ink);
    for (int i = 0; i <= 10; ++i)
    {
        const float a = start + (end - start) * (float) i / 10.0f;
        const float d = r + 5.0f;
        const float sz = (i == 0 || i == 5 || i == 10) ? 3.6f : 2.8f;
        g.fillRect (juce::Rectangle<float> (sz, sz).withCentre ({ c.x + d * std::sin (a), c.y - d * std::cos (a) }));
    }
    // ring (slightly heavier at the bottom like the drawing)
    g.setColour (lcd);
    g.fillEllipse (juce::Rectangle<float> (2 * r, 2 * r).withCentre (c));
    g.setColour (ink);
    g.drawEllipse (juce::Rectangle<float> (2 * r - 5.0f, 2 * r - 5.0f).withCentre (c), 5.0f);
    g.fillRect (juce::Rectangle<float> (r * 0.9f, 3.0f).withCentre ({ c.x, c.y + r - 2.0f }));
    // pointer
    const float a = start + pos * (end - start);
    juce::Point<float> p0 (c.x + r * 0.30f * std::sin (a), c.y - r * 0.30f * std::cos (a));
    juce::Point<float> p1 (c.x + r * 0.78f * std::sin (a), c.y - r * 0.78f * std::cos (a));
    g.drawLine ({ p0, p1 }, 4.0f);
}

// ============================================================================
Segmented::Segmented (juce::RangedAudioParameter& p, Mode m, juce::StringArray l, Icons ic)
    : id (p.getParameterID()), param (p), mode (m), labels (std::move (l)), icons (ic),
      attach (p, [this] (float v) { value = v; repaint(); })
{
    attach.sendInitialUpdate();
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

static void drawDutyIcon (juce::Graphics& g, juce::Rectangle<float> r, int duty)
{
    static const float d[4] = { 0.125f, 0.25f, 0.5f, 0.75f };
    r = r.reduced (r.getWidth() * 0.16f, r.getHeight() * 0.28f);
    juce::Path p;
    const float hiW = r.getWidth() * 0.62f * d[duty] / 0.75f + 3.0f;
    const float x0 = r.getX(), xr = r.getRight(), yT = r.getY(), yB = r.getBottom();
    p.startNewSubPath (x0, yB);
    p.lineTo (x0, yT); p.lineTo (x0 + hiW, yT); p.lineTo (x0 + hiW, yB); p.lineTo (xr, yB);
    g.strokePath (p, juce::PathStrokeType (2.4f, juce::PathStrokeType::mitered, juce::PathStrokeType::square));
}

static void drawWaveIcon (juce::Graphics& g, juce::Rectangle<float> r, int shape)
{
    if (shape == 0) { pixelfont::drawCentred (g, "RAM", r, 1.3f); return; }
    uint8_t ram[32]; params::fillShape (shape, ram);
    r = r.reduced (r.getWidth() * 0.18f, r.getHeight() * 0.27f);
    juce::Path p;
    for (int i = 0; i < 32; ++i)
    {
        const float x = r.getX() + r.getWidth() * i / 31.0f;
        const float y = r.getBottom() - r.getHeight() * ram[i] / 15.0f;
        if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
    }
    g.strokePath (p, juce::PathStrokeType (2.2f, juce::PathStrokeType::mitered, juce::PathStrokeType::square));
}

void Segmented::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (ink);
    g.fillRoundedRectangle (b, 3.0f);
    const int n = mode == Mode::Choice ? labels.size() : 1;
    const float segW = b.getWidth() / (float) n;
    const int cur = (int) std::lround (value);
    const float px = b.getHeight() > 29 ? 1.75f : 1.6f;

    for (int i = 0; i < n; ++i)
    {
        auto sr = juce::Rectangle<float> (b.getX() + segW * i, b.getY(), segW, b.getHeight());
        bool sel = false;
        juce::String text;
        if (mode == Mode::Choice) { sel = (i == cur); text = labels[i]; }
        else if (mode == Mode::Toggle) { sel = value > 0.5f; text = labels[0]; }
        else { sel = false; text = labels[juce::jlimit (0, labels.size() - 1, cur)]; }

        if (sel)
        {
            g.setColour (lcd);
            g.fillRect (sr.reduced (3.0f, 3.0f));
            g.setColour (ink);
        }
        else
        {
            if (mode == Mode::Choice && i > 0 && ! (i == cur + 1))
            {
                g.setColour (lcdOff);
                g.fillRect (juce::Rectangle<float> (1.2f, b.getHeight() * 0.5f).withCentre ({ sr.getX(), sr.getCentreY() }));
            }
            g.setColour (lcdDim);
        }
        if (icons == Icons::Duty) drawDutyIcon (g, sr, i);
        else if (icons == Icons::Wave) drawWaveIcon (g, sr, i);
        else pixelfont::drawCentred (g, text, sr, px);
    }
}

void Segmented::mouseDown (const juce::MouseEvent& e)
{
    if (mode == Mode::Choice)
    {
        const int n = labels.size();
        const int i = juce::jlimit (0, n - 1, (int) (e.position.x / getWidth() * n));
        attach.setValueAsCompleteGesture ((float) i);
    }
    else if (mode == Mode::Toggle) attach.setValueAsCompleteGesture (value > 0.5f ? 0.0f : 1.0f);
    else attach.setValueAsCompleteGesture ((float) (((int) std::lround (value) + 1) % juce::jmax (1, labels.size())));
}

// ============================================================================
LedToggle::LedToggle (juce::RangedAudioParameter& p, juce::String t)
    : text (std::move (t)), attach (p, [this] (float v) { on = v > 0.5f; repaint(); })
{
    attach.sendInitialUpdate();
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void LedToggle::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    if (text.isEmpty())
    {
        g.setColour (ink);
        if (on) g.fillRect (b.reduced (1.0f));
        else g.drawRect (b.reduced (1.0f), 2.0f);
        return;
    }
    if (on)
    {
        g.setColour (ink); g.fillRect (b);
        g.setColour (lcd);
    }
    else
    {
        g.setColour (ink.withAlpha (0.18f)); g.fillRect (b);
        g.setColour (ink.withAlpha (0.35f));
    }
    pixelfont::drawCentred (g, text, b, 2.0f);
}

// ============================================================================
void Scope::paint (juce::Graphics& g)
{
    const int N = SameBoyProcessor::scopeSize;
    const int w = proc.scopeWrite.load (std::memory_order_acquire);
    auto at = [&] (int back) { return proc.scope[(size_t) ((w - back + N) & (N - 1))]; };
    // find the two most recent rising zero crossings -> period estimate
    int c1 = -1, c2 = -1;
    for (int back = 2; back < N / 2; ++back)
        if (at (back + 1) <= 0.0f && at (back) > 0.0f)
        {
            if (c1 < 0) c1 = back;
            else if (back - c1 > 8) { c2 = back; break; }
        }
    int view = 900;
    if (c1 > 0 && c2 > 0) view = juce::jlimit (240, 2400, (c2 - c1) * 3);
    int startBack = view + 1;
    for (int back = view + 1; back < view + 2400 && back < N - 2; ++back)
        if (at (back + 1) <= 0.0f && at (back) > 0.0f) { startBack = back; break; }
    const auto r = getLocalBounds().toFloat().reduced (4.0f, 6.0f);
    const int cols = 120;
    float peak = 0.0f;
    for (int k = 0; k < view; ++k) peak = juce::jmax (peak, std::abs (at (startBack - k)));
    const float gain = juce::jlimit (1.0f, 8.0f, 0.95f / juce::jmax (1.0e-4f, peak));
    juce::Path p;
    for (int c = 0; c < cols; ++c)
    {
        const int idx = (w - startBack + (c * view) / cols + N) & (N - 1);
        (void) at;
        float v = juce::jlimit (-1.0f, 1.0f, proc.scope[(size_t) idx] * gain);
        v = std::round (v * 7.0f) / 7.0f;  // LCD rows
        const float x0 = r.getX() + r.getWidth() * c / cols, x1 = r.getX() + r.getWidth() * (c + 1) / cols;
        const float y = r.getCentreY() - v * r.getHeight() * 0.5f;
        if (c == 0) p.startNewSubPath (x0, y); else p.lineTo (x0, y);
        p.lineTo (x1, y);
    }
    g.setColour (ink);
    g.strokePath (p, juce::PathStrokeType (3.2f, juce::PathStrokeType::mitered, juce::PathStrokeType::square));
}

// ============================================================================
void WaveRam::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (6.0f, 3.0f);
    const float colW = r.getWidth() / 32.0f;
    const float rowH = r.getHeight() / 16.0f;
    g.setColour (ink);
    for (int i = 0; i < 32; ++i)
    {
        const int v = proc.waveRam[(size_t) i].load();
        for (int k = 0; k <= v; ++k)
        {
            const float y = r.getBottom() - (k + 1) * rowH;
            g.fillRect (juce::Rectangle<float> (r.getX() + i * colW + 0.8f, y + 0.6f, colW - 1.6f, rowH - 1.6f));
        }
    }
}

void WaveRam::setAt (juce::Point<float> p)
{
    auto r = getLocalBounds().toFloat().reduced (6.0f, 3.0f);
    const int col = juce::jlimit (0, 31, (int) ((p.x - r.getX()) / r.getWidth() * 32.0f));
    const int val = juce::jlimit (0, 15, (int) ((r.getBottom() - p.y) / r.getHeight() * 16.0f));
    proc.waveRam[(size_t) col].store ((uint8_t) val);
}

void WaveRam::mouseDown (const juce::MouseEvent& e)
{
    if (auto* sh = proc.apvts.getParameter ("w_shape"))
        if (sh->getValue() > 0.0f) sh->setValueNotifyingHost (0.0f);   // switch to CUSTOM
    last = e.position; setAt (e.position); repaint();
}

void WaveRam::mouseDrag (const juce::MouseEvent& e)
{
    const int steps = juce::jmax (1, (int) last.getDistanceFrom (e.position) / 3);
    for (int i = 1; i <= steps; ++i) setAt (last + (e.position - last) * ((float) i / steps));
    last = e.position;
    proc.presetDirty.store (true);
    repaint();
}

// ============================================================================
void HitZone::paint (juce::Graphics& g)
{
    if (! down || ! showPress) return;
    g.setColour (juce::Colours::black.withAlpha (0.22f));
    if (round) g.fillEllipse (getLocalBounds().toFloat().reduced (2.0f));
    else g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);
}

// ============================================================================
SameBoyEditor::SameBoyEditor (SameBoyProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    plate = juce::ImageCache::getFromMemory (BinaryData::plate_png, BinaryData::plate_pngSize);
    addAndMakeVisible (canvas);
    canvas.setBounds (0, 0, layout::W, layout::H);

    for (auto& k : layout::knobs)
    {
        auto kn = std::make_unique<Knob> (k.id, k.r);
        kn->setLookAndFeel (&knobLnF);
        const int pad = k.r + 9;
        kn->setBounds (k.cx - pad, k.cy - pad, 2 * pad, 2 * pad);
        canvas.addAndMakeVisible (*kn);
        knobAttach.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, k.id, *kn));
        if (auto* prm = proc.apvts.getParameter (k.id))
            kn->setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
        knobs.push_back (std::move (kn));
    }

    using M = Segmented::Mode; using I = Segmented::Icons;
    for (auto& s : layout::segs)
    {
        auto* prm = proc.apvts.getParameter (s.id);
        jassert (prm != nullptr);
        const juce::String id (s.id);
        std::unique_ptr<Segmented> seg;
        if (id == "p1_duty" || id == "p2_duty")   seg = std::make_unique<Segmented> (*prm, M::Choice, juce::StringArray { "", "", "", "" }, I::Duty);
        else if (id == "w_shape")                  seg = std::make_unique<Segmented> (*prm, M::Choice, juce::StringArray { "", "", "", "", "", "", "", "" }, I::Wave);
        else if (id == "p1_sweep")                 seg = std::make_unique<Segmented> (*prm, M::Toggle, juce::StringArray { "ON" });
        else if (id == "p1_sweepdir")              seg = std::make_unique<Segmented> (*prm, M::Cycle, prm->getAllValueStrings());
        else                                       seg = std::make_unique<Segmented> (*prm, M::Choice, prm->getAllValueStrings());
        seg->setBounds (s.x, s.y, s.w, s.h);
        canvas.addAndMakeVisible (*seg);
        segs.push_back (std::move (seg));
    }
    for (auto& l : layout::leds)
    {
        auto led = std::make_unique<LedToggle> (*proc.apvts.getParameter (l.id));
        led->setBounds (l.x, l.y, l.w, l.h);
        canvas.addAndMakeVisible (*led);
        leds.push_back (std::move (led));
    }
    {
        const char* ids[4] = { "p1_on", "p2_on", "w_on", "n_on" };
        const char* names[4] = { "P1", "P2", "WU", "NZ" };
        auto r = layout::chToggles;
        const int w = (r.getWidth() - 3 * 4) / 4;
        for (int i = 0; i < 4; ++i)
        {
            auto t = std::make_unique<LedToggle> (*proc.apvts.getParameter (ids[i]), names[i]);
            t->setBounds (r.getX() + i * (w + 4), r.getY() + 2, w, r.getHeight() - 4);
            canvas.addAndMakeVisible (*t);
            chToggles.push_back (std::move (t));
        }
    }

    scope = std::make_unique<Scope> (proc);
    scope->setBounds (layout::scope);
    canvas.addAndMakeVisible (*scope);
    waveRam = std::make_unique<WaveRam> (proc);
    waveRam->setBounds (layout::waveRam);
    canvas.addAndMakeVisible (*waveRam);

    auto addZone = [this] (juce::Rectangle<int> r, bool round, std::function<void()> press, std::function<void()> release = {})
    {
        auto z = std::make_unique<HitZone> (round);
        z->setBounds (r); z->onPress = std::move (press); z->onRelease = std::move (release);
        canvas.addAndMakeVisible (*z);
        zones.push_back (std::move (z));
        return zones.back().get();
    };
    const int nPresets = proc.getNumPrograms();
    auto step = [this, nPresets] (int d) { proc.loadPreset ((proc.presetIndex.load() + d + nPresets) % nPresets); };
    addZone (layout::presetPrev, false, [step] { step (-1); });
    addZone (layout::presetNext, false, [step] { step (1); });
    auto* name = addZone (layout::presetName, false, [this] { showPresetMenu(); });
    name->showPress = false;
    // D-pad: left/up = previous, right/down = next
    auto* dpad = addZone (layout::case_dpad, false, {});
    dpad->onPressAt = [step, dpad] (const juce::MouseEvent& e)
    {
        const auto c = dpad->getLocalBounds().getCentre().toFloat();
        const auto d = e.position - c;
        if (std::abs (d.x) > std::abs (d.y)) step (d.x > 0 ? 1 : -1);
        else step (d.y > 0 ? 1 : -1);
    };
    addZone (layout::case_a, true, [this] { proc.auditionNote.store (60); }, [this] { proc.auditionNote.store (-1); });
    addZone (layout::case_b, true, [this] { proc.auditionNote.store (48); }, [this] { proc.auditionNote.store (-1); });
    addZone (layout::case_select, false, [this] { proc.loadPreset (0); });
    addZone (layout::case_start, false, [this] { proc.randomize(); });

    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio ((double) layout::W / layout::H);
    setResizeLimits (layout::W / 2, layout::H / 2, (int) (layout::W * 1.3), (int) (layout::H * 1.3));
    setSize ((int) (layout::W * 0.75), (int) (layout::H * 0.75));
    timerCallback();
    startTimerHz (20);
}

SameBoyEditor::~SameBoyEditor()
{
    for (auto& k : knobs) k->setLookAndFeel (nullptr);
}

void SameBoyEditor::resized()
{
    const float s = (float) getWidth() / (float) layout::W;
    canvas.setTransform (juce::AffineTransform::scale (s));
}

void SameBoyEditor::showPresetMenu()
{
    juce::PopupMenu m;
    for (int i = 0; i < proc.getNumPrograms(); ++i)
        m.addItem (i + 1, proc.getProgramName (i), true, i == proc.presetIndex.load());
    m.addSeparator();
    m.addItem (1000, "Randomize (START)");
    m.addItem (1001, "Init (SELECT)");
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (zones[2].get()),
                     [this] (int r)
                     {
                         if (r >= 1 && r <= proc.getNumPrograms()) proc.loadPreset (r - 1);
                         else if (r == 1000) proc.randomize();
                         else if (r == 1001) proc.loadPreset (0);
                     });
}

juce::String SameBoyEditor::describe (const juce::String& id)
{
    auto* p = proc.apvts.getParameter (id);
    if (p == nullptr) return {};
    const float v = p->convertFrom0to1 (p->getValue());
    static const std::map<juce::String, juce::String> names = {
        { "env_vol", "VOLUME" }, { "env_pace", "ENV PACE" }, { "env_len", "LENGTH" }, { "env_rel", "RELEASE" },
        { "glide", "GLIDE" }, { "bend", "BEND" }, { "vib_rate", "VIB RATE" }, { "vib_depth", "VIB DEPTH" },
        { "vib_delay", "VIB DELAY" }, { "p1_level", "P1 LEVEL" }, { "p1_pace", "SWEEP PACE" }, { "p1_shift", "SWEEP SHIFT" },
        { "p2_level", "P2 LEVEL" }, { "p2_detune", "DETUNE" }, { "p2_oct", "P2 OCT" }, { "w_level", "WAVE LEVEL" },
        { "w_oct", "WAVE OCT" }, { "n_level", "NOISE LEVEL" }, { "n_pitch", "NOISE PITCH" }, { "arp_speed", "ARP SPEED" },
        { "arp_oct", "ARP OCT" }, { "master", "MASTER" } };
    auto it = names.find (id);
    juce::String n = it != names.end() ? it->second : p->getName (16).toUpperCase();
    juce::String val;
    if (id.endsWith ("level") || id == "master") val = juce::String (juce::roundToInt (v * 100)) + "%";
    else if (id == "env_len") val = v >= 64 ? "OFF" : juce::String (juce::roundToInt (v / 256.0f * 1000.0f)) + "MS";
    else if (id == "env_rel" || id == "glide" || id == "vib_delay") val = juce::String (juce::roundToInt (v * 1000)) + "MS";
    else if (id == "vib_rate" || id == "arp_speed") val = juce::String (v, 1) + "HZ";
    else if (id == "vib_depth" || id == "p2_detune") val = juce::String (juce::roundToInt (v)) + "CT";
    else if (id == "bend") val = "+-" + juce::String (juce::roundToInt (v)) + "ST";
    else if (id == "n_pitch")
    {
        gb::NoiseTable t; const double hz = t.fromKnob (v);
        val = hz >= 1000 ? juce::String (hz / 1000.0, 1) + "KHZ" : juce::String (juce::roundToInt (hz)) + "HZ";
    }
    else if (id.endsWith ("oct") && id != "arp_oct") val = (v > 0 ? "+" : "") + juce::String (juce::roundToInt (v));
    else val = p->getCurrentValueAsText().toUpperCase();
    return n + " " + val;
}

void SameBoyEditor::timerCallback()
{
    juce::String t = "SB-04 DISPLAY";
    for (auto& k : knobs)
        if (k->isMouseButtonDown() || k->isMouseOver()) { t = describe (k->id); break; }
    for (auto& s : segs)
        if (s->isMouseOver()) { t = describe (s->id); break; }
    for (int c = 0; c < 4; ++c)
    {
        const float a = proc.activity[(size_t) c].load();
        act[c] = a > 0.001f ? 1.0f : act[c] * 0.6f;
    }
    const auto pn = proc.presetDisplayName();
    if (t != footerText || pn != lastPresetName || true) // footer LEDs animate
    {
        footerText = t; lastPresetName = pn;
        canvas.repaint (layout::footerLeft.expanded (2));
        canvas.repaint (layout::footerLeds.expanded (2));
        canvas.repaint (layout::presetName);
        canvas.repaint (layout::waveName);
    }
}

void SameBoyEditor::Canvas::paint (juce::Graphics& g)
{
    g.drawImage (ed.plate, getLocalBounds().toFloat());

    // preset name
    g.setColour (ink);
    pixelfont::drawCentred (g, ed.lastPresetName, layout::presetName.toFloat(), 2.2f);

    // wave shape name
    {
        const int shape = juce::roundToInt (ed.proc.apvts.getRawParameterValue ("w_shape")->load());
        const auto name = params::kShapes[shape];
        auto r = layout::waveName.toFloat();
        pixelfont::draw (g, name, r.getX() + 6, r.getCentreY() - 6.0f, 1.72f);
    }

    // footer: parameter readout + channel activity "LEDs"
    g.setColour (ink);
    g.fillRect (layout::footerLeft);
    g.setColour (lcd.withAlpha (0.85f));
    pixelfont::draw (g, ed.footerText, (float) layout::footerLeft.getX() + 6, (float) layout::footerLeft.getY() + 2.0f, 1.55f);

    g.setColour (ink);
    g.fillRect (layout::footerLeds);
    const char* names[4] = { "P1", "P2", "WU", "NZ" };
    const float xs[4] = { 522, 552, 587, 622 };
    for (int i = 0; i < 4; ++i)
    {
        g.setColour (lcdOff.interpolatedWith (lcd, ed.act[i]));
        pixelfont::draw (g, names[i], xs[i], (float) layout::footerLeds.getY() + 2.0f, 1.55f);
    }
}

void SameBoyEditor::Canvas::paintOverChildren (juce::Graphics& g)
{
    // faint LCD pixel grid to unify drawn controls with the screen texture
    const juce::Rectangle<int> lcdArea (128, 80, 1060, 668);
    g.setColour (juce::Colours::black.withAlpha (0.035f));
    for (int x = lcdArea.getX(); x < lcdArea.getRight(); x += 3) g.fillRect (x, lcdArea.getY(), 1, lcdArea.getHeight());
    for (int y = lcdArea.getY(); y < lcdArea.getBottom(); y += 3) g.fillRect (lcdArea.getX(), y, lcdArea.getWidth(), 1);
}

juce::AudioProcessorEditor* SameBoyProcessor::createEditor() { return new SameBoyEditor (*this); }
