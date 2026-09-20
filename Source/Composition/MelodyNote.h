#pragma once

namespace w27
{

/**
    A single note in the user-drawn ComposedMelody (see ComposedMelody.h).

    Unlike NoteEvent (MIDI/NoteEvent.h), which times everything in SECONDS
    relative to a fixed clock, a MelodyNote is timed in BEATS relative to
    the start of the melody's own loop. Beats -- not seconds -- are what
    let a drawn melody keep its musical shape (note spacing/lengths)
    regardless of the host project's tempo, and follow a live tempo change
    without needing to be re-timed: Composition::MelodySequencer converts
    beats to actual sample offsets fresh, every audio block, using the
    host's current BPM (see Composition/README.md).
*/
struct MelodyNote
{
    int noteNumber = 60;        // 0-127
    float velocity = 0.85f;     // 0.0-1.0, applied as the triggered sample's note-on velocity

    double startBeat = 0.0;     // beats from the loop's start (0 = loop start)
    double lengthBeats = 0.5;   // in beats -- determines both the on-screen note length/drag-resize
                                 // AND, since a note now actually stops on note-off (see
                                 // Audio/SamplerEngine), how long the sample actually plays for:
                                 // Composition::MelodySequencer schedules a note-off at getEndBeat().

    double getEndBeat() const noexcept { return startBeat + lengthBeats; }
};

} // namespace w27
