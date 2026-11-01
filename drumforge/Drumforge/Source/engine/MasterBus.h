#pragma once

#include <algorithm>
#include <cmath>

#include "forja/dsp/ForjaDspCommon.h"
#include "forja/dsp/ForjaLixa.h"
#include "forja/dsp/ForjaSalao.h"
#include "forja/dsp/ForjaTempera.h"

#include "Mapping.h"

namespace forja::drumforge
{

// Barramento master: retornos FX (SALAO e LIXA), TRIM, compressor ButterComp
// (TEMPERA) e limitador de pico sem latencia. O limitador e camada Forja.
class MasterBus
{
public:
    struct Meter
    {
        float peakLeft = 0.0f;
        float peakRight = 0.0f;
        float reductionDb = 0.0f;
        float compReductionDb = 0.0f;
        float limReductionDb = 0.0f;
        bool clip = false;
    };

    void prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
        tempera.prepare (sampleRate);
        salao.prepare (sampleRate);
        lixa.prepare (sampleRate);
        trimSmoothing = mapping::smoothingCoefficient (12.0, sampleRate);
        limiterRelease = mapping::smoothingCoefficient (80.0, sampleRate);
        reset();
    }

    void reset() noexcept
    {
        tempera.reset();
        salao.reset();
        lixa.reset();
        trimGain = 0.0;
        limiterGain = 1.0f;
        peakLeft = peakRight = 0.0f;
        reductionDb = 0.0f;
        compMeterDb = limMeterDb = 0.0f;
        clip = false;
        primed = false;
    }

    void setParameters (const MasterParameters& newParameters) noexcept
    {
        parameters = newParameters;
    }

    void process (float* left, float* right,
                  float* fx1Left, float* fx1Right,
                  float* fx2Left, float* fx2Right,
                  int numSamples) noexcept
    {
        if (numSamples <= 0)
            return;

        const bool fx1Active = parameters.fx1Regen > 0.0005f;
        const bool fx2Active = parameters.fx2Drive > 0.0005f;

        if (fx1Active)
        {
            forja::dsp::SalaoDSP::Parameters fx1;
            fx1.regen = parameters.fx1Regen;
            fx1.position = parameters.fx1Position;
            fx1.downRez = 1.0f;
            fx1.distance = 0.3f;
            fx1.dryWet = 1.0f;
            // O kRockstar2 original gera room tone proprio (pico perto de -52 dBFS)
            // e o master fica com o SALAO ligado por padrao. Sem isso o plugin
            // assobia com a mesa parada; o SALAO standalone mantem o room tone.
            fx1.roomTone = 0.0f;
            salao.setParameters (fx1);
            salao.process (fx1Left, fx1Right, numSamples);
        }

        if (fx2Active)
        {
            forja::dsp::LixaDSP::Parameters fx2;
            fx2.drive = parameters.fx2Drive;
            lixa.setParameters (fx2);
            lixa.process (fx2Left, fx2Right, numSamples);
        }

        for (int i = 0; i < numSamples; ++i)
        {
            if (fx1Active)
            {
                left[i] += fx1Left[i];
                right[i] += fx1Right[i];
            }

            if (fx2Active)
            {
                left[i] += fx2Left[i];
                right[i] += fx2Right[i];
            }
        }

        const double trimTarget = (double) mapping::dbToGain (mapping::trimDb (parameters.trim));
        if (! primed)
        {
            trimGain = trimTarget;
            primed = true;
        }

        for (int i = 0; i < numSamples; ++i)
        {
            trimGain += (trimTarget - trimGain) * trimSmoothing;
            left[i] *= (float) trimGain;
            right[i] *= (float) trimGain;
        }

        float compressorReduction = 0.0f;

        if (parameters.comp > 0.0005f)
        {
            forja::dsp::TemperaDSP::Parameters compressor;
            compressor.compress = parameters.comp;
            compressor.output = 0.5f;
            compressor.mix = 1.0f;
            tempera.setParameters (compressor);
            tempera.process (left, right, numSamples);

            const auto frame = tempera.readMeters (true);
            compressorReduction = frame.gainReductionDb;
        }

        if (parameters.limiter)
            applyLimiter (left, right, numSamples);
        else
            limiterReductionDb = 0.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            const float magnitudeLeft = std::abs (left[i]);
            const float magnitudeRight = std::abs (right[i]);
            peakLeft = std::max (peakLeft, magnitudeLeft);
            peakRight = std::max (peakRight, magnitudeRight);

            if (magnitudeLeft >= 1.0f || magnitudeRight >= 1.0f)
                clip = true;

            left[i] = std::clamp (left[i], -1.0f, 1.0f);
            right[i] = std::clamp (right[i], -1.0f, 1.0f);
        }

        reductionDb = std::min (reductionDb, compressorReduction + limiterReductionDb);
        compMeterDb = std::min (compMeterDb, compressorReduction);
        limMeterDb = std::min (limMeterDb, limiterReductionDb);
    }

    Meter readMeters() noexcept
    {
        Meter meter;
        meter.peakLeft = peakLeft;
        meter.peakRight = peakRight;
        meter.reductionDb = reductionDb;
        meter.compReductionDb = compMeterDb;
        meter.limReductionDb = limMeterDb;
        meter.clip = clip;

        peakLeft = peakRight = 0.0f;
        reductionDb = 0.0f;
        compMeterDb = 0.0f;
        limMeterDb = 0.0f;
        clip = false;
        return meter;
    }

    void resetClip() noexcept
    {
        clip = false;
    }

private:
    void applyLimiter (float* left, float* right, int numSamples) noexcept
    {
        // Teto de -0,3 dBFS: ataque instantaneo e retorno de 80 ms. Sem lookahead,
        // portanto sem latencia; o clamp final e apenas rede de seguranca.
        constexpr float ceiling = 0.9660509f;
        float blockReduction = 0.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            const float peak = std::max (std::abs (left[i]), std::abs (right[i]));
            const float desired = peak > ceiling ? ceiling / peak : 1.0f;

            if (desired < limiterGain)
                limiterGain = desired;
            else
                limiterGain += (1.0f - limiterGain) * (float) limiterRelease;

            left[i] *= limiterGain;
            right[i] *= limiterGain;
            blockReduction = std::min (blockReduction, forja::dsp::gainToDb (limiterGain));
        }

        limiterReductionDb = blockReduction;
    }

    double sampleRate = 48000.0;
    MasterParameters parameters {};
    forja::dsp::TemperaDSP tempera;
    forja::dsp::SalaoDSP salao;
    forja::dsp::LixaDSP lixa;

    double trimGain = 0.0;
    double trimSmoothing = 0.01;
    double limiterRelease = 0.001;
    float limiterGain = 1.0f;
    float limiterReductionDb = 0.0f;
    bool primed = false;

    float peakLeft = 0.0f;
    float peakRight = 0.0f;
    float reductionDb = 0.0f;
    float compMeterDb = 0.0f;
    float limMeterDb = 0.0f;
    bool clip = false;
};

} // namespace forja::drumforge
