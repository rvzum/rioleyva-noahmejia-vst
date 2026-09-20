#include "PianoRollView.h"
#include "../Theme/ThemeColors.h"
#include "../Utils/PitchUtils.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace w27
{

PianoRollView::PianoRollView (const MidiInputHandler& midiInputHandlerToUse, ComposedMelody& composedMelodyToUse,
                               const EffectsChain& effectsChainToUse)
    : midiInputHandler (midiInputHandlerToUse),
      composedMelody (composedMelodyToUse),
      effectsChain (effectsChainToUse),
      lastProcessedNoteStartTime (midiInputHandlerToUse.getCurrentTimeSeconds())
{
    // Vertical pan slider: a thin fader-style scrollbar. JUCE's default
    // LinearVertical direction (up = higher value) already matches what we
    // want -- drag up to see higher notes, drag down for lower ones.
    verticalPanSlider.setSliderStyle (juce::Slider::LinearVertical);
    verticalPanSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    verticalPanSlider.setRange ((double) PitchUtils::minPianoNote, (double) PitchUtils::maxPianoNote, 1.0);
    verticalPanSlider.setValue (verticalPanCenterNote, juce::dontSendNotification);
    // Muted, minimal styling -- closer to FL Studio's own plain scrollbar
    // than a brightly-coloured control: transparent background (blends with
    // whatever's behind it instead of a solid panel box), a dim track, and
    // a neutral grey thumb rather than the green accent highlight.
    verticalPanSlider.setColour (juce::Slider::backgroundColourId, juce::Colours::transparentBlack);
    verticalPanSlider.setColour (juce::Slider::trackColourId, Theme::gridLine);
    verticalPanSlider.setColour (juce::Slider::thumbColourId, Theme::brandSubtitle);
    verticalPanSlider.onValueChange = [this]
    {
        verticalPanCenterNote = verticalPanSlider.getValue();
        repaint();
    };
    addAndMakeVisible (verticalPanSlider);

    startTimerHz (30);
}

PianoRollView::~PianoRollView()
{
    stopTimer();
}

juce::Rectangle<float> PianoRollView::getNoteAreaBounds() const
{
    return getLocalBounds().toFloat()
        .withTrimmedLeft (keyboardWidth)
        .withTrimmedRight (verticalPanSliderWidth)
        .withTrimmedTop (barRulerHeight);
}

juce::Rectangle<float> PianoRollView::getKeyboardAreaBounds() const
{
    return getLocalBounds().toFloat().withWidth (keyboardWidth).withTrimmedTop (barRulerHeight);
}

void PianoRollView::getVisibleNoteRange (int& lowNoteOut, int& highNoteOut) const noexcept
{
    const int count = juce::jmax (12, (int) std::lround (PitchUtils::numPianoKeys / verticalZoom));
    const int centerNote = juce::jlimit (PitchUtils::minPianoNote, PitchUtils::maxPianoNote,
                                          (int) std::lround (verticalPanCenterNote));

    int low = centerNote - count / 2;
    int high = low + count - 1;

    if (low < PitchUtils::minPianoNote)
    {
        high += (PitchUtils::minPianoNote - low);
        low = PitchUtils::minPianoNote;
    }
    if (high > PitchUtils::maxPianoNote)
    {
        low -= (high - PitchUtils::maxPianoNote);
        high = PitchUtils::maxPianoNote;
    }

    lowNoteOut = juce::jmax (PitchUtils::minPianoNote, low);
    highNoteOut = juce::jmin (PitchUtils::maxPianoNote, high);
}

float PianoRollView::yForNote (int noteNumber, juce::Rectangle<float> noteArea, int lowNote, int highNote) const noexcept
{
    const int clamped = juce::jlimit (lowNote, highNote, noteNumber);
    const int range = juce::jmax (1, highNote - lowNote);
    const float fraction = (float) (highNote - clamped) / (float) range;
    return noteArea.getY() + fraction * noteArea.getHeight();
}

double PianoRollView::getVisibleWindowSeconds() const noexcept
{
    return baseVisibleWindowSeconds / (double) horizontalZoom;
}

double PianoRollView::getPatternVisibleWindowSeconds() const noexcept
{
    if (loadedPattern.durationSeconds > 1.0e-6)
        return loadedPattern.durationSeconds / (double) horizontalZoom;

    return getVisibleWindowSeconds(); // fallback -- shouldn't normally happen, isValid() implies notes exist
}

double PianoRollView::getPatternRelativeNow()
{
    if (midiInputHandler.isHostPositionKnown())
    {
        const double hostSeconds = midiInputHandler.getHostPositionSeconds();

        if (loadedPattern.durationSeconds > 1.0e-6)
        {
            double relative = std::fmod (hostSeconds, loadedPattern.durationSeconds);
            if (relative < 0.0)
                relative += loadedPattern.durationSeconds;
            return relative;
        }

        return juce::jmax (0.0, hostSeconds);
    }

    // Fallback for hosts that don't expose a transport position at all
    // (rare -- e.g. some Standalone configurations): behave as before,
    // driven by this plugin's own note-capture clock instead of true host
    // position. No seek/scrub/loop-restart tracking is possible here.
    return midiInputHandler.getCurrentTimeSeconds() - patternEpochSeconds;
}

void PianoRollView::updateTouchAnimations (const std::vector<NoteEvent>& sourceNotes, double now)
{
    double newestStartSeen = lastProcessedNoteStartTime;

    for (const auto& note : sourceNotes)
    {
        if (note.startTimeSeconds <= lastProcessedNoteStartTime)
            continue;
        if (note.startTimeSeconds > now)
            continue;

        newestStartSeen = juce::jmax (newestStartSeen, note.startTimeSeconds);

        if (touchAnimations.size() >= maxTouchAnimations)
            touchAnimations.erase (touchAnimations.begin()); // drop the oldest -- keep this cheap and bounded

        touchAnimations.push_back ({ note.noteNumber, note.startTimeSeconds });
    }

    lastProcessedNoteStartTime = newestStartSeen;

    touchAnimations.erase (std::remove_if (touchAnimations.begin(), touchAnimations.end(),
                                            [now] (const TouchAnimation& anim)
                                            { return (now - anim.startTimeSeconds) >= touchAnimationDurationSeconds; }),
                            touchAnimations.end());
}

void PianoRollView::drawTouchAnimations (juce::Graphics& g, juce::Rectangle<float> keyboardArea, juce::Rectangle<float> noteArea,
                                          int lowNote, int highNote, juce::Colour accentColour, double now) const
{
    if (touchAnimations.empty())
        return;

    const float keyHeight = noteArea.getHeight() / (float) juce::jmax (1, highNote - lowNote + 1);
    const float keyEdgeX = noteArea.getX(); // touch point -- the keyboard sits on the left

    for (const auto& anim : touchAnimations)
    {
        if (anim.noteNumber < lowNote || anim.noteNumber > highNote)
            continue; // scrolled out of the currently-zoomed pitch range

        const double progress = juce::jlimit (0.0, 1.0, (now - anim.startTimeSeconds) / touchAnimationDurationSeconds);
        const float fade = 1.0f - (float) progress;

        if (fade <= 0.0f)
            continue;

        const float cy = yForNote (anim.noteNumber, noteArea, lowNote, highNote);

        juce::Rectangle<float> keyFlashBounds (keyboardArea.getX(), cy - keyHeight * 0.5f,
                                                keyboardArea.getWidth(), keyHeight);
        g.setColour (accentColour.withAlpha (fade * 0.85f));
        g.fillRect (keyFlashBounds);

        const float radius = keyHeight * 0.6f + (float) progress * keyboardWidth * 1.4f;

        juce::ColourGradient gradient (accentColour.withAlpha (fade * 0.5f), keyEdgeX, cy,
                                        accentColour.withAlpha (0.0f), keyEdgeX + radius, cy,
                                        true);
        g.setGradientFill (gradient);
        g.fillEllipse (keyEdgeX - radius, cy - radius, radius * 2.0f, radius * 2.0f);
    }
}

void PianoRollView::paintLiveNotes (juce::Graphics& g, juce::Rectangle<float> noteArea, int lowNote, int highNote,
                                     double now, double visibleWindowSeconds, const std::vector<NoteEvent>& snapshot)
{
    const float keyHeight = noteArea.getHeight() / (float) juce::jmax (1, highNote - lowNote + 1);
    const double windowStart = now - visibleWindowSeconds;

    for (const auto& note : snapshot)
    {
        const double noteEnd = note.isActive() ? now : note.endTimeSeconds;
        if (noteEnd < windowStart)
            continue;

        const double clampedStart = juce::jmax (note.startTimeSeconds, windowStart);

        const float xNear = noteArea.getX() + (float) ((now - noteEnd) / visibleWindowSeconds) * noteArea.getWidth();
        const float xFar  = noteArea.getX() + (float) ((now - clampedStart) / visibleWindowSeconds) * noteArea.getWidth();

        const float y = yForNote (note.noteNumber, noteArea, lowNote, highNote);
        // Taller, more rounded, pure-white bars per explicit user request
        // (Theme::noteFill) -- a thin dark outline keeps them legible over
        // a bright patch of the background video.
        juce::Rectangle<float> barBounds (xNear, y - keyHeight * 0.5f + 0.5f,
                                           juce::jmax (2.0f, xFar - xNear), juce::jmax (3.0f, keyHeight - 1.0f));
        const float cornerRadius = juce::jmin (8.0f, barBounds.getHeight() * 0.5f);

        juce::ColourGradient gradient (Theme::noteFill.withAlpha (0.0f), xFar, 0.0f,
                                        Theme::noteFill.withAlpha (1.0f), xNear, 0.0f,
                                        false);
        g.setGradientFill (gradient);
        g.fillRoundedRectangle (barBounds, cornerRadius);
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.drawRoundedRectangle (barBounds, cornerRadius, 1.0f);
    }
}

void PianoRollView::paintPatternNotes (juce::Graphics& g, juce::Rectangle<float> noteArea, int lowNote, int highNote,
                                        double now, double visibleWindowSeconds)
{
    const float keyHeight = noteArea.getHeight() / (float) juce::jmax (1, highNote - lowNote + 1);

    for (const auto& note : loadedPattern.notes)
    {
        if (note.startTimeSeconds > visibleWindowSeconds)
            continue;

        const double clampedEnd = juce::jmin (note.endTimeSeconds, visibleWindowSeconds);

        const float xStart = noteArea.getX() + (float) (note.startTimeSeconds / visibleWindowSeconds) * noteArea.getWidth();
        const float xEnd   = noteArea.getX() + (float) (clampedEnd            / visibleWindowSeconds) * noteArea.getWidth();

        const float y = yForNote (note.noteNumber, noteArea, lowNote, highNote);
        // Taller, more rounded, pure-white bars per explicit user request
        // (Theme::noteFill) -- the currently-playing note is picked out
        // with a coloured outline instead of a pitch-tinted fill.
        juce::Rectangle<float> barBounds (xStart, y - keyHeight * 0.5f + 0.5f,
                                           juce::jmax (3.0f, xEnd - xStart), juce::jmax (3.0f, keyHeight - 1.0f));
        const float cornerRadius = juce::jmin (7.0f, barBounds.getHeight() * 0.4f);

        const bool isPlayingNow = note.startTimeSeconds <= now && note.endTimeSeconds >= now;

        g.setColour (Theme::noteFill);
        g.fillRoundedRectangle (barBounds, cornerRadius);
        g.setColour (isPlayingNow ? Theme::keyActiveHighlight : juce::Colours::black.withAlpha (0.35f));
        g.drawRoundedRectangle (barBounds, cornerRadius, isPlayingNow ? 2.0f : 1.0f);
    }
}

void PianoRollView::paintBarRuler (juce::Graphics& g, juce::Rectangle<float> noteArea, double visibleWindowSeconds) const
{
    juce::Rectangle<float> rulerArea (noteArea.getX(), 0.0f, noteArea.getWidth(), barRulerHeight);

    g.setColour (Theme::panelOverlay);
    g.fillRect (rulerArea);

    if (! loadedPattern.isValid() || visibleWindowSeconds <= 1.0e-6)
        return;

    const double barSeconds = loadedPattern.getBarDurationSeconds();
    if (barSeconds <= 1.0e-6)
        return;

    const int lastBarIndex = (int) std::ceil (visibleWindowSeconds / barSeconds);

    g.setFont (juce::Font (11.0f));

    for (int bar = 0; bar <= lastBarIndex; ++bar)
    {
        const double barStartSeconds = bar * barSeconds;
        if (barStartSeconds > visibleWindowSeconds)
            break;

        const float x = noteArea.getX() + (float) (barStartSeconds / visibleWindowSeconds) * noteArea.getWidth();

        g.setColour (Theme::gridLine);
        g.drawVerticalLine ((int) x, rulerArea.getBottom() - 6.0f, rulerArea.getBottom());

        g.setColour (Theme::brandSubtitle);
        g.drawText (juce::String (bar + 1), (int) x + 3, 0, 40, (int) barRulerHeight,
                    juce::Justification::centredLeft, false);
    }
}

// -- Compose mode ---------------------------------------------------------

double PianoRollView::getComposeVisibleWindowBeats() const noexcept
{
    const double loopBeats = composedMelody.getLengthBeats();
    return loopBeats > 1.0e-6 ? loopBeats / (double) horizontalZoom : 16.0;
}

double PianoRollView::getComposeRelativeNowBeats()
{
    const double loopBeats = composedMelody.getLengthBeats();
    if (loopBeats <= 1.0e-6)
        return 0.0;

    if (! midiInputHandler.isHostMusicalPositionKnown())
        return 0.0; // no transport/tempo reported -- still fully editable, just no moving playhead

    double relative = std::fmod (midiInputHandler.getHostPpqPosition(), loopBeats);
    if (relative < 0.0)
        relative += loopBeats;
    return relative;
}

float PianoRollView::xForBeat (double beat, juce::Rectangle<float> noteArea, double visibleWindowBeats) const noexcept
{
    if (visibleWindowBeats <= 1.0e-6)
        return noteArea.getX();

    return noteArea.getX() + (float) (beat / visibleWindowBeats) * noteArea.getWidth();
}

double PianoRollView::beatForX (float x, juce::Rectangle<float> noteArea, double visibleWindowBeats) const noexcept
{
    if (noteArea.getWidth() <= 1.0e-6)
        return 0.0;

    return ((double) (x - noteArea.getX()) / (double) noteArea.getWidth()) * visibleWindowBeats;
}

int PianoRollView::noteForY (float y, juce::Rectangle<float> noteArea, int lowNote, int highNote) const noexcept
{
    // Inverse of yForNote(): higher on screen = higher pitch.
    const int range = juce::jmax (1, highNote - lowNote);
    const float fraction = juce::jlimit (0.0f, 1.0f, (y - noteArea.getY()) / juce::jmax (1.0f, noteArea.getHeight()));
    const int note = highNote - (int) std::lround (fraction * (float) range);
    return juce::jlimit (lowNote, highNote, note);
}

double PianoRollView::snapBeat (double beat) const noexcept
{
    return std::round (beat / gridResolutionBeats) * gridResolutionBeats;
}

void PianoRollView::paintComposeNotes (juce::Graphics& g, juce::Rectangle<float> noteArea, int lowNote, int highNote,
                                        double nowBeats, double visibleWindowBeats)
{
    const float keyHeight = noteArea.getHeight() / (float) juce::jmax (1, highNote - lowNote + 1);
    const auto notes = composedMelody.getSnapshot();

    for (const auto& note : notes)
    {
        if (note.startBeat > visibleWindowBeats)
            continue;

        const double clampedEnd = juce::jmin (note.getEndBeat(), visibleWindowBeats);

        const float xStart = xForBeat (note.startBeat, noteArea, visibleWindowBeats);
        const float xEnd   = xForBeat (clampedEnd, noteArea, visibleWindowBeats);

        const float y = yForNote (note.noteNumber, noteArea, lowNote, highNote);
        // Taller, more rounded, pure-white bars per explicit user request
        // (Theme::noteFill) -- the currently-playing note is picked out
        // with a coloured outline instead of a pitch-tinted fill.
        juce::Rectangle<float> barBounds (xStart, y - keyHeight * 0.5f + 0.5f,
                                           juce::jmax (3.0f, xEnd - xStart), juce::jmax (3.0f, keyHeight - 1.0f));
        const float cornerRadius = juce::jmin (7.0f, barBounds.getHeight() * 0.4f);

        const bool isPlayingNow = note.startBeat <= nowBeats && note.getEndBeat() >= nowBeats;

        g.setColour (Theme::noteFill);
        g.fillRoundedRectangle (barBounds, cornerRadius);
        g.setColour (isPlayingNow ? Theme::keyActiveHighlight : juce::Colours::black.withAlpha (0.35f));
        g.drawRoundedRectangle (barBounds, cornerRadius, isPlayingNow ? 2.0f : 1.0f);
    }
}

void PianoRollView::paintComposeBarRuler (juce::Graphics& g, juce::Rectangle<float> noteArea, double visibleWindowBeats) const
{
    juce::Rectangle<float> rulerArea (noteArea.getX(), 0.0f, noteArea.getWidth(), barRulerHeight);
    g.setColour (Theme::panelOverlay);
    g.fillRect (rulerArea);

    if (visibleWindowBeats <= 1.0e-6)
        return;

    const int beatsPerBar = juce::jmax (1, composedMelody.getBeatsPerBar());
    const double barBeats = (double) beatsPerBar;
    const int lastBarIndex = (int) std::ceil (visibleWindowBeats / barBeats);

    g.setFont (juce::Font (11.0f));

    for (int bar = 0; bar <= lastBarIndex; ++bar)
    {
        const double barStartBeats = bar * barBeats;
        if (barStartBeats > visibleWindowBeats)
            break;

        const float x = xForBeat (barStartBeats, noteArea, visibleWindowBeats);

        g.setColour (Theme::gridLine);
        g.drawVerticalLine ((int) x, rulerArea.getBottom() - 6.0f, rulerArea.getBottom());

        g.setColour (Theme::brandSubtitle);
        g.drawText (juce::String (bar + 1), (int) x + 3, 0, 40, (int) barRulerHeight,
                    juce::Justification::centredLeft, false);
    }
}

void PianoRollView::setComposeModeEnabled (bool shouldBeEnabled)
{
    composeModeEnabled = shouldBeEnabled;
    currentDragMode = DragMode::none;
    repaint();
}

void PianoRollView::mouseDown (const juce::MouseEvent& e)
{
    if (! composeModeEnabled)
        return;

    const auto noteArea = getNoteAreaBounds();
    if (! noteArea.contains (e.position))
        return;

    int lowNote, highNote;
    getVisibleNoteRange (lowNote, highNote);
    const double visibleWindowBeats = getComposeVisibleWindowBeats();

    const double clickBeat = beatForX (e.position.x, noteArea, visibleWindowBeats);
    const int clickNote = noteForY (e.position.y, noteArea, lowNote, highNote);

    const auto notes = composedMelody.getSnapshot();

    // Hit-test existing notes back-to-front so the most recently added one
    // (drawn last, so visually "on top" whenever two overlap) wins.
    for (int i = (int) notes.size() - 1; i >= 0; --i)
    {
        const auto& note = notes[(size_t) i];
        if (note.noteNumber != clickNote)
            continue;
        if (clickBeat < note.startBeat || clickBeat > note.getEndBeat())
            continue;

        if (e.mods.isPopupMenu() || e.getNumberOfClicks() >= 2)
        {
            composedMelody.removeNoteAt ((size_t) i);
            currentDragMode = DragMode::none;
            repaint();
            return;
        }

        const float noteEndX = xForBeat (note.getEndBeat(), noteArea, visibleWindowBeats);
        const bool nearRightEdge = std::abs (e.position.x - noteEndX) <= resizeHandlePixels;

        draggedNoteIndex = (size_t) i;
        currentDragMode = nearRightEdge ? DragMode::resizingNote : DragMode::movingNote;
        dragStartBeatOffset = clickBeat - note.startBeat;
        return;
    }

    if (e.mods.isPopupMenu())
        return; // right-click on empty space -- nothing to delete

    // Empty space: create a new note here, then let mouseDrag adjust its
    // length -- the same "click and drag to draw a note of a given length"
    // gesture FL Studio's own piano roll uses.
    MelodyNote newNote;
    newNote.noteNumber = clickNote;
    newNote.velocity = 0.85f;
    newNote.startBeat = juce::jmax (0.0, snapBeat (clickBeat));
    newNote.lengthBeats = defaultNoteLengthBeats;

    draggedNoteIndex = notes.size(); // addNote() below appends, so this is its index
    composedMelody.addNote (newNote);

    currentDragMode = DragMode::creatingNote;
    dragStartBeatOffset = 0.0;
    repaint();
}

void PianoRollView::mouseDrag (const juce::MouseEvent& e)
{
    if (currentDragMode == DragMode::none)
        return;

    const auto noteArea = getNoteAreaBounds();
    int lowNote, highNote;
    getVisibleNoteRange (lowNote, highNote);
    const double visibleWindowBeats = getComposeVisibleWindowBeats();

    const double dragBeat = beatForX (e.position.x, noteArea, visibleWindowBeats);
    const int dragNote = noteForY (e.position.y, noteArea, lowNote, highNote);

    auto notes = composedMelody.getSnapshot();
    if (draggedNoteIndex >= notes.size())
        return;

    MelodyNote updated = notes[draggedNoteIndex];

    if (currentDragMode == DragMode::movingNote)
    {
        updated.startBeat = juce::jmax (0.0, snapBeat (dragBeat - dragStartBeatOffset));
        updated.noteNumber = juce::jlimit (0, 127, dragNote);
    }
    else // resizingNote or creatingNote -- both just extend/shrink the length toward the drag point
    {
        updated.lengthBeats = juce::jmax (minNoteLengthBeats, snapBeat (dragBeat) - updated.startBeat);
    }

    composedMelody.updateNoteAt (draggedNoteIndex, updated);
    repaint();
}

void PianoRollView::mouseUp (const juce::MouseEvent&)
{
    currentDragMode = DragMode::none;
}

void PianoRollView::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto noteArea = getNoteAreaBounds();
    const auto keyboardArea = getKeyboardAreaBounds();
    const double absoluteNow = midiInputHandler.getCurrentTimeSeconds();

    int lowNote, highNote;
    getVisibleNoteRange (lowNote, highNote);

    // Background animation runs on real wall-clock time, NOT absoluteNow
    // above -- per explicit user request it must always keep animating,
    // even while the host transport is stopped/paused. absoluteNow is
    // deliberately frozen in that case (see MidiInputHandler's doc
    // comment -- it's what makes note scrolling sit still until Play is
    // pressed, matching FL Studio's own Piano Roll), which is exactly
    // right for notes but was also freezing the decorative background
    // along with them. getMillisecondCounterHiRes() is a monotonic
    // wall-clock counter with no relation to audio processing or host
    // transport state, so it keeps advancing regardless.
    const double backgroundTimeSeconds = juce::Time::getMillisecondCounterHiRes() * 0.001;

    backgroundManager.render (g, bounds, backgroundTimeSeconds);

    if (composeModeEnabled)
    {
        // -- Compose mode: shows/edits ComposedMelody instead of any MIDI --
        // source, and takes over the whole view rather than overlaying
        // LIVE/PATTERN mode -- see the class doc comment.
        const double nowBeats = getComposeRelativeNowBeats();
        const double visibleWindowBeats = getComposeVisibleWindowBeats();

        paintComposeBarRuler (g, noteArea, visibleWindowBeats);
        // Beat/bar grid-line overlay AND the old per-row shading are both
        // intentionally removed -- per explicit user request, the
        // background should show only the loaded background media
        // (video/gif) with nothing drawn over it besides notes, the ruler
        // and the playhead (the row shading in particular read as a
        // translucent black-and-white striped overlay darkening the
        // background, which is exactly what was asked to go).

        std::array<bool, 128> activeNotes {};
        activeNotes.fill (false);
        for (const auto& note : composedMelody.getSnapshot())
            if (note.startBeat <= nowBeats && note.getEndBeat() >= nowBeats
                && note.noteNumber >= 0 && note.noteNumber < 128)
                activeNotes[(size_t) note.noteNumber] = true;

        paintComposeNotes (g, noteArea, lowNote, highNote, nowBeats, visibleWindowBeats);

        // -- Piano keyboard strip (left edge, below the ruler) ---------------
        const float keyHeight = noteArea.getHeight() / (float) (highNote - lowNote + 1);

        g.setColour (Theme::panelOverlay);
        g.fillRect (juce::Rectangle<float> (keyboardArea.getX(), 0.0f, keyboardArea.getWidth(), barRulerHeight));
        g.fillRect (keyboardArea);

        for (int note = lowNote; note <= highNote; ++note)
        {
            const float yTop = yForNote (note, noteArea, lowNote, highNote) - keyHeight * 0.5f;
            juce::Rectangle<float> keyBounds (keyboardArea.getX(), yTop, keyboardArea.getWidth(), keyHeight);

            const bool active = activeNotes[(size_t) note];
            const bool isBlack = PitchUtils::isBlackKey (note);

            juce::Colour keyColour = isBlack ? Theme::keyBlack : Theme::keyWhite;
            if (active)
                keyColour = Theme::keyActiveHighlight;

            g.setColour (keyColour);
            g.fillRect (isBlack ? keyBounds.withWidth (keyboardWidth * 0.62f) : keyBounds);
        }

        g.setColour (Theme::keyBorder);
        g.drawVerticalLine ((int) keyboardArea.getRight(), 0.0f, bounds.getHeight());

        // -- Moving playhead (only when the host reports a musical position) --
        if (visibleWindowBeats > 1.0e-6 && nowBeats <= visibleWindowBeats + 1.0e-6
            && midiInputHandler.isHostMusicalPositionKnown())
        {
            const float playheadX = xForBeat (nowBeats, noteArea, visibleWindowBeats);

            g.setColour (Theme::playheadLine);
            g.drawVerticalLine ((int) playheadX, 0.0f, noteArea.getBottom());

            juce::Path marker;
            marker.addTriangle (playheadX - 5.0f, 0.0f, playheadX + 5.0f, 0.0f, playheadX, barRulerHeight * 0.7f);
            g.setColour (Theme::playheadLine);
            g.fillPath (marker);
        }

        g.setColour (Theme::brandSubtitle);
        g.setFont (juce::Font (12.0f));
        g.drawText ("Click to add a note, drag to move/resize, right-click to delete",
                    noteArea.withTop (juce::jmax (noteArea.getY(), noteArea.getBottom() - 16.0f)),
                    juce::Justification::centredLeft, false);

        return;
    }

    // -- LIVE / PATTERN modes (Compose mode off) -----------------------------
    const bool hasPattern = loadedPattern.isValid();
    const double now = hasPattern ? getPatternRelativeNow() : absoluteNow;
    const double visibleWindowSeconds = hasPattern ? getPatternVisibleWindowSeconds() : getVisibleWindowSeconds();

    if (hasPattern)
    {
        if (patternNowInitialized && now + 0.01 < previousPatternNow)
        {
            lastProcessedNoteStartTime = -1.0;
            touchAnimations.clear();
        }

        previousPatternNow = now;
        patternNowInitialized = true;
    }

    // -- Bar ruler along the top of the note area ----------------------------
    paintBarRuler (g, noteArea, visibleWindowSeconds);
    // Beat/bar grid-line overlay AND the old per-row shading are both
    // intentionally removed -- per explicit user request, the background
    // should show only the loaded background media (video/gif) with
    // nothing drawn over it besides notes, the ruler and the playhead (the
    // row shading in particular read as a translucent black-and-white
    // striped overlay darkening the background, which is exactly what was
    // asked to go).

    // -- Which notes feed the view, and which pitches currently sound -------
    const auto snapshot = midiInputHandler.getNoteStorage().getSnapshot();
    const std::vector<NoteEvent>& sourceNotes = hasPattern ? loadedPattern.notes : snapshot;

    std::array<bool, 128> activeNotes {};
    activeNotes.fill (false);
    for (const auto& note : sourceNotes)
    {
        const bool isHeld = hasPattern
            ? (note.startTimeSeconds <= now && note.endTimeSeconds >= now)
            : note.isActive();

        if (isHeld && note.noteNumber >= 0 && note.noteNumber < 128)
            activeNotes[(size_t) note.noteNumber] = true;
    }

    updateTouchAnimations (sourceNotes, now);

    if (hasPattern)
        paintPatternNotes (g, noteArea, lowNote, highNote, now, visibleWindowSeconds);
    else
        paintLiveNotes (g, noteArea, lowNote, highNote, now, visibleWindowSeconds, snapshot);

    // -- Piano keyboard strip (left edge, below the ruler) -------------------
    const float keyHeight = noteArea.getHeight() / (float) (highNote - lowNote + 1);

    g.setColour (Theme::panelOverlay);
    g.fillRect (juce::Rectangle<float> (keyboardArea.getX(), 0.0f, keyboardArea.getWidth(), barRulerHeight));
    g.fillRect (keyboardArea);

    for (int note = lowNote; note <= highNote; ++note)
    {
        const float yTop = yForNote (note, noteArea, lowNote, highNote) - keyHeight * 0.5f;
        juce::Rectangle<float> keyBounds (keyboardArea.getX(), yTop, keyboardArea.getWidth(), keyHeight);

        const bool active = activeNotes[(size_t) note];
        const bool isBlack = PitchUtils::isBlackKey (note);

        juce::Colour keyColour = isBlack ? Theme::keyBlack : Theme::keyWhite;
        if (active)
            keyColour = Theme::keyActiveHighlight;

        g.setColour (keyColour);
        g.fillRect (isBlack ? keyBounds.withWidth (keyboardWidth * 0.62f) : keyBounds);
    }

    g.setColour (Theme::keyBorder);
    g.drawVerticalLine ((int) keyboardArea.getRight(), 0.0f, bounds.getHeight());

    if (hasPattern)
    {
        if (visibleWindowSeconds > 1.0e-6 && now <= visibleWindowSeconds + 1.0e-6)
        {
            const float playheadX = noteArea.getX() + (float) (now / visibleWindowSeconds) * noteArea.getWidth();

            g.setColour (Theme::playheadLine);
            g.drawVerticalLine ((int) playheadX, 0.0f, noteArea.getBottom());

            juce::Path marker;
            marker.addTriangle (playheadX - 5.0f, 0.0f, playheadX + 5.0f, 0.0f, playheadX, barRulerHeight * 0.7f);
            g.setColour (Theme::playheadLine);
            g.fillPath (marker);
        }
    }
    else
    {
        g.setColour (Theme::playheadLine.withAlpha (0.85f));
        g.drawVerticalLine (juce::jmax (0, (int) noteArea.getX()), noteArea.getY(), noteArea.getBottom());
    }

    drawTouchAnimations (g, keyboardArea, noteArea, lowNote, highNote, backgroundManager.getCurrentAccentColour(), now);

    if (isDraggingFileOver)
    {
        g.setColour (Theme::keyActiveHighlight.withAlpha (0.12f));
        g.fillRect (bounds);
        g.setColour (Theme::keyActiveHighlight.withAlpha (0.85f));
        g.drawRect (bounds, 2.0f);
        g.setColour (Theme::brandTitle);
        g.setFont (juce::Font (16.0f, juce::Font::bold));
        g.drawText ("Drop MIDI file to load", bounds, juce::Justification::centred, false);
    }
}

void PianoRollView::resized()
{
    auto bounds = getLocalBounds();
    auto sliderArea = bounds.removeFromRight ((int) verticalPanSliderWidth);
    sliderArea.removeFromTop ((int) barRulerHeight);
    verticalPanSlider.setBounds (sliderArea);
}

void PianoRollView::timerCallback()
{
    repaint();
}

void PianoRollView::zoomInVertical()
{
    verticalZoom = juce::jmin (maxVerticalZoom, verticalZoom + 0.25f);
    repaint();
}

void PianoRollView::zoomOutVertical()
{
    verticalZoom = juce::jmax (minVerticalZoom, verticalZoom - 0.25f);
    repaint();
}

void PianoRollView::zoomInHorizontal()
{
    horizontalZoom = juce::jmin (maxHorizontalZoom, horizontalZoom + 0.25f);
    repaint();
}

void PianoRollView::zoomOutHorizontal()
{
    horizontalZoom = juce::jmax (minHorizontalZoom, horizontalZoom - 0.25f);
    repaint();
}

void PianoRollView::selectNextBackground()
{
    backgroundManager.selectNext();
    repaint();
}

void PianoRollView::selectPreviousBackground()
{
    backgroundManager.selectPrevious();
    repaint();
}

juce::String PianoRollView::getCurrentBackgroundName() const
{
    return backgroundManager.getCurrentName();
}

juce::String PianoRollView::getLoadedPatternName() const
{
    return loadedPattern.isValid() ? loadedPattern.sourceFileName : juce::String();
}

bool PianoRollView::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& path : files)
        if (path.endsWithIgnoreCase (".mid") || path.endsWithIgnoreCase (".midi"))
            return true;

    return false;
}

void PianoRollView::filesDropped (const juce::StringArray& files, int /*x*/, int /*y*/)
{
    isDraggingFileOver = false;

    for (const auto& path : files)
    {
        if (! (path.endsWithIgnoreCase (".mid") || path.endsWithIgnoreCase (".midi")))
            continue;

        auto pattern = MidiFilePattern::loadFromFile (juce::File (path));
        if (! pattern.isValid())
            continue;

        loadedPattern = std::move (pattern);

        patternEpochSeconds = midiInputHandler.getCurrentTimeSeconds();
        patternNowInitialized = false;
        previousPatternNow = 0.0;
        lastProcessedNoteStartTime = -1.0;
        touchAnimations.clear();

        break;
    }

    repaint();
}

void PianoRollView::fileDragEnter (const juce::StringArray& /*files*/, int /*x*/, int /*y*/)
{
    isDraggingFileOver = true;
    repaint();
}

void PianoRollView::fileDragExit (const juce::StringArray& /*files*/)
{
    isDraggingFileOver = false;
    repaint();
}

} // namespace w27
