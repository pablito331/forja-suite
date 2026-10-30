#pragma once

// Forja adapter for Airwindows ButterComp2 (MIT, Chris Johnson).
// Reference: Forja/Source/forja/vendor/airwindows/ButterComp2, commit
// b110fac3991fff7c4655cc2d25d6a3e221f9799c. The gain computer and the four
// parallel controls are a direct transcription of processDoubleReplacing.
//
// Deviations: the original declares `static` noisesourceL/R inside the loop;
// they are per-instance members here. Forja adds bypass smoothing, metering
// and the APVTS-facing parameter struct. No heap allocation, logging or locks.

#include "forja/dsp/ForjaDspCommon.h"

namespace forja::dsp
{

class TemperaDSP
{
public:
    struct Parameters
    {
        float compress = 0.0f;
        float output = 0.5f;
        float mix = 1.0f;
        bool bypass = false;
    };

    void prepare (double newSampleRate) noexcept
    {
        setSampleRate (newSampleRate);
        reset();
    }

    void setSampleRate (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    }

    void reset() noexcept
    {
        controlAposL = controlAnegL = controlBposL = controlBnegL = 1.0;
        targetposL = targetnegL = 1.0;
        lastOutputL = 0.0;
        controlAposR = controlAnegR = controlBposR = controlBnegR = 1.0;
        targetposR = targetnegR = 1.0;
        lastOutputR = 0.0;
        flip = false;
        noisesourceL = 0;
        noisesourceR = 850010;
        fpdL = 22222u;
        fpdR = 22222u;
        bypassMix = params.bypass ? 1.0 : 0.0;
        grLinearMin = 1.0;
        clearMeters();
    }

    void setParameters (const Parameters& newParams) noexcept
    {
        params.compress = clamp01 (newParams.compress);
        params.output = clamp01 (newParams.output);
        params.mix = clamp01 (newParams.mix);
        params.bypass = newParams.bypass;
    }

    void process (float* left, float* right, int numSamples) noexcept
    {
        processSamples (left, right, numSamples);
    }

    void processDouble (double* left, double* right, int numSamples) noexcept
    {
        processSamples (left, right, numSamples);
    }

private:
    template <typename Sample>
    void processSamples (Sample* left, Sample* right, int numSamples) noexcept
    {
        const double overallscale = sampleRate / 44100.0;

        const double inputgain = std::pow (10.0, ((double) params.compress * 14.0) / 20.0);
        const double compfactor = 0.012 * ((double) params.compress / 135.0);
        const double output = (double) params.output * 2.0;
        const double wet = (double) params.mix;

        double outputgain = inputgain;
        outputgain -= 1.0;
        outputgain /= 1.5;
        outputgain += 1.0;

        const double targetBypass = params.bypass ? 1.0 : 0.0;
        const double smoothing = 1.0 - std::exp (-1.0 / (0.010 * sampleRate));

        for (int i = 0; i < numSamples; ++i)
        {
            const double dryInL = (double) left[i];
            const double dryInR = (double) right[i];
            double inputSampleL = dryInL;
            double inputSampleR = dryInR;

            inPeakL = std::max (inPeakL, (float) std::abs (left[i]));
            inPeakR = std::max (inPeakR, (float) std::abs (right[i]));
            if (std::abs (left[i]) >= 1.0f || std::abs (right[i]) >= 1.0f)
                inClip = true;

            noisesourceL = noisesourceL % 1700021;
            noisesourceL++;
            std::int32_t residue = wrapMul (noisesourceL, noisesourceL);
            residue %= 170003; residue = wrapMul (residue, residue);
            residue %= 17011;  residue = wrapMul (residue, residue);
            residue %= 1709;   residue = wrapMul (residue, residue);
            residue %= 173;    residue = wrapMul (residue, residue);
            residue %= 17;
            double applyresidue = (double) residue;
            applyresidue *= 0.00000001;
            applyresidue *= 0.00000001;
            inputSampleL += applyresidue;
            if (inputSampleL < 1.2e-38 && -inputSampleL < 1.2e-38)
                inputSampleL -= applyresidue;

            noisesourceR = noisesourceR % 1700021;
            noisesourceR++;
            residue = wrapMul (noisesourceR, noisesourceR);
            residue %= 170003; residue = wrapMul (residue, residue);
            residue %= 17011;  residue = wrapMul (residue, residue);
            residue %= 1709;   residue = wrapMul (residue, residue);
            residue %= 173;    residue = wrapMul (residue, residue);
            residue %= 17;
            applyresidue = (double) residue;
            applyresidue *= 0.00000001;
            applyresidue *= 0.00000001;
            inputSampleR += applyresidue;
            if (inputSampleR < 1.2e-38 && -inputSampleR < 1.2e-38)
                inputSampleR -= applyresidue;

            const double drySampleL = inputSampleL;
            const double drySampleR = inputSampleR;

            inputSampleL *= inputgain;
            inputSampleR *= inputgain;

            double divisor = compfactor / (1.0 + std::abs (lastOutputL));
            divisor /= overallscale;
            double remainder = divisor;
            divisor = 1.0 - divisor;

            double inputposL = inputSampleL + 1.0;
            if (inputposL < 0.0) inputposL = 0.0;
            const double outputposL = std::min (inputposL / 2.0, 1.0);
            inputposL *= inputposL;
            targetposL *= divisor;
            targetposL += (inputposL * remainder);
            const double calcposL = std::pow (1.0 / targetposL, 2);

            double inputnegL = (-inputSampleL) + 1.0;
            if (inputnegL < 0.0) inputnegL = 0.0;
            const double outputnegL = std::min (inputnegL / 2.0, 1.0);
            inputnegL *= inputnegL;
            targetnegL *= divisor;
            targetnegL += (inputnegL * remainder);
            const double calcnegL = std::pow (1.0 / targetnegL, 2);

            if (inputSampleL > 0.0)
            {
                if (flip)
                {
                    controlAposL *= divisor;
                    controlAposL += (calcposL * remainder);
                }
                else
                {
                    controlBposL *= divisor;
                    controlBposL += (calcposL * remainder);
                }
            }
            else
            {
                if (flip)
                {
                    controlAnegL *= divisor;
                    controlAnegL += (calcnegL * remainder);
                }
                else
                {
                    controlBnegL *= divisor;
                    controlBnegL += (calcnegL * remainder);
                }
            }

            divisor = compfactor / (1.0 + std::abs (lastOutputR));
            divisor /= overallscale;
            remainder = divisor;
            divisor = 1.0 - divisor;

            double inputposR = inputSampleR + 1.0;
            if (inputposR < 0.0) inputposR = 0.0;
            const double outputposR = std::min (inputposR / 2.0, 1.0);
            inputposR *= inputposR;
            targetposR *= divisor;
            targetposR += (inputposR * remainder);
            const double calcposR = std::pow (1.0 / targetposR, 2);

            double inputnegR = (-inputSampleR) + 1.0;
            if (inputnegR < 0.0) inputnegR = 0.0;
            const double outputnegR = std::min (inputnegR / 2.0, 1.0);
            inputnegR *= inputnegR;
            targetnegR *= divisor;
            targetnegR += (inputnegR * remainder);
            const double calcnegR = std::pow (1.0 / targetnegR, 2);

            if (inputSampleR > 0.0)
            {
                if (flip)
                {
                    controlAposR *= divisor;
                    controlAposR += (calcposR * remainder);
                }
                else
                {
                    controlBposR *= divisor;
                    controlBposR += (calcposR * remainder);
                }
            }
            else
            {
                if (flip)
                {
                    controlAnegR *= divisor;
                    controlAnegR += (calcnegR * remainder);
                }
                else
                {
                    controlBnegR *= divisor;
                    controlBnegR += (calcnegR * remainder);
                }
            }

            double totalmultiplierL;
            double totalmultiplierR;
            if (flip)
            {
                totalmultiplierL = (controlAposL * outputposL) + (controlAnegL * outputnegL);
                totalmultiplierR = (controlAposR * outputposR) + (controlAnegR * outputnegR);
            }
            else
            {
                totalmultiplierL = (controlBposL * outputposL) + (controlBnegL * outputnegL);
                totalmultiplierR = (controlBposR * outputposR) + (controlBnegR * outputnegR);
            }

            grLinearMin = std::min (grLinearMin, std::min (totalmultiplierL, totalmultiplierR));

            inputSampleL *= totalmultiplierL;
            inputSampleL /= outputgain;
            inputSampleR *= totalmultiplierR;
            inputSampleR /= outputgain;

            if (output != 1.0)
            {
                inputSampleL *= output;
                inputSampleR *= output;
            }

            if (wet != 1.0)
            {
                inputSampleL = (inputSampleL * wet) + (drySampleL * (1.0 - wet));
                inputSampleR = (inputSampleR * wet) + (drySampleR * (1.0 - wet));
            }

            lastOutputL = inputSampleL;
            lastOutputR = inputSampleR;

            flip = !flip;

            fpdL ^= fpdL << 13; fpdL ^= fpdL >> 17; fpdL ^= fpdL << 5;
            fpdR ^= fpdR << 13; fpdR ^= fpdR >> 17; fpdR ^= fpdR << 5;

            bypassMix += (targetBypass - bypassMix) * smoothing;

            const double outL = inputSampleL * (1.0 - bypassMix) + dryInL * bypassMix;
            const double outR = inputSampleR * (1.0 - bypassMix) + dryInR * bypassMix;

            outPeakL = std::max (outPeakL, (float) std::abs (outL));
            outPeakR = std::max (outPeakR, (float) std::abs (outR));
            if (std::abs (outL) >= 1.0 || std::abs (outR) >= 1.0)
                outClip = true;

            left[i] = (Sample) outL;
            right[i] = (Sample) outR;
        }
    }

public:
    MeterFrame readMeters (bool resetAfterRead = true) noexcept
    {
        MeterFrame frame;
        frame.inPeakL = inPeakL;
        frame.inPeakR = inPeakR;
        frame.outPeakL = outPeakL;
        frame.outPeakR = outPeakR;
        frame.gainReductionDb = std::min (0.0f, gainToDb (std::min (1.0f, (float) grLinearMin)));
        frame.inClip = inClip;
        frame.outClip = outClip;

        if (resetAfterRead)
        {
            grLinearMin = 1.0;
            clearMeters();
        }

        return frame;
    }

    void resetClip() noexcept
    {
        inClip = false;
        outClip = false;
    }

    int latencySamples() const noexcept { return 0; }

    // Test hook: keeps the parity harness deterministic.
    void setNoiseState (std::uint32_t newFpdL, std::uint32_t newFpdR) noexcept
    {
        fpdL = newFpdL;
        fpdR = newFpdR;
    }

private:
    static std::int32_t wrapMul (std::int32_t a, std::int32_t b) noexcept
    {
        return (std::int32_t) ((std::uint32_t) a * (std::uint32_t) b);
    }

    void clearMeters() noexcept
    {
        inPeakL = inPeakR = outPeakL = outPeakR = 0.0f;
        inClip = false;
        outClip = false;
    }

    double sampleRate = 48000.0;
    Parameters params {};

    double controlAposL = 1.0, controlAnegL = 1.0;
    double controlBposL = 1.0, controlBnegL = 1.0;
    double targetposL = 1.0, targetnegL = 1.0;
    double lastOutputL = 0.0;
    double controlAposR = 1.0, controlAnegR = 1.0;
    double controlBposR = 1.0, controlBnegR = 1.0;
    double targetposR = 1.0, targetnegR = 1.0;
    double lastOutputR = 0.0;
    bool flip = false;

    std::int32_t noisesourceL = 0;
    std::int32_t noisesourceR = 850010;
    std::uint32_t fpdL = 22222u;
    std::uint32_t fpdR = 22222u;

    double bypassMix = 0.0;
    double grLinearMin = 1.0;

    float inPeakL = 0.0f, inPeakR = 0.0f;
    float outPeakL = 0.0f, outPeakR = 0.0f;
    bool inClip = false, outClip = false;
};

} // namespace forja::dsp
