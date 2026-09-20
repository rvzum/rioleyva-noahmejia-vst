#pragma once

// This class is no longer used anywhere in the project (the sandbox this
// change was made from couldn't delete the file outright -- ask to have
// it removed by hand if you'd like it gone).
//
// It used to make every sample ignore note-off and always play out to its
// own natural length, like a drum hit, regardless of how long a key was
// held or a drawn note was. Per explicit user request that behavior was
// reversed project-wide: every sound now stops as soon as its note-off
// arrives (key released / drawn note ends), same as an ordinary sampler
// instrument. SamplerEngine now uses plain juce::SamplerVoice instead --
// see Source/Audio/SamplerEngine.cpp/.h for the current behavior and a
// short release time that avoids a click on stop.
