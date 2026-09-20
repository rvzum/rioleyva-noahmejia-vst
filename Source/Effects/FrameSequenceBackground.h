#pragma once

#include "VisualEffect.h"
#include <juce_graphics/juce_graphics.h>
#include <vector>

namespace w27
{

/**
    Plays back a fixed sequence of images as a background, cycling through
    them by elapsed time. Shared by both background sources:
    - animated GIFs (see GifBackground.h -- GifDecoder produces raw frames,
      createGifBackground() turns them into juce::Image and builds one of these)
    - .mp4-derived backgrounds (see BackgroundManager.cpp -- CMake extracts
      video frames to JPEGs at build time via ffmpeg, embedded like GIFs;
      BackgroundManager loads each with juce::ImageFileFormat and builds one
      of these directly, no GifDecoder involved)

    Draws each frame with a "cover" fit (fills the view, preserves aspect
    ratio, crops overflow, centred) plus a dark dimming overlay so the piano
    roll on top stays legible regardless of source brightness.
*/
class FrameSequenceBackground : public VisualEffect
{
public:
    struct Frame
    {
        juce::Image image;
        int delayMs = 100;
    };

    explicit FrameSequenceBackground (std::vector<Frame> framesToUse);

    bool isValid() const noexcept { return ! frames.empty(); }

    void render (juce::Graphics& g, juce::Rectangle<float> bounds, double timeSeconds) override;

    // Cheap, cached representative colour for this sequence: a small,
    // fixed-size grid of samples taken from the first frame the first time
    // this is called, averaged and nudged towards a usable highlight colour.
    // Never re-samples on every call, so it costs nothing in the render loop.
    juce::Colour getAccentColour() override;

private:
    std::vector<Frame> frames;
    int totalDurationMs = 0;

    bool accentColourCached = false;
    juce::Colour cachedAccentColour { juce::Colours::white };
};

} // namespace w27
