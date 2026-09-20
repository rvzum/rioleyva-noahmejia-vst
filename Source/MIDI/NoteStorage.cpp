#include "NoteStorage.h"

namespace w27
{

void NoteStorage::reset()
{
    const juce::ScopedLock sl (lock);
    notes.clear();
    totalNotesReceived = 0;
    activeNoteCount = 0;
}

void NoteStorage::noteOn (int channel, int noteNumber, float velocity, double timeSeconds)
{
    const juce::ScopedLock sl (lock);

    NoteEvent event;
    event.channel = channel;
    event.noteNumber = noteNumber;
    event.velocity = velocity;
    event.startTimeSeconds = timeSeconds;
    event.endTimeSeconds = -1.0;

    notes.push_back (event);
    ++totalNotesReceived;
    ++activeNoteCount;

    while (notes.size() > maxStoredNotes)
    {
        if (notes.front().isActive())
            --activeNoteCount;

        notes.pop_front();
    }
}

void NoteStorage::noteOff (int channel, int noteNumber, double timeSeconds)
{
    const juce::ScopedLock sl (lock);

    // Search from the most recent event backwards: a note-off almost always
    // matches one of the last few note-ons, so this is effectively O(1) in
    // practice despite the linear worst case.
    for (auto it = notes.rbegin(); it != notes.rend(); ++it)
    {
        if (it->isActive() && it->channel == channel && it->noteNumber == noteNumber)
        {
            it->endTimeSeconds = timeSeconds;
            --activeNoteCount;
            return;
        }
    }

    // No matching held note found (e.g. note-off arrived with no prior
    // note-on in this session, or the note-on was already trimmed) -- ignored.
}

NoteStorage::Stats NoteStorage::getStats() const
{
    const juce::ScopedLock sl (lock);
    Stats stats;
    stats.totalNotesReceived = totalNotesReceived;
    stats.activeNoteCount = activeNoteCount;
    return stats;
}

std::vector<NoteEvent> NoteStorage::getSnapshot() const
{
    const juce::ScopedLock sl (lock);
    return { notes.begin(), notes.end() };
}

} // namespace w27
