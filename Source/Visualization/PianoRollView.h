#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../MIDI/MidiInputHandler.h"
#include "../MIDI/NoteEvent.h"
#include "../MIDI/MidiFilePattern.h"
#include "../Composition/ComposedMelody.h"
#include "../Effects/BackgroundManager.h"
#include "../Audio/EffectsChain.h"
#include <vector>

namespace w27
{

/**
    Real-time MIDI visualization AND melody authoring: a piano-key strip
    plus note bars, similar in spirit to FL Studio's own Piano Roll, backed
    by a selectable animated background (Effects/BackgroundManager).

    Three modes now, chosen by (in priority order) the "Write Melody"
    toggle, then automatically by whether a MIDI file has been dropped:

    - COMPOSE mode (setComposeModeEnabled (true), wired to the toolbar's
      "Write Melody" button): shows and edits the user's own hand-drawn
      ComposedMelody (Composition/ComposedMelody.h) instead of any MIDI
      source. Click empty space in the note area to add a note there (drag
      before releasing to set its length, like FL Studio's paint tool);
      drag an existing note's body to move it (time + pitch), or its right
      edge to resize it; right-click or double-click a note to delete it.
      Takes over the WHOLE view when active -- see paint()'s early return
      -- rather than overlaying LIVE/PATTERN mode. This is the one mode
      that actually produces audible sound: Composition/MelodySequencer
      (driven from PluginProcessor::processBlock()) turns these same notes
      into real note-on events for Audio/SamplerEngine, completely
      independently of this view's own painting/editing code.

    - LIVE mode (default when Compose mode is off and no MIDI file has
      been dropped): notes come from MidiInputHandler's NoteStorage, i.e.
      real MIDI as the host streams it. A live-capture plugin has no way
      to show a note before it's actually received, so this mode keeps a
      "scrolling" model: notes appear at the instant they start, touching
      the key (at the fixed vertical line at the note area's left edge),
      then age away to the RIGHT as time passes.

    - PATTERN mode (Compose mode off, a standard MIDI file has been
      dropped onto this view -- see the juce::FileDragAndDropTarget
      overrides): the WHOLE file's notes are known up front, so this
      renders like a real DAW piano roll: notes sit at FIXED horizontal
      positions (paintPatternNotes()), and a moving PLAYHEAD sweeps across
      them as playback advances. See the detailed mechanism description
      further down (unchanged from before Compose mode was added).

    Either way, "now" only advances while the host transport is actually
    playing. See Visualization/README.md for the full LIVE/PATTERN
    mechanism (host-position-driven, fmod-wrapped, no accumulated clock)
    and Composition/README.md for how Compose mode's own musical-time
    "now" (driven by MidiInputHandler::getHostPpqPosition()/getHostBpm())
    mirrors that same approach for beats instead of seconds.

    Reads only NoteEvent/MelodyNote data -- never touches juce::MidiMessage
    or MidiBuffer directly, keeping this swappable independently of the
    MIDI engine, per the project's architecture rules. (MidiFilePattern is
    the one exception allowed to parse a MIDI file directly, exactly as
    MidiEventParser is the one exception for live MidiBuffers -- both live
    under MIDI/.)

    Layout: the piano-key strip sits at the LEFT edge, a thin vertical pan
    slider sits at the RIGHT edge, and a bar-ruler strip runs along the
    very TOP, above both; the note area fills what's left -- shared by all
    three modes.
*/
class PianoRollView : public juce::Component,
                       public juce::FileDragAndDropTarget,
                       private juce::Timer
{
public:
    PianoRollView (const MidiInputHandler& midiInputHandlerToUse, ComposedMelody& composedMelodyToUse,
                   const EffectsChain& effectsChainToUse);
    ~PianoRollView() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    void zoomInVertical();
    void zoomOutVertical();
    void zoomInHorizontal();
    void zoomOutHorizontal();

    void selectNextBackground();
    void selectPreviousBackground();
    juce::String getCurrentBackgroundName() const;

    // Empty when no MIDI file has been dropped yet (i.e. still in LIVE mode).
    juce::String getLoadedPatternName() const;

    // Switches between Compose mode (click/drag note editing over the
    // user's own ComposedMelody) and the ordinary LIVE/PATTERN display.
    // See the class doc comment above.
    void setComposeModeEnabled (bool shouldBeEnabled);
    bool isComposeModeEnabled() const noexcept { return composeModeEnabled; }

    // juce::FileDragAndDropTarget
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;
    void fileDragEnter (const juce::StringArray& files, int x, int y) override;
    void fileDragExit (const juce::StringArray& files) override;

private:
    void timerCallback() override;

    juce::Rectangle<float> getNoteAreaBounds() const;
    juce::Rectangle<float> getKeyboardAreaBounds() const;
    void getVisibleNoteRange (int& lowNoteOut, int& highNoteOut) const noexcept;
    float yForNote (int noteNumber, juce::Rectangle<float> noteArea, int lowNote, int highNote) const noexcept;
    double getVisibleWindowSeconds() const noexcept;

    // PATTERN mode's equivalent of getVisibleWindowSeconds(): the loaded
    // pattern's own duration divided by horizontalZoom (1.0 = whole pattern
    // fits across the note area width), rather than the fixed
    // baseVisibleWindowSeconds LIVE mode uses.
    double getPatternVisibleWindowSeconds() const noexcept;

    void paintLiveNotes (juce::Graphics& g, juce::Rectangle<float> noteArea, int lowNote, int highNote,
                          double now, double visibleWindowSeconds, const std::vector<NoteEvent>& snapshot);

    // Static layout -- notes sit at fixed positions given by their own
    // start/end time (no relation to `now` beyond the isPlayingNow
    // highlight); the moving playhead is drawn separately, in paint().
    void paintPatternNotes (juce::Graphics& g, juce::Rectangle<float> noteArea, int lowNote, int highNote,
                             double now, double visibleWindowSeconds);

    // Bar ruler along the top of the note area: numbered bar divisions
    // (tick + number, confined to the ruler strip), spaced using the loaded
    // pattern's detected tempo/time signature. Draws just the blank strip
    // fill if no pattern is loaded.
    void paintBarRuler (juce::Graphics& g, juce::Rectangle<float> noteArea, double visibleWindowSeconds) const;

    // Full-height vertical grid lines through the note area: brighter at
    // each bar boundary, dimmer subdividing each bar into beats. PATTERN
    // mode only (needs a known tempo/time signature).

    // PATTERN mode's "now", driven by the host's actual transport position
    // when available (with loop-restart detection), falling back to this
    // plugin's own clock otherwise. See the class doc comment above and the
    // .cpp for the full mechanism.
    double getPatternRelativeNow();

    // Detects newly-reached notes since the last call and spawns capped,
    // short-lived touch animations for them; also expires old ones. `now`
    // and each note's start/end times must be in the same clock (either
    // both absolute-live or both pattern-relative).
    void updateTouchAnimations (const std::vector<NoteEvent>& sourceNotes, double now);
    void drawTouchAnimations (juce::Graphics& g, juce::Rectangle<float> keyboardArea, juce::Rectangle<float> noteArea,
                               int lowNote, int highNote, juce::Colour accentColour, double now) const;

    // -- Compose mode ---------------------------------------------------
    // All timed in BEATS (see MelodyNote.h), not seconds -- the composed
    // melody follows the host's live tempo rather than one fixed BPM.

    // 1.0 zoom = the whole loop fits across the note area's width, same
    // "1.0 = the whole thing" convention getPatternVisibleWindowSeconds()
    // uses for a dropped file's duration.
    double getComposeVisibleWindowBeats() const noexcept;

    // Loop-relative "now" in beats, driven by MidiInputHandler's musical
    // position (see Composition/README.md) -- 0.0, with the playhead not
    // drawn at all, if the host doesn't report ppqPosition/tempo.
    double getComposeRelativeNowBeats();

    void paintComposeNotes (juce::Graphics& g, juce::Rectangle<float> noteArea, int lowNote, int highNote,
                             double nowBeats, double visibleWindowBeats);
    void paintComposeBarRuler (juce::Graphics& g, juce::Rectangle<float> noteArea, double visibleWindowBeats) const;

    float xForBeat (double beat, juce::Rectangle<float> noteArea, double visibleWindowBeats) const noexcept;
    double beatForX (float x, juce::Rectangle<float> noteArea, double visibleWindowBeats) const noexcept;
    int noteForY (float y, juce::Rectangle<float> noteArea, int lowNote, int highNote) const noexcept;
    double snapBeat (double beat) const noexcept;

    enum class DragMode { none, movingNote, resizingNote, creatingNote };
    DragMode currentDragMode = DragMode::none;
    size_t draggedNoteIndex = (size_t) 0;
    double dragStartBeatOffset = 0.0; // beat offset between the click point and the dragged note's own start, for natural-feeling moves

    const MidiInputHandler& midiInputHandler;
    ComposedMelody& composedMelody;
    const EffectsChain& effectsChain;

    bool composeModeEnabled = false;

    BackgroundManager backgroundManager;

    juce::Slider verticalPanSlider;

    float verticalZoom = 1.0f;   // 1.0 = all 88 keys visible
    float horizontalZoom = 1.0f; // 1.0 = default time window (LIVE) / whole pattern or loop (PATTERN/Compose)

    // Which pitch the visible vertical window is centred on -- dragged via
    // verticalPanSlider. A plain double (not a zoom-fixed constant) because,
    // unlike v1, this is now user-adjustable at runtime.
    double verticalPanCenterNote = 60.0; // starts at middle C

    // PATTERN mode state -- empty/invalid means "still in LIVE mode".
    MidiFilePattern loadedPattern;

    // Fallback-only: MidiInputHandler::getCurrentTimeSeconds() value
    // captured at the moment the current pattern was dropped, used as the
    // pattern-relative "now" origin only on hosts that don't expose a
    // transport position at all (see getPatternRelativeNow()).
    double patternEpochSeconds = 0.0;

    // Tracks pattern-relative "now" across frames purely to detect when it
    // has wrapped back toward a lower value -- a loop repeating, or the
    // user scrubbing/rewinding the host's own position backward (see
    // getPatternRelativeNow()) -- so touch-animation state can be reset and
    // notes flash again on every repeat/seek, not just the first pass.
    double previousPatternNow = 0.0;
    bool patternNowInitialized = false;

    bool isDraggingFileOver = false; // for the drop-zone highlight while a drag hovers

    struct TouchAnimation
    {
        int noteNumber = 0;
        double startTimeSeconds = 0.0;
    };

    std::vector<TouchAnimation> touchAnimations;

    // Initialised to "now" at construction (not 0) so re-opening the editor
    // while notes already exist in NoteStorage's rolling history doesn't
    // spawn a burst of animations for every already-past note-on. Reset to
    // -1 whenever a new pattern is loaded, since pattern-relative "now"
    // restarts at 0 for each drop and pattern note timestamps are never
    // negative.
    double lastProcessedNoteStartTime = 0.0;

    static constexpr double touchAnimationDurationSeconds = 0.4;
    static constexpr size_t maxTouchAnimations = 20;

    static constexpr float keyboardWidth = 44.0f;
    static constexpr float verticalPanSliderWidth = 18.0f;
    static constexpr float barRulerHeight = 20.0f;
    static constexpr double baseVisibleWindowSeconds = 4.0;
    static constexpr float minVerticalZoom = 1.0f;
    static constexpr float maxVerticalZoom = 4.0f;
    static constexpr float minHorizontalZoom = 0.25f;
    static constexpr float maxHorizontalZoom = 4.0f;

    static constexpr double defaultNoteLengthBeats = 0.5;
    static constexpr double minNoteLengthBeats = 1.0 / 16.0;
    static constexpr double gridResolutionBeats = 0.25; // snap resolution: a 16th note, given beat = quarter note
    static constexpr float resizeHandlePixels = 6.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PianoRollView)
};

} // namespace w27
