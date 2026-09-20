- SampleLibraryManager.h/.cpp -- reads a fixed folder on disk,
  `~/Music/27wav rioleyva & noahmejia banks/`, and mirrors it into two forms (see
  Assets/Samples/README.md for the on-disk convention and why samples live
  on disk rather than embedded in the plugin binary -- real packs run into
  the gigabytes):

  - `getRootNode()` -- the actual folder tree, exactly as found on disk,
    at whatever nesting depth it has. PluginEditor walks this directly to
    build a nested popup menu (a subfolder becomes a nested submenu,
    however deep) -- per explicit user request there is no flattening or
    regrouping into "one library per folder"; the plugin's menu simply IS
    the folder tree. A branch with no audio file anywhere beneath it is
    pruned so empty submenus never show up.
  - `getNumSamples()`/`getSampleAt()` -- the same samples as one flat,
    depth-first-ordered list, used by the toolbar's prev/next oneshot
    arrows (PluginEditor::stepSample()) to step through "the next sound"
    without caring which submenu it's nested under.

  rescan() re-walks the folder from scratch and is called once at
  construction and again every time PluginEditor's library menu (or its
  prev/next arrows) is used, so newly dropped files show up with no
  reload/rebuild needed -- no manifest file to keep in sync either way.

  PluginProcessor::selectSample() takes a flat index and gets back a
  `Sample` (display name, stable `relativePath` used for save/restore, and
  the actual file path), handing that path to
  Audio/SamplerEngine::loadSampleFromFile().

  Empty until `~/Music/27wav rioleyva & noahmejia banks/` actually has files in it --
  PluginEditor shows a "no libraries yet" placeholder pointing at that
  folder in that case.

Nothing here plays audio or knows about MIDI/notes/melodies -- purely
"what's on disk and how it's organized". See Source/Audio/README.md for
actual playback, and Source/Composition/README.md for how a melody gets
turned into the note-on events that trigger it.
