#pragma once

#include <cmath>

namespace forja::pianoforge
{

#ifndef M_PI
  #define M_PI 3.14159265358979323846
#endif

// Biquad TDF-II de 6 tipos (Lowpass/Highpass/Bandpass/LowShelf/HighShelf/Peaking),
// porta fiel de pianoforge/src/core/dsp/BiquadFilter.hpp. Guarda o estado nas
// amostras anteriores com trava de denormal; nenhuma alocacao. Um filtro e um
// membro de valor, nunca heap.
class Biquad
{
public:
    enum Type
    {
        Lowpass,
        Highpass,
        Bandpass,
        LowShelf,
        HighShelf,
        Peaking
    };

    void reset() noexcept
    {
        z1 = 0.0f;
        z2 = 0.0f;
    }

    void setup (Type type, float frequencyHz, float sampleRate,
                float q = 0.7071f, float gainDb = 0.0f) noexcept
    {
        if (sampleRate <= 0.0f)
            return;

        const float f0 = std::fmax (10.0f, std::fmin (frequencyHz, sampleRate * 0.48f));
        const float w0 = 2.0f * static_cast<float> (M_PI) * f0 / sampleRate;
        const float cosW = std::cos (w0);
        const float sinW = std::sin (w0);
        const float alpha = sinW / (2.0f * std::fmax (0.01f, q));
        const float A = std::pow (10.0f, gainDb / 40.0f);

        float a0 = 1.0f;

        switch (type)
        {
            case Lowpass:
                b0 = (1.0f - cosW) * 0.5f;
                b1 = 1.0f - cosW;
                b2 = (1.0f - cosW) * 0.5f;
                a0 = 1.0f + alpha;
                a1 = -2.0f * cosW;
                a2 = 1.0f - alpha;
                break;

            case Highpass:
                b0 = (1.0f + cosW) * 0.5f;
                b1 = -(1.0f + cosW);
                b2 = (1.0f + cosW) * 0.5f;
                a0 = 1.0f + alpha;
                a1 = -2.0f * cosW;
                a2 = 1.0f - alpha;
                break;

            case Bandpass:
                b0 = alpha;
                b1 = 0.0f;
                b2 = -alpha;
                a0 = 1.0f + alpha;
                a1 = -2.0f * cosW;
                a2 = 1.0f - alpha;
                break;

            case LowShelf:
            {
                const float sqrtA = std::sqrt (A);
                b0 = A * ((A + 1.0f) - (A - 1.0f) * cosW + 2.0f * sqrtA * alpha);
                b1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cosW);
                b2 = A * ((A + 1.0f) - (A - 1.0f) * cosW - 2.0f * sqrtA * alpha);
                a0 = (A + 1.0f) + (A - 1.0f) * cosW + 2.0f * sqrtA * alpha;
                a1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * cosW);
                a2 = (A + 1.0f) + (A - 1.0f) * cosW - 2.0f * sqrtA * alpha;
                break;
            }

            case HighShelf:
            {
                const float sqrtA = std::sqrt (A);
                b0 = A * ((A + 1.0f) + (A - 1.0f) * cosW + 2.0f * sqrtA * alpha);
                b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cosW);
                b2 = A * ((A + 1.0f) + (A - 1.0f) * cosW - 2.0f * sqrtA * alpha);
                a0 = (A + 1.0f) - (A - 1.0f) * cosW + 2.0f * sqrtA * alpha;
                a1 = 2.0f * ((A - 1.0f) - (A + 1.0f) * cosW);
                a2 = (A + 1.0f) - (A - 1.0f) * cosW - 2.0f * sqrtA * alpha;
                break;
            }

            case Peaking:
                b0 = 1.0f + alpha * A;
                b1 = -2.0f * cosW;
                b2 = 1.0f - alpha * A;
                a0 = 1.0f + alpha / A;
                a1 = -2.0f * cosW;
                a2 = 1.0f - alpha / A;
                break;
        }

        const float invA0 = 1.0f / a0;
        b0 *= invA0;
        b1 *= invA0;
        b2 *= invA0;
        a1 *= invA0;
        a2 *= invA0;
    }

    inline float process (float in) noexcept
    {
        // Transposed Direct Form II
        const float out = b0 * in + z1;
        z1 = b1 * in - a1 * out + z2;
        z2 = b2 * in - a2 * out;

        if (std::fabs (z1) < 1e-18f) z1 = 0.0f;
        if (std::fabs (z2) < 1e-18f) z2 = 0.0f;

        return out;
    }

private:
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
    float a1 = 0.0f, a2 = 0.0f;
    float z1 = 0.0f, z2 = 0.0f;
};

// Curva de dinamica aplicada ANTES do backend (porta de VelocityCurve.hpp):
// curve 0.5 e linear; gamma = exp((0.5 - curve) * 3).
class VelocityCurve
{
public:
    static int process (int rawVelocity, float curve) noexcept
    {
        if (rawVelocity <= 0)
            return 0;
        if (rawVelocity >= 127)
            return 127;

        const float norm = static_cast<float> (rawVelocity) / 127.0f;
        const float gamma = std::exp ((0.5f - curve) * 3.0f);
        const float curved = std::pow (norm, gamma);

        const int result = static_cast<int> (std::round (curved * 127.0f));
        return std::clamp (result, 1, 127);
    }
};

} // namespace forja::pianoforge
