#pragma once

#include <array>
#include <cmath>
#include <cstdint>

#include "Biquad.h"

namespace forja::pianoforge
{

// xorshift32: duas sequencias independentes por voz, uma por canal. Ruido
// identico nos dois lados colaria o transiente stereo no centro da imagem.
inline float hammerNoise (std::uint32_t& s) noexcept
{
    s ^= s << 13; s ^= s >> 17; s ^= s << 5;
    return static_cast<float> (static_cast<std::int32_t> (s)) * (1.0f / 2147483648.0f);
}

// O ruido e uniforme em [-1,1), desvio padrao 1/sqrt(3). A normalizacao por
// resposta ao impulso assume entrada de variancia 1; sem o fator o clique sai
// 5,5 dB mais alto do que foi medido.
inline constexpr float kHammerNoiseStd = 0.57735027f;

// Soma h[n]^2 da resposta ao impulso (Parseval): a variancia na saida de ruido
// branco unitario. E o fator de normalizacao exato do caminho.
inline float irEnergy (Biquad a) noexcept
{
    a.reset();
    float acc = 0.0f;
    for (int i = 0; i < 256; ++i)
    {
        const float h = a.process (i == 0 ? 1.0f : 0.0f);
        acc += h * h;
    }
    return std::sqrt (acc);
}

inline float irEnergy (Biquad a, Biquad b) noexcept
{
    a.reset();
    b.reset();
    float acc = 0.0f;
    for (int i = 0; i < 256; ++i)
    {
        const float h = b.process (a.process (i == 0 ? 1.0f : 0.0f));
        acc += h * h;
    }
    return std::sqrt (acc);
}

struct HammerVoice
{
    bool active = false;
    float envClick = 0.0f;
    float envThump = 0.0f;
    float decayClick = 0.99f;
    float decayThump = 0.999f;
    float gainClickL = 0.0f, gainClickR = 0.0f;
    float gainThump = 0.0f;
    Biquad clickL;
    Biquad clickR;
    Biquad thump;
    std::uint32_t rngA = 0x9e3779b9u;
    std::uint32_t rngB = 0x85ebca6bu;
    std::uint32_t rngC = 0x1b873593u;
};

// Transiente de martelo: o "click" (feltro saindo da corda, 800 Hz..5 kHz,
// ~10 ms) e o "thump" (massa do martelo no fundo da caixa, grave, ~32 ms).
// Porta fiel de HammerModel.hpp. Tudo em std::array fixo: nenhuma alocacao no
// prepare (que na suite e o prepareToPlay) nem no process. O lowpass de teto
// (9,5 kHz) e compartilhado e aplica na SOMA dos cliques (linearidade do filtro),
// entao custa 2 biquads por bloco, nao 2 por voz por amostra.
class HammerModel
{
public:
    void prepare (float sampleRate)
    {
        sr = sampleRate > 1000.0f ? sampleRate : 44100.0f;
        clickTail_[0].setup (Biquad::Lowpass, kTailHz, sr, 0.7071f);
        clickTail_[1].setup (Biquad::Lowpass, kTailHz, sr, 0.7071f);
        clickTail_[0].reset();
        clickTail_[1].reset();

        for (auto& v : voices)
        {
            v.active = false;
            v.envClick = 0.0f;
            v.envThump = 0.0f;
            v.clickL.reset();
            v.clickR.reset();
            v.thump.reset();
        }

        buildCal();
    }

    // Nivel de cada caminho, por nota, medido uma vez fora do tempo real.
    void buildCal()
    {
        const float tone = toneValue();

        for (int n = 0; n < 128; ++n)
        {
            const float noteNorm = static_cast<float> (n) / 127.0f;
            const float toneFactor = std::pow (2.0f, (tone - 0.5f) * 2.0f);
            const float clickFreq = std::fmin (700.0f + noteNorm * 3800.0f, sr * 0.45f) * toneFactor;
            const float thumpFreq = 90.0f + noteNorm * 140.0f;

            Biquad bp, lp = clickTail_[0], th;
            bp.setup (Biquad::Bandpass, clickFreq, sr, 1.4f);
            th.setup (Biquad::Lowpass, thumpFreq, sr, 0.7071f);
            cal_[n].rmsC = irEnergy (bp, lp) * kHammerNoiseStd;
            cal_[n].rmsT = irEnergy (th) * kHammerNoiseStd;
        }

        calSr = sr;
    }

    // So o hammerTone muda a tabela, e o degrau e um inteiro de proposito:
    // comparar o float quantizado com o guardado nunca dava igual e o primeiro
    // trigger() de cada nota refazia a tabela inteira dentro do bloco de audio.
    void refreshCalFor (float hammerTone)
    {
        int step = static_cast<int> (std::lround (hammerTone * 50.0f));
        step = std::min (50, std::max (0, step));

        if (calSr == sr && calToneStep == step)
            return;

        calToneStep = step;
        buildCal();
    }

    float toneValue() const { return static_cast<float> (calToneStep) * 0.02f; }

    void trigger (int note, int velocity, float hammerAmount, float hammerTone) noexcept
    {
        if (hammerAmount <= 0.001f || velocity <= 0)
            return;

        int slot = -1;
        float minEnv = 2.0f;

        for (std::size_t i = 0; i < voices.size(); ++i)
        {
            if (! voices[i].active) { slot = static_cast<int> (i); break; }
            if (voices[i].envClick < minEnv) { minEnv = voices[i].envClick; slot = int (i); }
        }

        if (slot < 0)
            slot = 0;

        auto& v = voices[slot];
        const float noteNorm = static_cast<float> (note & 127) / 127.0f;
        refreshCalFor (hammerTone);

        const float velNorm = static_cast<float> (velocity) / 127.0f;

        // Martelo real: um toque mais forte e mais BRILHANTE (o feltro comprime e
        // deixa passar mais alto) e mais SECO (o contato encurta). Antes o clique
        // tinha o mesmo timbre do pianissimo ao fortissimo -- o "plastic" que se
        // ouvia. brightness sobe o centro do passa-faixa ate ~+25% no fortissimo e
        // encurta o clique, sem tocar na energia medida (que segue por nota).
        const float brightness = 0.80f + 0.45f * velNorm;              // 0,80 .. 1,25
        const float toneFactor = std::pow (2.0f, (toneValue() - 0.5f) * 2.0f);
        const float clickFreq = std::fmin ((700.0f + noteNorm * 3800.0f) * brightness,
                                           sr * 0.45f) * toneFactor;
        const float clickSec = (0.020f - noteNorm * 0.011f) * (1.0f - 0.35f * velNorm);
        const float thumpFreq = 90.0f + noteNorm * 140.0f;   // 90 Hz -> 230 Hz
        const float thumpSec = 0.032f - noteNorm * 0.014f;   // 32 ms -> 18 ms

        v.clickL.setup (Biquad::Bandpass, clickFreq, sr, 1.4f);
        v.clickR.setup (Biquad::Bandpass, clickFreq, sr, 1.4f);
        v.thump.setup (Biquad::Lowpass, thumpFreq, sr, 0.7071f);
        v.decayClick = std::exp (-1.0f / (clickSec * sr));
        v.decayThump = std::exp (-1.0f / (thumpSec * sr));

        const float rmsC = cal_[note & 127].rmsC;
        const float rmsT = cal_[note & 127].rmsT;

        if (rmsC < 1e-6f || rmsT < 1e-6f)
            return;

        // O thump do fundo da caixa sobe de 0,034 para 0,052: e o "baque" de madeira
        // no ataque que faltava; o click segue a normalizacao por nota (o excesso com
        // velocity e fisico -- toque mais forte carrega mais energia).
        const float click = std::pow (velNorm, 0.55f) * hammerAmount * 0.115f / rmsC;
        const float thump = std::pow (velNorm, 0.80f) * hammerAmount * 0.052f / rmsT;

        const float pan = (noteNorm - 0.5f) * 0.6f;
        v.gainClickL = click * (1.0f - pan) * 0.5f;
        v.gainClickR = click * (1.0f + pan) * 0.5f;
        v.gainThump = thump;
        v.envClick = 1.0f;
        v.envThump = 1.0f;

        rngTick += 0x2545F491u;
        v.rngA = 0x9e3779b9u ^ (std::uint32_t (note) * 0x6c078965u) ^ rngTick;
        v.rngB = 0x85ebca6bu ^ (std::uint32_t ((note + 41) & 127) * 0x6c078965u) ^ rngTick;
        v.rngC = 0x1b873593u ^ (std::uint32_t ((note + 89) & 127) * 0x85ebca6bu) ^ (rngTick * 3u);
        if (v.rngA == 0) v.rngA = 0x6d2b79f5u;
        if (v.rngB == 0) v.rngB = 0x3c3ef461u;
        if (v.rngC == 0) v.rngC = 0x27d4eb2fu;
        v.active = true;
    }

    void process (float* left, float* right, std::size_t numFrames) noexcept
    {
        for (std::size_t i = 0; i < numFrames; ++i)
        {
            float sumL = 0.0f, sumR = 0.0f, thump = 0.0f;

            for (auto& v : voices)
            {
                if (! v.active)
                    continue;

                const float e = v.envClick, t = v.envThump;
                thump += v.thump.process (hammerNoise (v.rngA)) * t * v.gainThump;
                sumL += v.clickL.process (hammerNoise (v.rngB)) * e * v.gainClickL;
                sumR += v.clickR.process (hammerNoise (v.rngC)) * e * v.gainClickR;
                v.envClick *= v.decayClick;
                v.envThump *= v.decayThump;

                if (v.envClick < 0.0005f && v.envThump < 0.0005f)
                    v.active = false;
            }

            left[i] += clickTail_[0].process (sumL) + thump;
            right[i] += clickTail_[1].process (sumR) + thump;
        }
    }

private:
    static constexpr float kTailHz = 9500.0f;
    float sr = 44100.0f;
    std::uint32_t rngTick = 0;
    Biquad clickTail_[2];

    struct Cal { float rmsC = 1.0f, rmsT = 1.0f; };
    std::array<Cal, 128> cal_;
    float calSr = 0.0f;
    int calToneStep = 25;
    std::array<HammerVoice, 16> voices;
};

} // namespace forja::pianoforge
