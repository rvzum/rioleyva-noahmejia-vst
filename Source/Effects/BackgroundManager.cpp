#include "BackgroundManager.h"
#include "AnimatedGradientBackground.h"
#include "FrameSequenceBackground.h"
#include "GifBackground.h"
#include "GifDecoder.h"
#include "../Theme/ThemeColors.h"
#include <algorithm>
#include <map>

#if defined (W27_HAS_BACKGROUND_DATA)
 #include <BinaryData.h>
 #define W27_HAS_BACKGROUND_BINARY_DATA 1
#else
 #define W27_HAS_BACKGROUND_BINARY_DATA 0
#endif

namespace w27
{

namespace
{
    // Must match the fps ffmpeg extracts video-frame JPEGs at, in CMakeLists.txt's
    // background-media block ("-vf fps=12,..."). Only affects playback smoothness,
    // never correctness, if the two ever drift out of sync.
    constexpr int videoFrameFps = 12;
    constexpr int videoFrameDelayMs = 1000 / videoFrameFps;
}

BackgroundManager::BackgroundManager()
{
    // The built-in gradient is no longer a selectable list entry (per
    // explicit user request -- "оставь только те которые я добавил"): it
    // now exists only as the emergency fallback inside getOrCreateCurrent()
    // if a real background ever fails to decode. The selectable list is
    // exactly the .gif/.mp4 files embedded from Assets/Backgrounds/, below.
   #if W27_HAS_BACKGROUND_BINARY_DATA
    // Video-frame resources are named "<videoBase>_frame_NNNN_jpg" by the
    // CMake extraction step -- group them back together by <videoBase>,
    // ordered by frame number.
    std::map<juce::String, std::vector<std::pair<int, const char*>>> videoGroups;

    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
    {
        const char* name = BinaryData::namedResourceList[i];
        const juce::String resourceName (name);

        if (resourceName.endsWithIgnoreCase ("_gif"))
        {
            Entry entry;
            entry.kind = Kind::gif;
            const char* originalFilename = BinaryData::getNamedResourceOriginalFilename (name);
            entry.displayName = originalFilename != nullptr
                                     ? juce::File (originalFilename).getFileNameWithoutExtension()
                                     : resourceName;
            entry.gifBinaryDataName = name;
            entries.push_back (std::move (entry));
            continue;
        }

        if (resourceName.endsWithIgnoreCase ("_jpg") || resourceName.endsWithIgnoreCase ("_png"))
        {
            const int frameMarker = resourceName.lastIndexOf ("_frame_");
            if (frameMarker < 0)
                continue; // not a recognised video-frame name -- ignore

            const juce::String videoBase = resourceName.substring (0, frameMarker);
            const juce::String afterMarker = resourceName.substring (frameMarker + (int) juce::String ("_frame_").length());
            const juce::String frameDigits = afterMarker.upToFirstOccurrenceOf ("_", false, false);
            const int frameIndex = frameDigits.getIntValue();

            videoGroups[videoBase].push_back ({ frameIndex, name });
        }
    }

    for (auto& group : videoGroups)
    {
        std::sort (group.second.begin(), group.second.end(),
                   [] (const auto& a, const auto& b) { return a.first < b.first; });

        Entry entry;
        entry.kind = Kind::videoFrames;
        entry.displayName = group.first.replaceCharacter ('_', ' ');

        entry.videoFrameBinaryDataNames.reserve (group.second.size());
        for (auto& indexedName : group.second)
            entry.videoFrameBinaryDataNames.push_back (indexedName.second);

        entries.push_back (std::move (entry));
    }
   #endif
}

juce::String BackgroundManager::getCurrentName() const
{
    return getName (currentIndex);
}

juce::String BackgroundManager::getName (int index) const
{
    if (index < 0 || index >= (int) entries.size())
        return {};

    return entries[(size_t) index].displayName;
}

void BackgroundManager::selectNext()
{
    if (entries.empty())
        return;

    currentIndex = (currentIndex + 1) % (int) entries.size();
}

void BackgroundManager::selectPrevious()
{
    if (entries.empty())
        return;

    currentIndex = (currentIndex - 1 + (int) entries.size()) % (int) entries.size();
}

void BackgroundManager::select (int index)
{
    if (index >= 0 && index < (int) entries.size())
        currentIndex = index;
}

VisualEffect& BackgroundManager::getOrCreateCurrent()
{
    auto& entry = entries[(size_t) currentIndex];

    if (entry.instance == nullptr)
    {
       #if W27_HAS_BACKGROUND_BINARY_DATA
        if (entry.kind == Kind::gif && entry.gifBinaryDataName != nullptr)
        {
            int dataSize = 0;
            const char* data = BinaryData::getNamedResource (entry.gifBinaryDataName, dataSize);

            if (data != nullptr && dataSize > 0)
            {
                const auto decoded = GifDecoder::decode (data, (size_t) dataSize);
                entry.instance = createGifBackground (decoded);
            }
        }
        else if (entry.kind == Kind::videoFrames && ! entry.videoFrameBinaryDataNames.empty())
        {
            std::vector<FrameSequenceBackground::Frame> frames;
            frames.reserve (entry.videoFrameBinaryDataNames.size());

            for (const char* frameName : entry.videoFrameBinaryDataNames)
            {
                int dataSize = 0;
                const char* data = BinaryData::getNamedResource (frameName, dataSize);
                if (data == nullptr || dataSize <= 0)
                    continue;

                juce::Image image = juce::ImageFileFormat::loadFrom (data, (size_t) dataSize);
                if (image.isValid())
                    frames.push_back ({ std::move (image), videoFrameDelayMs });
            }

            if (! frames.empty())
            {
                auto sequence = std::make_unique<FrameSequenceBackground> (std::move (frames));
                if (sequence->isValid())
                    entry.instance = std::move (sequence);
            }
        }
       #endif

        if (entry.instance == nullptr)
            entry.instance = std::make_unique<AnimatedGradientBackground>(); // fallback / built-in
    }

    return *entry.instance;
}

void BackgroundManager::render (juce::Graphics& g, juce::Rectangle<float> bounds, double timeSeconds)
{
    if (entries.empty())
    {
        // No .gif/.mp4 has been added to Assets/Backgrounds/ yet -- fall
        // back to the built-in gradient rather than leaving the window
        // blank black.
        if (emptyListFallback == nullptr)
            emptyListFallback = std::make_unique<AnimatedGradientBackground>();

        emptyListFallback->render (g, bounds, timeSeconds);
        return;
    }

    getOrCreateCurrent().render (g, bounds, timeSeconds);
}

juce::Colour BackgroundManager::getCurrentAccentColour()
{
    if (entries.empty())
        return Theme::keyActiveHighlight;

    return getOrCreateCurrent().getAccentColour();
}

} // namespace w27
