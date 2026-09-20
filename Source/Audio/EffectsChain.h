#pragma once

#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <cmath>

namespace w27
{

/**
    Post-sampler audio effects stage -- added per explicit user request for
    a row of effect "knobs" at the bottom of the plugin (Reverb, Delay,
    Chorus, Phaser, Flanger, Distortion, High Pass, Low Pass) plus a
    top-right Volume knob controlling the played note's overall loudness.

    Owned by W27PluginProcessor, run once per block in processBlock()
    immediately after SamplerEngine::renderNextBlock() -- this class never
    touches MIDI or sample loading, only the rendered audio.

    Every knob is exposed as a plain 0-1 "amount" (the UI shows 0-100%),
    read from a std::atomic<float> so the message-thread sliders in
    PluginEditor can write to it directly while the audio thread reads it
    in process() -- the same lock-free pattern already used elsewhere in
    this codebase (e.g. ComposedMelody). For the six "wet/dry" style
    effects (Reverb, Delay, Chorus, Phaser, Flanger, Distortion), 0% means
    fully dry/untouched and each stage is skipped entirely at exactly 0%
    for a guaranteed true bypass (no residual coloration from a filter or
    delay line still technically running at a near-zero setting). For the
    two filters, 0% means "no filtering at all" (High Pass at its lowest
    cutoff, Low Pass at its highest) rather than a wet/dry blend, since a
    cutoff-frequency knob is the more natural way to control a filter than
    a mix knob would be; they're likewise skipped entirely at 0% for the
    same true-bypass guarantee.

    Signal order (deliberately filters/drive first, modulation next,
    time-based effects after, output gain last -- the conventional pedal-
    board/channel-strip ordering): High Pass -> Low Pass -> Distortion ->
    Chorus -> Flanger -> Phaser -> Delay -> Reverb -> Volume.

    Chorus and Phaser use JUCE's own juce::dsp::Chorus/Phaser (both have a
    built-in setMix() the amount knob drives directly). Reverb uses
    juce::dsp::Reverb, crossfading its own dryLevel/wetLevel parameters.
    High Pass/Low Pass use juce::dsp::StateVariableTPTFilter. Distortion,
    Flanger and Delay have no ready-made JUCE class that does what's
    wanted here (a simple wet-amount knob), so they're hand-rolled: a
    tanh() waveshaper for Distortion, a short modulated feedback delay for
    Flanger, and a fixed-time feedback echo for Delay -- all three built on
    top of juce::dsp::DelayLine where they need one.
*/
class EffectsChain
{
public:
    EffectsChain();

    void prepare (double sampleRate, int samplesPerBlock, int numChannels);
    void reset();

    // Processes the buffer in place, in the fixed order documented above.
    void process (juce::AudioBuffer<float>& buffer);

    // -- Effect amount knobs -- 0-1 (UI shows 0-100%), all default to 0 -----
    // (fully bypassed) except Volume, which defaults to 0.75 (75%) per
    // explicit user request.
    void setReverbAmount (float v)      { reverbAmount.store (juce::jlimit (0.0f, 1.0f, v)); }
    void setDelayAmount (float v)       { delayAmount.store (juce::jlimit (0.0f, 1.0f, v)); }
    void setChorusAmount (float v)      { chorusAmount.store (juce::jlimit (0.0f, 1.0f, v)); }
    void setPhaserAmount (float v)      { phaserAmount.store (juce::jlimit (0.0f, 1.0f, v)); }
    void setFlangerAmount (float v)     { flangerAmount.store (juce::jlimit (0.0f, 1.0f, v)); }
    void setDistortionAmount (float v)  { distortionAmount.store (juce::jlimit (0.0f, 1.0f, v)); }
    void setHighPassAmount (float v)    { highPassAmount.store (juce::jlimit (0.0f, 1.0f, v)); }
    void setLowPassAmount (float v)     { lowPassAmount.store (juce::jlimit (0.0f, 1.0f, v)); }

    // Final output gain -- 0-1 (UI shows 0-100%), defaults to 0.75.
    void setVolume (float v)            { volume.store (juce::jlimit (0.0f, 1.0f, v)); }

    float getReverbAmount() const      { return reverbAmount.load(); }
    float getDelayAmount() const       { return delayAmount.load(); }
    float getChorusAmount() const      { return chorusAmount.load(); }
    float getPhaserAmount() const      { return phaserAmount.load(); }
    float getFlangerAmount() const     { return flangerAmount.load(); }
    float getDistortionAmount() const  { return distortionAmount.load(); }
    float getHighPassAmount() const    { return highPassAmount.load(); }
    float getLowPassAmount() const     { return lowPassAmount.load(); }
    float getVolume() const            { return volume.load(); }

private:
    std::atomic<float> reverbAmount { 0.0f };
    std::atomic<float> delayAmount { 0.0f };
    std::atomic<float> chorusAmount { 0.0f };
    std::atomic<float> phaserAmount { 0.0f };
    std::atomic<float> flangerAmount { 0.0f };
    std::atomic<float> distortionAmount { 0.0f };
    std::atomic<float> highPassAmount { 0.0f };
    std::atomic<float> lowPassAmount { 0.0f };
    std::atomic<float> volume { 0.75f };

    juce::dsp::StateVariableTPTFilter<float> highPassFilter;
    juce::dsp::StateVariableTPTFilter<float> lowPassFilter;

    juce::dsp::Chorus<float> chorus;
    juce::dsp::Phaser<float> phaser;
    juce::dsp::Reverb reverb;

    // Flanger: short modulated feedback delay (~2-8ms, LFO-swept). Separate
    // from the Delay effect's own DelayLine below -- different delay-time
    // range and each needs its own independent read/write position.
    juce::dsp::DelayLine<float> flangerDelay;
    float flangerLfoPhase = 0.0f;

    // Delay: fixed-time (350ms) feedback echo.
    juce::dsp::DelayLine<float> delayLine;

    juce::SmoothedValue<float> volumeSmoothed;

    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EffectsChain)
};

} // namespace w27
