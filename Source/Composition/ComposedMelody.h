#pragma once

#include <memory>
#include <vector>
#include <juce_core/juce_core.h>
#include "MelodyNote.h"

namespace w27
{

/**
    The user's own hand-drawn melody: a loop of MelodyNotes the user places,
    moves, resizes and deletes directly on the piano roll (see
    Visualization/PianoRollView's Compose-mode mouse handling, enabled by
    the toolbar's "Write Melody" toggle).

    This is a fully separate note source from both MIDI/NoteStorage (live
    MIDI as the host streams it) and MIDI/MidiFilePattern (a dropped .mid
    file) -- ComposedMelody is authored entirely inside this plugin, has no
    upstream MIDI source at all, and (together with live MIDI input) is
    what this plugin turns into actual audible sound (via
    Composition/MelodySequencer + Audio/SamplerEngine) rather than only
    visualizing.

    Timed in BEATS (see MelodyNote.h), not seconds, so it follows the
    host's live tempo rather than being tied to one fixed BPM.
    lengthBars * beatsPerBar is the loop length in beats --
    MelodySequencer wraps playback at that point, and PianoRollView's
    Compose mode always shows exactly one full loop across its width (the
    same "1.0 zoom = the whole thing fits" convention MidiFilePattern's
    PATTERN mode uses for a whole dropped file's duration).

    Notes are kept in a plain, unordered list -- nothing here needs
    startBeat ordering (MelodySequencer and PianoRollView's painting both
    just scan every note each time), so edits are a simple O(1)
    append/erase/overwrite rather than an insertion-sort.

    Written from the message/UI thread only (mouse editing in
    PianoRollView) and read from the audio thread (MelodySequencer, once
    per block) and the UI thread (painting) -- like NoteStorage, edit/read
    rates are low enough that a plain CriticalSection is an acceptable,
    simple choice.
*/
class ComposedMelody
{
public:
    ComposedMelody() = default;

    double getLengthBeats() const noexcept { return (double) lengthBars * (double) beatsPerBar; }
    int getLengthBars() const noexcept { return lengthBars; }
    int getBeatsPerBar() const noexcept { return beatsPerBar; }

    void addNote (const MelodyNote& note);

    // Index space matches whatever getSnapshot() most recently returned to
    // that same caller -- PianoRollView always re-fetches getSnapshot()
    // rather than caching an index across separate mouse events, since
    // only it ever mutates this object (single UI-thread writer).
    void removeNoteAt (size_t index);
    void updateNoteAt (size_t index, const MelodyNote& updated);

    void clear();

    std::vector<MelodyNote> getSnapshot() const;

    // XML round-trip for plugin state save/load (see PluginProcessor::
    // getStateInformation/setStateInformation).
    std::unique_ptr<juce::XmlElement> toXml() const;
    void restoreFromXml (const juce::XmlElement& xml);

private:
    mutable juce::CriticalSection lock;
    std::vector<MelodyNote> notes;

    int lengthBars = 4;
    int beatsPerBar = 4;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ComposedMelody)
};

} // namespace w27
