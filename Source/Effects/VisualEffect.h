#pragma once

#include <juce_graphics/juce_graphics.h>
#include "../Theme/ThemeColors.h"

namespace w27
{

/**
    Minimal interface for a background/ambient visual effect. New effects
    (Phase 8 theming, reactive effects driven by NoteStorage, etc.) only need
    to implement this -- Visualization code depends only on this interface,
    never on a concrete effect class.
*/
class VisualEffect
{
public:
    virtual ~VisualEffect() = default;

    // timeSeconds uses the same "seconds since prepare()/reset()" clock
    // MidiInputHandler exposes, so effects animate smoothly and can later
    // react to note timing without needing a different clock source.
    virtual void render (juce::Graphics& g, juce::Rectangle<float> bounds, double timeSeconds) = 0;

    // A single representative colour for this background. Used by other UI
    // elements -- currently PianoRollView's note-touch animation -- that
    // want to visually match whichever background is selected, without
    // needing to know anything about how that background is drawn.
    //
    // Default matches the plugin's standard accent colour (used by the
    // built-in gradient background too). Image-based backgrounds
    // (FrameSequenceBackground) override this with a cheap, cached sample
    // of their own frame instead.
    virtual juce::Colour getAccentColour() { return Theme::keyActiveHighlight; }
};

} // namespace w27
