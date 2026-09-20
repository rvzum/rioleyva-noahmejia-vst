#include "EffectsChain.h"

namespace w27
{

namespace
{
    constexpr float bypassThreshold = 0.0001f;
}

EffectsChain::EffectsChain()
{
}

void EffectsChain::prepare (double sampleRate, int samplesPerBlock, int numChannels)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, (juce::uint32) numChannels };

    highPassFilter.prepare (spec);
    highPassFilter.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    highPassFilter.setCutoffFrequency (20.0f);
    highPassFilter.setResonance (0.7071f); // Butterworth Q -- no resonant peak, just a clean roll-off

    lowPassFilter.prepare (spec);
    lowPassFilter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    lowPassFilter.setCutoffFrequency (20000.0f);
    lowPassFilter.setResonance (0.7071f);

    chorus.prepare (spec);
    chorus.setRate (1.1f);
    chorus.setDepth (0.25f);
    chorus.setCentreDelay (7.0f);
    chorus.setFeedback (0.0f);
    chorus.setMix (0.0f);

    phaser.prepare (spec);
    phaser.setRate (0.6f);
    phaser.setDepth (0.7f);
    phaser.setCentreFrequency (700.0f);
    phaser.setFeedback (0.3f);
    phaser.setMix (0.0f);

    reverb.prepare (spec); // also sets the wrapped juce::Reverb's sample rate
    {
        juce::dsp::Reverb::Parameters params;
        params.roomSize = 0.6f;
        params.damping = 0.4f;
        params.wetLevel = 0.0f;
        params.dryLevel = 1.0f;
        params.width = 1.0f;
        params.freezeMode = 0.0f;
        reverb.setParameters (params);
    }

    // ~2ms base + up to ~6ms of LFO sweep -- classic flanger range. A
    // little headroom on top of that (10ms) as the max, since
    // setMaximumDelayInSamples() also clears the line and must be called
    // before use.
    flangerDelay.prepare (spec);
    flangerDelay.setMaximumDelayInSamples ((int) (0.01 * sampleRate) + 8);
    flangerLfoPhase = 0.0f;

    // Fixed 350ms echo, up to 1.5s of headroom in the line itself (not
    // currently user-adjustable, but keeps setMaximumDelayInSamples() from
    // needing to change if a tempo-synced or adjustable time is added
    // later).
    delayLine.prepare (spec);
    delayLine.setMaximumDelayInSamples ((int) (1.5 * sampleRate) + 8);
    delayLine.setDelay ((float) (0.35 * sampleRate));

    volumeSmoothed.reset (sampleRate, 0.02); // 20ms glide -- avoids a click when Volume is moved during playback
    volumeSmoothed.setCurrentAndTargetValue (volume.load());

    reset();
}

void EffectsChain::reset()
{
    highPassFilter.reset();
    lowPassFilter.reset();
    chorus.reset();
    phaser.reset();
    reverb.reset();
    flangerDelay.reset();
    delayLine.reset();
}

void EffectsChain::process (juce::AudioBuffer<float>& buffer)
{
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    if (numChannels <= 0 || numSamples <= 0)
        return;

    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> context (block);

    // -- High Pass ------------------------------------------------------
    const float hpAmount = highPassAmount.load();
    if (hpAmount > bypassThreshold)
    {
        // 20 Hz (no audible cut) .. 5000 Hz (heavy cut), logarithmic so the
        // knob feels even across its range rather than all the action
        // being crammed into the first few percent.
        const float freq = 20.0f * std::pow (250.0f, hpAmount);
        highPassFilter.setCutoffFrequency (freq);
        highPassFilter.process (context);
    }

    // -- Low Pass ---------------------------------------------------------
    const float lpAmount = lowPassAmount.load();
    if (lpAmount > bypassThreshold)
    {
        // 20000 Hz (no audible cut) .. 200 Hz (heavy cut), logarithmic.
        const float freq = 20000.0f * std::pow (200.0f / 20000.0f, lpAmount);
        lowPassFilter.setCutoffFrequency (freq);
        lowPassFilter.process (context);
    }

    // -- Distortion (hand-rolled tanh waveshaper) --------------------------
    const float distAmount = distortionAmount.load();
    if (distAmount > bypassThreshold)
    {
        const float driveGain = 1.0f + distAmount * 9.0f; // 1x (clean) .. 10x (hard clip)
        const float normalise = std::tanh (driveGain);     // keeps perceived loudness roughly constant as drive increases

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* data = buffer.getWritePointer (ch);

            for (int i = 0; i < numSamples; ++i)
            {
                const float dry = data[i];
                const float wet = std::tanh (dry * driveGain) / normalise;
                data[i] = dry + (wet - dry) * distAmount; // amount also doubles as the dry/wet blend
            }
        }
    }

    // -- Chorus (juce::dsp::Chorus, native mix knob) -----------------------
    const float chorusAmt = chorusAmount.load();
    if (chorusAmt > bypassThreshold)
    {
        chorus.setMix (chorusAmt);
        chorus.process (context);
    }

    // -- Flanger (hand-rolled: short LFO-swept feedback delay) -------------
    const float flangerAmt = flangerAmount.load();
    if (flangerAmt > bypassThreshold)
    {
        const float baseDelaySamples = (float) (0.002 * currentSampleRate);       // 2ms
        const float depthSamples = flangerAmt * (float) (0.006 * currentSampleRate); // up to +6ms
        const float feedback = 0.35f;
        const float lfoIncrement = (float) (juce::MathConstants<double>::twoPi * 0.25 / currentSampleRate); // 0.25 Hz sweep

        for (int i = 0; i < numSamples; ++i)
        {
            const float lfo = 0.5f * (1.0f + std::sin (flangerLfoPhase));
            flangerDelay.setDelay (baseDelaySamples + depthSamples * lfo);

            for (int ch = 0; ch < numChannels; ++ch)
            {
                auto* data = buffer.getWritePointer (ch);
                const float input = data[i];
                const float delayed = flangerDelay.popSample (ch);
                flangerDelay.pushSample (ch, input + delayed * feedback);
                data[i] = input + (delayed - input) * flangerAmt;
            }

            flangerLfoPhase += lfoIncrement;
            if (flangerLfoPhase > juce::MathConstants<float>::twoPi)
                flangerLfoPhase -= juce::MathConstants<float>::twoPi;
        }
    }

    // -- Phaser (juce::dsp::Phaser, native mix knob) -----------------------
    const float phaserAmt = phaserAmount.load();
    if (phaserAmt > bypassThreshold)
    {
        phaser.setMix (phaserAmt);
        phaser.process (context);
    }

    // -- Delay (hand-rolled: fixed-time feedback echo) ---------------------
    const float delayAmt = delayAmount.load();
    if (delayAmt > bypassThreshold)
    {
        const float feedback = 0.35f;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* data = buffer.getWritePointer (ch);

            for (int i = 0; i < numSamples; ++i)
            {
                const float input = data[i];
                const float delayed = delayLine.popSample (ch);
                delayLine.pushSample (ch, input + delayed * feedback);
                data[i] = input + (delayed - input) * delayAmt;
            }
        }
    }

    // -- Reverb (juce::dsp::Reverb, crossfaded dry/wet) ---------------------
    const float reverbAmt = reverbAmount.load();
    if (reverbAmt > bypassThreshold && numChannels <= 2)
    {
        auto params = reverb.getParameters();
        params.wetLevel = reverbAmt * 0.6f;
        params.dryLevel = 1.0f - reverbAmt * 0.6f;
        reverb.setParameters (params);
        reverb.process (context);
    }

    // -- Volume (final output gain, smoothed to avoid a click if moved -----
    // while a note is ringing)
    volumeSmoothed.setTargetValue (volume.load());
    for (int i = 0; i < numSamples; ++i)
    {
        const float gain = volumeSmoothed.getNextValue();

        for (int ch = 0; ch < numChannels; ++ch)
            buffer.getWritePointer (ch)[i] *= gain;
    }
}

} // namespace w27
