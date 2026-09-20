#include "FrameSequenceBackground.h"
#include "../Theme/ThemeColors.h"
#include <cmath>

namespace w27
{

FrameSequenceBackground::FrameSequenceBackground (std::vector<Frame> framesToUse)
    : frames (std::move (framesToUse))
{
    for (const auto& f : frames)
        totalDurationMs += juce::jmax (20, f.delayMs);
}

void FrameSequenceBackground::render (juce::Graphics& g, juce::Rectangle<float> bounds, double timeSeconds)
{
    if (frames.empty())
    {
        g.fillAll (Theme::background);
        return;
    }

    int frameIndex = 0;

    if (frames.size() > 1 && totalDurationMs > 0)
    {
        const int elapsedMs = (int) std::fmod (timeSeconds * 1000.0, (double) totalDurationMs);
        int accumulated = 0;

        for (size_t i = 0; i < frames.size(); ++i)
        {
            accumulated += juce::jmax (20, frames[i].delayMs);
            if (elapsedMs < accumulated)
            {
                frameIndex = (int) i;
                break;
            }
        }
    }

    const auto& image = frames[(size_t) frameIndex].image;

    if (! image.isValid())
    {
        g.fillAll (Theme::background);
        return;
    }

    // "Cover" fit: scale to fill bounds completely, preserving aspect ratio,
    // cropping overflow, centred.
    const float scale = juce::jmax (bounds.getWidth() / (float) image.getWidth(),
                                     bounds.getHeight() / (float) image.getHeight());
    const float drawW = (float) image.getWidth() * scale;
    const float drawH = (float) image.getHeight() * scale;
    const float drawX = bounds.getCentreX() - drawW * 0.5f;
    const float drawY = bounds.getCentreY() - drawH * 0.5f;

    // High-quality resampling for the upscale/crop above -- per explicit
    // user request the background must not look distorted/low quality;
    // JUCE defaults to a cheap low-quality resample otherwise, which reads
    // as blocky/soft once this image is stretched to cover the window.
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImage (image, drawX, drawY, drawW, drawH, 0, 0, image.getWidth(), image.getHeight());

    // Light dim overlay so the piano roll/notes stay legible over a bright
    // source -- lowered from 0.45 per explicit user request that the
    // background read as too dark.
    g.setColour (juce::Colours::black.withAlpha (0.20f));
    g.fillRect (bounds);
}

juce::Colour FrameSequenceBackground::getAccentColour()
{
    if (accentColourCached)
        return cachedAccentColour;

    accentColourCached = true;
    cachedAccentColour = Theme::keyActiveHighlight; // fallback if sampling fails below

    if (! frames.empty() && frames.front().image.isValid())
    {
        const auto& image = frames.front().image;
        juce::Image::BitmapData bitmap (image, juce::Image::BitmapData::readOnly);

        if (bitmap.width > 0 && bitmap.height > 0)
        {
            constexpr int stepsX = 6;
            constexpr int stepsY = 6;

            juce::uint64 rSum = 0, gSum = 0, bSum = 0;
            int sampleCount = 0;

            for (int sy = 0; sy < stepsY; ++sy)
            {
                const int y = juce::jlimit (0, bitmap.height - 1, (int) ((sy + 0.5) * bitmap.height / stepsY));

                for (int sx = 0; sx < stepsX; ++sx)
                {
                    const int x = juce::jlimit (0, bitmap.width - 1, (int) ((sx + 0.5) * bitmap.width / stepsX));
                    const auto colour = bitmap.getPixelColour (x, y);

                    rSum += colour.getRed();
                    gSum += colour.getGreen();
                    bSum += colour.getBlue();
                    ++sampleCount;
                }
            }

            if (sampleCount > 0)
            {
                const auto averaged = juce::Colour ((juce::uint8) (rSum / (juce::uint64) sampleCount),
                                                      (juce::uint8) (gSum / (juce::uint64) sampleCount),
                                                      (juce::uint8) (bSum / (juce::uint64) sampleCount));

                // A flat average often comes out dim/muddy -- push it towards
                // a usable highlight colour rather than using it verbatim.
                cachedAccentColour = averaged.withSaturation (juce::jmax (0.55f, averaged.getSaturation()))
                                              .withBrightness (juce::jmax (0.75f, averaged.getBrightness()));
            }
        }
    }

    return cachedAccentColour;
}

} // namespace w27
