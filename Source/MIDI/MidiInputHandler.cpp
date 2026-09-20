#include "MidiInputHandler.h"
#include "MidiEventParser.h"

namespace w27
{

void MidiInputHandler::prepare (double sampleRate) noexcept
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    samplesProcessed = 0;
    currentTimeSeconds.store (0.0, std::memory_order_relaxed);
    noteStorage.reset();
}

void MidiInputHandler::reset() noexcept
{
    samplesProcessed = 0;
    currentTimeSeconds.store (0.0, std::memory_order_relaxed);
    noteStorage.reset();
}

void MidiInputHandler::processBlock (const juce::MidiBuffer& midiMessages, int numSamples, bool hostIsPlaying)
{
    const double blockStartTimeSeconds = (double) samplesProcessed / currentSampleRate;
    MidiEventParser::parseBlock (midiMessages, blockStartTimeSeconds, currentSampleRate, noteStorage);

    // Only advance the clock while the host is actually playing, so the
    // visualization freezes (rather than continuing to scroll notes away)
    // while the transport is stopped -- see the header comment above.
    if (hostIsPlaying)
        samplesProcessed += numSamples;

    currentTimeSeconds.store ((double) samplesProcessed / currentSampleRate, std::memory_order_relaxed);
}

} // namespace w27
