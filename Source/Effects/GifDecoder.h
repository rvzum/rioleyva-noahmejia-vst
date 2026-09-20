#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace w27::GifDecoder
{

struct Frame
{
    std::vector<std::uint8_t> rgba; // width * height * 4 bytes, straight (non-premultiplied) alpha
    int delayMs = 100;
};

struct DecodedGif
{
    int width = 0;
    int height = 0;
    std::vector<Frame> frames;

    bool isValid() const noexcept { return width > 0 && height > 0 && ! frames.empty(); }
};

// Minimal, self-contained GIF87a/89a + LZW decoder (no external dependencies
// -- network access to fetch a third-party library was unavailable both from
// this machine and from the build sandbox, so this is a from-scratch
// implementation). Supports what typical animated background loops use:
// global/local colour tables, transparency, per-frame delay, and interlaced
// images. Disposal method 3 ("restore to previous") is treated the same as
// "leave in place" -- a rare case for simple looping backgrounds; getting it
// slightly wrong only affects one frame's compositing, never crashes.
//
// Returns a DecodedGif with isValid() == false (empty frames) if data isn't
// a parseable GIF -- callers should check this before using the result.
DecodedGif decode (const void* data, size_t numBytes);

} // namespace w27::GifDecoder
