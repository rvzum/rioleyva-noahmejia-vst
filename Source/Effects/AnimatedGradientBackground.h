#pragma once

#include "VisualEffect.h"

namespace w27
{

/**
    First concrete VisualEffect: a slow, softly drifting dual-glow gradient.
    Purely decorative -- keeps the editor feeling alive even with no MIDI
    playing, without competing visually with the note area on top of it.
*/
class AnimatedGradientBackground : public VisualEffect
{
public:
    void render (juce::Graphics& g, juce::Rectangle<float> bounds, double timeSeconds) override;
};

} // namespace w27
