#pragma once

#include "VisualEffect.h"
#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <memory>
#include <vector>

namespace w27
{

/**
    Owns the set of selectable backgrounds and which one is currently active:
    - any .gif files dropped into Assets/Backgrounds/ (embedded at build time)
    - any .mp4 files dropped into Assets/Backgrounds/ (CMake extracts frames
      to JPEGs via ffmpeg at build time, embedded the same way as GIFs --
      see Assets/Backgrounds/README.md)

    The built-in animated gradient is NOT a selectable entry (per explicit
    user request) -- it exists only as an internal emergency fallback,
    used if a real background somehow fails to decode, or if the list ends
    up empty (no files in Assets/Backgrounds/ yet).

    Backgrounds are decoded/loaded lazily, the first time they're selected,
    and cached afterwards, so having several (possibly large) backgrounds
    embedded doesn't slow down plugin load in the host.
*/
class BackgroundManager
{
public:
    BackgroundManager();

    int getNumBackgrounds() const noexcept { return (int) entries.size(); }
    int getCurrentIndex() const noexcept { return currentIndex; }

    juce::String getCurrentName() const;
    juce::String getName (int index) const;

    void selectNext();
    void selectPrevious();
    void select (int index);

    // Renders the currently-selected background. Falls back to the built-in
    // gradient if a background fails to load, so this never leaves the
    // screen blank.
    void render (juce::Graphics& g, juce::Rectangle<float> bounds, double timeSeconds);

    // Representative colour of the currently-selected background (see
    // VisualEffect::getAccentColour()). Used by PianoRollView so its
    // note-touch animation matches whatever background is active. Lazily
    // loads/creates the current background if it hasn't been already, same
    // as render() does.
    juce::Colour getCurrentAccentColour();

private:
    enum class Kind { gradient, gif, videoFrames };

    struct Entry
    {
        juce::String displayName;
        Kind kind = Kind::gradient;
        const char* gifBinaryDataName = nullptr;                // Kind::gif
        std::vector<const char*> videoFrameBinaryDataNames;     // Kind::videoFrames, in playback order
        std::unique_ptr<VisualEffect> instance;                 // created lazily on first render
    };

    VisualEffect& getOrCreateCurrent();

    std::vector<Entry> entries;
    int currentIndex = 0;

    // Used only when entries is empty (no .gif/.mp4 added yet) so render()
    // has something other than a blank black screen to show.
    std::unique_ptr<VisualEffect> emptyListFallback;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BackgroundManager)
};

} // namespace w27
