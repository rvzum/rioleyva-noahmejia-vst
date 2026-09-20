Phase 2 — implemented.

- NoteEvent.h        Plain data struct: channel, note number, velocity, start/end time.
- NoteStorage.h/.cpp Thread-safe rolling store of NoteEvents (CriticalSection-guarded),
                      written from the audio thread, read from the message/UI thread.
- MidiEventParser.h/.cpp  Stateless: the ONLY code that reads a juce::MidiBuffer /
                      juce::MidiMessage directly. Converts note-on/off pairs into
                      NoteStorage updates.
- MidiInputHandler.h/.cpp Owned by W27PluginProcessor; called once per processBlock().
                      Tracks sample-accurate timing and drives MidiEventParser.
                      processBlock() takes a hostIsPlaying flag (from
                      AudioPlayHead, read in W27PluginProcessor::processBlock())
                      and only advances its internal clock while true, so
                      Visualization freezes while the host transport is
                      stopped and only scrolls while playing -- see
                      Source/Visualization/README.md. MIDI is still captured
                      into NoteStorage regardless of play state.
                      Separately, updateHostPosition() (called once per
                      block from W27PluginProcessor::processBlock()) records
                      the host's raw transport position in seconds, straight
                      from juce::AudioPlayHead::PositionInfo::getTimeInSeconds()
                      -- completely independent of the note-capture clock
                      above. getHostPositionSeconds()/isHostPositionKnown()
                      expose it read-only. This exists purely for
                      PianoRollView's PATTERN mode, which derives its whole
                      "now" straight from this value each frame (wrapped by
                      the loaded file's own duration) instead of a clock
                      this plugin maintains itself -- which is what makes
                      pausing/replaying, host loop repeats, and dragging
                      FL Studio's own position/seek slider all update the
                      displayed notes correctly, since they all just show
                      up as changes to this one value. See
                      Source/Visualization/README.md.
- MidiFilePattern.h/.cpp  Added for the drag-and-drop "preview a MIDI file"
                      feature. loadFromFile() opens a .mid/.midi file with
                      juce::MidiFile, converts tick timestamps to seconds
                      (convertTimestampTicksToSeconds()), and manually pairs
                      note-on/off events across all tracks into a flat,
                      time-sorted std::vector<NoteEvent> -- using the same
                      backward-search pairing convention NoteStorage::noteOff()
                      uses, so a file-derived NoteEvent looks identical in
                      shape to a live-captured one. Any note left unmatched
                      (a stray note-on with no note-off) is dropped rather
                      than guessed at. Returns an empty/invalid
                      MidiFilePattern (isValid() == false) on any failure --
                      missing file, unparsable data, or zero complete notes --
                      never throws. This is pure data prep: it doesn't know
                      about playback, "now", or rendering -- see
                      PianoRollView's PATTERN mode in
                      Source/Visualization/README.md for how the parsed notes
                      are actually displayed and advanced once loaded.
                      Note: it reads MidiFile/MidiMessage directly, which is
                      normally MIDI/-only territory -- this is intentional,
                      since it's parsing a static file rather than live
                      MidiBuffer/AudioPlayHead data, so it doesn't fit
                      MidiEventParser's real-time contract.

Nothing outside this folder should touch juce::MidiMessage/MidiBuffer directly --
Timeline and Visualization (Phase 3+) read only NoteEvent data via NoteStorage,
reached through W27PluginProcessor::getMidiInputHandler().getNoteStorage().

Phase 2 also added a temporary numeric readout in the editor ("MIDI notes
captured: N (active: M)") purely to verify end-to-end MIDI capture inside a
real host (FL Studio) before any real visualization exists. It is removed
once the Piano Roll renderer (Phase 5+) is in place.
