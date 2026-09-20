Added alongside the melody-writing feature -- this is what makes the
plugin produce actual sound for the first time (previously it was a pure
"MIDI Visualizer" that always output silence, see the root README.md's
Phase 1-2 history).

- OneShotSamplerVoice.h -- no longer used (see its own comment for the
  history/why). It used to make every sample ignore note-off and always
  play to its own natural end regardless of key release; per explicit
  user request that was reversed project-wide, so SamplerEngine now uses
  plain juce::SamplerVoice instead.
- SamplerEngine.h/.cpp -- owns the actual juce::Synthesiser: a pool of
  plain juce::SamplerVoices (numVoices = 16) sharing whichever single
  sample is currently selected (loadSampleFromFile(), called from
  PluginProcessor::selectSample(), reads the sample straight off disk --
  see Library/README.md for why samples live on disk rather than embedded
  in the plugin binary). A note now stops as soon as its note-off arrives
  -- released key, or a drawn melody note's own end -- with a short
  (0.03s) release fade on the SamplerSound to avoid an audible click
  rather than an instant cut. Every sample is assumed tuned to
  middleCRootNote (60, i.e. middle C) -- there's no per-sample root-note
  override yet, so a pack tuned to a different pitch will play back
  correspondingly transposed until that's added (see
  Assets/Samples/README.md). Note stealing is enabled so repeatedly
  retriggering the same note (a fast melody, or replaying a live key
  quickly) layers/overlaps across the voice pool like a drum machine
  instead of dropping new hits. Switching the selected sample
  (selectSample()) hard-stops every currently sounding voice first, so
  the old sound doesn't keep playing underneath the new one.

PluginProcessor::processBlock() is what actually drives this every block:
it merges live host MIDI (already includes real note-offs, so playing a
keyboard/Channel Rack pattern is now audible AND stops correctly on key
release, not just visualized) with Composition::MelodySequencer's
generated note-on/note-off pairs for the user's drawn melody loop into one
juce::MidiBuffer, then hands that to SamplerEngine::renderNextBlock()
instead of the old unconditional buffer.clear(). MIDI capture into
NoteStorage (for the visualization layer) is completely unaffected by any
of this.

- EffectsChain.h/.cpp -- the post-sampler effects stage, added per
  explicit user request for a row of effect knobs at the bottom of the
  plugin (Reverb, Delay, Chorus, Phaser, Flanger, Distortion, High Pass,
  Low Pass) plus a top-right Volume knob. PluginProcessor::processBlock()
  runs EffectsChain::process() on the buffer immediately after
  SamplerEngine::renderNextBlock() returns -- it never touches MIDI or
  sample loading, only the rendered audio. Every knob is a plain 0-1
  "amount" (std::atomic<float>, written directly from PluginEditor's
  sliders on the message thread, read once per block on the audio
  thread -- same lock-free pattern as ComposedMelody elsewhere in this
  project). For the six wet/dry-style effects, 0% is a guaranteed true
  bypass (the stage is skipped entirely, not just turned down to
  near-silent); for the two filters, 0% means no filtering at all (High
  Pass at its lowest cutoff, Low Pass at its highest) and they're
  likewise skipped entirely at 0%. Chorus and Phaser use JUCE's own
  juce::dsp::Chorus/Phaser (both have a native setMix()); Reverb uses
  juce::dsp::Reverb, crossfading its dryLevel/wetLevel; High Pass/Low
  Pass use juce::dsp::StateVariableTPTFilter. Distortion (tanh
  waveshaper), Flanger (short LFO-swept feedback delay) and Delay (fixed
  350ms feedback echo) have no ready-made JUCE class that does what's
  wanted here, so they're hand-rolled on top of juce::dsp::DelayLine
  where one is needed. Signal order: High Pass -> Low Pass -> Distortion
  -> Chorus -> Flanger -> Phaser -> Delay -> Reverb -> Volume (the
  conventional filters/drive-first, modulation-next, time-based-after,
  gain-last pedalboard ordering). All 9 values persist in plugin state
  (PluginProcessor::getStateInformation/setStateInformation) and default
  to 0% (off) for the 8 effects and 75% for Volume.
