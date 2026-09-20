#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <vector>
#include "NoteEvent.h"

namespace w27
{

/**
    A fixed, fully-known sequence of notes loaded from a standard MIDI file
    dragged onto the plugin editor (see PianoRollView's
    juce::FileDragAndDropTarget handling).

    This is fundamentally different from NoteStorage: NoteStorage only ever
    knows about MIDI as the host streams it live during playback, with no
    look-ahead at all -- a note simply doesn't exist yet, as far as the
    plugin can tell, until the instant it's actually received. A
    MidiFilePattern, by contrast, holds the WHOLE file up front, so
    PianoRollView can show notes before they're "reached" by the playhead --
    the one thing a live-MIDI-capture plugin can never do on its own, and
    exactly what makes a "notes visible immediately, scroll once you press
    Play" view possible.

    Every note here has both a start and an end time fully resolved (unlike
    NoteEvent's usual "-1 = still held" convention for live capture, which
    doesn't apply to a file that's already been read in full). Timestamps
    are seconds from the start of the FILE (t=0 = the file's first tick),
    using the file's own embedded tempo map -- a separate clock from
    MidiInputHandler's "seconds since prepare()/reset()". PianoRollView is
    responsible for translating between the two (see its patternEpochSeconds).
*/
struct MidiFilePattern
{
    std::vector<NoteEvent> notes;   // sorted by startTimeSeconds
    double durationSeconds = 0.0;   // end of the last note, for reference
    juce::String sourceFileName;    // just the file name (no path), for display

    // Tempo/time-signature detected from the file's own first tempo and
    // time-signature meta events (falls back to a plain 120 BPM 4/4 if the
    // file has none of either) -- used only to draw PianoRollView's bar
    // ruler with correctly-spaced, correctly-numbered bars. Not used for
    // note timing itself: convertTimestampTicksToSeconds() (called in
    // loadFromFile()) already baked the file's full tempo map into every
    // note's start/end seconds, including any tempo changes mid-file: this
    // bpm is just the first tempo seen, for drawing the ruler.
    double bpm = 120.0;
    int beatsPerBar = 4; // time-signature numerator
    int beatUnit = 4;    // time-signature denominator (4 = quarter, 8 = eighth, ...)

    double getBarDurationSeconds() const noexcept
    {
        const double safeBpm = bpm > 1.0 ? bpm : 120.0;
        const double safeBeatUnit = beatUnit > 0 ? (double) beatUnit : 4.0;
        return (double) juce::jmax (1, beatsPerBar) * (60.0 / safeBpm) * (4.0 / safeBeatUnit);
    }

    bool isValid() const noexcept { return ! notes.empty(); }

    // Returns an empty (isValid() == false) pattern if the file can't be
    // opened or parsed, or contains no complete (matched on/off) notes.
    // Never throws.
    static MidiFilePattern loadFromFile (const juce::File& file);
};

} // namespace w27
