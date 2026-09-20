Drop background files directly into this folder, then reconfigure and rebuild:

    cd ~/"27wav rioleyva x noahmejia VST"
    cmake -B build
    cmake --build build --config Debug

Every file that lands here shows up automatically in the in-plugin
background switcher (the "<" / ">" buttons + name label in the top toolbar)
after the next rebuild.

## .gif files
Compiled straight into the plugin as binary data. Animated GIFs play back at
their original per-frame timing; a static GIF also works fine, it just won't
move.

**Any .gif should work**, including ones that weren't hand-picked for this
project -- large canvas sizes, long/many-frame animations, oversized or
slightly malformed files. The decoder (`Source/Effects/GifDecoder.cpp`)
caps how much of a single GIF it decodes based on a fixed ~256 MB memory
budget for the whole animation: an oversized or extremely long GIF just has
its later frames dropped (the loop gets shorter) rather than risking a
multi-gigabyte allocation or freezing the plugin UI while it decodes. A file
that isn't actually a valid GIF (wrong extension, corrupted, etc.) is
detected and skipped -- the plugin falls back to the built-in animated
gradient for that slot instead of crashing.

## .mp4 files
There's no video codec in the plugin (writing one from scratch is a much
bigger undertaking than the from-scratch GIF decoder in
`Source/Effects/GifDecoder.cpp` -- H.264/HEVC are vastly more complex than
GIF's simple LZW scheme). Instead, **ffmpeg must be installed** on this Mac:

    brew install ffmpeg

When present, CMake automatically extracts each .mp4 into a sequence of JPEG
frames (12fps, capped at 480px wide, first 15 seconds only, moderate JPEG
quality) the first time you configure after adding it, and embeds those
frames exactly like a GIF's frames -- same switcher, same playback code.
Constraints, and why:

- **First 15 seconds only, then it loops.** Keeps the embedded binary a
  reasonable size for a background loop; a longer clip just needs to be
  pre-trimmed if you want to use more of it.
- **12 fps, max 480px wide.** A background doesn't need full video
  fidelity, and this keeps both the plugin's binary size and load time
  reasonable. Change the `-vf fps=...,scale=...` and `-q:v` values in
  CMakeLists.txt's "Background media" section if you want a different
  quality/size tradeoff.
- **Extraction is cached in the build folder, but content changes are
  detected automatically.** CMake stores the source .mp4's own last-modified
  time next to its extracted frames (`build/mp4_frames/<name>/.source_mtime`)
  and compares it on every reconfigure. If you replace an .mp4 with a new
  version under the same filename, the next `cmake -B build` notices the
  timestamp changed, clears the stale frames, and re-extracts automatically
  -- no manual deletion needed. (A plain `file(GLOB ...)` alone can't detect
  this: it only reacts to files being added or removed, never to an existing
  match's content changing, which is what this marker file is for.)
- If `ffmpeg` isn't installed, CMake prints a warning and simply skips any
  .mp4 files (the build still succeeds) -- install ffmpeg and re-run
  `cmake -B build` once it's available.

## General
- `cmake -B build` (not just `cmake --build`) is what re-scans this folder
  for new/removed files -- if a file you just added doesn't show up, run
  that first.
- There's no hard size limit on how many backgrounds you add, but more/larger
  ones make the plugin binary bigger and, for GIFs, take a moment longer to
  decode the first time that background is selected (results are cached
  after that).
- A file that fails to load for any reason (bad GIF data, ffmpeg failing on
  an .mp4) is simply skipped at runtime, falling back to the built-in
  gradient for that slot -- it won't crash the plugin or block the other
  backgrounds from working.
