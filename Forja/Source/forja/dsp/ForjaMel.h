#pragma once

// Forja adapter for Airwindows Sweeten (MIT, Chris Johnson).
// Reference: Forja/Source/forja/vendor/airwindows/Sweeten, commit
// b110fac3991fff7c4655cc2d25d6a3e221f9799c. The double loop is transcribed
// literally: a second-harmonic square term, filtered before and after the
// nonlinearity by stacked one-pole averaging, then subtracted from the dry
// signal. The averaging stack widens with the sample rate exactly as upstream.
//
// Deviations: Forja adds metering and a 10 ms bypass crossfade. The original's
// commented-out 64-bit dither stays commented; only the RNG advances.

#include "forja/dsp/ForjaDspCommon.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace forja::dsp::mel
{

class MelDSP
{
public:
    struct Parameters
    {
        float sweeten = 0.0f;
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
        for (double& value : savg)
            value = 0.0;

        fpdL = 22222u;
        fpdR = 22222u;
        bypassMix = params.bypass ? 1.0 : 0.0;
        clearMeters();
    }

    void setParameters (const Parameters& newParams) noexcept
    {
        params.sweeten = clamp01 (newParams.sweeten);
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

    MeterFrame readMeters (bool resetAfterRead = true) noexcept
    {
        MeterFrame frame;
        frame.inPeakL = inPeakL;
        frame.inPeakR = inPeakR;
        frame.outPeakL = outPeakL;
        frame.outPeakR = outPeakR;
        frame.gainReductionDb = 0.0f;
        frame.inClip = inClip;
        frame.outClip = outClip;

        if (resetAfterRead)
            clearMeters();

        return frame;
    }

    void resetClip() noexcept
    {
        inClip = false;
        outClip = false;
    }

    int latencySamples() const noexcept { return 0; }

    void setNoiseState (std::uint32_t newFpdL, std::uint32_t newFpdR) noexcept
    {
        fpdL = newFpdL;
        fpdR = newFpdR;
    }

private:
    template <typename Sample>
    void processSamples (Sample* left, Sample* right, int numSamples) noexcept
    {
        const double sampleRateNow = sampleRate;
        double overallscale = 1.0;
        overallscale /= 44100.0;
        overallscale *= sampleRateNow;
        int cycleEnd = (int) std::floor (overallscale);
        if (cycleEnd < 1) cycleEnd = 1;
        if (cycleEnd > 4) cycleEnd = 4;

        const int sweetBits = 10 - (int) std::floor ((double) params.sweeten * 10.0);
        double sweet = 1.0;
        switch (sweetBits)
        {
            case 11: sweet = 0.00048828125; break;
            case 10: sweet = 0.0009765625; break;
            case 9: sweet = 0.001953125; break;
            case 8: sweet = 0.00390625; break;
            case 7: sweet = 0.0078125; break;
            case 6: sweet = 0.015625; break;
            case 5: sweet = 0.03125; break;
            case 4: sweet = 0.0625; break;
            case 3: sweet = 0.125; break;
            case 2: sweet = 0.25; break;
            case 1: sweet = 0.5; break;
            case 0: sweet = 1.0; break;
            case -1: sweet = 2.0; break;
        }

        const double targetBypass = params.bypass ? 1.0 : 0.0;
        const double smoothing = 1.0 - std::exp (-1.0 / (0.010 * sampleRateNow));

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

            if (std::fabs (inputSampleL) < 1.18e-23)
                inputSampleL = (double) fpdL * 1.18e-17;
            if (std::fabs (inputSampleR) < 1.18e-23)
                inputSampleR = (double) fpdR * 1.18e-17;

            double sweetSample = inputSampleL;
            double sv = sweetSample; sweetSample = (sweetSample + savg[0]) * 0.5; savg[0] = sv;
            if (cycleEnd > 1)
            {
                sv = sweetSample; sweetSample = (sweetSample + savg[1]) * 0.5; savg[1] = sv;
                if (cycleEnd > 2)
                {
                    sv = sweetSample; sweetSample = (sweetSample + savg[2]) * 0.5; savg[2] = sv;
                    if (cycleEnd > 3)
                    {
                        sv = sweetSample; sweetSample = (sweetSample + savg[3]) * 0.5; savg[3] = sv;
                    }
                }
            }
            sweetSample = (sweetSample * sweetSample * sweet);
            sv = sweetSample; sweetSample = (sweetSample + savg[4]) * 0.5; savg[4] = sv;
            if (cycleEnd > 1)
            {
                sv = sweetSample; sweetSample = (sweetSample + savg[5]) * 0.5; savg[5] = sv;
                if (cycleEnd > 2)
                {
                    sv = sweetSample; sweetSample = (sweetSample + savg[6]) * 0.5; savg[6] = sv;
                    if (cycleEnd > 3)
                    {
                        sv = sweetSample; sweetSample = (sweetSample + savg[7]) * 0.5; savg[7] = sv;
                    }
                }
            }
            inputSampleL -= sweetSample;

            sweetSample = inputSampleR;
            sv = sweetSample; sweetSample = (sweetSample + savg[8]) * 0.5; savg[8] = sv;
            if (cycleEnd > 1)
            {
                sv = sweetSample; sweetSample = (sweetSample + savg[9]) * 0.5; savg[9] = sv;
                if (cycleEnd > 2)
                {
                    sv = sweetSample; sweetSample = (sweetSample + savg[10]) * 0.5; savg[10] = sv;
                    if (cycleEnd > 3)
                    {
                        sv = sweetSample; sweetSample = (sweetSample + savg[11]) * 0.5; savg[11] = sv;
                    }
                }
            }
            sweetSample = (sweetSample * sweetSample * sweet);
            sv = sweetSample; sweetSample = (sweetSample + savg[12]) * 0.5; savg[12] = sv;
            if (cycleEnd > 1)
            {
                sv = sweetSample; sweetSample = (sweetSample + savg[13]) * 0.5; savg[13] = sv;
                if (cycleEnd > 2)
                {
                    sv = sweetSample; sweetSample = (sweetSample + savg[14]) * 0.5; savg[14] = sv;
                    if (cycleEnd > 3)
                    {
                        sv = sweetSample; sweetSample = (sweetSample + savg[15]) * 0.5; savg[15] = sv;
                    }
                }
            }
            inputSampleR -= sweetSample;

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

    void clearMeters() noexcept
    {
        inPeakL = inPeakR = outPeakL = outPeakR = 0.0f;
        inClip = false;
        outClip = false;
    }

    double sampleRate = 48000.0;
    Parameters params {};

    std::uint32_t fpdL = 22222u;
    std::uint32_t fpdR = 22222u;

    double savg[19] {};
    double bypassMix = 0.0;

    float inPeakL = 0.0f, inPeakR = 0.0f;
    float outPeakL = 0.0f, outPeakR = 0.0f;
    bool inClip = false, outClip = false;
};

} // namespace forja::dsp::mel

namespace forja::dsp
{
using mel::MelDSP;
} // namespace forja::dsp
