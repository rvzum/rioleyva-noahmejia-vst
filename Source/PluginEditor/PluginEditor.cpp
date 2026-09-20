#include "PluginEditor.h"
#include "../PluginProcessor/PluginProcessor.h"
#include "../Theme/ThemeColors.h"
#include "../Audio/EffectsChain.h"

namespace w27
{

namespace
{
    constexpr int minWidth = 480;
    constexpr int minHeight = 320;
    constexpr int defaultWidth = 900;
    constexpr int defaultHeight = 560;

    void styleSmallButton (juce::TextButton& b)
    {
        // Light functional-UI palette per explicit user request --
        // PremiumLookAndFeel::drawButtonBackground repaints the actual
        // background itself; buttonColourId here just needs to stay
        // unsaturated so that code treats this as an "off" button rather
        // than an accented one.
        b.setColour (juce::TextButton::buttonColourId, Theme::functionalButtonBot);
        b.setColour (juce::TextButton::textColourOffId, Theme::functionalTextDark);
    }

    // Recursively mirrors a SampleLibraryManager::Node's children into a
    // juce::PopupMenu -- a subfolder becomes a nested submenu (however
    // deep), a sample leaf becomes a plain item whose ID is
    // (leaf.flatIndex + 1), decoded back in showLibraryMenu()'s callback.
    // Per explicit user request the on-disk tree is mirrored exactly, so
    // this does no flattening/regrouping of its own.
    void addNodeChildrenToMenu (juce::PopupMenu& menu, const w27::SampleLibraryManager::Node& node)
    {
        for (auto& child : node.children)
        {
            if (child.isFolder)
            {
                juce::PopupMenu sub;
                addNodeChildrenToMenu (sub, child);
                menu.addSubMenu (child.name, sub);
            }
            else
            {
                menu.addItem (child.flatIndex + 1, child.name);
            }
        }
    }
}

W27PluginEditor::W27PluginEditor (W27PluginProcessor& p)
    : juce::AudioProcessorEditor (&p),
      processorRef (p),
      pianoRoll (p.getMidiInputHandler(), p.getComposedMelody(), p.getEffectsChain()),
      brandLabel ("@27wav", juce::URL ("https://www.instagram.com/27wav/"))
{
    setLookAndFeel (&premiumLookAndFeel);

    addAndMakeVisible (pianoRoll);

    // Plain, non-interactive prefix -- only the "@27wav" part (below) is
    // clickable.
    designedByLabel.setText ("Designed by", juce::dontSendNotification);
    designedByLabel.setFont (juce::Font (13.0f));
    designedByLabel.setColour (juce::Label::textColourId, Theme::functionalTextSubtle);
    designedByLabel.setJustificationType (juce::Justification::centredLeft);
    designedByLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (designedByLabel);

    // Styled distinctly from designedByLabel -- accent colour AND an
    // underline, so it reads unambiguously as a clickable link rather than
    // more plain text, per explicit user request. juce::HyperlinkButton
    // itself still handles the click-to-open-browser behaviour and
    // pointing-hand cursor.
    brandLabel.setFont (juce::Font (14.0f, juce::Font::bold | juce::Font::underlined), false, juce::Justification::centredLeft);
    brandLabel.setColour (juce::HyperlinkButton::textColourId, Theme::effectReverb);
    addAndMakeVisible (brandLabel);

    // "Write Melody" toggle -- switches PianoRollView into (or out of) its
    // click/drag note-editing Compose mode. Highlighted with the theme's
    // accent colour while active, same colour PianoRollView already uses
    // elsewhere for "this is currently active" state.
    writeMelodyButton.setClickingTogglesState (true);
    writeMelodyButton.setColour (juce::TextButton::buttonOnColourId, Theme::keyActiveHighlight);
    writeMelodyButton.setColour (juce::TextButton::textColourOnId, Theme::background);
    writeMelodyButton.onClick = [this]
    {
        pianoRoll.setComposeModeEnabled (writeMelodyButton.getToggleState());
    };
    styleSmallButton (writeMelodyButton);
    addAndMakeVisible (writeMelodyButton);

    // Library button -- top centre of the toolbar, per explicit request.
    // Libraries are read live from ~/Music/27wav rioleyva & noahmejia banks/ (see
    // Library/SampleLibraryManager) every time this menu opens, so this
    // starts out showing a placeholder until both a library exists there
    // and one of its sounds has actually been picked.
    libraryButton.onClick = [this] { showLibraryMenu(); };
    styleSmallButton (libraryButton);
    addAndMakeVisible (libraryButton);
    updateLibraryButtonText();

    // Prev/next oneshot arrows, added per explicit user request -- step
    // through the same flat sample order the library menu shows, without
    // opening it.
    samplePrevButton.onClick = [this] { stepSample (-1); };
    sampleNextButton.onClick = [this] { stepSample (1); };
    styleSmallButton (samplePrevButton);
    styleSmallButton (sampleNextButton);
    addAndMakeVisible (samplePrevButton);
    addAndMakeVisible (sampleNextButton);

    // Randomizer toggle, immediately to the right of the arrows -- while
    // pressed, stepSample() (below) picks a random sample instead of
    // stepping sequentially. ShuffleIconButton draws its own on/off state
    // straight from Theme colours (see its paintButton()), so no colour
    // IDs or styleSmallButton() call are needed here.
    randomizerButton.setClickingTogglesState (true);
    randomizerButton.setTooltip ("Randomizer: when on, the arrows pick a random sound instead of stepping in order");
    addAndMakeVisible (randomizerButton);

    // -- Effect knobs + Volume -- all wired straight to the processor's
    // EffectsChain (see Audio/EffectsChain and setupKnob() below).
    auto& effects = processorRef.getEffectsChain();

    setupKnob (volumeKnob, volumeLabel, "Volume", effects.getVolume(), 75.0f, Theme::premiumGold,
               [this] (float v) { processorRef.getEffectsChain().setVolume (v); });

    // Each knob gets its own distinct accent colour, matching exactly the
    // colour that same effect paints on the notes in PianoRollView (see
    // ThemeColors.h's "per-effect accent colours" section) -- turning a
    // knob up lights the notes up in that same colour, per explicit user
    // request tying the knobs' look to the sound/visual effects.
    setupKnob (highPassKnob, highPassLabel, "High Pass", effects.getHighPassAmount(), 0.0f, Theme::effectHighPass,
               [this] (float v) { processorRef.getEffectsChain().setHighPassAmount (v); });
    setupKnob (lowPassKnob, lowPassLabel, "Low Pass", effects.getLowPassAmount(), 0.0f, Theme::effectLowPass,
               [this] (float v) { processorRef.getEffectsChain().setLowPassAmount (v); });
    setupKnob (distortionKnob, distortionLabel, "Distortion", effects.getDistortionAmount(), 0.0f, Theme::effectDistortion,
               [this] (float v) { processorRef.getEffectsChain().setDistortionAmount (v); });
    setupKnob (chorusKnob, chorusLabel, "Chorus", effects.getChorusAmount(), 0.0f, Theme::effectChorus,
               [this] (float v) { processorRef.getEffectsChain().setChorusAmount (v); });
    setupKnob (flangerKnob, flangerLabel, "Flanger", effects.getFlangerAmount(), 0.0f, Theme::effectFlanger,
               [this] (float v) { processorRef.getEffectsChain().setFlangerAmount (v); });
    setupKnob (phaserKnob, phaserLabel, "Phaser", effects.getPhaserAmount(), 0.0f, Theme::effectPhaser,
               [this] (float v) { processorRef.getEffectsChain().setPhaserAmount (v); });
    setupKnob (delayKnob, delayLabel, "Delay", effects.getDelayAmount(), 0.0f, Theme::effectDelay,
               [this] (float v) { processorRef.getEffectsChain().setDelayAmount (v); });
    setupKnob (reverbKnob, reverbLabel, "Reverb", effects.getReverbAmount(), 0.0f, Theme::effectReverb,
               [this] (float v) { processorRef.getEffectsChain().setReverbAmount (v); });

    vZoomOutButton.onClick = [this] { pianoRoll.zoomOutVertical(); };
    vZoomInButton.onClick  = [this] { pianoRoll.zoomInVertical(); };
    hZoomOutButton.onClick = [this] { pianoRoll.zoomOutHorizontal(); };
    hZoomInButton.onClick  = [this] { pianoRoll.zoomInHorizontal(); };

    for (auto* button : { &vZoomOutButton, &vZoomInButton, &hZoomOutButton, &hZoomInButton })
    {
        styleSmallButton (*button);
        addAndMakeVisible (*button);
    }

    setResizable (true, true);
    setResizeLimits (minWidth, minHeight, 4000, 3000);
    setSize (defaultWidth, defaultHeight);
}

W27PluginEditor::~W27PluginEditor()
{
    setLookAndFeel (nullptr);
}

void W27PluginEditor::paint (juce::Graphics& g)
{
    // Light functional-UI background for the toolbar/knob strips, per
    // explicit user request -- the piano-roll area fully repaints its own
    // (dark, video-backed) bounds every frame, so this colour is only
    // ever actually visible behind the top and bottom toolbar strips.
    g.fillAll (Theme::functionalPanelBg);
}

void W27PluginEditor::resized()
{
    auto bounds = getLocalBounds();

    // Top bar: brand label + "Write Melody" toggle on the left, zoom
    // buttons on the right, and the library button (plus its prev/next
    // arrows) centred in whatever space is left between them ("сверху
    // посередине" -- top, in the middle -- per explicit request). The
    // background-switch arrows that used to start this row were removed
    // per later explicit user request.
    auto topBar = bounds.removeFromTop (40);

    // "Designed by @27wav" -- widened from the old plain "27wav" label's
    // 56px to fit both parts; split roughly 55/45 between the plain
    // prefix and the (bolder, wider-looking) clickable handle.
    auto brandArea = topBar.removeFromLeft (150);
    designedByLabel.setBounds (brandArea.removeFromLeft (78).reduced (4, 2));
    brandLabel.setBounds (brandArea.reduced (2, 2));

    writeMelodyButton.setBounds (topBar.removeFromLeft (100).reduced (4, 2));

    // Volume knob -- top right, per explicit request. Reserved before
    // the zoom buttons so it always keeps its place at the far right
    // edge regardless of window width.
    layoutKnob (topBar.removeFromRight (44).reduced (2, 1), volumeLabel, volumeKnob);

    auto zoomArea = topBar.removeFromRight (176);
    hZoomInButton.setBounds  (zoomArea.removeFromRight (42).reduced (2));
    hZoomOutButton.setBounds (zoomArea.removeFromRight (42).reduced (2));
    vZoomInButton.setBounds  (zoomArea.removeFromRight (42).reduced (2));
    vZoomOutButton.setBounds (zoomArea.removeFromRight (42).reduced (2));

    // libraryButton + its two arrow buttons are centred together as one
    // group, so the arrows always sit immediately to libraryButton's
    // right regardless of window width. 24px was too narrow for JUCE's
    // TextButton to fit even a single "<"/">" glyph without truncating it
    // to ".." -- widened per explicit user report, and given less inner
    // padding than the wider buttons elsewhere so the glyph has more room.
    const int arrowButtonWidth = 34;
    const int randomizerButtonWidth = 34;
    const int libraryButtonWidth = juce::jmax (80, juce::jmin (220, topBar.getWidth() - 8 - 2 * arrowButtonWidth - randomizerButtonWidth));
    const int groupWidth = libraryButtonWidth + 2 * arrowButtonWidth + randomizerButtonWidth;
    auto libraryGroupArea = topBar.withSizeKeepingCentre (groupWidth, topBar.getHeight());
    libraryButton.setBounds (libraryGroupArea.removeFromLeft (libraryButtonWidth).reduced (4, 2));
    samplePrevButton.setBounds (libraryGroupArea.removeFromLeft (arrowButtonWidth).reduced (2, 1));
    sampleNextButton.setBounds (libraryGroupArea.removeFromLeft (arrowButtonWidth).reduced (2, 1));
    randomizerButton.setBounds (libraryGroupArea.removeFromLeft (randomizerButtonWidth).reduced (2, 1));

    // Bottom row: 8 effect knobs, per explicit request -- left-to-right in
    // the same order EffectsChain::process() actually applies them in.
    auto effectsBar = bounds.removeFromBottom (64);
    const int knobCount = 8;
    const int knobWidth = effectsBar.getWidth() / knobCount;

    juce::Slider* const knobOrder[knobCount] = { &highPassKnob, &lowPassKnob, &distortionKnob, &chorusKnob,
                                                  &flangerKnob, &phaserKnob, &delayKnob, &reverbKnob };
    juce::Label* const labelOrder[knobCount] = { &highPassLabel, &lowPassLabel, &distortionLabel, &chorusLabel,
                                                  &flangerLabel, &phaserLabel, &delayLabel, &reverbLabel };

    for (int i = 0; i < knobCount; ++i)
    {
        auto slot = effectsBar.removeFromLeft (knobWidth);
        layoutKnob (slot.reduced (4, 2), *labelOrder[i], *knobOrder[i]);
    }

    // No bottom status bar any more (removed per explicit user request) --
    // pianoRoll gets the full remaining height between the toolbar and the
    // effects row.
    pianoRoll.setBounds (bounds);
}

void W27PluginEditor::updateLibraryButtonText()
{
    if (processorRef.hasSampleSelected())
        libraryButton.setButtonText (processorRef.getSelectedSampleName());
    else
        libraryButton.setButtonText ("Select Sound...");
}

void W27PluginEditor::showLibraryMenu()
{
    auto& manager = processorRef.getSampleLibraryManager();
    manager.rescan(); // pick up anything dropped into the folder since this menu was last opened

    juce::PopupMenu menu;

    if (manager.getNumSamples() == 0)
    {
        menu.addItem (1, "No libraries yet -- add files to ~/Music/27wav rioleyva & noahmejia banks/<Library>/", false);
    }
    else
    {
        // Mirrors the on-disk folder tree exactly, at whatever depth it
        // actually has, per explicit user request -- see
        // addNodeChildrenToMenu() above and SampleLibraryManager's own
        // doc comment. Item IDs are (leaf.flatIndex + 1); decoded back in
        // the callback below.
        addNodeChildrenToMenu (menu, manager.getRootNode());
    }

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (libraryButton),
                         [this] (int result)
                         {
                             if (result <= 0)
                                 return;

                             if (processorRef.selectSample (result - 1))
                                 updateLibraryButtonText();
                         });
}

void W27PluginEditor::stepSample (int delta)
{
    auto& manager = processorRef.getSampleLibraryManager();
    manager.rescan(); // stay consistent with the menu -- pick up on-disk changes here too

    const int count = manager.getNumSamples();
    if (count == 0)
        return;

    const int current = processorRef.getSelectedSampleFlatIndex();
    int next;

    if (randomizerButton.getToggleState() && count > 1)
    {
        // Randomizer on: both arrows just pick a new random sound instead
        // of stepping sequentially -- delta's direction is ignored in this
        // mode. Retries a few times so the sound that's already selected
        // doesn't just get picked again by chance; if luck keeps landing on
        // it, gives up after a handful of tries and accepts the repeat
        // rather than looping forever.
        auto& random = juce::Random::getSystemRandom();
        int attempts = 0;
        do
        {
            next = random.nextInt (count);
        } while (next == current && ++attempts < 8);
    }
    else
    {
        // Wraps at either end. If nothing is selected yet (current == -1),
        // stepping forward starts at the first sample and stepping backward
        // starts at the last -- both reasonable "just pick one" starting
        // points rather than doing nothing.
        next = (current < 0 ? (delta > 0 ? 0 : count - 1)
                             : ((current + delta) % count + count) % count);
    }

    if (processorRef.selectSample (next))
        updateLibraryButtonText();
}

void W27PluginEditor::setupKnob (juce::Slider& slider, juce::Label& label, const juce::String& caption,
                                  float currentAmount, float doubleClickDefaultPercent, juce::Colour accentColour,
                                  std::function<void (float)> onAmountChanged)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    slider.setRange (0.0, 100.0, 1.0);
    slider.setDoubleClickReturnValue (true, doubleClickDefaultPercent);
    slider.setValue (currentAmount * 100.0, juce::dontSendNotification);
    slider.setTextValueSuffix ("%");
    slider.setColour (juce::Slider::rotarySliderFillColourId, accentColour);
    slider.setColour (juce::Slider::rotarySliderOutlineColourId, Theme::functionalKnobTrack);
    slider.setColour (juce::Slider::thumbColourId, Theme::functionalTextDark);

    // No permanent numeric readout (NoTextBox above) to keep the row
    // compact -- this shows the live value in a small floating bubble
    // only while actually dragging the knob.
    slider.setPopupDisplayEnabled (true, true, this);

    slider.onValueChange = [&slider, onAmountChanged]
    {
        onAmountChanged ((float) (slider.getValue() / 100.0));
    };
    addAndMakeVisible (slider);

    label.setText (caption, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setColour (juce::Label::textColourId, Theme::functionalTextSubtle);
    label.setFont (juce::Font (11.0f));
    addAndMakeVisible (label);
}

void W27PluginEditor::layoutKnob (juce::Rectangle<int> area, juce::Label& label, juce::Slider& slider)
{
    label.setBounds (area.removeFromTop (14));
    slider.setBounds (area);
}

} // namespace w27
