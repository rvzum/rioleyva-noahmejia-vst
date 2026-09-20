#include "MidiFilePattern.h"
#include <algorithm>

namespace w27
{

MidiFilePattern MidiFilePattern::loadFromFile (const juce::File& file)
{
    MidiFilePattern pattern;

    juce::FileInputStream stream (file);
    if (! stream.openedOk())
        return pattern;

    juce::MidiFile midiFile;
    if (! midiFile.readFrom (stream))
        return pattern;

    // Rewrites every track's event timestamps in place from MIDI ticks to
    // seconds, using the file's own embedded tempo/time-signature events
    // (falls back to a sensible default tempo if the file has none). After
    // this call, MidiMessage::getTimeStamp() on every event is already in
    // seconds from the start of the file.
    midiFile.convertTimestampTicksToSeconds();

    // Manual note-on/off pairing, deliberately mirroring
    // NoteStorage::noteOn()/noteOff()'s approach (search backwards for the
    // most recent still-open note of the same channel+pitch) so this reads
    // the exact same way live capture already does, rather than depending
    // on a separate JUCE pairing utility.
    std::vector<NoteEvent> collected;

    // First tempo / time-signature meta event seen anywhere in the file,
    // for PianoRollView's bar ruler -- see the header comment on
    // MidiFilePattern::bpm. Independent of the note on/off pairing below.
    bool tempoFound = false;
    bool timeSigFound = false;

    for (int t = 0; t < midiFile.getNumTracks(); ++t)
    {
        const auto* track = midiFile.getTrack (t);
        if (track == nullptr)
            continue;

        for (int i = 0; i < track->getNumEvents(); ++i)
        {
            const auto* eventHolder = track->getEventPointer (i);
            if (eventHolder == nullptr)
                continue;

            const auto& message = eventHolder->message;
            const double eventTimeSeconds = message.getTimeStamp();

            if (! tempoFound && message.isTempoMetaEvent())
            {
                const double secondsPerQuarterNote = message.getTempoSecondsPerQuarterNote();
                if (secondsPerQuarterNote > 0.0)
                {
                    pattern.bpm = 60.0 / secondsPerQuarterNote;
                    tempoFound = true;
                }
            }

            if (! timeSigFound && message.isTimeSignatureMetaEvent())
            {
                int numerator = 4, denominator = 4;
                message.getTimeSignatureInfo (numerator, denominator);
                if (numerator > 0 && denominator > 0)
                {
                    pattern.beatsPerBar = numerator;
                    pattern.beatUnit = denominator;
                    timeSigFound = true;
                }
            }

            // A note-on with velocity 0 is a de facto note-off (standard
            // MIDI running-status convention) -- checked before isNoteOn(),
            // exactly as MidiEventParser::parseBlock() does for live MIDI.
            if (message.isNoteOff() || (message.isNoteOn() && message.getFloatVelocity() <= 0.0f))
            {
                const int channel = message.getChannel();
                const int noteNumber = message.getNoteNumber();

                for (auto it = collected.rbegin(); it != collected.rend(); ++it)
                {
                    if (it->isActive() && it->channel == channel && it->noteNumber == noteNumber)
                    {
                        it->endTimeSeconds = eventTimeSeconds;
                        break;
                    }
                }
            }
            else if (message.isNoteOn())
            {
                NoteEvent event;
                event.channel = message.getChannel();
                event.noteNumber = message.getNoteNumber();
                event.velocity = message.getFloatVelocity();
                event.startTimeSeconds = eventTimeSeconds;
                event.endTimeSeconds = -1.0;
                collected.push_back (event);
            }
        }
    }

    // Drop anything left unmatched (e.g. a truncated/malformed file, or a
    // note-on with no corresponding note-off anywhere in the file) rather
    // than showing a note with no defined end.
    collected.erase (std::remove_if (collected.begin(), collected.end(),
                                      [] (const NoteEvent& e) { return e.isActive(); }),
                      collected.end());

    std::sort (collected.begin(), collected.end(),
               [] (const NoteEvent& a, const NoteEvent& b) { return a.startTimeSeconds < b.startTimeSeconds; });

    for (const auto& e : collected)
        pattern.durationSeconds = juce::jmax (pattern.durationSeconds, e.endTimeSeconds);

    pattern.notes = std::move (collected);
    pattern.sourceFileName = file.getFileName();

    return pattern;
}

} // namespace w27
