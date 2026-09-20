#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Theme/ThemeColors.h"

namespace w27
{

/**
    A toggle button drawing its own crossed-arrows "shuffle/randomizer"
    icon as vector paths, rather than relying on a Unicode glyph (U+21C4
    "left-right arrows" rendered as a missing-glyph box/"a" on at least one
    user's system -- fonts aren't guaranteed to cover that codepoint, so a
    hand-drawn icon is the only fully portable fix). Otherwise behaves
    exactly like any other juce::Button subclass: setClickingTogglesState()
    (inherited), onClick, getToggleState(), setTooltip() all work as
    normal -- PluginEditor wires it up exactly as it did the old
    juce::TextButton version, just without styleSmallButton() (which only
    applies to juce::TextButton's own colour IDs; this class reads Theme
    colours directly instead).
*/
class ShuffleIconButton : public juce::Button
{
public:
    ShuffleIconButton() : juce::Button ("Randomizer") {}

    void paintButton (juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (0.5f);
        const float cornerSize = juce::jmin (8.0f, bounds.getHeight() * 0.3f);

        // Light-toned per explicit user request: the "off" state reads as
        // a neutral white/light-grey chip (matching PremiumLookAndFeel's
        // button chrome elsewhere), and only the "on" state uses a
        // saturated accent fill.
        const bool on = getToggleState();
        auto base = on ? Theme::keyActiveHighlight : Theme::functionalButtonBot;
        if (shouldDrawButtonAsDown)
            base = base.darker (0.12f);
        else if (shouldDrawButtonAsHighlighted)
            base = base.brighter (0.06f);

        if (on)
        {
            g.setColour (base.withAlpha (0.30f));
            g.fillRoundedRectangle (bounds.expanded (2.0f), cornerSize + 2.0f);
        }

        juce::ColourGradient gradient (base.brighter (0.15f), bounds.getX(), bounds.getY(),
                                        base.darker (0.08f), bounds.getX(), bounds.getBottom(), false);
        g.setGradientFill (gradient);
        g.fillRoundedRectangle (bounds, cornerSize);
        g.setColour (Theme::functionalBorder);
        g.drawRoundedRectangle (bounds, cornerSize, 1.0f);

        g.setColour (on ? Theme::background : Theme::functionalTextDark);
        g.strokePath (buildShufflePath (bounds.reduced (bounds.getWidth() * 0.24f, bounds.getHeight() * 0.28f)),
                      juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

private:
    // The conventional "shuffle" icon -- two crossing strands, each ending
    // in an arrowhead on the right-hand side -- built fresh for whatever
    // bounds it's asked to fill so it scales cleanly with the button's own
    // size instead of being a fixed-size bitmap/glyph.
    static juce::Path buildShufflePath (juce::Rectangle<float> area)
    {
        juce::Path p;
        const float xL = area.getX();
        const float xR = area.getRight();
        const float yT = area.getY();
        const float yB = area.getBottom();
        const float h  = area.getHeight();
        const float arrow = juce::jmin (area.getWidth(), h) * 0.32f;

        // Strand 1: bottom-left curving up to top-right.
        const juce::Point<float> start1 (xL, yB - h * 0.12f);
        const juce::Point<float> end1   (xR - arrow * 0.7f, yT + h * 0.12f);
        p.startNewSubPath (start1);
        p.cubicTo (xL + (end1.x - xL) * 0.6f, start1.y,
                   end1.x - (end1.x - xL) * 0.6f, end1.y,
                   end1.x, end1.y);

        p.startNewSubPath (end1.x - arrow, end1.y - arrow * 0.25f);
        p.lineTo (end1.x + arrow * 0.3f, end1.y);
        p.lineTo (end1.x - arrow * 0.25f, end1.y + arrow);

        // Strand 2: top-left curving down to bottom-right (the crossing
        // partner).
        const juce::Point<float> start2 (xL, yT + h * 0.12f);
        const juce::Point<float> end2   (xR - arrow * 0.7f, yB - h * 0.12f);
        p.startNewSubPath (start2);
        p.cubicTo (xL + (end2.x - xL) * 0.6f, start2.y,
                   end2.x - (end2.x - xL) * 0.6f, end2.y,
                   end2.x, end2.y);

        p.startNewSubPath (end2.x - arrow, end2.y + arrow * 0.25f);
        p.lineTo (end2.x + arrow * 0.3f, end2.y);
        p.lineTo (end2.x - arrow * 0.25f, end2.y - arrow);

        return p;
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ShuffleIconButton)
};

} // namespace w27
