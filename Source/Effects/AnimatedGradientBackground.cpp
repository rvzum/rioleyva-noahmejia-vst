#include "AnimatedGradientBackground.h"
#include "../Theme/ThemeColors.h"
#include <cmath>

namespace w27
{

void AnimatedGradientBackground::render (juce::Graphics& g, juce::Rectangle<float> bounds, double timeSeconds)
{
    g.fillAll (Theme::background);

    const float w = bounds.getWidth();
    const float h = bounds.getHeight();

    // Two slowly-drifting soft radial glows, cycling smoothly via sine/cosine
    // so their motion never jumps or reverses abruptly.
    auto drawGlow = [&] (juce::Colour colour, double speed, float phaseOffset, float radiusScale)
    {
        const float angle = (float) (timeSeconds * speed) + phaseOffset;
        const float cx = bounds.getCentreX() + std::cos (angle) * w * 0.32f;
        const float cy = bounds.getCentreY() + std::sin (angle * 0.8f) * h * 0.32f;
        const float radius = juce::jmin (w, h) * radiusScale;

        if (radius <= 0.0f)
            return;

        juce::ColourGradient gradient (colour.withAlpha (0.16f), cx, cy,
                                        colour.withAlpha (0.0f), cx, cy - radius,
                                        true);
        g.setGradientFill (gradient);
        g.fillEllipse (cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);
    };

    drawGlow (Theme::keyActiveHighlight, 0.15, 0.0f, 0.6f);
    drawGlow (juce::Colour (0xff5a8fd1), 0.11, juce::MathConstants<float>::pi, 0.5f);
}

} // namespace w27
