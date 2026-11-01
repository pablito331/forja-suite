#pragma once

#include <algorithm>
#include <array>
#include <cmath>

namespace forja::drumforge
{

// O Drumforge fixa 16 pads para que o APVTS não mude de forma com o kit:
// o kit escolhe quais pads usa, mas os parâmetros de mixagem existem sempre.
inline constexpr int kNumPads = 16;
inline constexpr int kNumAuxBuses = 4;
inline constexpr int kNumOutputBuses = kNumAuxBuses + 1;
inline constexpr int kMaxVoices = 64;
inline constexpr int kMaxLayers = 8;
inline constexpr int kMaxRoundRobins = 16;

struct PadParameters
{
    float trim = 0.5f;             // 0..1 -> -24..+24 dB
    float fader = 0.91f;           // 0..1 -> -60..+6 dB (0,91 fica em 0 dB)
    float pan = 0.5f;              // 0..1 -> L..R
    float hpf = 0.0f;              // 0..1 -> 20..400 Hz
    float eqLow = 0.5f;            // 0..1 -> -15..+15 dB (shelf 80 Hz)
    float eqMid = 0.5f;            // 0..1 -> -15..+15 dB (peak 1,2 kHz)
    float eqHigh = 0.5f;           // 0..1 -> -15..+15 dB (shelf 8 kHz)
    float compThreshold = 0.7f;    // 0..1 -> -40..0 dB
    float compRatio = 0.1f;        // 0..1 -> 1..20 : 1
    float send1 = 0.0f;            // 0..1 -> FX 1 (ambiente)
    float send2 = 0.0f;            // 0..1 -> FX 2 (saturação)
    bool mute = false;
    bool solo = false;
    bool phase = false;
    int output = 0;                // 0 = master, 1..4 = aux
};

struct MasterParameters
{
    float trim = 0.5f;             // 0..1 -> -24..+24 dB
    float comp = 0.0f;             // 0..1 -> ButterComp (TÊMPERA)
    bool limiter = true;
    float fx1Regen = 0.5f;
    float fx1Position = 1.0f;
    float fx2Drive = 0.2f;
};

struct EngineParameters
{
    std::array<PadParameters, kNumPads> pads {};
    MasterParameters master {};
};

namespace mapping
{

inline float unit (float value) noexcept
{
    return std::clamp (value, 0.0f, 1.0f);
}

inline float trimDb (float value) noexcept
{
    return -24.0f + 48.0f * unit (value);
}

inline float faderDb (float value) noexcept
{
    return -60.0f + 66.0f * unit (value);
}

inline float panUnit (float value) noexcept
{
    return 2.0f * unit (value) - 1.0f;
}

inline float hpfHz (float value) noexcept
{
    return 20.0f * std::pow (20.0f, unit (value));
}

inline float eqDb (float value) noexcept
{
    return -15.0f + 30.0f * unit (value);
}

inline float thresholdDb (float value) noexcept
{
    return -40.0f + 40.0f * unit (value);
}

inline float ratio (float value) noexcept
{
    return 1.0f + 19.0f * unit (value);
}

// Envio ao FX1 (SALAO): a volta e 100% wet e as somas do reverb empilham, entao
// o botao entra com atenuacao forte. Lei quadrada, topo em 0,25 (-12 dB): um
// dedo no knob (0,1) injeta -52 dB, sala audivel so passa de 0,5. O FX2 recebe
// nivel de drive (LIXA e non-linear), entao esse segue linear.
inline float wetSendGain (float value) noexcept
{
    const float v = unit (value);
    return v * v * 0.25f;
}

// Coeficiente de suavização de um polo para uma constante de tempo em ms.
inline double smoothingCoefficient (double timeMs, double sampleRate) noexcept
{
    if (sampleRate <= 0.0)
        return 1.0;

    return 1.0 - std::exp (-1.0 / ((timeMs * 0.001) * sampleRate));
}

inline float dbToGain (float db) noexcept
{
    return std::pow (10.0f, db * 0.05f);
}

inline float gainToDb (float gain) noexcept
{
    return gain > 0.0f ? 20.0f * std::log10 (gain) : -100.0f;
}

inline float panGainLeft (float pan) noexcept
{
    const float angle = (pan + 1.0f) * 0.25f * 3.14159265358979f;
    return std::cos (angle);
}

inline float panGainRight (float pan) noexcept
{
    const float angle = (pan + 1.0f) * 0.25f * 3.14159265358979f;
    return std::sin (angle);
}

} // namespace mapping

} // namespace forja::drumforge
