#pragma once

// Adaptador Forja para o DSP do Airwindows Tube2 (MIT, Chris Johnson).
// Fonte original em Airwindows/Saturation/Brasa/Source/forja/vendor/
// airwindows/Tube2/Tube2Proc.cpp, commit
// b110fac3991fff7c4655cc2d25d6a3e221f9799c. O arquivo original não é
// compilado: o laço abaixo é a porta fiel de processDoubleReplacing.
//
// Além da válvula, há dois estágios Forja: trim de saída (-24..+24 dB) e
// bypass com crossfade de 10 ms. Nada aqui aloca, registra log ou bloqueia.

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace forja
{

inline float dbToGain (float db) noexcept
{
    return std::pow (10.0f, db * 0.05f);
}

inline float gainToDb (float gain) noexcept
{
    return gain > 0.0f ? 20.0f * std::log10 (gain) : -100.0f;
}

inline float clamp01 (float value) noexcept
{
    return std::clamp (value, 0.0f, 1.0f);
}

class BrasaDSP
{
public:
    static constexpr float clipLevel = 1.0f;

    struct Parameters
    {
        float input = 0.5f;
        float tube = 0.5f;
        float outputDb = 0.0f;
        bool bypass = false;
    };

    struct MeterFrame
    {
        float inPeakL = 0.0f;
        float inPeakR = 0.0f;
        float outPeakL = 0.0f;
        float outPeakR = 0.0f;
        bool inClip = false;
        bool outClip = false;
    };

    void prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
        reset();
    }

    void reset() noexcept
    {
        previousSampleA = previousSampleB = previousSampleC = 0.0;
        previousSampleD = previousSampleE = previousSampleF = 0.0;
        fpdL = 22222u;
        fpdR = 22222u;
        outputGain = std::pow (10.0, (double) params.outputDb * 0.05);
        bypassMix = params.bypass ? 1.0 : 0.0;
        clearMeters();
        clearClip();
    }

    void setParameters (const Parameters& newParams) noexcept
    {
        params.input = clamp01 (newParams.input);
        params.tube = clamp01 (newParams.tube);
        params.outputDb = std::clamp (newParams.outputDb, -24.0f, 24.0f);
        params.bypass = newParams.bypass;
    }

    static int powerFactorFor (float tube) noexcept
    {
        const double iterations = 1.0 - (double) clamp01 (tube);
        return std::max (1, (int) (9.0 * iterations + 1.0));
    }

    // Curva estática da válvula, sem histerese e sem média de sample rate
    // alto. Usada apenas para desenhar o gráfico de transferência.
    static float shapeStatic (float input, float inputPad, float tube) noexcept
    {
        const int powerfactor = powerFactorFor (tube);
        const double asymPad = (double) powerfactor;
        const double gainscaling = 1.0 / (double) (powerfactor + 1);
        const double outputscaling = 1.0 + 1.0 / (double) powerfactor;

        double sample = (double) input;
        if (inputPad < 1.0f)
            sample *= (double) inputPad;
        sample = std::clamp (sample, -1.0, 1.0);

        sample /= asymPad;
        double sharpen = -sample;
        if (sharpen > 0.0)
            sharpen = 1.0 + std::sqrt (sharpen);
        else
            sharpen = 1.0 - std::sqrt (-sharpen);
        sample -= sample * std::abs (sample) * sharpen * 0.25;
        sample *= asymPad;

        double factor = sample;
        for (int x = 0; x < powerfactor; ++x)
            factor *= sample;
        if ((powerfactor % 2 == 1) && (sample != 0.0))
            factor = (factor / sample) * std::abs (sample);
        factor *= gainscaling;
        sample -= factor;
        sample *= outputscaling;

        return (float) sample;
    }

    void process (float* left, float* right, int numSamples) noexcept
    {
        const double overallscale = sampleRate / 44100.0;
        const double inputPad = (double) params.input;
        const double iterations = 1.0 - (double) params.tube;
        const int powerfactor = std::max (1, (int) (9.0 * iterations + 1.0));
        const double asymPad = (double) powerfactor;
        const double gainscaling = 1.0 / (double) (powerfactor + 1);
        const double outputscaling = 1.0 + (1.0 / (double) powerfactor);
        const bool highRate = overallscale > 1.9;

        const double targetOutput = std::pow (10.0, (double) params.outputDb * 0.05);
        const double targetBypass = params.bypass ? 1.0 : 0.0;
        const double smoothing = 1.0 - std::exp (-1.0 / (0.010 * sampleRate));

        for (int i = 0; i < numSamples; ++i)
        {
            const float dryL = left[i];
            const float dryR = right[i];
            double inputSampleL = (double) dryL;
            double inputSampleR = (double) dryR;

            const float absInL = std::abs (dryL);
            const float absInR = std::abs (dryR);
            inPeakL = std::max (inPeakL, absInL);
            inPeakR = std::max (inPeakR, absInR);
            if (absInL >= clipLevel || absInR >= clipLevel)
                inClip = true;

            if (std::abs (inputSampleL) < 1.18e-23)
                inputSampleL = (double) fpdL * 1.18e-17;
            if (std::abs (inputSampleR) < 1.18e-23)
                inputSampleR = (double) fpdR * 1.18e-17;

            if (inputPad < 1.0)
            {
                inputSampleL *= inputPad;
                inputSampleR *= inputPad;
            }

            if (highRate)
            {
                double stored = inputSampleL;
                inputSampleL += previousSampleA;
                previousSampleA = stored;
                inputSampleL *= 0.5;
                stored = inputSampleR;
                inputSampleR += previousSampleB;
                previousSampleB = stored;
                inputSampleR *= 0.5;
            }

            if (inputSampleL > 1.0)
                inputSampleL = 1.0;
            if (inputSampleL < -1.0)
                inputSampleL = -1.0;
            if (inputSampleR > 1.0)
                inputSampleR = 1.0;
            if (inputSampleR < -1.0)
                inputSampleR = -1.0;

            // modelador assimétrico: base achatada, topo pontudo
            inputSampleL /= asymPad;
            double sharpen = -inputSampleL;
            if (sharpen > 0.0)
                sharpen = 1.0 + std::sqrt (sharpen);
            else
                sharpen = 1.0 - std::sqrt (-sharpen);
            inputSampleL -= inputSampleL * std::abs (inputSampleL) * sharpen * 0.25;
            inputSampleL *= asymPad;

            inputSampleR /= asymPad;
            sharpen = -inputSampleR;
            if (sharpen > 0.0)
                sharpen = 1.0 + std::sqrt (sharpen);
            else
                sharpen = 1.0 - std::sqrt (-sharpen);
            inputSampleR -= inputSampleR * std::abs (inputSampleR) * sharpen * 0.25;
            inputSampleR *= asymPad;

            // iterações de potência do Tube original
            double factor = inputSampleL;
            for (int x = 0; x < powerfactor; ++x)
                factor *= inputSampleL;
            if ((powerfactor % 2 == 1) && (inputSampleL != 0.0))
                factor = (factor / inputSampleL) * std::abs (inputSampleL);
            factor *= gainscaling;
            inputSampleL -= factor;
            inputSampleL *= outputscaling;

            factor = inputSampleR;
            for (int x = 0; x < powerfactor; ++x)
                factor *= inputSampleR;
            if ((powerfactor % 2 == 1) && (inputSampleR != 0.0))
                factor = (factor / inputSampleR) * std::abs (inputSampleR);
            factor *= gainscaling;
            inputSampleR -= factor;
            inputSampleR *= outputscaling;

            if (highRate)
            {
                double stored = inputSampleL;
                inputSampleL += previousSampleC;
                previousSampleC = stored;
                inputSampleL *= 0.5;
                stored = inputSampleR;
                inputSampleR += previousSampleD;
                previousSampleD = stored;
                inputSampleR *= 0.5;
            }

            // histerese / fuzz pontudo
            double slew = previousSampleE - inputSampleL;
            if (highRate)
            {
                const double stored = inputSampleL;
                inputSampleL += previousSampleE;
                previousSampleE = stored;
                inputSampleL *= 0.5;
            }
            else
            {
                previousSampleE = inputSampleL;
            }

            if (slew > 0.0)
                slew = 1.0 + (std::sqrt (slew) * 0.5);
            else
                slew = 1.0 - (std::sqrt (-slew) * 0.5);

            inputSampleL -= inputSampleL * std::abs (inputSampleL) * slew * gainscaling;
            if (inputSampleL > 0.52)
                inputSampleL = 0.52;
            if (inputSampleL < -0.52)
                inputSampleL = -0.52;
            inputSampleL *= 1.923076923076923;

            slew = previousSampleF - inputSampleR;
            if (highRate)
            {
                const double stored = inputSampleR;
                inputSampleR += previousSampleF;
                previousSampleF = stored;
                inputSampleR *= 0.5;
            }
            else
            {
                previousSampleF = inputSampleR;
            }

            if (slew > 0.0)
                slew = 1.0 + (std::sqrt (slew) * 0.5);
            else
                slew = 1.0 - (std::sqrt (-slew) * 0.5);

            inputSampleR -= inputSampleR * std::abs (inputSampleR) * slew * gainscaling;
            if (inputSampleR > 0.52)
                inputSampleR = 0.52;
            if (inputSampleR < -0.52)
                inputSampleR = -0.52;
            inputSampleR *= 1.923076923076923;

            // dither de 32 bits do original
            int expon = 0;
            std::frexp ((float) inputSampleL, &expon);
            fpdL ^= fpdL << 13;
            fpdL ^= fpdL >> 17;
            fpdL ^= fpdL << 5;
            inputSampleL += ((double) fpdL - 2147483647.0) * 5.5e-36
                            * std::pow (2.0, expon + 62);

            std::frexp ((float) inputSampleR, &expon);
            fpdR ^= fpdR << 13;
            fpdR ^= fpdR >> 17;
            fpdR ^= fpdR << 5;
            inputSampleR += ((double) fpdR - 2147483647.0) * 5.5e-36
                            * std::pow (2.0, expon + 62);

            // estágio Forja: trim de saída + bypass com rampa de 10 ms
            outputGain += (targetOutput - outputGain) * smoothing;
            bypassMix += (targetBypass - bypassMix) * smoothing;

            const float outL = (float) (inputSampleL * outputGain * (1.0 - bypassMix)
                                        + (double) dryL * bypassMix);
            const float outR = (float) (inputSampleR * outputGain * (1.0 - bypassMix)
                                        + (double) dryR * bypassMix);

            const float absOutL = std::abs (outL);
            const float absOutR = std::abs (outR);
            outPeakL = std::max (outPeakL, absOutL);
            outPeakR = std::max (outPeakR, absOutR);
            if (absOutL >= clipLevel || absOutR >= clipLevel)
                outClip = true;

            left[i] = outL;
            right[i] = outR;
        }
    }

    MeterFrame readMeters (bool resetAfterRead = true) noexcept
    {
        MeterFrame frame { inPeakL, inPeakR, outPeakL, outPeakR, inClip, outClip };
        if (resetAfterRead)
            clearMeters();
        return frame;
    }

    void resetClip() noexcept
    {
        clearClip();
    }

private:
    void clearMeters() noexcept
    {
        inPeakL = inPeakR = outPeakL = outPeakR = 0.0f;
    }

    void clearClip() noexcept
    {
        inClip = false;
        outClip = false;
    }

    double sampleRate = 48000.0;
    Parameters params {};

    double previousSampleA = 0.0;
    double previousSampleB = 0.0;
    double previousSampleC = 0.0;
    double previousSampleD = 0.0;
    double previousSampleE = 0.0;
    double previousSampleF = 0.0;
    std::uint32_t fpdL = 22222u;
    std::uint32_t fpdR = 22222u;

    double outputGain = 1.0;
    double bypassMix = 0.0;

    float inPeakL = 0.0f;
    float inPeakR = 0.0f;
    float outPeakL = 0.0f;
    float outPeakR = 0.0f;
    bool inClip = false;
    bool outClip = false;
};

} // namespace forja
