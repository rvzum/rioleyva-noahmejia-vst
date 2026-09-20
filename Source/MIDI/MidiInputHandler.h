#pragma once

#include <atomic>
#include <juce_audio_basics/juce_audio_basics.h>
#include "NoteStorage.h"

namespace w27
{

/**
    Audio-thread-facing MIDI capture. Owned by W27PluginProcessor and driven
    once per processBlock(). Nothing outside MIDI/ should own or drive this
    directly -- PluginProcessor exposes read-only access to its NoteStorage
    for the editor/visualization layer.
*/
class MidiInputHandler
{
public:
    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    // Read-only with respect to midiMessages: never removes or edits events,
    // so the same buffer can still be passed through to the host afterwards.
    //
    // hostIsPlaying gates the "seconds since prepare()/reset()" clock
    // (getCurrentTimeSeconds()): it only advances while the host transport
    // is actually running, so the visualization sits still -- showing
    // whatever was last captured -- while stopped, and only starts
    // scrolling again once playback resumes. This matches how FL Studio's
    // own Piano Roll behaves (static until you press Play), and is as
    // close as a live MIDI-capture plugin can get to that without access
    // to the host's actual pattern data (which isn't available to a
    // VST3 MIDI/instrument plugin -- it only ever sees MIDI as the host
    // streams it, never in advance). Incoming MIDI is still captured
    // into NoteStorage regardless of hostIsPlaying.
    void processBlock (const juce::MidiBuffer& midiMessages, int numSamples, bool hostIsPlaying);

    NoteStorage& getNoteStorage() noexcept { return noteStorage; }
    const NoteStorage& getNoteStorage() const noexcept { return noteStorage; }

    // Thread-safe "seconds since prepare()/reset()" position, for UI-thread
    // consumers (Visualization) that need to know "now" without reaching
    // into audio-thread-only state directly.
    double getCurrentTimeSeconds() const noexcept { return currentTimeSeconds.load (std::memory_order_relaxed); }

    // Raw host transport position, as last reported by
    // juce::AudioPlayHead::PositionInfo::getTimeInSeconds(). Updated once per
    // block from PluginProcessor::processBlock() via updateHostPosition() --
    // completely separate from the note-capture clock above. Used by
    // PianoRollView's PATTERN mode so a dropped MIDI file's visual playback
    // position tracks the host's *actual* transport position rather than
    // this plugin's own guess at elapsed time: when the host loops (or the
    // user rewinds), its reported position jumps backward, which is exactly
    // the signal PianoRollView needs to replay the pattern from its own
    // beginning again. Not every host/wrapper exposes a time position (some
    // Standalone configurations don't) -- isHostPositionKnown() says whether
    // the last updateHostPosition() call actually had one.
    void updateHostPosition (double seconds, bool known) noexcept
    {
        hostPositionSeconds.store (seconds, std::memory_order_relaxed);
        hostPositionKnown.store (known, std::memory_order_relaxed);
    }

    double getHostPositionSeconds() const noexcept { return hostPositionSeconds.load (std::memory_order_relaxed); }
    bool isHostPositionKnown() const noexcept { return hostPositionKnown.load (std::memory_order_relaxed); }

    // Musical-time counterpart of updateHostPosition()/getHostPositionSeconds()
    // above -- completely separate state, updated once per block from
    // PluginProcessor::processBlock() via juce::AudioPlayHead::PositionInfo::
    // getPpqPosition()/getBpm(). Used by Composition::MelodySequencer (to
    // schedule the drawn melody's playback) and PianoRollView's Compose
    // mode (to draw its own playhead) -- both need beats/tempo rather than
    // plain seconds, since the composed melody's timing follows the host's
    // live tempo rather than being fixed to one BPM. known is false on a
    // host/config that doesn't report ppqPosition and/or tempo at all
    // (rare -- some Standalone configurations); callers treat that as "no
    // musical clock available" rather than guessing at a fallback tempo.
    void updateHostMusicalPosition (double ppqPosition, double bpm, bool known) noexcept
    {
        hostPpqPosition.store (ppqPosition, std::memory_order_relaxed);
        hostBpm.store (bpm, std::memory_order_relaxed);
        hostMusicalPositionKnown.store (known, std::memory_order_relaxed);
    }

    double getHostPpqPosition() const noexcept { return hostPpqPosition.load (std::memory_order_relaxed); }
    double getHostBpm() const noexcept { return hostBpm.load (std::memory_order_relaxed); }
    bool isHostMusicalPositionKnown() const noexcept { return hostMusicalPositionKnown.load (std::memory_order_relaxed); }

private:
    NoteStorage noteStorage;
    double currentSampleRate = 44100.0;
    juce::int64 samplesProcessed = 0;
    std::atomic<double> currentTimeSeconds { 0.0 };
    std::atomic<double> hostPositionSeconds { 0.0 };
    std::atomic<bool> hostPositionKnown { false };
    std::atomic<double> hostPpqPosition { 0.0 };
    std::atomic<double> hostBpm { 120.0 };
    std::atomic<bool> hostMusicalPositionKnown { false };
};

} // namespace w27
