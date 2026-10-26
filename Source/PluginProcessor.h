#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "GBApu.h"
#include "Params.h"
#include "Presets.h"

class SameBoyProcessor : public juce::AudioProcessor,
                         private juce::AudioProcessorValueTreeState::Listener
{
public:
    SameBoyProcessor();
    ~SameBoyProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    using AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Same Boy"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override { return (int) presets::all().size(); }
    int getCurrentProgram() override { return presetIndex.load(); }
    void setCurrentProgram (int index) override { loadPreset (index); }
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- shared with the editor ----
    juce::AudioProcessorValueTreeState apvts;
    std::array<std::atomic<uint8_t>, 32> waveRam;
    std::atomic<int> presetIndex { 3 };
    std::atomic<bool> presetDirty { false };
    std::atomic<bool> loadingPreset { false };
    void loadPreset (int index);
    void randomize();
    juce::String presetDisplayName() const;

    static constexpr int scopeSize = 8192;
    std::array<float, scopeSize> scope {};
    std::atomic<int> scopeWrite { 0 };
    std::array<std::atomic<float>, 4> activity;

    // on-screen A/B buttons audition a note
    std::atomic<int> auditionNote { -1 };

private:
    void parameterChanged (const juce::String& id, float value) override;

    struct Voice
    {
        bool active = false, gate = false, released = false, sustained = false;
        int note = 60; float vel = 1.0f; uint64_t age = 0;
        double curNote = 60, targetNote = 60;
        int envVol = 15; double envTimer = 0;
        int relStep = 15, volAtRelease = 15; double relTimer = 0;
        double lenTimer = 0;
        double sweepX0 = 0, sweepX = 0, sweepTimer = 0; bool sweepDead = false;
        double vibTime = 0;
        gb::PulseOsc p1, p2; gb::WaveOsc w; gb::NoiseOsc n;
    };

    struct Snapshot;    // per-block parameter snapshot
    void triggerVoice (Voice& v, int note, float vel, bool legato, const Snapshot& s);
    void releaseVoice (Voice& v, const Snapshot& s);
    void noteOn (int note, float vel, const Snapshot& s);
    void noteOff (int note, const Snapshot& s);
    void arpStep (const Snapshot& s);
    void allNotesOff();

    static constexpr int kMaxVoices = 8;
    static constexpr int kOsOrder = 2;          // 4x oversampling
    static constexpr int kOs = 1 << kOsOrder;
    std::array<Voice, kMaxVoices> voices;
    uint64_t ageCounter = 0;

    std::vector<int> held;          // notes held (in played order)
    std::vector<int> pedalHeld;
    bool sustainPedal = false;
    int arpPos = 0, arpDir = 1;
    double arpTimer = 0;
    int lastMode = 0;

    double pitchBend = 0.0;          // -1..1
    double modWheel = 0.0;           // 0..1

    double sr = 44100.0;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    gb::NoiseTable noiseTable;
    std::array<gb::Biquad, 2> hp, lp, pk;
    int lastRoute = -1;
    uint8_t ramLocal[32] {};
    int lastAudition = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SameBoyProcessor)
};
