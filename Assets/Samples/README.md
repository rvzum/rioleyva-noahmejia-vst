Sample packs are **not** stored here anymore, and this folder is no longer
read by the build at all. Real Rio Leyva packs run into the gigabytes, and
compiling that much data into the plugin binary (the approach still used
for Assets/Backgrounds/'s much smaller GIFs/frames) made `cmake --build`
impractically slow and would have produced a multi-gigabyte plugin bundle.

Instead, drop your sample packs into this fixed folder on disk, which the
plugin reads live at runtime -- no reconfigure, no rebuild:

    ~/Music/27wav rioleyva & noahmejia banks/

The plugin creates that folder automatically the first time it loads if it
doesn't already exist. Every time you open the plugin's top-centre
"Select Sound..." button, it re-scans that folder from scratch, so files
you just dropped in show up immediately.

## Folder layout

Per explicit user request, the plugin's "Library" popup menu mirrors
whatever is under `~/Music/27wav rioleyva & noahmejia banks/` **exactly**, at
whatever nesting depth it actually has -- no flattening, no regrouping.
Every subfolder becomes a nested submenu (however many levels deep, mixed
freely with plain sound entries at the same level), and every audio file
becomes one selectable **oneshot sound**, right where it sits in the tree:

    ~/Music/27wav rioleyva & noahmejia banks/
      Rio Leyva Melodics/                    <- submenu "Rio Leyva Melodics"
        Pluck A.wav                             <- sound "Pluck A"
        Pluck B.wav                             <- sound "Pluck B"
      Rio Leyva Analog Kit/                  <- submenu "Rio Leyva Analog Kit"
        Juno 60/                                 <- nested submenu "Juno 60"
          Juno Stab.wav                             <- sound "Juno Stab"
        Jupiter 8/                                <- nested submenu "Jupiter 8"
          Jupiter Pad.wav                           <- sound "Jupiter Pad"

A folder that contains no audio file anywhere beneath it (recursively) is
left out entirely, so you never get an empty submenu -- but an in-between
wrapper folder like "Rio Leyva Analog Kit" above still shows up as its own
submenu (it's not skipped the way an early version of this feature did;
its own children are what determine whether it's shown, at any depth).

The toolbar's two small arrows next to "Select Sound..." step to the
previous/next sound without opening this menu at all -- in the same
depth-first order the menu itself lists things in, regardless of which
submenu something is nested under.

Two different folders anywhere under the runtime root that happen to share
the exact same folder name would end up as two identically-named
submenus -- harmless (they stay visually separate, unlike the old
one-flat-library-per-name scheme where they'd have merged), but rename one
if it's ever confusing.

## Supported formats

`.wav`, `.aif`, `.aiff`, `.mp3` (case-insensitive extension). Anything else
in a library folder is ignored. MP3 decoding relies on macOS's own codecs
(via JUCE's CoreAudioFormat) and only works because this project is
macOS-only -- it would silently fail to load on a hypothetical Windows/Linux
build.

## What "oneshot" means here

Whichever single sound you pick from the library button becomes the
plugin's one active instrument voice: it's re-pitched (sped up/slowed down,
the same way a sampler always has) to match whatever note is playing --
live MIDI, or the melody you draw directly on the piano roll (see
Source/Composition/README.md) -- so one oneshot recording becomes a
playable melodic instrument. Every pack is currently assumed to be tuned so
its natural/recorded pitch sits at MIDI note 60 (middle C) -- see
Source/Audio/README.md's `middleCRootNote` -- there's no per-sample root
note override yet. If a specific Rio Leyva pack is tuned to a different
note, everything drawn on it will sound correspondingly transposed until
that's added.

A triggered sound stops as soon as its note-off arrives -- the key is
released, or a drawn note reaches its own end -- same as an ordinary
sampler instrument, with a short release fade rather than an instant cut.
"Oneshot" here describes the SOURCE material (a single hit/pluck/stab, not
a multi-sampled sustained instrument), not the playback envelope -- that
used to also mean "always plays to the end regardless of note-off," but
that behavior was reversed project-wide per explicit user request. See
Source/Audio/README.md for the current mechanism.

## Why disk instead of embedding

`Source/Library/SampleLibraryManager` walks the runtime folder directly
with `juce::File` and hands `PluginProcessor::selectSample()` an actual
file path; `Source/Audio/SamplerEngine::loadSampleFromFile()` reads it
straight off disk via `juce::AudioFormatManager`, the same reader machinery
that used to read from an in-memory embedded buffer -- just pointed at a
real file instead. There's no manifest, no build-time staging, and no
practical limit on how large a pack can be; the only cost is a
(cheap, non-audio) directory walk each time the library menu opens, versus
the old approach's ever-growing compile time and binary size as packs got
bigger.

One implication: this project folder itself no longer needs to contain
your sample audio at all (moving 1.7GB+ of packs in here would just bloat
your git history/backups for a folder the build ignores) -- keep the actual
audio only in `~/Music/27wav rioleyva & noahmejia banks/`.
