#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "../Visualization/PianoRollView.h"
#include "PremiumLookAndFeel.h"
#include "ShuffleIconButton.h"

namespace w27
{

class W27PluginProcessor;

/**
    RIO LEYVA x NOAH MEJIA VST -- plugin editor.

    Thin container/controller, per the project's architecture rules: all
    real rendering lives in Visualization/PianoRollView (backed by
    Effects/BackgroundManager for selectable backgrounds). This class only
    wires up a small toolbar to PianoRollView's public API -- it never
    touches MIDI data, NoteStorage, or juce::MidiMessage/MidiBuffer
    directly.

    Toolbar controls added for the melody-writing feature:
    - "Write Melody" (next to the brand label): toggles PianoRollView's
      Compose mode on/off -- see PianoRollView::setComposeModeEnabled().
    - "Library" (top-centre, per explicit request): opens a popup menu
      mirroring the on-disk folder tree under
      ~/Music/27wav rioleyva & noahmejia banks/ exactly, however deeply nested (see
      Library/SampleLibraryManager), and asks the processor to load
      whichever sample is picked (W27PluginProcessor::selectSample()).
    - Two small arrows immediately to its right step to the
      previous/next oneshot in that same library, in flat depth-first
      order, without opening the menu (see stepSample()).
    - A "randomizer" toggle button (crossed-arrows glyph) immediately to
      the right of those arrows: while pressed/on, the same two arrows
      pick a random sample instead of stepping sequentially (see
      stepSample()).
    - A row of 8 effect knobs along the bottom (Reverb, Delay, Chorus,
      Phaser, Flanger, Distortion, High Pass, Low Pass) plus a Volume
      knob at the top right -- all wired straight to
      W27PluginProcessor::getEffectsChain() (see Audio/EffectsChain).

    Per earlier explicit user request, no status/diagnostic text is shown:
    no background-file-name label, no bottom status bar. The brand label
    ("27wav") is a clickable juce::HyperlinkButton that opens
    https://www.instagram.com/27wav/ in the user's default browser. The
    background-switching arrows that used to sit to its left were removed
    per later explicit user request (PianoRollView::selectNextBackground()/
    selectPreviousBackground() are still there, just no longer wired to a
    toolbar button).
*/
class W27PluginEditor : public juce::AudioProcessorEditor
{
public:
    explicit W27PluginEditor (W27PluginProcessor&);
    ~W27PluginEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void showLibraryMenu();
    void updateLibraryButtonText();

    // Wires up one rotary effect/volume knob: sets its range/style,
    // labels it, initialises its value from the processor's current
    // EffectsChain state (so reopening the editor window shows whatever
    // was last set, not always the default), and forwards further
    // changes to onAmountChanged (which the caller wires to the matching
    // EffectsChain setter). defaultPercent is only the value shown
    // before there's ever been any saved state -- see setupKnob()'s
    // definition. accentColour gives this knob its own distinct colour
    // (Theme::effectXxx / Theme::premiumGold), matching the colour that
    // same effect paints on the notes in PianoRollView -- see
    // ThemeColors.h's "per-effect accent colours" section.
    void setupKnob (juce::Slider& slider, juce::Label& label, const juce::String& caption,
                    float currentAmount, float doubleClickDefaultPercent, juce::Colour accentColour,
                    std::function<void (float)> onAmountChanged);

    // Positions one label+knob pair as a small vertical stack (label on
    // top, rotary slider filling the rest of the given area).
    static void layoutKnob (juce::Rectangle<int> area, juce::Label& label, juce::Slider& slider);

    // Steps to the previous (delta < 0) or next (delta > 0) oneshot in
    // SampleLibraryManager's flat sample order, wrapping around at either
    // end. Does nothing if there are no samples at all.
    void stepSample (int delta);

    W27PluginProcessor& processorRef;

    // Applied to the whole editor window (setLookAndFeel() in the
    // constructor, setLookAndFeel(nullptr) in the destructor) -- see
    // PremiumLookAndFeel's own doc comment.
    PremiumLookAndFeel premiumLookAndFeel;

    PianoRollView pianoRoll;

    // Brand credit, split into two parts per explicit user request: plain,
    // non-interactive "Designed by" text, followed by a clickable "@27wav"
    // that visually reads as a link (accent colour + underline) and
    // launches https://www.instagram.com/27wav/ -- juce::HyperlinkButton
    // handles the click-to-open browser launch and pointing-hand cursor
    // itself, no extra mouse-handling code needed.
    juce::Label designedByLabel;
    juce::HyperlinkButton brandLabel;

    // Toggle: turns PianoRollView's note-drawing Compose mode on/off.
    juce::TextButton writeMelodyButton { "Write Melody" };

    // Top-centre: opens the sample-library popup menu. Its text always
    // shows the currently-selected sound (or a placeholder before anything
    // is selected / before any library has been added yet) -- see
    // updateLibraryButtonText(). samplePrevButton/sampleNextButton sit
    // immediately to its right and step through the same flat sample
    // order without opening the menu -- see stepSample().
    juce::TextButton libraryButton { "Select Sound..." };
    juce::TextButton samplePrevButton { "<" };
    juce::TextButton sampleNextButton { ">" };

    // Randomizer toggle -- a hand-drawn crossed-arrows icon (see
    // ShuffleIconButton) rather than a Unicode glyph, which was rendering
    // as a missing-glyph box on at least one user's system. While on,
    // stepSample() (called from samplePrevButton/sampleNextButton) picks
    // a random sample instead of stepping sequentially. Purely an editor-
    // side UI convenience -- not persisted in plugin state, same as the
    // zoom level.
    ShuffleIconButton randomizerButton;

    juce::TextButton vZoomOutButton { "V-" };
    juce::TextButton vZoomInButton  { "V+" };
    juce::TextButton hZoomOutButton { "H-" };
    juce::TextButton hZoomInButton  { "H+" };

    // Top-right: overall output volume for the played note -- see
    // Audio/EffectsChain. Defaults to 75%.
    juce::Label volumeLabel;
    juce::Slider volumeKnob;

    // Bottom row: 8 effect knobs, each 0-100%, defaulting to 0% (off).
    // See Audio/EffectsChain for what each one actually does to the
    // audio and why a plain amount knob was chosen for it.
    juce::Label highPassLabel;
    juce::Slider highPassKnob;
    juce::Label lowPassLabel;
    juce::Slider lowPassKnob;
    juce::Label distortionLabel;
    juce::Slider distortionKnob;
    juce::Label chorusLabel;
    juce::Slider chorusKnob;
    juce::Label flangerLabel;
    juce::Slider flangerKnob;
    juce::Label phaserLabel;
    juce::Slider phaserKnob;
    juce::Label delayLabel;
    juce::Slider delayKnob;
    juce::Label reverbLabel;
    juce::Slider reverbKnob;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (W27PluginEditor)
};

} // namespace w27
