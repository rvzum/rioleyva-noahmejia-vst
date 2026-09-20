#pragma once

namespace w27::PitchUtils
{

// Standard 88-key piano range. Visualization maps every note in this range
// to a vertical position; notes outside it are clamped rather than dropped.
constexpr int minPianoNote = 21;  // A0
constexpr int maxPianoNote = 108; // C8
constexpr int numPianoKeys = maxPianoNote - minPianoNote + 1; // 88

inline bool isBlackKey (int midiNoteNumber) noexcept
{
    static constexpr bool blackKeys[12] =
    {
        false, true, false, true, false, false, true, false, true, false, true, false
    };
    return blackKeys[((midiNoteNumber % 12) + 12) % 12];
}

} // namespace w27::PitchUtils
