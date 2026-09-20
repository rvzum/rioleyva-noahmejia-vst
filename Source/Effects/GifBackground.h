#pragma once

#include "FrameSequenceBackground.h"
#include "GifDecoder.h"
#include <memory>

namespace w27
{

// Converts a decoded GIF into a FrameSequenceBackground (one juce::Image per
// frame -- the only place GifDecoder's raw RGBA buffers get turned into
// juce::Image objects). Returns nullptr if decoded was invalid or produced
// no usable frames.
std::unique_ptr<FrameSequenceBackground> createGifBackground (const GifDecoder::DecodedGif& decoded);

} // namespace w27
