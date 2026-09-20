- VisualEffect.h -- the interface every background implements:
  render(Graphics&, bounds, timeSeconds), plus getAccentColour() -- a single
  representative colour for the background, used by PianoRollView's
  note-touch animation so it visually matches whichever background is
  selected. Defaults to the plugin's standard accent colour; visual code
  depends only on this interface, never a concrete effect class.
- AnimatedGradientBackground.h/.cpp -- a slow, softly drifting dual-glow
  gradient. No longer a selectable entry in BackgroundManager's list (per
  explicit user request -- only backgrounds actually added to
  Assets/Backgrounds/ are selectable); still used internally as an
  emergency fallback if a real background fails to load or the list is
  empty.
- FrameSequenceBackground.h/.cpp -- plays back any fixed sequence of
  juce::Image frames by elapsed time, with a "cover" fit + dark dimming
  overlay so the piano roll stays legible. Shared by both real background
  sources below -- it doesn't know or care whether the frames came from a
  GIF or a video. getAccentColour() samples a small fixed grid of pixels
  from the first frame once, caches the (nudged-up) average, and returns
  that from then on -- cheap enough to call every repaint without ever
  re-sampling.
- GifDecoder.h/.cpp -- self-contained GIF87a/89a + LZW decoder (no external
  dependencies -- network access to fetch a third-party image library was
  unavailable from both this machine and the build sandbox, so this is a
  from-scratch implementation). Pure data in/out, no JUCE dependency.
- GifBackground.h/.cpp -- createGifBackground(): converts a
  GifDecoder::DecodedGif into a FrameSequenceBackground (the only place
  GifDecoder's raw RGBA buffers become juce::Image objects).
- BackgroundManager.h/.cpp -- the switchable list of backgrounds: every
  .gif embedded from Assets/Backgrounds/, and every .mp4 there too (CMake
  pre-extracts video frames to JPEGs via ffmpeg at build time -- there's no
  video codec here, see Assets/Backgrounds/README.md for why and its
  constraints). Everything decodes/loads lazily on first selection and is
  cached after that. render()/getCurrentAccentColour() fall back to a
  lazily-created AnimatedGradientBackground instance if the list is empty
  (no files added yet) rather than leaving the window blank.
