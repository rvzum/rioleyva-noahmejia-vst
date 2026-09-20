#include "ComposedMelody.h"

namespace w27
{

void ComposedMelody::addNote (const MelodyNote& note)
{
    const juce::ScopedLock sl (lock);
    notes.push_back (note);
}

void ComposedMelody::removeNoteAt (size_t index)
{
    const juce::ScopedLock sl (lock);
    if (index < notes.size())
        notes.erase (notes.begin() + (long) index);
}

void ComposedMelody::updateNoteAt (size_t index, const MelodyNote& updated)
{
    const juce::ScopedLock sl (lock);
    if (index < notes.size())
        notes[index] = updated;
}

void ComposedMelody::clear()
{
    const juce::ScopedLock sl (lock);
    notes.clear();
}

std::vector<MelodyNote> ComposedMelody::getSnapshot() const
{
    const juce::ScopedLock sl (lock);
    return notes;
}

std::unique_ptr<juce::XmlElement> ComposedMelody::toXml() const
{
    auto xml = std::make_unique<juce::XmlElement> ("Melody");

    const juce::ScopedLock sl (lock);

    xml->setAttribute ("lengthBars", lengthBars);
    xml->setAttribute ("beatsPerBar", beatsPerBar);

    for (const auto& note : notes)
    {
        auto* noteXml = xml->createNewChildElement ("Note");
        noteXml->setAttribute ("note", note.noteNumber);
        noteXml->setAttribute ("vel", (double) note.velocity);
        noteXml->setAttribute ("start", note.startBeat);
        noteXml->setAttribute ("length", note.lengthBeats);
    }

    return xml;
}

void ComposedMelody::restoreFromXml (const juce::XmlElement& xml)
{
    const juce::ScopedLock sl (lock);

    notes.clear();
    lengthBars = xml.getIntAttribute ("lengthBars", lengthBars);
    beatsPerBar = xml.getIntAttribute ("beatsPerBar", beatsPerBar);

    for (auto* child : xml.getChildIterator())
    {
        if (! child->hasTagName ("Note"))
            continue;

        MelodyNote note;
        note.noteNumber = child->getIntAttribute ("note", 60);
        note.velocity = (float) child->getDoubleAttribute ("vel", 0.85);
        note.startBeat = child->getDoubleAttribute ("start", 0.0);
        note.lengthBeats = child->getDoubleAttribute ("length", 0.5);
        notes.push_back (note);
    }
}

} // namespace w27
