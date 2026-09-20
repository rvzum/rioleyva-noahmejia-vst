#include "GifDecoder.h"
#include <algorithm>
#include <cstring>

namespace w27::GifDecoder
{

namespace
{
    struct RGB { std::uint8_t r, g, b; };

    struct Reader
    {
        const std::uint8_t* data;
        size_t size;
        size_t pos = 0;

        bool hasBytes (size_t n) const noexcept { return pos + n <= size; }

        std::uint8_t u8()
        {
            return hasBytes (1) ? data[pos++] : (std::uint8_t) 0;
        }

        std::uint16_t u16le()
        {
            if (! hasBytes (2)) { pos = size; return 0; }
            const std::uint16_t v = (std::uint16_t) (data[pos] | (data[pos + 1] << 8));
            pos += 2;
            return v;
        }

        bool matchSignature (const char* sig, size_t n)
        {
            if (! hasBytes (n))
                return false;

            const bool ok = std::memcmp (data + pos, sig, n) == 0;
            if (ok)
                pos += n;
            return ok;
        }

        // Reads a run of GIF sub-blocks (a size byte followed by that many
        // data bytes, terminated by a zero size byte) and concatenates them.
        std::vector<std::uint8_t> readSubBlocksConcat()
        {
            std::vector<std::uint8_t> out;

            while (pos < size)
            {
                const std::uint8_t blockSize = u8();
                if (blockSize == 0)
                    break;

                if (! hasBytes (blockSize))
                {
                    pos = size;
                    break;
                }

                out.insert (out.end(), data + pos, data + pos + blockSize);
                pos += blockSize;
            }

            return out;
        }

        void skipSubBlocks()
        {
            while (pos < size)
            {
                const std::uint8_t blockSize = u8();
                if (blockSize == 0)
                    break;

                pos = hasBytes (blockSize) ? pos + blockSize : size;
            }
        }
    };

    std::vector<RGB> readColorTable (Reader& r, int numEntries)
    {
        std::vector<RGB> table;
        table.reserve ((size_t) numEntries);

        for (int i = 0; i < numEntries; ++i)
        {
            RGB c;
            c.r = r.u8();
            c.g = r.u8();
            c.b = r.u8();
            table.push_back (c);
        }

        return table;
    }

    // Standard GIF LZW decompression. Always returns exactly expectedPixelCount
    // indices (padded with 0 / truncated) so callers never index out of range,
    // even on a malformed or truncated stream.
    std::vector<int> lzwDecode (const std::vector<std::uint8_t>& compressed, int lzwMinCodeSize, size_t expectedPixelCount)
    {
        std::vector<int> output;
        output.reserve (expectedPixelCount);

        const int clearCode = 1 << lzwMinCodeSize;
        const int endCode = clearCode + 1;

        size_t bytePos = 0;
        std::uint32_t bitBuffer = 0;
        int bitsInBuffer = 0;

        auto readCode = [&] (int codeSize) -> int
        {
            while (bitsInBuffer < codeSize)
            {
                if (bytePos >= compressed.size())
                    return -1;

                bitBuffer |= (std::uint32_t) compressed[bytePos++] << bitsInBuffer;
                bitsInBuffer += 8;
            }

            const int code = (int) (bitBuffer & ((1u << codeSize) - 1));
            bitBuffer >>= codeSize;
            bitsInBuffer -= codeSize;
            return code;
        };

        std::vector<std::vector<int>> dictionary;

        auto resetDictionary = [&]
        {
            dictionary.clear();
            dictionary.reserve (4096);
            for (int i = 0; i < clearCode; ++i)
                dictionary.push_back ({ i });
            dictionary.push_back ({}); // clearCode slot -- never used as data
            dictionary.push_back ({}); // endCode slot -- never used as data
        };
        resetDictionary();

        int codeSize = lzwMinCodeSize + 1;
        int previousCode = -1;

        while (output.size() < expectedPixelCount)
        {
            const int code = readCode (codeSize);
            if (code < 0 || code == endCode)
                break;

            if (code == clearCode)
            {
                resetDictionary();
                codeSize = lzwMinCodeSize + 1;
                previousCode = -1;
                continue;
            }

            std::vector<int> entry;

            if (code < (int) dictionary.size())
            {
                entry = dictionary[(size_t) code];
            }
            else if (code == (int) dictionary.size() && previousCode >= 0)
            {
                entry = dictionary[(size_t) previousCode];
                entry.push_back (entry.front());
            }
            else
            {
                break; // corrupt stream -- stop rather than reading garbage
            }

            output.insert (output.end(), entry.begin(), entry.end());

            if (previousCode >= 0 && dictionary.size() < 4096)
            {
                std::vector<int> newEntry = dictionary[(size_t) previousCode];
                newEntry.push_back (entry.front());
                dictionary.push_back (std::move (newEntry));

                if (dictionary.size() == (size_t) (1 << codeSize) && codeSize < 12)
                    ++codeSize;
            }

            previousCode = code;
        }

        output.resize (expectedPixelCount, 0);
        return output;
    }
}

DecodedGif decode (const void* rawData, size_t numBytes)
{
    DecodedGif result;

    if (rawData == nullptr || numBytes < 13)
        return result;

    Reader r { (const std::uint8_t*) rawData, numBytes, 0 };

    if (! r.matchSignature ("GIF87a", 6) && ! r.matchSignature ("GIF89a", 6))
        return result;

    const int screenWidth = r.u16le();
    const int screenHeight = r.u16le();
    const std::uint8_t screenPacked = r.u8();
    r.u8(); // background colour index -- unused, canvas always starts fully transparent
    r.u8(); // pixel aspect ratio -- unused

    if (screenWidth <= 0 || screenHeight <= 0 || screenWidth > 8192 || screenHeight > 8192)
        return result; // sanity guard against corrupt headers

    const bool hasGlobalColorTable = (screenPacked & 0x80) != 0;
    const int globalColorTableSize = 2 << (screenPacked & 0x07);

    std::vector<RGB> globalColorTable;
    if (hasGlobalColorTable)
        globalColorTable = readColorTable (r, globalColorTableSize);

    result.width = screenWidth;
    result.height = screenHeight;

    std::vector<std::uint8_t> canvas ((size_t) screenWidth * (size_t) screenHeight * 4, 0);

    // Guard against pathological / huge animated GIFs (huge canvas and/or an
    // enormous number of frames) consuming unbounded memory or hanging the UI
    // thread that decodes it. Cap the number of frames we keep based on a fixed
    // total memory budget for the decoded animation -- similar in spirit to the
    // MP4 pipeline's 15-second/480px caps. Anything beyond the cap is simply not
    // decoded; the animation just loops over the frames that were kept.
    constexpr size_t maxTotalFrameBytes = (size_t) 256 * 1024 * 1024; // ~256 MB
    const size_t bytesPerFrame = (size_t) screenWidth * (size_t) screenHeight * 4;
    const int maxFrames = bytesPerFrame > 0
        ? (int) std::clamp (maxTotalFrameBytes / bytesPerFrame, (size_t) 1, (size_t) 600)
        : 1;

    bool haveGce = false;
    bool gceTransparent = false;
    int gceTransparentIndex = 0;
    int gceDelayMs = 100;
    int disposalMethod = 0;

    while (r.pos < r.size)
    {
        const std::uint8_t blockType = r.u8();

        if (blockType == 0x3B) // trailer
            break;

        if (blockType == 0x21) // extension introducer
        {
            const std::uint8_t label = r.u8();

            if (label == 0xF9) // Graphic Control Extension
            {
                r.u8(); // block size, always 4 -- not needed, sub-block reading below handles the terminator
                const std::uint8_t packedGce = r.u8();
                disposalMethod = (packedGce >> 2) & 0x07;
                gceTransparent = (packedGce & 0x01) != 0;
                const std::uint16_t delayCentiseconds = r.u16le();
                gceDelayMs = delayCentiseconds > 0 ? delayCentiseconds * 10 : 100;
                gceDelayMs = gceDelayMs < 20 ? 20 : gceDelayMs; // avoid a runaway/instant loop
                gceTransparentIndex = r.u8();
                r.u8(); // block terminator
                haveGce = true;
            }
            else
            {
                r.skipSubBlocks();
            }

            continue;
        }

        if (blockType == 0x2C) // image descriptor
        {
            const int imgLeft = r.u16le();
            const int imgTop = r.u16le();
            const int imgWidth = r.u16le();
            const int imgHeight = r.u16le();
            const std::uint8_t imgPacked = r.u8();

            const bool hasLocalColorTable = (imgPacked & 0x80) != 0;
            const bool interlaced = (imgPacked & 0x40) != 0;
            const int localColorTableSize = 2 << (imgPacked & 0x07);

            std::vector<RGB> localColorTable;
            if (hasLocalColorTable)
                localColorTable = readColorTable (r, localColorTableSize);

            const std::vector<RGB>& activeColorTable = hasLocalColorTable ? localColorTable : globalColorTable;

            const std::uint8_t lzwMinCodeSize = r.u8();
            const std::vector<std::uint8_t> compressed = r.readSubBlocksConcat();

            if (imgWidth > 0 && imgHeight > 0 && ! activeColorTable.empty())
            {
                const size_t pixelCount = (size_t) imgWidth * (size_t) imgHeight;
                const std::vector<int> indices = lzwDecode (compressed, lzwMinCodeSize, pixelCount);

                // Row order for interlaced images (GIF: 4 interleaved passes).
                std::vector<int> rowOrder;
                rowOrder.reserve ((size_t) imgHeight);

                if (interlaced)
                {
                    static const int starts[4]  = { 0, 4, 2, 1 };
                    static const int strides[4] = { 8, 8, 4, 2 };
                    for (int pass = 0; pass < 4; ++pass)
                        for (int y = starts[pass]; y < imgHeight; y += strides[pass])
                            rowOrder.push_back (y);
                }
                else
                {
                    for (int y = 0; y < imgHeight; ++y)
                        rowOrder.push_back (y);
                }

                for (int srcRow = 0; srcRow < imgHeight && srcRow < (int) rowOrder.size(); ++srcRow)
                {
                    const int destRow = rowOrder[(size_t) srcRow];
                    const int canvasY = imgTop + destRow;
                    if (canvasY < 0 || canvasY >= screenHeight)
                        continue;

                    for (int x = 0; x < imgWidth; ++x)
                    {
                        const int canvasX = imgLeft + x;
                        if (canvasX < 0 || canvasX >= screenWidth)
                            continue;

                        const int paletteIndex = indices[(size_t) srcRow * (size_t) imgWidth + (size_t) x];

                        if (gceTransparent && paletteIndex == gceTransparentIndex)
                            continue; // leave existing canvas pixel showing through

                        if (paletteIndex < 0 || (size_t) paletteIndex >= activeColorTable.size())
                            continue;

                        const RGB c = activeColorTable[(size_t) paletteIndex];
                        const size_t offset = ((size_t) canvasY * (size_t) screenWidth + (size_t) canvasX) * 4;
                        canvas[offset + 0] = c.r;
                        canvas[offset + 1] = c.g;
                        canvas[offset + 2] = c.b;
                        canvas[offset + 3] = 255;
                    }
                }
            }

            Frame frame;
            frame.rgba = canvas; // snapshot -- each output frame is fully composited
            frame.delayMs = haveGce ? gceDelayMs : 100;
            result.frames.push_back (std::move (frame));

            if (disposalMethod == 2) // restore to background: clear this frame's region before the next one
            {
                for (int y = 0; y < imgHeight; ++y)
                {
                    const int canvasY = imgTop + y;
                    if (canvasY < 0 || canvasY >= screenHeight)
                        continue;

                    for (int x = 0; x < imgWidth; ++x)
                    {
                        const int canvasX = imgLeft + x;
                        if (canvasX < 0 || canvasX >= screenWidth)
                            continue;

                        const size_t offset = ((size_t) canvasY * (size_t) screenWidth + (size_t) canvasX) * 4;
                        canvas[offset + 0] = 0;
                        canvas[offset + 1] = 0;
                        canvas[offset + 2] = 0;
                        canvas[offset + 3] = 0;
                    }
                }
            }

            haveGce = false;
            gceTransparent = false;
            disposalMethod = 0;
            gceDelayMs = 100;

            if ((int) result.frames.size() >= maxFrames)
                break; // reached the memory-budget frame cap -- keep what decoded so far

            continue;
        }

        // Unknown/unsupported block type -- stop parsing gracefully rather
        // than risk an infinite loop on malformed data.
        break;
    }

    if (result.frames.empty())
    {
        result.width = 0;
        result.height = 0;
    }

    return result;
}

} // namespace w27::GifDecoder
