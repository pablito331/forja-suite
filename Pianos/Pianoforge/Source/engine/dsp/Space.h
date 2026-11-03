#pragma once

#include <algorithm>
#include <array>
#include <cmath>

namespace forja::pianoforge
{

#ifndef M_PI
  #define M_PI 3.14159265358979323846
#endif

// Dimensionamento fixo para 192 kHz (a regra da suite manda pre-dimensionar no
// prepare em vez de alocar): chorus ate 50 ms + folga, comb do FDN ate o maior
// comprimento primo (1379) escalado a 192k, allpass idem. Tudo round-up folgado.
inline constexpr std::size_t kMaxChorusDelay = 10240;   // 192k*0,05 + folga
inline constexpr std::size_t kMaxCombDelay = 8192;      // 1379 * (192/44,1) + folga
inline constexpr std::size_t kMaxAllpassDelay = 4096;   // 568 * (192/44,1) + folga

// Chorus de 2 vozes (porta de Chorus.hpp): LFO 0,8 Hz, base 15 ms, profundidade
// 4 ms, leitura interpolada. O vetor do antigo virou std::array fixo; o
// comprimento real do anel (maxDelay) e guardado a parte e modulo por ele.
class Chorus
{
public:
    void prepare (float sampleRate)
    {
        sr = sampleRate > 1000.0f ? sampleRate : 44100.0f;

        const std::size_t wanted = static_cast<std::size_t> (sr * 0.05f) + 16;
        maxDelay = wanted < kMaxChorusDelay ? wanted : kMaxChorusDelay;

        bufferL.fill (0.0f);
        bufferR.fill (0.0f);
        writePos = 0;
        phase = 0.0f;
        lfoInc = 2.0f * static_cast<float> (M_PI) * 0.8f / sr;   // 0,8 Hz
    }

    void setMix (float mixAmount) noexcept
    {
        mix = std::fmax (0.0f, std::fmin (1.0f, mixAmount));
    }

    void process (float* left, float* right, std::size_t numFrames) noexcept
    {
        if (mix < 0.001f)
            return;

        const float baseDelay = sr * 0.015f;
        const float depth = sr * 0.004f;

        for (std::size_t i = 0; i < numFrames; ++i)
        {
            const float inL = left[i];
            const float inR = right[i];

            bufferL[writePos] = inL;
            bufferR[writePos] = inR;

            const float lfoL = std::sin (phase);
            const float lfoR = std::cos (phase);   // defasagem de 90 graus

            const float readPosL = static_cast<float> (writePos) - (baseDelay + depth * lfoL);
            const float readPosR = static_cast<float> (writePos) - (baseDelay + depth * lfoR);

            const float wetL = readInterpolated (bufferL, readPosL);
            const float wetR = readInterpolated (bufferR, readPosR);

            left[i] = inL * (1.0f - mix * 0.5f) + wetL * (mix * 0.7f);
            right[i] = inR * (1.0f - mix * 0.5f) + wetR * (mix * 0.7f);

            writePos = (writePos + 1) % maxDelay;
            phase += lfoInc;

            if (phase >= 2.0f * static_cast<float> (M_PI))
                phase -= 2.0f * static_cast<float> (M_PI);
        }
    }

private:
    float readInterpolated (const std::array<float, kMaxChorusDelay>& buf, float pos) const noexcept
    {
        const float ring = static_cast<float> (maxDelay);

        while (pos < 0.0f)
            pos += ring;

        const std::size_t idx0 = static_cast<std::size_t> (pos) % maxDelay;
        const std::size_t idx1 = (idx0 + 1) % maxDelay;
        const float frac = pos - std::floor (pos);
        return buf[idx0] * (1.0f - frac) + buf[idx1] * frac;
    }

    float sr = 44100.0f;
    float mix = 0.1f;
    std::size_t maxDelay = 2048;
    std::size_t writePos = 0;
    float phase = 0.0f;
    float lfoInc = 0.0f;

    std::array<float, kMaxChorusDelay> bufferL{};
    std::array<float, kMaxChorusDelay> bufferR{};
};

// Comb com amortecimento (one-pole no caminho de feedback), porta de
// CombFilter.hpp. Anel em std::array fixo; comprimento real guardado em len.
class CombFilter
{
public:
    void setup (std::size_t delayLength, float feedbackGain, float dampingCoeff)
    {
        len = (delayLength == 0 || delayLength > kMaxCombDelay) ? kMaxCombDelay : delayLength;
        buffer.fill (0.0f);
        writeIdx = 0;
        feedback = feedbackGain;
        damp = dampingCoeff;
        filterState = 0.0f;
    }

    inline float process (float in) noexcept
    {
        const float out = buffer[writeIdx];
        filterState = out * (1.0f - damp) + filterState * damp;
        buffer[writeIdx] = in + filterState * feedback;
        writeIdx = (writeIdx + 1) % len;
        return out;
    }

private:
    std::array<float, kMaxCombDelay> buffer{};
    std::size_t len = 1116;
    std::size_t writeIdx = 0;
    float feedback = 0.8f;
    float damp = 0.2f;
    float filterState = 0.0f;
};

class AllpassFilter
{
public:
    void setup (std::size_t delayLength, float feedbackGain = 0.5f)
    {
        len = (delayLength == 0 || delayLength > kMaxAllpassDelay) ? kMaxAllpassDelay : delayLength;
        buffer.fill (0.0f);
        writeIdx = 0;
        feedback = feedbackGain;
    }

    inline float process (float in) noexcept
    {
        const float bufOut = buffer[writeIdx];
        const float out = -in + bufOut;
        buffer[writeIdx] = in + bufOut * feedback;
        writeIdx = (writeIdx + 1) % len;
        return out;
    }

private:
    std::array<float, kMaxAllpassDelay> buffer{};
    std::size_t len = 225;
    std::size_t writeIdx = 0;
    float feedback = 0.5f;
};

// Reverb FDN (porta de Reverb.hpp): 4 combs por canal (primos 1116..1379) em
// paralelo + 2 allpass em serie por canal, 4 tipos por feedback/damping. Crossfade
// equal-power na saida. Nenhuma alocacao no prepare.
class Reverb
{
public:
    void prepare (float sampleRate)
    {
        sr = sampleRate > 1000.0f ? sampleRate : 44100.0f;
        updateType();
    }

    void setMix (float mixAmount) noexcept
    {
        mix = std::fmax (0.0f, std::fmin (1.0f, mixAmount));
    }

    void setType (int type) noexcept
    {
        const int clamped = std::clamp (type, 0, 3);

        if (clamped != currentType)
        {
            currentType = clamped;
            updateType();
        }
    }

    void process (float* left, float* right, std::size_t numFrames) noexcept
    {
        if (mix < 0.001f)
            return;

        for (std::size_t i = 0; i < numFrames; ++i)
        {
            const float inL = left[i];
            const float inR = right[i];
            const float monoIn = (inL + inR) * 0.5f;

            float outCombL = combsL[0].process (monoIn) + combsL[1].process (monoIn)
                           + combsL[2].process (monoIn) + combsL[3].process (monoIn);
            float outCombR = combsR[0].process (monoIn) + combsR[1].process (monoIn)
                           + combsR[2].process (monoIn) + combsR[3].process (monoIn);

            const float wetL = allpassesL[1].process (allpassesL[0].process (outCombL * 0.25f));
            const float wetR = allpassesR[1].process (allpassesR[0].process (outCombR * 0.25f));

            left[i] = inL * (1.0f - mix * 0.4f) + wetL * (mix * 0.8f);
            right[i] = inR * (1.0f - mix * 0.4f) + wetR * (mix * 0.8f);
        }
    }

private:
    void updateType() noexcept
    {
        const float scale = sr / 44100.0f;

        const std::size_t combLengthsL[4] = {
            static_cast<std::size_t> (1116 * scale),
            static_cast<std::size_t> (1188 * scale),
            static_cast<std::size_t> (1277 * scale),
            static_cast<std::size_t> (1356 * scale)
        };
        const std::size_t combLengthsR[4] = {
            static_cast<std::size_t> (1139 * scale),
            static_cast<std::size_t> (1211 * scale),
            static_cast<std::size_t> (1299 * scale),
            static_cast<std::size_t> (1379 * scale)
        };

        float feedback = 0.8f;
        float damp = 0.2f;

        switch (currentType)
        {
            case 0: feedback = 0.72f; damp = 0.45f; break;   // Sala pequena
            case 1: feedback = 0.84f; damp = 0.25f; break;   // Studio
            case 2: feedback = 0.88f; damp = 0.15f; break;   // Prato
            case 3: feedback = 0.93f; damp = 0.10f; break;   // Catedral
        }

        for (int k = 0; k < 4; ++k)
        {
            combsL[k].setup (combLengthsL[k], feedback, damp);
            combsR[k].setup (combLengthsR[k], feedback, damp);
        }

        allpassesL[0].setup (static_cast<std::size_t> (225 * scale), 0.5f);
        allpassesL[1].setup (static_cast<std::size_t> (556 * scale), 0.5f);
        allpassesR[0].setup (static_cast<std::size_t> (237 * scale), 0.5f);
        allpassesR[1].setup (static_cast<std::size_t> (568 * scale), 0.5f);
    }

    float sr = 44100.0f;
    float mix = 0.25f;
    int currentType = 1;

    std::array<CombFilter, 4> combsL;
    std::array<CombFilter, 4> combsR;
    std::array<AllpassFilter, 2> allpassesL;
    std::array<AllpassFilter, 2> allpassesR;
};

} // namespace forja::pianoforge
