#include "MelodySequencer.h"
#include <cmath>

namespace w27
{

namespace
{
    // One scheduled event -- either end of a MelodyNote -- already wrapped
    // into [0, loopLengthBeats) so both ends of every note are treated
    // identically by collectInRange() below, regardless of whether the
    // note's raw start/end beat fell inside the loop or past its end.
    struct Event
    {
        double beat = 0.0;
        int noteNumber = 60;
        float velocity = 0.85f;
        bool isNoteOn = true;
    };

    // Collects every event whose (already-wrapped) beat falls in
    // [rangeStartBeat, rangeEndBeat) into outTriggers, converting each
    // one's position within that range to a sample offset via
    // samplesPerBeat -- rangeOriginBeat is the beat value that corresponds
    // to sample offset 0 for this particular range (needed separately from
    // rangeStartBeat because a wrapped second segment starts counting
    // sample offsets partway through the block, not at 0 beats).
    void collectInRange (const std::vector<Event>& events, double rangeStartBeat, double rangeEndBeat,
                          double rangeOriginBeat, double samplesPerBeat, int numSamples,
                          std::vector<MelodySequencer::Trigger>& out)
    {
        for (const auto& ev : events)
        {
            if (ev.beat < rangeStartBeat || ev.beat >= rangeEndBeat)
                continue;

            const double beatsFromOrigin = ev.beat - rangeOriginBeat;
            int sampleOffset = (int) std::lround (beatsFromOrigin * samplesPerBeat);
            sampleOffset = juce::jlimit (0, juce::jmax (0, numSamples - 1), sampleOffset);

            out.push_back ({ ev.noteNumber, ev.velocity, sampleOffset, ev.isNoteOn });
        }
    }

    double wrapIntoLoop (double beat, double loopLengthBeats)
    {
        double wrapped = std::fmod (beat, loopLengthBeats);
        if (wrapped < 0.0)
            wrapped += loopLengthBeats;
        return wrapped;
    }
}

void MelodySequencer::collectTriggers (const ComposedMelody& melody, bool playbackActive,
                                        double ppqPositionAtBlockStart, double bpm, double sampleRate,
                                        int numSamples, std::vector<Trigger>& outTriggers)
{
    if (! playbackActive || numSamples <= 0 || sampleRate <= 0.0)
        return;

    const double safeBpm = bpm > 1.0 ? bpm : 120.0;
    const double samplesPerBeat = sampleRate / (safeBpm / 60.0);

    const double loopLengthBeats = melody.getLengthBeats();
    if (loopLengthBeats <= 1.0e-6)
        return;

    const auto notes = melody.getSnapshot();
    if (notes.empty())
        return;

    // Every note contributes one note-on (at its wrapped start beat) and
    // one note-off (at its wrapped end beat) -- wrapping each end
    // independently is what makes a note-off scheduled past the loop
    // boundary land correctly early in the next iteration instead of
    // being lost off the end.
    std::vector<Event> events;
    events.reserve (notes.size() * 2);
    for (const auto& note : notes)
    {
        events.push_back ({ wrapIntoLoop (note.startBeat, loopLengthBeats), note.noteNumber, note.velocity, true });
        events.push_back ({ wrapIntoLoop (note.getEndBeat(), loopLengthBeats), note.noteNumber, note.velocity, false });
    }

    const double blockLengthBeats = (double) numSamples / samplesPerBeat;

    double startMod = wrapIntoLoop (ppqPositionAtBlockStart, loopLengthBeats);
    const double endMod = startMod + blockLengthBeats;

    if (endMod <= loopLengthBeats)
    {
        collectInRange (events, startMod, endMod, startMod, samplesPerBeat, numSamples, outTriggers);
        return;
    }

    // Wraps mid-block: [startMod, loopLengthBeats) still counts sample
    // offsets from this block's own start; [0, wrappedEnd) continues
    // counting from the sample where the wrap actually happens.
    collectInRange (events, startMod, loopLengthBeats, startMod, samplesPerBeat, numSamples, outTriggers);

    const double wrappedEnd = endMod - loopLengthBeats;
    const double samplesUntilWrap = (loopLengthBeats - startMod) * samplesPerBeat;

    std::vector<Trigger> secondSegment;
    collectInRange (events, 0.0, wrappedEnd, 0.0, samplesPerBeat, numSamples, secondSegment);

    for (auto& trigger : secondSegment)
        trigger.sampleOffset = juce::jlimit (0, juce::jmax (0, numSamples - 1),
                                              trigger.sampleOffset + (int) std::lround (samplesUntilWrap));

    outTriggers.insert (outTriggers.end(), secondSegment.begin(), secondSegment.end());
}

} // namespace w27
