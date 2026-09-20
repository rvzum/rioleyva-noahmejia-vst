#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../MIDI/MidiInputHandler.h"
#include "../Audio/SamplerEngine.h"
#include "../Audio/EffectsChain.h"
#include "../Composition/ComposedMelody.h"
#include "../Library/SampleLibraryManager.h"

namespace w27
{

/**
    RIO LEYVA x NOAH MEJIA VST -- audio processor.

    Originally a pure "MIDI Visualizer" that produced NO sound at all (see
    the Phase 1-2 history further down and in the root README.md) -- it now
    also plays back a single selectable oneshot sample (Library/
    SampleLibraryManager lists what's available; Audio/SamplerEngine plays
    it, pitch-shifted per note, via JUCE's own SamplerSound/SamplerVoice),
    so the plugin can actually be used as a real melodic instrument, not
    just a monitor.

    Two independent things feed that sample engine, merged into one MIDI
    buffer once per block in processBlock():
    - Live incoming host MIDI (a MIDI keyboard, a Channel Rack pattern) --
      passed straight through to the sampler, so playing it live is now
      audible, in addition to still being visualized as before.
    - The user's own hand-drawn melody (Composition/ComposedMelody, edited
      directly on PianoRollView once the toolbar's "Write Melody" toggle is
      on -- see Source/Composition/README.md), turned into sample-accurate
      note-on events by Composition/MelodySequencer, looping in sync with
      the host's own transport position exactly the way PianoRollView's
      PATTERN mode already keeps a dropped MIDI file in sync (see
      Source/Visualization/README.md) -- same host-position-driven,
      stateless-fmod-wrap philosophy, applied to actual audio instead of
      just the on-screen display.

    MIDI capture into NoteStorage (for the visualization layer) is
    unaffected by any of this -- see MidiInputHandler. The plugin is still
    declared IS_SYNTH TRUE with a stereo output bus for the same FL Studio
    routing reason as before (see README.md's "FL Studio MIDI routing"
    section) -- that bus just isn't silent any more.
*/
class W27PluginProcessor : public juce::AudioProcessor
{
public:
    W27PluginProcessor();
    ~W27PluginProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Read-only access for the editor/visualization layer. Nothing outside
    // MIDI/ should ever reach into MidiInputHandler's internals -- everyone
    // else reads NoteEvent data via NoteStorage.
    MidiInputHandler& getMidiInputHandler() noexcept { return midiInputHandler; }
    const MidiInputHandler& getMidiInputHandler() const noexcept { return midiInputHandler; }

    // The user's hand-drawn melody -- edited directly by PianoRollView's
    // mouse handling (see Source/Composition/README.md), read every block
    // by Composition::MelodySequencer inside processBlock().
    ComposedMelody& getComposedMelody() noexcept { return composedMelody; }

    // List of oneshot libraries/samples read live from disk (see
    // Library/SampleLibraryManager::getRuntimeLibraryRoot(), currently
    // ~/Music/27wav rioleyva & noahmejia banks/) -- read by PluginEditor's
    // top-centre library button to build its popup menu. The non-const
    // overload lets the editor call rescan() right before building that
    // menu, so newly dropped files show up with no reload/rebuild needed.
    const SampleLibraryManager& getSampleLibraryManager() const noexcept { return sampleLibraryManager; }
    SampleLibraryManager& getSampleLibraryManager() noexcept { return sampleLibraryManager; }

    // Loads the given sample (read from disk -- see
    // Library/SampleLibraryManager) into the sampler engine and remembers
    // it (by its stable relativePath, not its flat index -- see
    // getStateInformation/setStateInformation; files being added/removed
    // on disk shifts indices between sessions) as the current instrument
    // sound. Called from PluginEditor's library popup menu and its
    // prev/next oneshot arrows. Returns false if flatIndex doesn't
    // resolve to a real, loadable sample.
    bool selectSample (int flatIndex);

    juce::String getSelectedSampleName() const { return selectedSampleName; }

    // -1 if nothing is selected, or the selected sample was removed from
    // disk since -- used by the toolbar's prev/next oneshot arrows to know
    // where to step from.
    int getSelectedSampleFlatIndex() const { return sampleLibraryManager.indexOfRelativePath (selectedSampleRelativePath); }

    bool hasSampleSelected() const { return samplerEngine.hasSampleLoaded(); }

    // The post-sampler effects stage (Reverb/Delay/Chorus/Phaser/Flanger/
    // Distortion/High Pass/Low Pass knobs + the top-right Volume knob) --
    // see Audio/EffectsChain. PluginEditor wires its knobs directly to
    // this, both to read the current value when the editor is (re)opened
    // and to write a new one when a knob is moved.
    EffectsChain& getEffectsChain() noexcept { return effectsChain; }
    const EffectsChain& getEffectsChain() const noexcept { return effectsChain; }

private:
    MidiInputHandler midiInputHandler;

    SampleLibraryManager sampleLibraryManager;
    SamplerEngine samplerEngine;
    EffectsChain effectsChain;
    ComposedMelody composedMelody;

    // Stable identity used to persist the selection across sessions (see
    // getStateInformation/setStateInformation) -- a flat index isn't
    // stable since files being added/removed on disk shifts indices.
    juce::String selectedSampleRelativePath;
    juce::String selectedSampleName; // display name only, for the toolbar button text

    // Scratch buffer rebuilt once per block in processBlock(): incoming
    // host MIDI plus MelodySequencer's own generated note-ons for the
    // composed melody, merged before being handed to samplerEngine. A
    // member (not a local) purely to avoid a per-block heap allocation --
    // juce::MidiBuffer reuses its internal storage across clear() calls.
    juce::MidiBuffer scratchSynthMidi;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (W27PluginProcessor)
};

} // namespace w27
