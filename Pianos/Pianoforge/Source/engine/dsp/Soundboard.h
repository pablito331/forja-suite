#pragma once

#include <array>
#include <cmath>

#include "Biquad.h"

namespace forja::pianoforge
{

// Corpo acustico do piano (o que faltava para "qualquer soundfont soar melhor
// ao abrir no Pianoforge"). Duas camadas fisicas, ambas aditivas/unity-quando-zero:
// nenhum estado e destruido e, com o knob no minimo, a cadeia volta a amostra crua
// (mesma garantia do bypass). Nada aloca; tudo em std::array fixo, taxa por prepare.

// 1. Resposta do tampo (SoundboardResponse): a funcao de transferencia de um tampo
//    de cauda real, aproximada por uma cascata de biquads. Nao e um tone-control
//    arbitrario -- e a curva de radiacao do corpo: o tampo e eficiente entre ~60 e
//    ~500 Hz (o "peito" do piano), tem o pico da cavidade/bridge no baixo-medio, um
//    leve scoop em ~900 Hz (tira o "box" de amostra), o pico de articulacao do
//    bridge em ~2,8 kHz (o "cantar" da nota), e ar acima. Abaixo do primeiro modo
//    (~50 Hz) o tampo quase nao radia, entao cortamos o sub (por isso ENCARACOLAR
//    grave e lodo, nao corpo -- o erro que deixavamos antes). Todo ganho escala de
//    0 dB (amount 0) ate o alvo (amount 1) por um unico knob.
class SoundboardResponse
{
public:
    void prepare (float sampleRate)
    {
        sr = sampleRate > 1000.0f ? sampleRate : 44100.0f;
        reset();
        amount = -1.0f;   // forca o primeiro setAmount a reconstruir
    }

    void reset() noexcept
    {
        subL.reset(); subR.reset();
        chestL.reset(); chestR.reset();
        bridgeL.reset(); bridgeR.reset();
        scoopL.reset(); scoopR.reset();
        singL.reset(); singR.reset();
        airL.reset(); airR.reset();
    }

    // amount 0..1 escala toda a curva. Recosta os filtros so quando o amount se
    // move alem de 0,01 (gate), para nao refazer 12 biquads por bloco quieto.
    void setAmount (float a) noexcept
    {
        const float clamped = std::fmax (0.0f, std::fmin (1.0f, a));

        if (std::fabs (clamped - amount) < 0.01f)
            return;

        amount = clamped;
        rebuild();
    }

    void process (float* left, float* right, std::size_t numFrames) noexcept
    {
        if (amount <= 0.001f)
            return;

        for (std::size_t i = 0; i < numFrames; ++i)
        {
            float l = left[i];
            float r = right[i];

            l = airL.process (singL.process (scoopL.process (bridgeL.process (chestL.process (subL.process (l))))));
            r = airR.process (singR.process (scoopR.process (bridgeR.process (chestR.process (subR.process (r))))));

            left[i] = l;
            right[i] = r;
        }
    }

private:
    void rebuild() noexcept
    {
        const float k = amount;

        // Alvos em dB da curva cheia; escalados linearmente por k.
        subL.setup (Biquad::LowShelf, 55.0f, sr, 0.7071f, k * -3.0f);      // domar sub
        subR.setup (Biquad::LowShelf, 55.0f, sr, 0.7071f, k * -3.0f);
        chestL.setup (Biquad::LowShelf, 92.0f, sr, 0.80f, k * 5.5f);       // peito
        chestR.setup (Biquad::LowShelf, 92.0f, sr, 0.80f, k * 5.5f);
        bridgeL.setup (Biquad::Peaking, 262.0f, sr, 0.9f, k * 3.0f);       // cavidade
        bridgeR.setup (Biquad::Peaking, 262.0f, sr, 0.9f, k * 3.0f);
        scoopL.setup (Biquad::Peaking, 900.0f, sr, 1.0f, k * -2.5f);       // anti-box
        scoopR.setup (Biquad::Peaking, 900.0f, sr, 1.0f, k * -2.5f);
        singL.setup (Biquad::Peaking, 2800.0f, sr, 1.3f, k * 3.5f);        // articulacao
        singR.setup (Biquad::Peaking, 2800.0f, sr, 1.3f, k * 3.5f);
        airL.setup (Biquad::HighShelf, 7500.0f, sr, 0.7071f, k * 2.5f);    // ar
        airR.setup (Biquad::HighShelf, 7500.0f, sr, 0.7071f, k * 2.5f);
    }

    float sr = 44100.0f;
    float amount = 0.0f;

    Biquad subL, subR;
    Biquad chestL, chestR;
    Biquad bridgeL, bridgeR;
    Biquad scoopL, scoopR;
    Biquad singL, singR;
    Biquad airL, airR;
};

// 2. Modos do tampo (SoundboardModes): o "vibra dentro do piano". Um tampo real e
//    um conjunto de ressonancias fixas (independentes da corda tocada) que continuam
//    soando depois que a nota morre -- o brilho de madeira que um arquivo seco nao
//    tem. Banco de passa-faixas de alto Q nas frequencias modais tipicas de um tampo
//    de cauda (primeiro modo ~60 Hz, depois 89/184... e densificando). Cada modo e
//    excitado pela soma mono do sinal e reinjetado nos dois canais com peso proprio;
//    o L e o R sao levemente desencontrados para dar largura sem virar chorus.
//    Aditivo puro: amount 0 = bypass, e como nao ha realimentacao, sempre estavel.
class SoundboardModes
{
public:
    void prepare (float sampleRate)
    {
        sr = sampleRate > 1000.0f ? sampleRate : 44100.0f;

        for (auto& m : modes)
        {
            m.filterL.reset();
            m.filterR.reset();
        }

        // Frequencias modais sao Hz acusticos fixos: o Biquad ja mapeia para a taxa
        // atual. O L e o R diferem em ~4 centavos para o corpo ter largura real, nao
        // para virar chorus (por isso o offset e minimo e constante, sem LFO).
        for (std::size_t i = 0; i < modes.size(); ++i)
        {
            modes[i].filterL.setup (Biquad::Bandpass, kFreq[i], sr, kQ);
            modes[i].filterR.setup (Biquad::Bandpass, kFreq[i] * 1.004f, sr, kQ);
            modes[i].weight = kWeight[i];
        }
    }

    void setAmount (float a) noexcept
    {
        amount = std::fmax (0.0f, std::fmin (1.0f, a));
    }

    void process (float* left, float* right, std::size_t numFrames) noexcept
    {
        if (amount <= 0.001f)
            return;

        for (std::size_t i = 0; i < numFrames; ++i)
        {
            const float monoIn = (left[i] + right[i]) * 0.5f;

            float ringL = 0.0f;
            float ringR = 0.0f;

            for (auto& m : modes)
            {
                ringL += m.filterL.process (monoIn) * m.weight;
                ringR += m.filterR.process (monoIn) * m.weight;
            }

            left[i] += ringL * amount * kInject;
            right[i] += ringR * amount * kInject;
        }
    }

private:
    struct Mode
    {
        Biquad filterL;
        Biquad filterR;
        float weight = 0.0f;
    };

    // Q alto = anel longo (a madeira continua vibrando depois da nota). 11 da cauda
    // de dezenas de ms sem virar "sino": o peso por modo e baixo e a soma e modulada
    // pelo knob, entao o anel e um brilho de corpo, nao um resonancia isolada.
    static constexpr float kQ = 11.0f;
    static constexpr float kInject = 0.16f;   // soma das vozes ja pesada; escala final

    static constexpr std::size_t kCount = 10;
    static constexpr std::array<float, kCount> kFreq {
        70.0f, 96.0f, 130.0f, 185.0f, 233.0f, 294.0f, 370.0f, 466.0f, 587.0f, 740.0f };
    // Pesos decrescentes com a frequencia: o corpo fala mais no grave/medio-grave.
    static constexpr std::array<float, kCount> kWeight {
        0.9f, 1.0f, 0.95f, 0.8f, 0.7f, 0.6f, 0.5f, 0.42f, 0.34f, 0.26f };

    std::array<Mode, kCount> modes;
    float sr = 44100.0f;
    float amount = 0.0f;
};

} // namespace forja::pianoforge
