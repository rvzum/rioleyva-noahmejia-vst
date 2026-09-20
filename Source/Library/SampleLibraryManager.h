#pragma once

#include <juce_core/juce_core.h>
#include <vector>

namespace w27
{

/**
    Enumerates the oneshot sample libraries the plugin can play, read live
    from TWO fixed folders on disk -- rather than embedded into the plugin
    binary at build time. Real Rio Leyva packs run into the gigabytes, and
    compiling that much data into the binary (the approach
    Effects/BackgroundManager still uses for its much smaller GIFs/frames)
    made builds impractically slow and would have produced a
    multi-gigabyte plugin bundle, so samples are the one asset type that
    stays on disk and gets read at runtime instead.

    The two folders are merged into one flat tree/list (see rescan()):
    - getRuntimeLibraryRoot() (~/Music/27wav rioleyva & noahmejia banks/)
      -- per-user, for libraries YOU drop in by hand. Created automatically
      if missing so there's always somewhere obvious to put files.
    - getBundledLibraryRoot() (/Library/Application Support/27wav/Sample
      Banks on macOS, C:\ProgramData\27wav\Sample Banks on Windows) --
      machine-wide "factory" content an installer places there (see
      Scripts/build_installer.sh / Packaging/windows/installer.iss), so a
      fresh install on someone else's machine has sounds to play
      immediately without any separate file transfer. A machine-wide
      location was chosen specifically because it needs no per-user
      elevation trickery from either installer -- see Packaging/README.md.
      Never auto-created by the app itself (only an installer should
      populate it).

    Per explicit user request, the on-disk folder tree is mirrored EXACTLY
    in the plugin's library menu, at whatever nesting depth it actually
    has -- no flattening. getRootNode() returns that tree: each Node is
    either a folder (isFolder == true, with child Nodes -- a mix of
    subfolders and/or sample files, in the same order the popup menu
    should show them) or a sample leaf (isFolder == false, filePath/
    relativePath point at one audio file). A folder branch that contains
    no audio file anywhere beneath it (recursively) is pruned entirely, so
    empty submenus never show up. PluginEditor walks this tree directly to
    build a nested juce::PopupMenu with one submenu per subfolder, however
    deep that goes.

    getNumSamples()/getSampleAt() additionally expose every leaf as one
    flat, depth-first-ordered list, independent of the tree -- this is
    what the toolbar's oneshot prev/next arrows step through, since
    "next sound" needs a single linear order regardless of which submenu
    things are nested under.

    rescan() re-walks the folder from scratch and is cheap (a filesystem
    directory walk, not decoding any audio) -- PluginEditor calls it every
    time the "Select Sound..." button is opened, so newly dropped files
    show up immediately with no plugin reload or rebuild needed. The
    constructor also calls it once, and creates the runtime root folder if
    it doesn't exist yet so there's always somewhere obvious to drop files.

    Empty (getNumSamples() == 0) until the user actually has files under
    the runtime root -- PluginEditor's library button shows a "no
    libraries yet" placeholder pointing at the folder in that case, rather
    than an empty/broken menu.
*/
class SampleLibraryManager
{
public:
    SampleLibraryManager();

    // One node in the on-disk folder tree -- see the class doc comment.
    struct Node
    {
        juce::String name;                 // this folder's/file's own name (file name has no extension)
        bool isFolder = true;
        juce::File filePath;                // only meaningful when !isFolder
        juce::String relativePath;          // only meaningful when !isFolder -- see Sample::relativePath
        int flatIndex = -1;                 // only meaningful when !isFolder -- index into the flat sample list
        std::vector<Node> children;         // only meaningful when isFolder
    };

    // One sample, addressed the same way regardless of how deep it's
    // nested -- this is the flat, depth-first ordering of every leaf in
    // getRootNode().
    struct Sample
    {
        juce::String displayName;   // leaf file's own name, no extension
        juce::String relativePath;  // path from the runtime root, forward-slash separated, WITH
                                     // extension -- a stable identity that survives libraries being
                                     // added/removed elsewhere; used for save/restore (see
                                     // PluginProcessor::getStateInformation/setStateInformation) and
                                     // for the prev/next toolbar arrows.
        juce::File filePath;
    };

    // ~/Music/27wav rioleyva & noahmejia banks -- fixed, not user-configurable (yet).
    static juce::File getRuntimeLibraryRoot();

    // /Library/Application Support/27wav/Sample Banks (macOS) or
    // C:\ProgramData\27wav\Sample Banks (Windows) -- see the class doc
    // comment above.
    static juce::File getBundledLibraryRoot();

    // Re-walks both getRuntimeLibraryRoot() and getBundledLibraryRoot()
    // from scratch and merges them into one tree/list. Safe to call
    // anytime from the message thread; not real-time safe (does file
    // I/O), so never call this from the audio thread.
    void rescan();

    // Root of the on-disk folder tree; its own name/relativePath/filePath
    // are meaningless (it represents the runtime root folder itself) --
    // only its children matter. See the class doc comment.
    const Node& getRootNode() const noexcept { return root; }

    int getNumSamples() const noexcept { return (int) flatSamples.size(); }
    const Sample* getSampleAt (int flatIndex) const;

    // -1 if not found (e.g. it was removed from disk since being selected).
    int indexOfRelativePath (const juce::String& relativePath) const;

    const Sample* findSampleByRelativePath (const juce::String& relativePath) const;

private:
    Node root;
    std::vector<Sample> flatSamples; // depth-first order, matches Node::flatIndex

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SampleLibraryManager)
};

} // namespace w27
