# RIO LEYVA x NOAH MEJIA VST — MIDI Visualizer + Melody Instrument

MIDI-in, visuals-out utility plugin for FL Studio (and any VST3 host),
built around an animated, fully customizable Piano Roll. Originally a pure
visualizer that never generated or processed audio (Phase 1-2 below) --
it now also plays back a selectable oneshot sample as a real melodic
instrument, either from live MIDI or from a melody drawn directly in the
plugin. See "Writing melodies" further down.

## Status

**Phase 1 — Project Initialization** ✅ Build verified, confirmed loading in FL Studio
**Phase 2 — MIDI Capability** ✅ Confirmed working in FL Studio (28 notes captured, 4 active, live test)
**Visualization + Effects + Theme** ✅ Implemented ahead of the original phase order, at the user's request
**Writing Melodies + Oneshot Sample Playback** ✅ Implemented at the user's request -- see "Writing melodies" below

Phase 1: a JUCE + CMake VST3/Standalone project that builds and installs
cleanly on macOS (AppleClang 17 / macOS 15 SDK). Confirmed loading inside
FL Studio.

Phase 2: real MIDI capture. `MidiInputHandler` reads every incoming MIDI
message per audio block, `MidiEventParser` turns note-on/off pairs into
`NoteEvent`s, `NoteStorage` holds them thread-safely. Confirmed receiving
real notes from FL Studio's Channel Rack.

Visualization (originally Phase 5-6) and Effects/Theme (originally Phase
5+/8) were implemented next, ahead of Timeline (Phase 3-4), at the user's
request once MIDI capture was verified: `PianoRollView` renders a
real-time scrolling piano roll over a selectable animated background
(`Effects/BackgroundManager`), all colors sourced from `Theme/ThemeColors.h`.
This is a real-time monitor, not a bar/beat-synced timeline view -- see
`Source/Visualization/README.md` for what that means and what changes if
host tempo sync is added later.

Note styling: plain white, rounded, no stroke, with a horizontal alpha
gradient (transparent at the note's start, fully opaque near its end).
The piano-key strip sits on the LEFT (matching FL Studio's own Piano
Roll). By default (LIVE mode -- no file loaded), "now" sits right at that
key edge, so a note visually touches its key the instant it's played and
stays touching while held, then trails away to the RIGHT (fading out) as
it ages -- i.e. notes travel left-to-right, newest at the key. (Not the
other way around in this mode, since a live MIDI monitor has no future
notes to show ahead of time -- it can only display a note once it's
actually started.)

**Drag a .mid/.midi file onto the plugin window** to switch into PATTERN
mode, which renders like a real DAW piano roll: every note in the file is
parsed immediately (`MidiFilePattern`, see `Source/MIDI/README.md`) and
laid out at a FIXED position on screen (notes do not move), while a
moving PLAYHEAD line sweeps left-to-right across them as playback
advances -- "ползунок который идет и играет ноты слева направо". A bar
ruler along the top of the note area shows numbered bar divisions, spaced
using the tempo/time-signature detected from the dropped file itself.
Playhead position is driven directly by FL Studio's own real transport
position every frame (not a clock this plugin keeps itself), so: the
file's notes are visible the instant it's dropped; pausing and playing
again (or rewinding) immediately shows the correct playhead position; a
loop repeating makes the playhead restart from the beginning
automatically; and dragging FL Studio's own position/seek slider moves
the plugin's playhead in lock-step with it. Caveat: PATTERN mode still
only knows the tempo/time-signature baked into the dropped file itself,
not the host project's, so a mismatch can land the loop wrap point (and
the bar ruler's spacing) slightly out of phase with the host's real
project. Not possible for a VST3 plugin: writing the dropped file's notes
into FL Studio's own internal Piano Roll editor -- a plugin can only
receive MIDI from the host, never inject notes back into the host's own
pattern data, so PATTERN mode's display lives only in this plugin's own
window. LIVE mode (no file dropped) keeps the older "notes scroll toward
a fixed touch point at the key" behavior, unchanged, since it has no
future notes to lay out statically. See `Source/Visualization/README.md`
for the full LIVE vs PATTERN mechanism.

The view also now sits still while FL Studio's transport is stopped and
only starts scrolling once you press Play, matching FL Studio's own Piano
Roll behavior -- see `Source/Visualization/README.md` for the mechanism
(a hostIsPlaying flag threaded from `AudioPlayHead` down into
`MidiInputHandler`'s clock) and its one known edge case (notes played
while stopped stack up until Play is pressed, since the clock isn't
advancing to space them out).

Backgrounds: any .gif or .mp4 files dropped into `Assets/Backgrounds/`
(compiled in as binary data on the next `cmake -B build` + rebuild --
.mp4 requires `ffmpeg` on the build machine; see that folder's README.md
for both formats and their constraints). Per explicit user request, the
built-in gradient is no longer in the selectable list -- only the
backgrounds actually added to `Assets/Backgrounds/` show up; the gradient
still exists internally as an emergency fallback if a real background
ever fails to load, or if the list is empty. Switch between them with the
two buttons together at the top-LEFT corner of the toolbar ("<" / ">"),
next to the current background's name. Zoom: the toolbar's "V-"/"V+"
buttons (now a bit larger, per explicit request) change how much pitch
range is visible; "H-"/"H+" change how many seconds are visible (LIVE
mode) or how much of the loaded pattern is visible (PATTERN mode, 1.0 =
the whole pattern fits the window) -- both change what's shown, not the
window size (the window itself was already resizable since Phase 1). A
thin vertical slider on the right edge of the piano roll pans the visible
pitch range up/down across the full 88 keys (drag up for higher notes,
down for lower) -- independent of the zoom level, which only controls how
wide that window is.

Note-touch animation: the instant a note-on is captured, that key flashes
and a brief glow blooms out from it, coloured to match whichever
background is currently selected (a fixed colour for the built-in
gradient, a cached sampled colour for GIF/video backgrounds -- see
`VisualEffect::getAccentColour()` in `Source/Effects/README.md`). Capped
at 20 concurrent animations and ~0.4s each, so it stays cheap regardless
of how much MIDI is coming in -- see `Source/Visualization/README.md` for
the exact mechanism.

Verified build output: `RIO LEYVA x NOAH MEJIA VST.vst3` (VST3) and `RIO LEYVA x NOAH MEJIA VST.app`
(Standalone), both ad-hoc signed and auto-installed to
`~/Library/Audio/Plug-Ins/VST3/RIO LEYVA x NOAH MEJIA VST.vst3`.

## Writing melodies (oneshot sample libraries)

The plugin is no longer silent. It can now play back a single selectable
oneshot sample, re-pitched per note -- so it works as a real melodic
instrument, not just a MIDI monitor -- fed by two sources:

- **Live MIDI** -- a MIDI keyboard, or notes drawn in FL Studio's own
  Channel Rack Piano Roll, is now audible as well as visualized.
- **A melody you draw directly in the plugin.** Click the **"Write
  Melody"** toolbar toggle (next to the "27wav" brand label) to switch the
  piano roll into an editable mode: click empty space to add a note (drag
  before releasing to set its length, like FL Studio's own paint tool),
  drag an existing note's body to move it, drag its right edge to resize
  it, and right-click (or double-click) a note to delete it. The drawn
  melody loops (4 bars by default) in sync with the host's transport --
  play/stop, and the host's own loop, both apply to it directly, the same
  way PATTERN mode already keeps a dropped MIDI file in sync (see
  `Source/Visualization/README.md`) -- and is saved with the host project.

**Sound comes from oneshot sample libraries**, picked via the **"Library"**
button at the top-centre of the toolbar, with two small **prev/next
arrows** immediately to its right for stepping to the previous/next
oneshot without opening the menu. Drop audio files anywhere under
`~/Music/27wav rioleyva & noahmejia banks/` (see `Assets/Samples/README.md`) -- read
live from disk rather than compiled into the plugin, since real packs run
into the gigabytes. The Library button's popup menu mirrors that folder
tree **exactly**, at whatever nesting depth it has -- a subfolder becomes a
nested submenu, however many levels deep -- and picks up newly dropped
files the moment you open it or use the arrows, no rebuild or plugin
reload needed. Picking a sound (from the menu or the arrows) makes it the
plugin's current instrument voice and immediately mutes whatever oneshot
was still ringing from the previous one. All Rio Leyva pack oneshots are
currently assumed tuned to MIDI note 60 (middle C) -- see
`Source/Audio/README.md`'s `middleCRootNote` -- there's no per-sample root
note override yet.

A triggered sample now stops as soon as its note-off arrives -- release
the MIDI key, or reach the end of a drawn note -- the same way a normal
sampler instrument works, with a short release fade rather than an
instant cut so releasing doesn't click. This applies to every sound,
whether played live or drawn on the piano roll. Picking a different
sound from the Library button (or its prev/next arrows) also immediately
mutes whatever was still sounding from the previous one (see
`Source/Audio/README.md` for both mechanisms).

No sample libraries are bundled yet -- the Library button shows a "no
libraries yet" placeholder until at least one is added under
`~/Music/27wav rioleyva & noahmejia banks/` (the plugin creates that folder
automatically on first load).

## Architecture

MIDI handling, note storage, playback sync, rendering, melody authoring,
and sample playback are kept in separate modules so any one of them can be
reworked without touching the others:

```
Source/
  PluginProcessor/   JUCE AudioProcessor — merges live MIDI + the drawn melody, drives the sampler
  PluginEditor/       Thin container/controller for the GUI
  MIDI/               (Phase 2+) MidiInputHandler, MidiEventParser, NoteEvent, NoteStorage, MidiFilePattern
  Timeline/           (Phase 3-4) TimelineState, PlaybackState
  Visualization/      (Phase 5-6) PianoRollRenderer, NoteRenderer, PlayheadRenderer — now also Compose-mode note editing
  Effects/            (Phase 5+) VisualEffect interface + future effects
  Theme/              (Phase 8) ThemeManager + theme presets
  Utils/              Shared helpers (pitch <-> note name/octave/position)
  Composition/        The user's drawn melody (ComposedMelody) + MelodySequencer (beats -> sample-accurate triggers)
  Audio/              SamplerEngine (oneshot sample playback, stops on note-off) + EffectsChain (Reverb/Delay/Chorus/Phaser/Flanger/Distortion/High Pass/Low Pass + Volume)
  Library/            SampleLibraryManager — enumerates libraries read live from ~/Music/27wav rioleyva & noahmejia banks/
```

## Building (macOS)

Requires Xcode (or the Xcode Command Line Tools) and CMake 3.22+.

JUCE resolution order (see `CMakeLists.txt`):
1. If `ThirdParty/JUCE/CMakeLists.txt` exists, that local checkout is
   used directly (`add_subdirectory`) — no network needed. This repo
   currently builds against a local JUCE 7.0.12 checkout placed there
   manually (JUCE is not vendored in git — see `.gitignore`).
2. Otherwise JUCE 7.0.12 is fetched from GitHub via `FetchContent` on
   first configure (needs a working internet connection).

```bash
cmake -B build -G Xcode
cmake --build build --config Debug
```

or, for a plain Makefile build (no full Xcode.app required — Xcode
Command Line Tools are enough):

```bash
cmake -B build
cmake --build build --config Debug
```

Whenever a source file is added to or removed from `CMakeLists.txt`'s
`target_sources()` list (as happened when `MidiFilePattern.cpp` was
added), a plain `cmake --build` isn't enough -- re-run the `cmake -B
build [...]` configure step first (same command as above), then build,
so the generated project actually picks up the new file list.

### Known JUCE 7.0.12 / macOS 15 SDK fix

JUCE 7.0.12 predates the macOS 15 SDK. `CGWindowListCreateImage` (an
internal helper JUCE uses for window drag-image snapshots — unrelated
to this plugin's own functionality) is marked fully `unavailable` in
that SDK, which is a hard compile error, not a warning. This is patched
directly in the vendored copy at
`ThirdParty/JUCE/modules/juce_gui_basics/native/juce_Windowing_mac.mm`:
on SDK macOS 15+, the affected function becomes a no-op instead of
calling the removed API. A `.bak` of the original JUCE file sits next
to it. If you replace `ThirdParty/JUCE` with a fresh checkout, re-apply
this patch (or upgrade to a newer JUCE release that fixes it upstream)
before building on macOS 15 SDKs.

`COPY_PLUGIN_AFTER_BUILD` is enabled, so a successful build copies the
VST3 straight to `~/Library/Audio/Plug-Ins/VST3/RIO LEYVA x NOAH MEJIA VST.vst3` and the
Standalone app to `~/Applications` — no manual copy step needed. Restart
FL Studio (or re-scan the VST3 folder in FL Studio's plugin manager)
after the first build so it picks up the new plugin.

The Standalone target (`RIO LEYVA x NOAH MEJIA VST.app`, also built above) is the fastest
way to sanity-check the GUI without opening FL Studio at all.

## Distribution: installer for other users

To let other people install the plugin (rather than building it themselves),
`Scripts/build_installer.sh` builds a Release universal binary and packages
it into a `.pkg` installer that places `RIO LEYVA x NOAH MEJIA VST.vst3` in
`/Library/Audio/Plug-Ins/VST3/` for every user on the target Mac:

```bash
chmod +x Scripts/build_installer.sh   # first time only
./Scripts/build_installer.sh
```

Output: `Packaging/build/RIO LEYVA x NOAH MEJIA VST Installer.pkg`. This installer is
**unsigned** (no Apple Developer ID configured) -- see `Packaging/README.md`
for exactly what that means for recipients (a one-time Gatekeeper warning
they need to click through) and how to remove it later if a paid Developer
ID is ever added.

## FL Studio MIDI routing

Confirmed empirically during Phase 2 testing:

- **Mixer FX slot (e.g. Master):** does NOT work. FL Studio's Mixer only
  routes audio between inserts -- it never delivers note MIDI to a plugin
  sitting in an FX slot, regardless of what the plugin declares.
- **Channel Rack "Select generator plugin" picker, while declared
  `IS_MIDI_EFFECT`:** did NOT show the plugin at all (0 results). FL
  Studio's generator picker only lists plugins in the Instrument/Synth
  VST3 category; a MIDI-effect-category plugin is only offered in Mixer/FX
  pickers, which brings you back to the point above -- a dead end for a
  MIDI-only VST3 in FL Studio.

Fix applied: the plugin is now declared `IS_SYNTH TRUE` with a silent
stereo output bus (`IS_MIDI_EFFECT` is now `FALSE`) purely so FL Studio's
plugin database treats it as an instrument and lists it in the Channel
Rack generator picker. It still performs no real audio synthesis --
`processBlock()` clears the output buffer unconditionally.

**To test MIDI reception in FL Studio:**
1. Open the **Channel Rack** → **+** → "Select generator plugin".
2. Search "27wav" -- it should now appear (after rebuilding with the
   `IS_SYNTH` change).
3. Add it, draw notes in that channel's Piano Roll (or play live from a
   MIDI keyboard routed to that channel), press play.
4. Open the plugin's GUI: notes should appear directly in the piano-roll
   visualization as they play (there is no diagnostic text/counter in the
   UI any more -- that was removed per explicit user request; verification
   is purely visual now).
