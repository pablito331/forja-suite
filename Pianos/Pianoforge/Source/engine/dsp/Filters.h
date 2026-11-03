#pragma once

#include <cmath>

#include "Biquad.h"

namespace forja::pianoforge
{

// EQ de 3 bandas do corpo (porta de SoundboardEQ.hpp): low shelf 120 Hz, peak
// 1000 Hz Q0.8, high shelf 5000 Hz, +-8 dB por banda. Reconstroi os filtros
// so quando o ganho muda alem de 0,05 dB (o gate antigo), para nao re-fazer
// seis biquads a cada bloco com automation quieta.
class SoundboardEQ
{
public:
    void prepare (float sampleRate)
    {
        sr = sampleRate > 1000.0f ? sampleRate : 44100.0f;
        reset();
        updateFilters();
    }

    void reset() noexcept
    {
        lowL.reset(); lowR.reset();
        midL.reset(); midR.reset();
        highL.reset(); highR.reset();
    }

    void setGains (float bass, float mid, float high) noexcept
    {
        const float newBassDb = (bass - 0.5f) * 16.0f;
        const float newMidDb = (mid - 0.5f) * 16.0f;
        const float newHighDb = (high - 0.5f) * 16.0f;

        if (std::fabs (newBassDb - bassDb) > 0.05f
            || std::fabs (newMidDb - midDb) > 0.05f
            || std::fabs (newHighDb - highDb) > 0.05f)
        {
            bassDb = newBassDb;
            midDb = newMidDb;
            highDb = newHighDb;
            updateFilters();
        }
    }

    void process (float* left, float* right, std::size_t numFrames) noexcept
    {
        for (std::size_t i = 0; i < numFrames; ++i)
        {
            float l = highL.process (midL.process (lowL.process (left[i])));
            float r = highR.process (midR.process (lowR.process (right[i])));
            left[i] = l;
            right[i] = r;
        }
    }

private:
    void updateFilters() noexcept
    {
        lowL.setup (Biquad::LowShelf, 120.0f, sr, 0.7071f, bassDb);
        lowR.setup (Biquad::LowShelf, 120.0f, sr, 0.7071f, bassDb);
        midL.setup (Biquad::Peaking, 1000.0f, sr, 0.8f, midDb);
        midR.setup (Biquad::Peaking, 1000.0f, sr, 0.8f, midDb);
        highL.setup (Biquad::HighShelf, 5000.0f, sr, 0.7071f, highDb);
        highR.setup (Biquad::HighShelf, 5000.0f, sr, 0.7071f, highDb);
    }

    float sr = 44100.0f;
    float bassDb = 0.0f;
    float midDb = 0.0f;
    float highDb = 0.0f;

    Biquad lowL, lowR;
    Biquad midL, midR;
    Biquad highL, highR;
};

// Filtro acustico da tampa (porta de LidFilter.hpp): lowpass com cutoff
// exponencial 2500 Hz (fechada) a 16000 Hz (aberta). Transparente em >=0,98.
class LidFilter
{
public:
    void prepare (float sampleRate)
    {
        sr = sampleRate > 1000.0f ? sampleRate : 44100.0f;
        reset();
        updateFilter();
    }

    void reset() noexcept
    {
        filterL.reset();
        filterR.reset();
    }

    void setLid (float lidPos) noexcept
    {
        if (std::fabs (lidPos - lid) > 0.02f)
        {
            lid = lidPos;
            updateFilter();
        }
    }

    void process (float* left, float* right, std::size_t numFrames) noexcept
    {
        if (lid >= 0.98f)
            return;

        for (std::size_t i = 0; i < numFrames; ++i)
        {
            left[i] = filterL.process (left[i]);
            right[i] = filterR.process (right[i]);
        }
    }

private:
    void updateFilter() noexcept
    {
        const float cutoff = 2500.0f * std::pow (16000.0f / 2500.0f, lid);
        const float q = 0.5f + 0.2f * lid;
        filterL.setup (Biquad::Lowpass, cutoff, sr, q);
        filterR.setup (Biquad::Lowpass, cutoff, sr, q);
    }

    float sr = 44100.0f;
    float lid = 0.7f;
    Biquad filterL;
    Biquad filterR;
};

} // namespace forja::pianoforge
