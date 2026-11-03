#pragma once

#include <algorithm>
#include <array>
#include <cmath>

#include "Biquad.h"

namespace forja::pianoforge
{

struct Resonator
{
    int note = -1;
    Biquad filterL;
    Biquad filterR;
    float gain = 0.0f;
    float env = 0.0f;
    float decay = 0.999f;
    bool active = false;
};

// Ressonancia simpatica: banco de 16 ressonadores bandpass (Q 8) ligados as
// teclas AGUAS ativas. O pedal de sustain nao acende tudo -- ele so segura as
// notas ja acesas (noteOff, com o pedal no fundo, nao recolhe o banco), e o
// env e rampado para nao estalar no press/release. Porta fiel de
// SympatheticResonance.hpp; tudo em std::array fixo, sem alocacao.
class SympatheticResonance
{
public:
    void prepare (float sampleRate)
    {
        sr = sampleRate > 1000.0f ? sampleRate : 44100.0f;

        // Ataque ~8 ms para o env subir liso (sem estalo) quando um ressonador
        // acende ou troca de nota.
        attackRate = 1.0f - std::exp (-1.0f / (sr * 0.008f));

        for (auto& r : resonators)
        {
            r.active = false;
            r.env = 0.0f;
            r.filterL.reset();
            r.filterR.reset();
        }

        std::fill (activeKeys.begin(), activeKeys.end(), false);
        sustainPedal = false;
    }

    void noteOn (int note, int /*velocity*/) noexcept
    {
        if (note >= 0 && note < 128)
        {
            activeKeys[note] = true;
            updateResonators();
        }
    }

    void noteOff (int note) noexcept
    {
        if (note >= 0 && note < 128)
        {
            activeKeys[note] = false;

            if (! sustainPedal)
                updateResonators();
        }
    }

    void setSustainPedal (bool down) noexcept
    {
        if (sustainPedal != down)
        {
            sustainPedal = down;
            updateResonators();
        }
    }

    void setParameters (float resonanceAmount, float resonanceTune) noexcept
    {
        amount = resonanceAmount;
        tuneOffset = (resonanceTune - 0.5f) * 0.1f;
    }

    void process (float* left, float* right, std::size_t numFrames) noexcept
    {
        if (amount < 0.001f)
            return;

        for (std::size_t i = 0; i < numFrames; ++i)
        {
            const float monoIn = (left[i] + right[i]) * 0.5f;

            float resL = 0.0f;
            float resR = 0.0f;

            for (auto& r : resonators)
            {
                if (! r.active && r.env < 0.0001f)
                    continue;

                resL += r.filterL.process (monoIn) * r.env * r.gain;
                resR += r.filterR.process (monoIn) * r.env * r.gain;

                // Ataque suave para nao estalar quando um ressonador e ligado
                // ou retocado; release segue o decay proprio quando se apaga.
                if (r.active)
                    r.env += (1.0f - r.env) * attackRate;
                else
                    r.env *= r.decay;
            }

            left[i] += resL * amount * 0.4f;
            right[i] += resR * amount * 0.4f;
        }
    }

private:
    float noteToFreq (int note) const noexcept
    {
        return 440.0f * std::pow (2.0f, (note - 69.0f) / 12.0f);
    }

    void updateResonators() noexcept
    {
        // O pedal NAO acende os 128 ressonadores. Nao ha 128 slots, e fazer
        // todos apontarem para as notas 0..15 (sub-audio) com env=1 de uma vez
        // era um snap violento a cada press/release -- o "estalo/travada" que
        // o pedal dava. O sustain ja segura as notas soltas porque noteOff, com
        // o pedal no ar, nao chama este rebuild; soltar o pedal recolhe tudo
        // com decay suave. Retocar so quando a nota do slot MUDA, e de 0.
        int activeCount = 0;

        for (int n = 0; n < 128; ++n)
        {
            if (! activeKeys[n])
                continue;

            if (activeCount < static_cast<int> (resonators.size()))
            {
                auto& r = resonators[activeCount];

                if (r.note != n)
                {
                    r.note = n;
                    r.gain = 1.0f / (1.0f + 0.1f * activeCount);

                    const float freq = noteToFreq (n) * (1.0f + tuneOffset);

                    if (freq < sr * 0.45f)
                    {
                        r.filterL.setup (Biquad::Bandpass, freq, sr, 8.0f);
                        r.filterR.setup (Biquad::Bandpass, freq * 1.002f, sr, 8.0f);   // spread stereo
                    }

                    r.env = 0.0f;   // re-ataca do silencio em vez de estalar em 1,0
                }

                r.active = true;
                activeCount++;
            }
        }

        for (std::size_t i = static_cast<std::size_t> (activeCount); i < resonators.size(); ++i)
            resonators[i].active = false;
    }

    float sr = 44100.0f;
    float amount = 0.5f;
    float tuneOffset = 0.0f;
    float attackRate = 0.0f;
    bool sustainPedal = false;
    std::array<bool, 128> activeKeys{};
    std::array<Resonator, 16> resonators{};
};

} // namespace forja::pianoforge
