// Offline test: renders presets to WAV and the editor to PNG.
#include <juce_audio_utils/juce_audio_utils.h>
#include "../Source/PluginProcessor.h"

static void renderPreset (SameBoyProcessor& p, int preset, const juce::File& out, std::vector<int> notes, double sr = 48000)
{
    p.loadPreset (preset);
    p.prepareToPlay (sr, 512);
    const int total = (int) (sr * 4.0);
    juce::AudioBuffer<float> all (2, total);
    juce::AudioBuffer<float> buf (2, 512);
    for (int pos = 0; pos < total; pos += 512)
    {
        const int n = juce::jmin (512, total - pos);
        buf.setSize (2, n, false, false, true);
        juce::MidiBuffer m;
        for (size_t i = 0; i < notes.size(); ++i)
        {
            const int on = (int) (sr * 0.5 * i), off = on + (int) (sr * 0.4);
            if (on >= pos && on < pos + n) m.addEvent (juce::MidiMessage::noteOn (1, notes[i], (juce::uint8) 120), on - pos);
            if (off >= pos && off < pos + n) m.addEvent (juce::MidiMessage::noteOff (1, notes[i]), off - pos);
        }
        p.processBlock (buf, m);
        for (int c = 0; c < 2; ++c) all.copyFrom (c, pos, buf, c, 0, n);
    }
    float peak = all.getMagnitude (0, total);
    std::cout << out.getFileName() << " peak=" << peak << " rms=" << all.getRMSLevel (0, 0, total) << std::endl;
    out.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (out), sr, 2, 24, {}, 0));
    w->writeFromAudioSampleBuffer (all, 0, total);
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    juce::File dir (argc > 1 ? argv[1] : "/tmp");
    SameBoyProcessor p;
    std::vector<int> melody { 60, 64, 67, 72, 67, 64, 60, 48 };
    for (int i = 0; i < p.getNumPrograms(); ++i)
        renderPreset (p, i, dir.getChildFile ("preset_" + juce::String (i + 1).paddedLeft ('0', 2) + ".wav"),
                      i == 9 || i == 4 ? std::vector<int> { 60 } : melody);
    // chord in arp / poly
    p.loadPreset (3);
    // state round trip
    juce::MemoryBlock mb; p.getStateInformation (mb);
    SameBoyProcessor q; q.setStateInformation (mb.getData(), (int) mb.getSize());
    std::cout << "state roundtrip preset=" << q.presetIndex.load() << " dirty=" << q.presetDirty.load() << std::endl;

    // editor snapshot
    p.loadPreset (3);
    p.prepareToPlay (48000, 512);
    {   // feed some audio so scope has content
        juce::AudioBuffer<float> buf (2, 512); juce::MidiBuffer m;
        m.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 120), 0);
        for (int k = 0; k < 20; ++k) { p.processBlock (buf, m); m.clear(); }
    }
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    ed->setSize (1316, 905);
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
    juce::PNGImageFormat png; juce::FileOutputStream os (dir.getChildFile ("ui.png"));
    os.setPosition (0); os.truncate(); png.writeImageToStream (img, os);
    std::cout << "ui snapshot written" << std::endl;
    ed.reset();
    return 0;
}
