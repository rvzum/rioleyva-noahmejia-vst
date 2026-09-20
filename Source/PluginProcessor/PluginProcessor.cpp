#include "PluginProcessor.h"
#include "../PluginEditor/PluginEditor.h"
#include "../Composition/MelodySequencer.h"
#include <vector>

namespace w27
{

W27PluginProcessor::W27PluginProcessor()
    : juce::AudioProcessor (BusesProperties()
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
    // A stereo output bus is declared purely so hosts (FL Studio in
    // particular) categorize this as an instrument and allow it on a
    // Channel Rack slot, where it actually receives MIDI -- see
    // README.md's "FL Studio MIDI routing" section. It now also carries
    // this plugin's own real audio output (see processBlock()), where it
    // previously carried unconditional silence.
{
}

W27PluginProcessor::~W27PluginProcessor() = default;

void W27PluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    midiInputHandler.prepare (sampleRate);
    samplerEngine.prepare (sampleRate, samplesPerBlock);
    effectsChain.prepare (sampleRate, samplesPerBlock, getTotalNumOutputChannels());
    // Full playback/timeline synchronisation (host tempo, bar/beat position)
    // is still deferred for the visualization layer (Phase 3-4 -- see
    // Source/Visualization/README.md); the melody engine below gets its own
    // per-block tempo/position directly from AudioPlayHead instead.
}

void W27PluginProcessor::releaseResources()
{
    samplerEngine.allNotesOff();
}

void W27PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    // Ask the host whether the transport is actually playing, so the
    // visualization can stay still (matching FL Studio's own Piano Roll)
    // until playback starts -- see MidiInputHandler::processBlock(). Not
    // every host/wrapper exposes this (e.g. some standalone configurations);
    // default to "playing" in that case so the view behaves as it always
    // has, rather than silently freezing forever.
    //
    // Also grab the host's actual transport position in seconds, its
    // ppqPosition (beats since the host's own time zero) and its current
    // tempo, when available. Seconds drives PianoRollView's PATTERN mode
    // (see MidiInputHandler::updateHostPosition()); ppqPosition + tempo
    // drive the composed-melody loop below and PianoRollView's Compose mode
    // (see MidiInputHandler::updateHostMusicalPosition()) -- both use the
    // same "derive fresh from the host's own position every block, never
    // accumulate our own clock" approach, for the same reasons documented
    // in Visualization/README.md (play/stop, host loop repeats, and
    // dragging the host's own position slider all "just work" as a result).
    bool hostIsPlaying = true;
    double hostPositionSeconds = 0.0;
    bool hostPositionKnown = false;
    double hostPpqPosition = 0.0;
    double hostBpm = 120.0;
    bool hostMusicalPositionKnown = false;

    if (auto* playHead = getPlayHead())
    {
        if (const auto position = playHead->getPosition())
        {
            hostIsPlaying = position->getIsPlaying();

            if (const auto timeInSeconds = position->getTimeInSeconds())
            {
                hostPositionSeconds = *timeInSeconds;
                hostPositionKnown = true;
            }

            const auto ppq = position->getPpqPosition();
            const auto bpm = position->getBpm();
            if (ppq && bpm)
            {
                hostPpqPosition = *ppq;
                hostBpm = *bpm;
                hostMusicalPositionKnown = true;
            }
        }
    }

    // Capture note events into NoteStorage for the visual layer.
    midiInputHandler.processBlock (midiMessages, buffer.getNumSamples(), hostIsPlaying);
    midiInputHandler.updateHostPosition (hostPositionSeconds, hostPositionKnown);
    midiInputHandler.updateHostMusicalPosition (hostPpqPosition, hostBpm, hostMusicalPositionKnown);

    // -- Build this block's MIDI for the sampler: live host MIDI (so -------
    // playing a keyboard/Channel Rack pattern is now audible, not just
    // visualized -- and already includes real note-offs, so it stops
    // correctly on key release too) plus the composed melody's own
    // generated note-on/note-off pairs. A melody only actually plays back
    // when the host is both playing AND reports a musical position -- see
    // MelodySequencer's doc comment.
    scratchSynthMidi.clear();
    scratchSynthMidi.addEvents (midiMessages, 0, buffer.getNumSamples(), 0);

    const bool melodyPlaybackActive = hostIsPlaying && hostMusicalPositionKnown;

    std::vector<MelodySequencer::Trigger> triggers;
    MelodySequencer::collectTriggers (composedMelody, melodyPlaybackActive, hostPpqPosition, hostBpm,
                                       getSampleRate(), buffer.getNumSamples(), triggers);

    for (const auto& trigger : triggers)
    {
        const auto message = trigger.isNoteOn
                                  ? juce::MidiMessage::noteOn (1, trigger.noteNumber, trigger.velocity)
                                  : juce::MidiMessage::noteOff (1, trigger.noteNumber);
        scratchSynthMidi.addEvent (message, trigger.sampleOffset);
    }

    // This plugin's only audio output is the sampler engine now -- clear
    // first (the buffer may contain whatever was left over from a previous
    // block/host) then let the synth write into it.
    buffer.clear();
    samplerEngine.renderNextBlock (buffer, scratchSynthMidi, 0, buffer.getNumSamples());

    // Reverb/Delay/Chorus/Phaser/Flanger/Distortion/High Pass/Low Pass +
    // the final Volume gain stage -- see Audio/EffectsChain. Runs on
    // whatever the sampler just rendered, every block, regardless of
    // whether any knob is actually turned up (each stage no-ops itself
    // at 0% -- see EffectsChain::process()).
    effectsChain.process (buffer);
}

bool W27PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // No inputs; stereo (or mono) output only.
    if (! layouts.getMainInputChannelSet().isDisabled())
        return false;

    const auto mainOutput = layouts.getMainOutputChannelSet();
    return mainOutput == juce::AudioChannelSet::stereo()
        || mainOutput == juce::AudioChannelSet::mono();
}

juce::AudioProcessorEditor* W27PluginProcessor::createEditor()
{
    return new W27PluginEditor (*this);
}

bool W27PluginProcessor::hasEditor() const
{
    return true;
}

const juce::String W27PluginProcessor::getName() const
{
    return JucePlugin_Name;
}

bool W27PluginProcessor::acceptsMidi() const
{
    return true;
}

bool W27PluginProcessor::producesMidi() const
{
    return false;
}

bool W27PluginProcessor::isMidiEffect() const
{
    return false;
}

double W27PluginProcessor::getTailLengthSeconds() const
{
    // A note now stops on its own note-off rather than always ringing out
    // to the sample's natural end (see Audio/SamplerEngine), with only a
    // short release fade after that (SamplerSound's release time -- 0.03s
    // as of this writing). This just needs to comfortably cover that
    // release so hosts that use it to decide when it's safe to stop
    // calling processBlock() (e.g. freezing/bouncing a track) don't cut
    // the fade off early.
    return 0.5;
}

int W27PluginProcessor::getNumPrograms()
{
    return 1;
}

int W27PluginProcessor::getCurrentProgram()
{
    return 0;
}

void W27PluginProcessor::setCurrentProgram (int /*index*/)
{
}

const juce::String W27PluginProcessor::getProgramName (int /*index*/)
{
    return {};
}

void W27PluginProcessor::changeProgramName (int /*index*/, const juce::String& /*newName*/)
{
}

void W27PluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::XmlElement root ("W27PluginState");
    root.setAttribute ("selectedSamplePath", selectedSampleRelativePath);
    root.setAttribute ("selectedSampleName", selectedSampleName);

    root.setAttribute ("reverbAmount", (double) effectsChain.getReverbAmount());
    root.setAttribute ("delayAmount", (double) effectsChain.getDelayAmount());
    root.setAttribute ("chorusAmount", (double) effectsChain.getChorusAmount());
    root.setAttribute ("phaserAmount", (double) effectsChain.getPhaserAmount());
    root.setAttribute ("flangerAmount", (double) effectsChain.getFlangerAmount());
    root.setAttribute ("distortionAmount", (double) effectsChain.getDistortionAmount());
    root.setAttribute ("highPassAmount", (double) effectsChain.getHighPassAmount());
    root.setAttribute ("lowPassAmount", (double) effectsChain.getLowPassAmount());
    root.setAttribute ("volume", (double) effectsChain.getVolume());

    if (auto melodyXml = composedMelody.toXml())
        root.addChildElement (melodyXml.release());

    copyXmlToBinary (root, destData);
}

void W27PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> root (getXmlFromBinary (data, sizeInBytes));
    if (root == nullptr)
        return;

    selectedSampleRelativePath = root->getStringAttribute ("selectedSamplePath");
    selectedSampleName = root->getStringAttribute ("selectedSampleName");

    // Defaults match EffectsChain's own in-class member initialisers, so
    // a project saved before this feature existed (missing these
    // attributes entirely) loads with every effect off and Volume at 75%,
    // same as a brand new plugin instance.
    effectsChain.setReverbAmount ((float) root->getDoubleAttribute ("reverbAmount", 0.0));
    effectsChain.setDelayAmount ((float) root->getDoubleAttribute ("delayAmount", 0.0));
    effectsChain.setChorusAmount ((float) root->getDoubleAttribute ("chorusAmount", 0.0));
    effectsChain.setPhaserAmount ((float) root->getDoubleAttribute ("phaserAmount", 0.0));
    effectsChain.setFlangerAmount ((float) root->getDoubleAttribute ("flangerAmount", 0.0));
    effectsChain.setDistortionAmount ((float) root->getDoubleAttribute ("distortionAmount", 0.0));
    effectsChain.setHighPassAmount ((float) root->getDoubleAttribute ("highPassAmount", 0.0));
    effectsChain.setLowPassAmount ((float) root->getDoubleAttribute ("lowPassAmount", 0.0));
    effectsChain.setVolume ((float) root->getDoubleAttribute ("volume", 0.75));

    if (auto* melodyXml = root->getChildByName ("Melody"))
        composedMelody.restoreFromXml (*melodyXml);

    // Re-scan first: the project may be reopened after packs were added,
    // removed or renamed on disk since it was last saved.
    sampleLibraryManager.rescan();

    if (selectedSampleRelativePath.isNotEmpty())
    {
        if (auto* sample = sampleLibraryManager.findSampleByRelativePath (selectedSampleRelativePath))
            samplerEngine.loadSampleFromFile (sample->filePath, sample->displayName);
    }
}

bool W27PluginProcessor::selectSample (int flatIndex)
{
    if (auto* sample = sampleLibraryManager.getSampleAt (flatIndex))
    {
        if (samplerEngine.loadSampleFromFile (sample->filePath, sample->displayName))
        {
            selectedSampleRelativePath = sample->relativePath;
            selectedSampleName = sample->displayName;
            return true;
        }
    }

    return false;
}

} // namespace w27

// This creates new instances of the plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new w27::W27PluginProcessor();
}
