Implemented ahead of the original phase order, at the user's request, once
Phase 2 (MIDI capture) was confirmed working in FL Studio.

- PianoRollView.h/.cpp -- the visualization. A piano-key strip sits on the
  LEFT edge (matching FL Studio's own Piano Roll), with a thin vertical
  pan slider along the RIGHT edge and a bar-ruler strip (barRulerHeight =
  20px) along the very TOP, above both. It renders in one of two modes,
  chosen automatically by whether a MIDI file has been dropped onto it,
  each with a different rendering model since only one of them has advance
  knowledge of the notes:

  **LIVE mode** (default, no file loaded): notes come from
  MidiInputHandler's NoteStorage as they're actually played. Since a live
  capture has no way to know about a note before it starts, this mode
  keeps the "scrolling" model: "now" is anchored at the note area's LEFT
  edge -- right next to the keys -- so a note touches its key at the exact
  instant it plays (and stays touching while held), then trails away to
  the RIGHT as it ages. Note bars: plain white, rounded, no stroke, with a
  horizontal alpha gradient (transparent at the older/far end, opaque at
  the key-touching end). Unchanged from previous rounds.

  **PATTERN mode** (after a .mid/.midi file is dropped onto the plugin
  window): the entire file is parsed up front by MidiFilePattern (see
  Source/MIDI/README.md), so every note's timing is known in advance. Per
  explicit user request ("сделай плагин таким как будто это пианоролл fl
  studio... с ползунком который идет и играет ноты слева направо"), this
  mode now renders like a real DAW piano roll instead of the earlier
  "notes move, playhead fixed" model:
  - **paintPatternNotes()** lays every note out at a FIXED horizontal
    position given by its own start/end time -- notes never move.
    Currently-sounding notes are tinted with the accent colour
    (Theme::keyActiveHighlight) instead of plain white, as a subtle
    "what's playing right now" cue.
  - A **moving playhead** (drawn directly in paint(), not a separate
    method) sweeps left-to-right across that fixed layout instead, tracking
    pattern-relative "now" -- a thin vertical line plus a small triangular
    flag at the top, in Theme::playheadLine.
  - **paintBarRuler()** draws numbered bar divisions along the top strip,
    spaced using the tempo/time-signature MidiFilePattern detected from
    the dropped file's own meta events (bpm/beatsPerBar/beatUnit, defaulting
    to 120 BPM 4/4 if the file has none) -- tick marks stay confined to the
    ruler strip itself.
  - **paintVerticalGridLines()** (new) draws a matching full-height grid
    through the note area itself: a brighter line at each bar boundary
    (Theme::octaveLine) and dimmer lines subdividing each bar into beats
    (Theme::gridLine). This reverses the earlier "no vertical lines"
    request -- the user supplied a reference screenshot of FL Studio's own
    Piano Roll, which does draw this grid, and asked for "что-то подобное"
    (something like that). PATTERN mode only, since it needs a known
    tempo/time signature; LIVE mode has none, so it keeps its plain
    background (flagged to the user as a scoping choice).
  - **paintKeyRowShading()** (new) adds a subtle dark fill (not a line)
    behind every black-key row, matching FL Studio's row shading, without
    reintroducing the horizontal grid LINES removed earlier. There is
    still no per-pitch horizontal grid LINE in either mode.
  - horizontalZoom means something different in PATTERN mode than LIVE
    mode: getPatternVisibleWindowSeconds() = loadedPattern.durationSeconds
    / horizontalZoom, so 1.0 (the default) fits the WHOLE pattern across
    the note area's width; zooming in shows a magnified view of only the
    pattern's beginning. There's no horizontal scrolling/panning, so a
    zoomed-in view of a long pattern can't currently show a later portion
    of it -- a known limitation, not requested to be fixed yet.

  Both modes still share drawTouchAnimations()/updateTouchAnimations() and
  the piano-key drawing.

  Dropping a file: PianoRollView implements juce::FileDragAndDropTarget.
  isInterestedInFileDrag() accepts .mid/.midi only. filesDropped() loads
  the first valid file via MidiFilePattern::loadFromFile(), stores it as
  loadedPattern, and resets touch-animation tracking state.
  fileDragEnter()/fileDragExit() just toggle a boolean that paint() uses
  to draw a highlighted "Drop MIDI file to load" overlay while a drag is
  in progress. getLoadedPatternName() exposes the loaded file's name (or
  an empty string in LIVE mode) for the editor's status label.

  PATTERN mode's "now" (`getPatternRelativeNow()`) is driven directly by
  the HOST's actual transport position, not this plugin's own clock:
  `MidiInputHandler::getHostPositionSeconds()` is fed each block from
  `juce::AudioPlayHead::PositionInfo::getTimeInSeconds()` (see
  `updateHostPosition()` in Source/MIDI/README.md). The formula is
  deliberately simple and stateless: `fmod(hostPositionSeconds,
  loadedPattern.durationSeconds)`, measured from the project's own time
  zero rather than from any epoch this plugin invents when the file is
  dropped. That one formula is what makes all of the following work
  correctly, in lock-step with the host, with no extra heuristics needed:
  - Pausing and playing again (or rewinding) immediately shows the right
    playhead position, since "now" is recomputed fresh from the live host
    position every frame instead of being accumulated by this plugin.
  - A host loop repeating wraps seamlessly via `fmod`, so the playhead (and
    touch animations) restart from the beginning, matching how a real
    piano roll's loop behaves ("когда луп обновляется и играется заново,
    ноты должны появиться заново").
  - Dragging FL Studio's own position/seek slider (the horizontal
    transport scrubber) moves the playhead in lock-step, since any change
    in the reported host position feeds straight through -- "если мы
    двигаем этот ползунок в FL Studio, то и в плагине ноты должны
    смещаться соответственно".
  This assumes the dropped file's own duration matches the host's actual
  loop length starting from position 0 -- true for the common case of a
  single pattern's MIDI export played back from the start of the
  project/pattern timeline. Every time `paint()` sees pattern-relative
  "now" drop compared to the previous frame (a loop wrap or a backward
  seek), it resets touch-animation tracking (`lastProcessedNoteStartTime =
  -1.0`, clears `touchAnimations`) so struck keys flash again on every
  repeat/seek, not just the first pass.
  On a host that doesn't expose a transport position at all (rare -- some
  Standalone configurations), PATTERN mode falls back to the previous
  approach: this plugin's own note-capture clock relative to the moment
  the file was dropped (`patternEpochSeconds`) -- functional, but without
  seek/loop tracking.

  Known caveat: PATTERN mode still only knows the tempo baked into the
  dropped MIDI file itself (`MidiFile::convertTimestampTicksToSeconds()`
  bakes in whatever tempo events that file contains), not the host
  project's tempo. Using the host's real transport position keeps display
  in lock-step with actual playback/scrubbing, but if the file's own
  duration doesn't match the host's actual loop length, the fmod wrap
  point can land slightly out of phase with the host's real loop
  boundary; and the bar ruler's spacing (from the file's own detected
  tempo/time signature) can visually disagree with the host project's
  actual bars if they differ. Full host tempo sync remains unimplemented
  (see the Timeline/PlaybackState note further down).

  Not implemented (raised but out of scope for a VST3 plugin): having the
  dropped MIDI file also appear inside FL Studio's own internal Piano Roll
  editor. A VST3 plugin has no API to write notes into the host's own
  editable pattern/arrangement data -- it can only receive MIDI from the
  host, never inject it back into the host's own editor. What PATTERN mode
  does instead is show the dropped file in this plugin's own FL-Studio-style
  view, which is the closest equivalent achievable from a plugin.

  Other details: zoomInVertical()/zoomOutVertical() (how much pitch range
  is visible) and zoomInHorizontal()/zoomOutHorizontal() (how many seconds
  are visible, meaning differs by mode -- see above) drive the editor's
  toolbar buttons. The vertical pan slider (verticalPanSlider, a JUCE
  Slider styled as a plain vertical fader) lets the user drag the visible
  pitch window up/down across the full 88-key range --
  verticalPanCenterNote holds the pitch it's currently centred on, read by
  getVisibleNoteRange() exactly where the old fixed "middle C" constant
  used to be. No horizontal panning. Reads only NoteEvent data (live via
  MidiInputHandler's NoteStorage, or pre-parsed via MidiFilePattern) --
  never touches juce::MidiMessage/MidiBuffer directly.

Freezes while the host transport is stopped, scrolls only while playing
(per explicit user request, to match FL Studio's own Piano Roll feel), in
both LIVE and PATTERN modes since both derive "now" from a host-gated
clock: MidiInputHandler::processBlock() takes a hostIsPlaying flag (read
from juce::AudioPlayHead::getPosition().getIsPlaying() in
W27PluginProcessor::processBlock(), defaulting to true if the host/wrapper
doesn't expose it) and only advances its internal clock
(getCurrentTimeSeconds()) while that's true -- LIVE mode renders directly
from this. PATTERN mode instead reads the host's own transport position
directly (see above), which naturally doesn't advance while stopped. MIDI
is still captured into NoteStorage regardless of play state in either
case. This is still not full tempo/transport sync (TimelineState/
PlaybackState, Phase 3-4, remains unimplemented -- no true bar/beat
awareness beyond the bar ruler's display-only tempo detection).

One known edge case (LIVE mode only): if MIDI arrives while the host is
stopped (e.g. the user plays a physical MIDI keyboard without pressing
Play), those notes all get the same (frozen) timestamp until playback
resumes, since the clock isn't advancing. Visually they'll stack instead
of showing normal spacing/duration until Play is pressed.

Note-touch animation: PianoRollView keeps a small, capped
(maxTouchAnimations = 20) list of short-lived animations
(touchAnimationDurationSeconds = 0.4s each). Every paint(), it compares
each note's startTimeSeconds against the highest start time already
processed (lastProcessedNoteStartTime) to spot notes that just started,
spawns one animation per new note-on at that note's key position, and
expires old ones -- no per-note IDs or extra bookkeeping needed. In
PATTERN mode, notes with a future startTimeSeconds are explicitly skipped
by this comparison (only notes at or before "now" can trigger a touch);
lastProcessedNoteStartTime and the animation list are reset whenever a new
file is dropped, or whenever a loop repeat/seek is detected (see above).
Each active animation draws two things in drawTouchAnimations() (on top of
everything else so it always reads): a quick fading flash filled directly
over that key's cell (the clearest "just struck" cue), plus a soft
radial-gradient glow blooming from the key out into the note area. Both
use BackgroundManager::getCurrentAccentColour() so the colour matches
whichever background is selected. lastProcessedNoteStartTime is
initialised to "now" (not 0) at construction specifically so reopening the
editor while NoteStorage already holds note history doesn't fire a burst
of animations for old notes. With the animation count capped and the draw
cost being a fillRect + a gradient-filled ellipse per active animation at
30fps, this stays cheap regardless of how much MIDI is coming in.
