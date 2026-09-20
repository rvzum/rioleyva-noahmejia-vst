#include "MidiEventParser.h"

namespace w27
{

namespace MidiEventParser
{
    void parseBlock (const juce::MidiBuffer& midiMessages,
                      double blockStartTimeSeconds,
                      double sampleRate,
                      NoteStorage& storage)
    {
        for (const auto metadata : midiMessages)
        {
            const auto message = metadata.getMessage();

            const double eventTimeSeconds = blockStartTimeSeconds
                + (sampleRate > 0.0 ? (double) metadata.samplePosition / sampleRate : 0.0);

            // A note-on with velocity 0 is a de facto note-off (standard MIDI
            // running-status convention) -- must be checked before isNoteOn().
            if (message.isNoteOff() || (message.isNoteOn() && message.getFloatVelocity() <= 0.0f))
            {
                storage.noteOff (message.getChannel(), message.getNoteNumber(), eventTimeSeconds);
            }
            else if (message.isNoteOn())
            {
                storage.noteOn (message.getChannel(), message.getNoteNumber(),
                                 message.getFloatVelocity(), eventTimeSeconds);
            }
        }
    }
}

} // namespace w27
