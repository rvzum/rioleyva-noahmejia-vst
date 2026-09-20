#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>

namespace w27
{

/**
    Owns the juce::Synthesiser that actually makes this plugin produce
    sound -- previously it never did (see PluginProcessor.h's doc comment
    for the "MIDI Visualizer" history). A pool of plain juce::SamplerVoices
    all share whichever single oneshot sample is currently selected via the
    plugin's top-centre library button (Library/SampleLibraryManager lists
    what's available; PluginProcessor::selectSample() loads the chosen one
    here).

    loadSampleFromFile() wraps the chosen sample in a plain
    juce::SamplerSound, re-pitched across the whole keyboard from a fixed
    assumed root note (middleCRootNote -- see Assets/Samples/README.md for
    why there's no per-sample override yet). That pitching is what actually
    lets a melody be "written": playing/drawing different notes plays the
    same recording back at different speeds/pitches, same as any ordinary
    sampler instrument. Samples are read straight from disk (see
    Library/SampleLibraryManager) rather than embedded in the plugin binary
    -- real packs run into the gigabytes, too much to compile in.

    Per explicit user request, a note stops as soon as its note-off arrives
    -- for every sound, live MIDI or drawn melody alike -- rather than
    always playing out to the sample's own natural end regardless of key
    release. This project used to do the opposite on purpose (a dedicated
    OneShotSamplerVoice subclass that ignored note-off entirely, treating
    every sample as a drum-hit-style one-shot); that subclass is gone now
    that the behavior it existed for is no longer wanted. A short release
    time on the SamplerSound (see loadSampleFromFile()) avoids an audible
    click on release instead of cutting off at full volume instantly.

    Fed by two independent MIDI sources, merged into one buffer once per
    block in PluginProcessor::processBlock(): live incoming host MIDI
    (already includes real note-offs from the host/keyboard, passed
    straight through) and Composition::MelodySequencer's generated
    note-on/note-off pairs for the user's own drawn melody loop
    (Composition/README.md).
*/
class SamplerEngine
{
public:
    SamplerEngine();

    void prepare (double sampleRate, int samplesPerBlock);

    // Loads a new sample from a file on disk (see
    // Library/SampleLibraryManager::Sample::filePath), replacing whatever
    // was previously loaded. Safe to call from the message thread while the
    // audio thread renders: juce::Synthesiser locks internally around
    // sound/voice list changes, which is exactly the pattern JUCE's own
    // sampler examples rely on for "change instrument while playing".
    // Returns false if the file couldn't be read as a supported audio
    // format (nothing is changed in that case). On success, also hard-
    // stops whatever was still ringing from the previously loaded sample
    // first -- switching sounds mutes the old one immediately rather than
    // letting its tail play out underneath the new one.
    bool loadSampleFromFile (const juce::File& file, const juce::String& displayName);

    bool hasSampleLoaded() const noexcept { return sampleLoaded; }
    juce::String getLoadedSampleName() const { return loadedSampleName; }

    void renderNextBlock (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midiMessages,
                          int startSample, int numSamples);

    // Immediately silences every voice (hard stop, no tail) -- used from
    // PluginProcessor::releaseResources().
    void allNotesOff();

    // Assumed root MIDI note for every oneshot sample, until libraries ship
    // their own per-sample root-note metadata (see Assets/Samples/README.md).
    // 60 = middle C -- the common convention for melodic oneshot packs.
    static constexpr int middleCRootNote = 60;

private:
    juce::AudioFormatManager formatManager;
    juce::Synthesiser synth;

    bool sampleLoaded = false;
    juce::String loadedSampleName;

    static constexpr int numVoices = 16;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SamplerEngine)
};

} // namespace w27
