#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "NoteStorage.h"

namespace w27
{

/**
    Stateless MIDI parsing. This is the ONLY place in the plugin that reads
    a juce::MidiBuffer / juce::MidiMessage directly -- it turns raw MIDI
    into NoteStorage updates so every other module can stay JUCE-MIDI-agnostic.
*/
namespace MidiEventParser
{
    // blockStartTimeSeconds: time of sample 0 of this block, in the same
    // clock NoteStorage's timestamps use (seconds since prepare()/reset()).
    void parseBlock (const juce::MidiBuffer& midiMessages,
                      double blockStartTimeSeconds,
                      double sampleRate,
                      NoteStorage& storage);
}

} // namespace w27
