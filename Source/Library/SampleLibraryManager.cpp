#include "SampleLibraryManager.h"
#include <algorithm>

namespace w27
{

namespace
{
    bool isSupportedAudioFile (const juce::File& f)
    {
        const auto ext = f.getFileExtension().toLowerCase();
        return ext == ".wav" || ext == ".aif" || ext == ".aiff" || ext == ".mp3";
    }

    bool branchHasAnySample (const SampleLibraryManager::Node& node)
    {
        if (! node.isFolder)
            return true;

        for (auto& c : node.children)
            if (branchHasAnySample (c))
                return true;

        return false;
    }

    // Builds one folder node mirroring "dir" exactly (subfolders and
    // sample files, alphabetically interleaved), recursing into every
    // subfolder, and appends every leaf sample found -- in the same
    // depth-first order used for display -- to flatSamples so tree order
    // and flat order always agree.
    SampleLibraryManager::Node buildFolderNode (const juce::File& dir,
                                                 const juce::String& relativePrefix,
                                                 std::vector<SampleLibraryManager::Sample>& flatSamples)
    {
        SampleLibraryManager::Node node;
        node.name = dir.getFileName();
        node.isFolder = true;

        struct RawEntry { juce::String name; bool isDir; juce::File file; };
        std::vector<RawEntry> raw;

        for (auto& f : dir.findChildFiles (juce::File::findDirectories, false))
            raw.push_back ({ f.getFileName(), true, f });

        for (auto& f : dir.findChildFiles (juce::File::findFiles, false))
            if (isSupportedAudioFile (f))
                raw.push_back ({ f.getFileNameWithoutExtension(), false, f });

        std::sort (raw.begin(), raw.end(), [] (const RawEntry& a, const RawEntry& b)
        {
            return a.name.compareIgnoreCase (b.name) < 0;
        });

        for (auto& e : raw)
        {
            if (e.isDir)
            {
                const juce::String childPrefix = relativePrefix.isEmpty()
                                                      ? e.name
                                                      : relativePrefix + "/" + e.name;
                auto child = buildFolderNode (e.file, childPrefix, flatSamples);

                if (branchHasAnySample (child))
                    node.children.push_back (std::move (child));
            }
            else
            {
                SampleLibraryManager::Node leaf;
                leaf.name = e.name;
                leaf.isFolder = false;
                leaf.filePath = e.file;
                leaf.relativePath = relativePrefix.isEmpty()
                                         ? e.file.getFileName()
                                         : relativePrefix + "/" + e.file.getFileName();
                leaf.flatIndex = (int) flatSamples.size();

                flatSamples.push_back ({ leaf.name, leaf.relativePath, leaf.filePath });
                node.children.push_back (std::move (leaf));
            }
        }

        return node;
    }
}

juce::File SampleLibraryManager::getRuntimeLibraryRoot()
{
    return juce::File::getSpecialLocation (juce::File::userMusicDirectory)
             .getChildFile ("27wav rioleyva & noahmejia banks");
}

juce::File SampleLibraryManager::getBundledLibraryRoot()
{
    return juce::File::getSpecialLocation (juce::File::commonApplicationDataDirectory)
             .getChildFile ("27wav")
             .getChildFile ("Sample Banks");
}

SampleLibraryManager::SampleLibraryManager()
{
    auto rootDir = getRuntimeLibraryRoot();
    if (! rootDir.exists())
        rootDir.createDirectory(); // best-effort -- gives the user an obvious place to drop files

    // getBundledLibraryRoot() is deliberately NEVER auto-created here --
    // only an installer should populate it (see the class doc comment).

    rescan();
}

void SampleLibraryManager::rescan()
{
    root = Node{};
    flatSamples.clear();

    // Both roots merge into one flat tree/list, in order: the user's own
    // manually-added libraries first, then any machine-wide "factory"
    // content an installer bundled -- see the class doc comment.
    // flatSamples is passed by reference into both buildFolderNode()
    // calls, so its indices stay one contiguous, depth-first sequence
    // spanning both roots; callers (prev/next toolbar arrows, save/
    // restore) have no idea there are two roots at all.
    for (auto rootDir : { getRuntimeLibraryRoot(), getBundledLibraryRoot() })
    {
        if (! rootDir.isDirectory())
            continue;

        auto node = buildFolderNode (rootDir, {}, flatSamples);
        for (auto& child : node.children)
            root.children.push_back (std::move (child));
    }
}

const SampleLibraryManager::Sample* SampleLibraryManager::getSampleAt (int flatIndex) const
{
    if (flatIndex < 0 || flatIndex >= (int) flatSamples.size())
        return nullptr;

    return &flatSamples[(size_t) flatIndex];
}

int SampleLibraryManager::indexOfRelativePath (const juce::String& relativePath) const
{
    for (size_t i = 0; i < flatSamples.size(); ++i)
        if (flatSamples[i].relativePath == relativePath)
            return (int) i;

    return -1;
}

const SampleLibraryManager::Sample* SampleLibraryManager::findSampleByRelativePath (const juce::String& relativePath) const
{
    const int idx = indexOfRelativePath (relativePath);
    return idx >= 0 ? &flatSamples[(size_t) idx] : nullptr;
}

} // namespace w27
