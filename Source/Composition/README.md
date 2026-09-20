Added for the melody-writing feature -- lets the user author their own
notes directly inside the plugin, rather than only visualizing MIDI that
comes from somewhere else.

- MelodyNote.h -- a single note, timed in BEATS from the loop's start
  (not seconds -- see its doc comment for why: it's what lets a drawn
  melody follow the host's live tempo instead of being baked to one BPM).
- ComposedMelody.h/.cpp -- the melody itself: an unordered list of
  MelodyNotes plus a loop length (lengthBars * beatsPerBar), edited
  directly by Visualization/PianoRollView's mouse handling once "Write
  Melody" is toggled on in the toolbar (click empty space to add a note
  and drag to set its length, drag an existing note's body to move it or
  its right edge to resize it, right-click/double-click to delete it --
  see PianoRollView.cpp's mouseDown/mouseDrag). CriticalSection-guarded
  like MIDI/NoteStorage, since it's written from the UI thread and read
  from both the UI thread (painting) and the audio thread (below).
  Round-trips to XML for save/reload with the host project (see
  PluginProcessor::getStateInformation/setStateInformation).
- MelodySequencer.h/.cpp -- stateless, audio-thread-side: turns
  ComposedMelody's notes into sample-accurate note-on AND note-off
  triggers for the current block (a note-off at each note's own
  getEndBeat(), since a note now actually stops there -- see
  Source/Audio/README.md), driven by the host's own ppqPosition/tempo
  every block (never a clock this plugin accumulates itself) -- see its
  own doc comment for the full mechanism and its documented edge cases.

PluginProcessor::processBlock() calls MelodySequencer::collectTriggers()
once per block and merges the resulting note-on/note-off pairs into the
same juce::MidiBuffer as live incoming host MIDI (which already carries
its own real note-offs), before handing it to
Audio/SamplerEngine::renderNextBlock() -- see Source/Audio/README.md for
the playback side, and Assets/Samples/README.md for where the actual
oneshot sounds it plays come from.

This is a fully separate note source from MIDI/NoteStorage (live MIDI) and
MIDI/MidiFilePattern (a dropped .mid file, used by PianoRollView's PATTERN
mode) -- Visualization/PianoRollView shows only one of LIVE/PATTERN/Compose
mode at a time (Compose mode, when the "Write Melody" toggle is on, takes
over the whole view -- see PianoRollView.cpp's paint()), rather than
overlaying all three.
