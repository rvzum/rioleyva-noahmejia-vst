#include "PremiumLookAndFeel.h"
#include "../Theme/ThemeColors.h"
#include <cmath>

namespace w27
{

PremiumLookAndFeel::PremiumLookAndFeel()
{
    setColour (juce::Slider::rotarySliderOutlineColourId, Theme::functionalKnobTrack);
}

void PremiumLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                                                bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    const float cornerSize = juce::jmin (8.0f, bounds.getHeight() * 0.3f);

    // A saturated backgroundColour means this button is in its "on"/accent
    // state (e.g. buttonOnColourId set to Theme::keyActiveHighlight for
    // "Write Melody") -- painted as a light tint of that accent rather
    // than a flat fill, so the toggle still reads clearly without
    // breaking the light functional-UI palette. Anything else (the
    // ordinary idle/off state) is a neutral white-to-light-grey gradient,
    // regardless of exactly what buttonColourId happens to be set to.
    const bool isAccented = backgroundColour.getSaturation() > 0.25f;

    juce::Colour top = Theme::functionalButtonTop;
    juce::Colour bottom = Theme::functionalButtonBot;

    if (isAccented)
    {
        top = backgroundColour.interpolatedWith (juce::Colours::white, 0.55f);
        bottom = backgroundColour.interpolatedWith (juce::Colours::white, 0.15f);
    }

    if (shouldDrawButtonAsDown)
    {
        top = top.darker (0.12f);
        bottom = bottom.darker (0.12f);
    }
    else if (shouldDrawButtonAsHighlighted)
    {
        top = top.brighter (0.05f);
        bottom = bottom.brighter (0.05f);
    }

    // Soft outer glow in the accent colour while toggled on -- reads as
    // "lit" without needing a dark background to pop against.
    if (isAccented)
    {
        g.setColour (backgroundColour.withAlpha (0.22f));
        g.fillRoundedRectangle (bounds.expanded (2.0f), cornerSize + 2.0f);
    }

    juce::ColourGradient gradient (top, bounds.getX(), bounds.getY(),
                                    bottom, bounds.getX(), bounds.getBottom(),
                                    false);
    g.setGradientFill (gradient);
    g.fillRoundedRectangle (bounds, cornerSize);

    // Bevel: a bright hairline along the top edge, a soft outline around
    // the whole shape -- reads as a subtly domed, light glass/plastic cap
    // rather than a flat rectangle.
    g.setColour (juce::Colours::white.withAlpha (shouldDrawButtonAsDown ? 0.3f : 0.9f));
    g.drawLine (bounds.getX() + cornerSize * 0.4f, bounds.getY() + 0.75f,
                bounds.getRight() - cornerSize * 0.4f, bounds.getY() + 0.75f, 1.0f);

    g.setColour (isAccented ? backgroundColour.darker (0.2f).withAlpha (0.6f) : Theme::functionalBorder);
    g.drawRoundedRectangle (bounds, cornerSize, 1.0f);
}

void PremiumLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                            float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                                            juce::Slider& slider)
{
    auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (2.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const float centreX = bounds.getCentreX();
    const float centreY = bounds.getCentreY();
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    const auto accent = slider.findColour (juce::Slider::rotarySliderFillColourId);
    const auto track = slider.findColour (juce::Slider::rotarySliderOutlineColourId);

    // Track: the full available sweep, light grey.
    juce::Path trackPath;
    trackPath.addCentredArc (centreX, centreY, radius - 3.0f, radius - 3.0f, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (track);
    g.strokePath (trackPath, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Value arc -- the knob's own accent colour, with a soft glow behind
    // it so each knob's colour still reads clearly against the light dial.
    if (sliderPos > 0.001f)
    {
        juce::Path valueArc;
        valueArc.addCentredArc (centreX, centreY, radius - 3.0f, radius - 3.0f, 0.0f, rotaryStartAngle, angle, true);

        g.setColour (accent.withAlpha (0.25f));
        g.strokePath (valueArc, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour (accent);
        g.strokePath (valueArc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Cap: a small light glass/plastic dome with a pointer, rather than
    // JUCE's default flat disc.
    const float capRadius = radius * 0.56f;
    juce::ColourGradient capGradient (Theme::functionalButtonTop, centreX, centreY - capRadius,
                                       Theme::functionalButtonBot, centreX, centreY + capRadius, false);
    g.setGradientFill (capGradient);
    g.fillEllipse (centreX - capRadius, centreY - capRadius, capRadius * 2.0f, capRadius * 2.0f);

    g.setColour (Theme::functionalBorder);
    g.drawEllipse (centreX - capRadius, centreY - capRadius, capRadius * 2.0f, capRadius * 2.0f, 1.0f);

    const juce::Point<float> pointerTip (centreX + std::cos (angle - juce::MathConstants<float>::halfPi) * capRadius * 0.85f,
                                          centreY + std::sin (angle - juce::MathConstants<float>::halfPi) * capRadius * 0.85f);
    g.setColour (accent);
    g.drawLine (centreX, centreY, pointerTip.x, pointerTip.y, 2.2f);
    g.fillEllipse (pointerTip.x - 2.0f, pointerTip.y - 2.0f, 4.0f, 4.0f);
}

} // namespace w27
