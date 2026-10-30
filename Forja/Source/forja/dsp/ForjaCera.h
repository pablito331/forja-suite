#pragma once

// Forja adapter for Airwindows PurestWarm3 (MIT, Chris Johnson).
// Reference: Forja/Source/forja/vendor/airwindows/PurestWarm3, commit
// b110fac3991fff7c4655cc2d25d6a3e221f9799c. The double loop is transcribed
// literally: an asymmetric waveshaper scaled by (A - 0.5) * 8, followed by a
// derivative chain (velocity, acceleration, jerk, snap) that modulates the
// sample. At A = 0.5 the asymmetry is exactly zero and the plugin is neutral.
//
// Deviations: Forja adds metering and a 10 ms bypass crossfade. The original's
// commented-out 64-bit dither stays commented; only the RNG advances (including
// the asymmetric extra step on the right channel).

#include "forja/dsp/ForjaDspCommon.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace forja::dsp::cera
{

enum
{
    bip_dvLA, bip_dvLB, bip_dvLC, bip_dvLD, bip_pvLA, bip_pvLB, bip_pvLC, bip_pvLD,
    bip_dvRA, bip_dvRB, bip_dvRC, bip_dvRD, bip_pvRA, bip_pvRB, bip_pvRC, bip_pvRD,
    bip_total
};

class CeraDSP
{
public:
    struct Parameters
    {
        float warmth = 0.5f;
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
        for (double& value : bip)
            value = 0.0;

        fpdL = 22222u;
        fpdR = 22222u;
        bypassMix = params.bypass ? 1.0 : 0.0;
        clearMeters();
    }

    void setParameters (const Parameters& newParams) noexcept
    {
        params.warmth = clamp01 (newParams.warmth);
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
        const double asym = ((double) params.warmth - 0.5) * 8.0;

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

            if (asym != 0.0)
            {
                double bip_delta = inputSampleL;
                inputSampleL *= asym;
                inputSampleL = ((inputSampleL * inputSampleL) * -0.166666666666666) * (inputSampleL * inputSampleL) + inputSampleL;
                inputSampleL /= asym;
                bip[bip_dvLA] = bip_delta - inputSampleL;
                bip[bip_dvLB] = bip[bip_pvLA] - bip[bip_dvLA]; bip[bip_pvLA] = bip[bip_dvLA];
                bip[bip_dvLC] = bip[bip_pvLB] - bip[bip_dvLB]; bip[bip_pvLB] = bip[bip_dvLB];
                bip[bip_dvLD] = bip[bip_pvLC] - bip[bip_dvLC]; bip[bip_pvLC] = bip[bip_dvLC];
                const double bip_dvE = bip[bip_pvLD] - bip[bip_dvLD]; bip[bip_pvLD] = bip[bip_dvLD];
                inputSampleL *= (1.0 + (std::fabs (bip[bip_dvLC]) * 0.0618)
                                      + (std::fabs (bip[bip_dvLD]) * -0.05982)
                                      + (std::fabs (bip_dvE) * 0.0206));

                bip_delta = inputSampleR;
                inputSampleR *= asym;
                inputSampleR = ((inputSampleR * inputSampleR) * -0.166666666666666) * (inputSampleR * inputSampleR) + inputSampleR;
                inputSampleR /= asym;
                bip[bip_dvRA] = bip_delta - inputSampleR;
                bip[bip_dvRB] = bip[bip_pvRA] - bip[bip_dvRA]; bip[bip_pvRA] = bip[bip_dvRA];
                bip[bip_dvRC] = bip[bip_pvRB] - bip[bip_dvRB]; bip[bip_pvRB] = bip[bip_dvRB];
                bip[bip_dvRD] = bip[bip_pvRC] - bip[bip_dvRC]; bip[bip_pvRC] = bip[bip_dvRC];
                const double bip_dvER = bip[bip_pvRD] - bip[bip_dvRD]; bip[bip_pvRD] = bip[bip_dvRD];
                inputSampleR *= (1.0 + (std::fabs (bip[bip_dvRC]) * 0.0618)
                                      + (std::fabs (bip[bip_dvRD]) * -0.05982)
                                      + (std::fabs (bip_dvER) * 0.0206));
            }

            fpdL ^= fpdL << 13; fpdL ^= fpdL >> 17; fpdL ^= fpdL << 5;
            fpdR ^= fpdR << 13; fpdR ^= fpdR >> 17; fpdR ^= fpdR << 5;
            if (fpdL - fpdR < 1073741824u || fpdR - fpdL < 1073741824u)
            {
                fpdR ^= fpdR << 13; fpdR ^= fpdR >> 17; fpdR ^= fpdR << 5;
            }

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

    double bip[bip_total] {};
    double bypassMix = 0.0;

    float inPeakL = 0.0f, inPeakR = 0.0f;
    float outPeakL = 0.0f, outPeakR = 0.0f;
    bool inClip = false, outClip = false;
};

} // namespace forja::dsp::cera

namespace forja::dsp
{
using cera::CeraDSP;
} // namespace forja::dsp
