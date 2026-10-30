#pragma once

// Forja adapter for Airwindows Cabs2 (MIT, Chris Johnson).
// Reference: Forja/Source/forja/vendor/airwindows/Cabs2, commit
// b110fac3991fff7c4655cc2d25d6a3e221f9799c. The double loop is transcribed
// literally: a 21-pole highpass and a 13-pole lowpass with no prewarping,
// both driven by pow() coefficients scaled by the sample rate.
//
// Deviations: Forja adds metering and a 10 ms bypass crossfade. The original's
// commented-out 64-bit dither stays commented; only the RNG advances.

#include "forja/dsp/ForjaDspCommon.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace forja::dsp::gabinete
{

class GabineteDSP
{
public:
    struct Parameters
    {
        float lowpass = 1.0f;
        float hipass = 0.0f;
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
        for (int count = 0; count < 23; ++count)
        {
            iirHPositionL[count] = 0.0;
            iirHAngleL[count] = 0.0;
            iirHPositionR[count] = 0.0;
            iirHAngleR[count] = 0.0;
        }
        hBypass = false;

        for (int count = 0; count < 15; ++count)
        {
            iirLPositionL[count] = 0.0;
            iirLAngleL[count] = 0.0;
            iirLPositionR[count] = 0.0;
            iirLAngleR[count] = 0.0;
        }
        lBypass = false;

        fpdL = 22222u;
        fpdR = 22222u;
        bypassMix = params.bypass ? 1.0 : 0.0;
        clearMeters();
    }

    void setParameters (const Parameters& newParams) noexcept
    {
        params.lowpass = clamp01 (newParams.lowpass);
        params.hipass = clamp01 (newParams.hipass);
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
        const double overallscale = 1.0 / 44100.0 * sampleRate;
        const double lFreq = std::pow (std::fmax (params.lowpass, 0.002), overallscale);
        const double hFreq = std::pow (params.hipass, overallscale + 2.0);

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

            if (std::fabs (inputSampleL) < 1.18e-23)
                inputSampleL = (double) fpdL * 1.18e-17;
            if (std::fabs (inputSampleR) < 1.18e-23)
                inputSampleR = (double) fpdR * 1.18e-17;

            if (hFreq > 0.0)
            {
                double lowSampleL = inputSampleL;
                double lowSampleR = inputSampleR;
                for (int count = 0; count < 21; ++count)
                {
                    iirHAngleL[count] = (iirHAngleL[count] * (1.0 - hFreq))
                                        + ((lowSampleL - iirHPositionL[count]) * hFreq);
                    lowSampleL = ((iirHPositionL[count] + (iirHAngleL[count] * hFreq))
                                  * (1.0 - hFreq)) + (lowSampleL * hFreq);
                    iirHPositionL[count] = ((iirHPositionL[count] + (iirHAngleL[count] * hFreq))
                                            * (1.0 - hFreq)) + (lowSampleL * hFreq);
                    inputSampleL -= (lowSampleL * (1.0 / 21.0));

                    iirHAngleR[count] = (iirHAngleR[count] * (1.0 - hFreq))
                                        + ((lowSampleR - iirHPositionR[count]) * hFreq);
                    lowSampleR = ((iirHPositionR[count] + (iirHAngleR[count] * hFreq))
                                  * (1.0 - hFreq)) + (lowSampleR * hFreq);
                    iirHPositionR[count] = ((iirHPositionR[count] + (iirHAngleR[count] * hFreq))
                                            * (1.0 - hFreq)) + (lowSampleR * hFreq);
                    inputSampleR -= (lowSampleR * (1.0 / 21.0));
                }
                hBypass = false;
            }
            else
            {
                if (! hBypass)
                {
                    hBypass = true;
                    for (int count = 0; count < 22; ++count)
                    {
                        iirHPositionL[count] = 0.0;
                        iirHAngleL[count] = 0.0;
                        iirHPositionR[count] = 0.0;
                        iirHAngleR[count] = 0.0;
                    }
                }
            }

            if (lFreq < 1.0)
            {
                for (int count = 0; count < 13; ++count)
                {
                    iirLAngleL[count] = (iirLAngleL[count] * (1.0 - lFreq))
                                        + ((inputSampleL - iirLPositionL[count]) * lFreq);
                    inputSampleL = ((iirLPositionL[count] + (iirLAngleL[count] * lFreq))
                                    * (1.0 - lFreq)) + (inputSampleL * lFreq);
                    iirLPositionL[count] = ((iirLPositionL[count] + (iirLAngleL[count] * lFreq))
                                            * (1.0 - lFreq)) + (inputSampleL * lFreq);

                    iirLAngleR[count] = (iirLAngleR[count] * (1.0 - lFreq))
                                        + ((inputSampleR - iirLPositionR[count]) * lFreq);
                    inputSampleR = ((iirLPositionR[count] + (iirLAngleR[count] * lFreq))
                                    * (1.0 - lFreq)) + (inputSampleR * lFreq);
                    iirLPositionR[count] = ((iirLPositionR[count] + (iirLAngleR[count] * lFreq))
                                            * (1.0 - lFreq)) + (inputSampleR * lFreq);
                }
                lBypass = false;
            }
            else
            {
                if (! lBypass)
                {
                    lBypass = true;
                    for (int count = 0; count < 14; ++count)
                    {
                        iirLPositionL[count] = 0.0;
                        iirLAngleL[count] = 0.0;
                        iirLPositionR[count] = 0.0;
                        iirLAngleR[count] = 0.0;
                    }
                }
            }

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

    double iirHPositionL[23] {}, iirHAngleL[23] {};
    double iirHPositionR[23] {}, iirHAngleR[23] {};
    bool hBypass = false;

    double iirLPositionL[15] {}, iirLAngleL[15] {};
    double iirLPositionR[15] {}, iirLAngleR[15] {};
    bool lBypass = false;

    std::uint32_t fpdL = 22222u;
    std::uint32_t fpdR = 22222u;

    double bypassMix = 0.0;

    float inPeakL = 0.0f, inPeakR = 0.0f;
    float outPeakL = 0.0f, outPeakR = 0.0f;
    bool inClip = false, outClip = false;
};

} // namespace forja::dsp::gabinete

namespace forja::dsp
{
using gabinete::GabineteDSP;
} // namespace forja::dsp
