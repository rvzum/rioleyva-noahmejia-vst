#pragma once

#include <juce_graphics/juce_graphics.h>

namespace w27::Theme
{

// Central color palette. Every visual module (PluginEditor, Visualization,
// Effects) reads colors from here and nowhere else -- reskinning the whole
// plugin later (Phase 8: swappable theme presets) means changing this file,
// not touching MIDI, Timeline, or rendering logic.

inline const juce::Colour background        { 0xff0c0d10 };
inline const juce::Colour panelOverlay       { 0x99121317 };

inline const juce::Colour brandTitle         { 0xffe8e8ec };
inline const juce::Colour brandSubtitle      { 0xff8a8a92 };

inline const juce::Colour keyWhite           { 0xffe4e4ea };
inline const juce::Colour keyBlack           { 0xff17181c };
inline const juce::Colour keyActiveHighlight { 0xff5ad1a8 };
inline const juce::Colour keyBorder          { 0xff2a2b30 };

inline const juce::Colour gridLine           { 0x22ffffff };
inline const juce::Colour octaveLine         { 0x3cffffff };

// Note bars are plain white with a directional alpha gradient (see
// PianoRollView) rather than a fixed accent colour.
inline const juce::Colour noteFill           { 0xffffffff };

inline const juce::Colour playheadLine       { 0xffe8e8ec };

inline const juce::Colour diagnosticText     { 0xff5ad1a8 };

// -- Per-effect accent colours ---------------------------------------------
// One distinct colour per effect knob, in the same left-to-right order
// EffectsChain::process() applies them (PluginEditor::setupKnob's
// accentColour) -- each knob keeps its own accent colour under the light
// functional-UI palette below.
inline const juce::Colour effectHighPass     { 0xff36e2ff }; // icy cyan
inline const juce::Colour effectLowPass      { 0xff8f6bff }; // deep violet
inline const juce::Colour effectDistortion   { 0xffff3b3b }; // hot red
inline const juce::Colour effectChorus       { 0xff3bffb0 }; // mint green
inline const juce::Colour effectFlanger      { 0xff42a8ff }; // ripple blue
inline const juce::Colour effectPhaser       { 0xffb96bff }; // electric purple
inline const juce::Colour effectDelay        { 0xffffb23b }; // amber
inline const juce::Colour effectReverb       { 0xffff5ad1 }; // magenta glow
inline const juce::Colour premiumGold        { 0xffe8c97a }; // Volume knob's own accent (the "master" control)

// -- Light "functional UI" palette (toolbar / buttons / knobs) -------------
// Per explicit user request, the plugin's FUNCTIONAL interface -- the top
// and bottom toolbar strips, their buttons, and the effect/volume knobs --
// is light-toned, distinct from the dark video canvas the piano roll draws
// its (plain white, per explicit request -- see Theme::noteFill) notes
// over. Only PianoRollView's own note-area drawing stays on the dark/video
// background; everything PremiumLookAndFeel and the toolbar buttons paint
// reads from this palette instead.
inline const juce::Colour functionalPanelBg     { 0xfff2f2f5 }; // toolbar/knob strip fill
inline const juce::Colour functionalButtonTop   { 0xffffffff }; // button/knob-cap gradient, top
inline const juce::Colour functionalButtonBot   { 0xffe2e2e8 }; // button/knob-cap gradient, bottom
inline const juce::Colour functionalBorder      { 0x40000000 }; // thin outline on buttons/knob cap
inline const juce::Colour functionalTextDark    { 0xff26272c }; // primary text/icon colour on light buttons
inline const juce::Colour functionalTextSubtle  { 0xff6c6c74 }; // knob captions, secondary labels
inline const juce::Colour functionalKnobTrack   { 0xffd6d6dc }; // rotary knob's unfilled track

} // namespace w27::Theme
