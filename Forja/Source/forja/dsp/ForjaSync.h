#pragma once

// Shared musical-division table for host-tempo sync (REPIQUE, ECO).
// The `division` parameter is 0-1 with six discrete positions; the DSP maps it
// to beats and the host BPM turns it into seconds. Nothing here allocates.

#include <cmath>

namespace forja::dsp
{

inline constexpr int syncDivisionCount = 6;

// 1/16, 1/8, 1/4, 1/2, 1/1, 2/1 in beats (a beat is 60 / BPM seconds).
inline constexpr double syncDivisionBeats[syncDivisionCount] = {
    0.25, 0.5, 1.0, 2.0, 4.0, 8.0
};

inline constexpr const char* syncDivisionLabels[syncDivisionCount] = {
    "1/16", "1/8", "1/4", "1/2", "1/1", "2/1"
};

inline int syncDivisionIndex (float value) noexcept
{
    if (! (value > 0.0f))
        value = 0.0f;
    if (value > 1.0f)
        value = 1.0f;

    const int index = (int) (value * (float) (syncDivisionCount - 1) + 0.5f);
    return index < 0 ? 0 : (index >= syncDivisionCount ? syncDivisionCount - 1 : index);
}

// Returns 0 when the host reports no valid tempo.
inline double syncDivisionSeconds (float value, double bpm) noexcept
{
    if (! (bpm > 0.0) || ! std::isfinite (bpm))
        return 0.0;

    return (60.0 / bpm) * syncDivisionBeats[syncDivisionIndex (value)];
}

} // namespace forja::dsp
