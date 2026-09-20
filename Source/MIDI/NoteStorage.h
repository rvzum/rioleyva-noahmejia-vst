#pragma once

#include <deque>
#include <vector>
#include <juce_core/juce_core.h>
#include "NoteEvent.h"

namespace w27
{

/**
    Thread-safe holding area for captured NoteEvents.

    Written from the audio thread (via MidiEventParser, one call per MIDI
    message per block) and read from the message/UI thread (Phase 2: a
    diagnostic counter in the editor; Phase 5+: the Visualization layer).

    MIDI event rates are low compared to audio-sample rates, so a plain
    CriticalSection is an acceptable and simple choice here -- this is not
    the audio-sample hot path. If profiling in a later phase shows lock
    contention, this can be swapped for a lock-free structure without any
    caller-visible API change.
*/
class NoteStorage
{
public:
    // Rolling capacity: oldest events are dropped once this is exceeded,
    // so a long-running session can't grow this without bound.
    static constexpr size_t maxStoredNotes = 4096;

    // Declared explicitly: JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR below
    // declares a (deleted) copy constructor, which under normal C++ rules
    // suppresses the implicitly-generated default constructor. Without this,
    // NoteStorage -- and anything holding one by value, like MidiInputHandler --
    // becomes non-default-constructible.
    NoteStorage() = default;

    void reset();

    void noteOn (int channel, int noteNumber, float velocity, double timeSeconds);
    void noteOff (int channel, int noteNumber, double timeSeconds);

    struct Stats
    {
        int totalNotesReceived = 0;
        int activeNoteCount = 0;
    };
    Stats getStats() const;

    // Snapshot copy for readers that need the actual note data (Visualization,
    // starting Phase 5). Not used yet in Phase 2, but the API is settled now
    // so later phases don't need to touch this class's locking again.
    std::vector<NoteEvent> getSnapshot() const;

private:
    mutable juce::CriticalSection lock;
    std::deque<NoteEvent> notes;
    int totalNotesReceived = 0;
    int activeNoteCount = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NoteStorage)
};

} // namespace w27
