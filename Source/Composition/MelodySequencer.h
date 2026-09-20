#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include "ComposedMelody.h"

namespace w27
{

/**
    Turns ComposedMelody's beat-timed notes into sample-accurate note-on
    AND note-off triggers for the current audio block, so Audio/
    SamplerEngine can play the selected sample at the right moments and
    stop it again at the drawn note's own end -- per explicit user request,
    every sound now stops on note-off rather than always playing out to its
    own natural length, so MelodyNote::lengthBeats has to actually be
    scheduled, not just drawn.

    Deliberately stateless across calls beyond the melody itself: exactly
    like PianoRollView's PATTERN-mode "now" (see Visualization/README.md),
    every block's trigger range is derived fresh from the host's own
    ppqPosition, wrapped (fmod) by the melody's own loop length -- never
    accumulated by this plugin. That's what makes play/stop, host loop
    repeats, and dragging the host's own position slider all "just work"
    for the composed melody's playback, the same way it already does for
    PATTERN mode's on-screen note display. Each note's end beat
    (getEndBeat()) is wrapped into the loop the same way its start beat is,
    so a note-off scheduled past the loop boundary correctly lands early in
    the next iteration rather than being lost.

    Known limitations:
    - A host seek/scrub or loop wrap that lands MID-BLOCK is handled by
      splitting the block's beat range at the wrap point (so events right
      at the loop boundary aren't silently skipped), matching PATTERN
      mode's own documented approach -- but a LARGE jump (e.g. manually
      dragging the host's position far away) can still skip or re-trigger
      events near the jump, since there's no per-note "already
      triggered/already stopped" bookkeeping across arbitrary seeks -- only
      forward, contiguous block-by-block playback is fully exact.
    - A note whose lengthBeats happens to equal an exact multiple of the
      loop length produces a note-on and note-off at the same wrapped beat
      (and therefore the same sample offset); which one JUCE's synthesiser
      processes first at that position is unspecified. Not worth special-
      casing for how rarely a drawn note would land exactly there.
    - Also requires the host to report both ppqPosition and tempo
      (hostMusicalPositionKnown in PluginProcessor) -- on a host/config
      that doesn't (rare -- some Standalone configurations), the composed
      melody stays fully visible and editable but silent, rather than
      guessing at a clock of its own.
*/
class MelodySequencer
{
public:
    struct Trigger
    {
        int noteNumber = 60;
        float velocity = 0.85f;
        int sampleOffset = 0; // 0..numSamples-1, position within this block
        bool isNoteOn = true; // false = this is a note-off for noteNumber
    };

    // Appends this block's note-on AND note-off triggers to outTriggers
    // (does not clear it first -- caller's choice; note-offs use
    // Trigger::velocity == 0 by convention, though JUCE ignores a note-off
    // message's velocity anyway). ppqPositionAtBlockStart and bpm come
    // straight from juce::AudioPlayHead::PositionInfo for this block;
    // playbackActive should already fold in both "is the host transport
    // actually playing" and "does the host report a musical position at
    // all" -- a paused host, or one that never reports ppq/tempo, produces
    // no triggers (existing notes already sounding are NOT force-stopped
    // by this -- see PluginProcessor::processBlock()).
    static void collectTriggers (const ComposedMelody& melody,
                                  bool playbackActive,
                                  double ppqPositionAtBlockStart,
                                  double bpm,
                                  double sampleRate,
                                  int numSamples,
                                  std::vector<Trigger>& outTriggers);
};

} // namespace w27
