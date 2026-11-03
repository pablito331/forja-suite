#pragma once

#include <algorithm>
#include <cmath>

namespace forja::pianoforge
{

// Estagio final (porta de MasterStage.hpp): largura M/S, trim dB, volume e o
// soft-clip cubico de joelho 1,494 cuja cauda racional sobe assintoticamente a
// 1,0 sem nunca encostar -- |saida| < 1 por construcao, nenhum host ve FS.
class MasterStage
{
public:
    static constexpr float kKnee = 1.494f;
    static constexpr float kCubic = 1.0f / (3.0f * kKnee * kKnee);
    static constexpr float kRoof = 2.0f * kKnee / 3.0f;   // = 0,996, o topo no joelho
    static constexpr float kTailDelta = 1.0f - kRoof;     // = 0,004
    static constexpr float kTailQ = 4.0f;

    static inline float softClip (float x) noexcept
    {
        const float sign = x < 0.0f ? -1.0f : 1.0f;
        const float a = sign * x;                 // |x| sem chamar fabs
        if (a <= kKnee)
            return sign * (a - kCubic * a * a * a);
        const float u = a - kKnee;
        return sign * (1.0f - kTailDelta / (1.0f + kTailQ * u * u));
    }

    void process (float* left, float* right, std::size_t numFrames,
                  float volume, float trimDb, float stereoWidth) noexcept
    {
        const float trimGain = std::pow (10.0f, trimDb / 20.0f);
        const float totalGain = volume * trimGain;
        const float width = std::clamp (stereoWidth, 0.0f, 2.0f);

        for (std::size_t i = 0; i < numFrames; ++i)
        {
            const float l = left[i];
            const float r = right[i];

            const float mid = 0.5f * (l + r);
            const float side = 0.5f * (l - r) * width;

            left[i] = softClip ((mid + side) * totalGain);
            right[i] = softClip ((mid - side) * totalGain);
        }
    }
};

} // namespace forja::pianoforge
