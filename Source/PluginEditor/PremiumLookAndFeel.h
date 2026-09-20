#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace w27
{

/**
    Shared "premium" chrome for every button and rotary knob in the plugin
    -- replaces JUCE's flat default LookAndFeel_V2 rendering with a light,
    glass/plastic-style gradient, subtle bevelled edge and soft accent glow
    on buttons, and a proper dial-with-pointer look on rotary sliders
    (rather than the default's thin bare arc).

    Light-toned per explicit user request: the plugin's FUNCTIONAL UI (this
    chrome, plus the toolbar strip it sits on -- see
    Theme::functionalPanelBg) uses a white/light-grey palette, distinct
    from the dark video canvas the piano roll draws its notes over. One
    instance is owned by W27PluginEditor and applied to the whole editor
    window via setLookAndFeel()/setLookAndFeel(nullptr) in its
    constructor/destructor.

    Deliberately does NOT touch each knob's own accent colour -- every
    button/knob keeps setting its own buttonColourId/buttonOnColourId/
    rotarySliderFillColourId etc. exactly as before (see
    PluginEditor.cpp's styleSmallButton() and setupKnob()); this class only
    changes HOW those colours get painted, and paints a light neutral
    background for anything that isn't currently showing a saturated
    accent colour (i.e. not toggled on).
*/
class PremiumLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PremiumLookAndFeel();

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                                bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                            float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle,
                            juce::Slider&) override;
};

} // namespace w27
