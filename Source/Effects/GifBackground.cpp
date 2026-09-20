#include "GifBackground.h"

namespace w27
{

std::unique_ptr<FrameSequenceBackground> createGifBackground (const GifDecoder::DecodedGif& decoded)
{
    if (! decoded.isValid())
        return nullptr;

    std::vector<FrameSequenceBackground::Frame> frames;
    frames.reserve (decoded.frames.size());

    for (const auto& src : decoded.frames)
    {
        juce::Image image (juce::Image::ARGB, decoded.width, decoded.height, true);

        {
            juce::Image::BitmapData bitmap (image, juce::Image::BitmapData::writeOnly);

            for (int y = 0; y < decoded.height; ++y)
            {
                for (int x = 0; x < decoded.width; ++x)
                {
                    const size_t offset = ((size_t) y * (size_t) decoded.width + (size_t) x) * 4;
                    bitmap.setPixelColour (x, y,
                        juce::Colour (src.rgba[offset], src.rgba[offset + 1],
                                      src.rgba[offset + 2], src.rgba[offset + 3]));
                }
            }
        }

        frames.push_back ({ std::move (image), juce::jmax (20, src.delayMs) });
    }

    auto result = std::make_unique<FrameSequenceBackground> (std::move (frames));
    return result->isValid() ? std::move (result) : nullptr;
}

} // namespace w27
