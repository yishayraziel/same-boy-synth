#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "Layout.h"

namespace ui
{
const juce::Colour ink     { 37, 46, 28 };
const juce::Colour lcd     { 178, 192, 92 };
const juce::Colour lcdDim  { 146, 160, 74 };
const juce::Colour lcdOff  { 88, 100, 52 };

class KnobLnF : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float start, float end, juce::Slider&) override;
};

class Knob : public juce::Slider
{
public:
    Knob (juce::String paramId, int ringRadius) : id (std::move (paramId)), ring (ringRadius)
    {
        setSliderStyle (RotaryVerticalDrag);
        setTextBoxStyle (NoTextBox, false, 0, 0);
        setRotaryParameters (juce::degreesToRadians (-135.0f) + juce::MathConstants<float>::twoPi,
                             juce::degreesToRadians (135.0f) + juce::MathConstants<float>::twoPi, true);
        setMouseDragSensitivity (220);
        getProperties().set ("ring", ringRadius);
    }
    juce::String id; int ring;
};

class Segmented : public juce::Component
{
public:
    enum class Mode { Choice, Toggle, Cycle };
    enum class Icons { None, Duty, Wave };
    Segmented (juce::RangedAudioParameter& p, Mode m, juce::StringArray labels, Icons ic = Icons::None);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    juce::String id;
    juce::RangedAudioParameter& param;
private:
    Mode mode; juce::StringArray labels; Icons icons;
    float value = 0;
    juce::ParameterAttachment attach;
};

class LedToggle : public juce::Component
{
public:
    LedToggle (juce::RangedAudioParameter& p, juce::String text = {});
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { attach.setValueAsCompleteGesture (on ? 0.0f : 1.0f); }
    float lit = 0;   // activity
private:
    juce::String text; bool on = false;
    juce::ParameterAttachment attach;
};

class Scope : public juce::Component, private juce::Timer
{
public:
    explicit Scope (SameBoyProcessor& p) : proc (p) { startTimerHz (30); setInterceptsMouseClicks (false, false); }
    void paint (juce::Graphics&) override;
private:
    void timerCallback() override { repaint(); }
    SameBoyProcessor& proc;
};

class WaveRam : public juce::Component, private juce::Timer
{
public:
    explicit WaveRam (SameBoyProcessor& p) : proc (p) { startTimerHz (20); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
private:
    void timerCallback() override { repaint(); }
    void setAt (juce::Point<float>);
    SameBoyProcessor& proc;
    juce::Point<float> last;
};

class HitZone : public juce::Component
{
public:
    explicit HitZone (bool round_) : round (round_) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    std::function<void()> onPress, onRelease;
    std::function<void (const juce::MouseEvent&)> onPressAt;
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override { down = true; repaint(); if (onPressAt) onPressAt (e); if (onPress) onPress(); }
    void mouseUp (const juce::MouseEvent&) override { down = false; repaint(); if (onRelease) onRelease(); }
    bool round; bool down = false; bool showPress = true;
};
} // namespace ui

class SameBoyEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SameBoyEditor (SameBoyProcessor&);
    ~SameBoyEditor() override;
    void paint (juce::Graphics&) override {}
    void resized() override;

private:
    class Canvas : public juce::Component
    {
    public:
        explicit Canvas (SameBoyEditor& e) : ed (e) {}
        void paint (juce::Graphics&) override;
        void paintOverChildren (juce::Graphics&) override;
        SameBoyEditor& ed;
    };
    void timerCallback() override;
    void showPresetMenu();
    juce::String describe (const juce::String& id);

    SameBoyProcessor& proc;
    ui::KnobLnF knobLnF;
    Canvas canvas { *this };
    juce::Image plate;
    std::vector<std::unique_ptr<ui::Knob>> knobs;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> knobAttach;
    std::vector<std::unique_ptr<ui::Segmented>> segs;
    std::vector<std::unique_ptr<ui::LedToggle>> leds, chToggles;
    std::unique_ptr<ui::Scope> scope;
    std::unique_ptr<ui::WaveRam> waveRam;
    std::vector<std::unique_ptr<ui::HitZone>> zones;
    juce::String footerText, lastPresetName;
    float act[4] {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SameBoyEditor)
};
