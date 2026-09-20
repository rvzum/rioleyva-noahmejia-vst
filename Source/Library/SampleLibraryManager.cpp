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

SampleLibraryManager::SampleLibraryManager()
{
    auto rootDir = getRuntimeLibraryRoot();
    if (! rootDir.exists())
        rootDir.createDirectory(); // best-effort -- gives the user an obvious place to drop files

    rescan();
}

void SampleLibraryManager::rescan()
{
    root = Node{};
    flatSamples.clear();

    auto rootDir = getRuntimeLibraryRoot();
    if (! rootDir.isDirectory())
        return;

    root = buildFolderNode (rootDir, {}, flatSamples);
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
