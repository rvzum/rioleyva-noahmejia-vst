#include "SamplerEngine.h"

namespace w27
{

SamplerEngine::SamplerEngine()
{
    formatManager.registerBasicFormats(); // WAV, AIFF, FLAC, Ogg, and (Mac-only) MP3 via CoreAudioFormat

    for (int i = 0; i < numVoices; ++i)
        synth.addVoice (new juce::SamplerVoice());

    // Retriggering the same note while a previous hit is still ringing (a
    // fast repeated melody note, or replaying the same live key quickly)
    // should layer/overlap like a drum machine, not silently drop the new
    // hit -- note stealing is what lets Synthesiser hand it a different
    // voice from the pool instead.
    synth.setNoteStealingEnabled (true);
}

void SamplerEngine::prepare (double sampleRate, int /*samplesPerBlock*/)
{
    synth.setCurrentPlaybackSampleRate (sampleRate);
}

bool SamplerEngine::loadSampleFromFile (const juce::File& file, const juce::String& displayName)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));

    if (reader == nullptr)
        return false;

    juce::BigInteger allNotes;
    allNotes.setRange (0, 128, true);

    // 0.001s attack avoids a click at trigger. 0.03s release avoids a
    // click on the OTHER end -- when a note-off arrives (key released,
    // drawn note ends), the sample now actually stops there (per explicit
    // user request) instead of always playing out to its own natural end;
    // a short fade rather than an instant cut keeps that stop inaudible as
    // a click. 30s max length is generous headroom for however long a
    // note gets held.
    auto* newSound = new juce::SamplerSound (displayName, *reader, allNotes, middleCRootNote,
                                              0.001, 0.03, 30.0);

    // Switching the active oneshot must mute whatever was still sounding
    // from the previous one, not let it keep playing underneath the new
    // sound (per explicit user request). allowTailOff == false is a hard,
    // immediate stop -- no release fade -- for every currently active
    // voice.
    synth.allNotesOff (0, false);

    synth.clearSounds();
    synth.addSound (newSound);

    sampleLoaded = true;
    loadedSampleName = displayName;
    return true;
}

void SamplerEngine::renderNextBlock (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midiMessages,
                                     int startSample, int numSamples)
{
    synth.renderNextBlock (buffer, midiMessages, startSample, numSamples);
}

void SamplerEngine::allNotesOff()
{
    synth.allNotesOff (0, false);
}

} // namespace w27
