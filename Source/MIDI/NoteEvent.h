#pragma once

namespace w27
{

/**
    A single captured MIDI note, expressed independently of JUCE's raw
    MIDI types. This is the ONLY note representation the rest of the
    plugin (Timeline, Visualization, Effects) is allowed to depend on --
    nothing outside MIDI/ should ever touch a juce::MidiMessage or
    juce::MidiBuffer directly.
*/
struct NoteEvent
{
    int channel = 1;                  // MIDI channel, 1-16
    int noteNumber = 0;               // 0-127
    float velocity = 0.0f;            // note-on velocity, 0.0-1.0

    double startTimeSeconds = 0.0;    // time (since plugin prepare/reset) the note-on was received
    double endTimeSeconds = -1.0;     // time the matching note-off was received; -1 while still held

    bool isActive() const noexcept { return endTimeSeconds < 0.0; }
};

} // namespace w27
