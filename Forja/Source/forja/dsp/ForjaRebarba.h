#pragma once

// Forja adapter for Airwindows Slew3 (MIT, Chris Johnson).
// Reference: Forja/Source/forja/vendor/airwindows/Slew3, commit
// b110fac3991fff7c4655cc2d25d6a3e221f9799c. The double loop is transcribed
// literally: a third-order prediction of the next slope (0.381966 and 0.618034
// are the golden-ratio split), clamped to a threshold of (1 - A)^4, and a
// golden-ratio blend between the raw and smoothed samples.
//
// Deviations: Forja adds metering and a 10 ms bypass crossfade. The original's
// commented-out 64-bit dither stays commented; only the RNG advances.

#include "forja/dsp/ForjaDspCommon.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace forja::dsp::rebarba
{

class RebarbaDSP
{
public:
    struct Parameters
    {
        float slew = 0.0f;
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
        lastSampleAL = lastSampleBL = lastSampleCL = 0.0;
        lastSampleAR = lastSampleBR = lastSampleCR = 0.0;
        fpdL = 22222u;
        fpdR = 22222u;
        bypassMix = params.bypass ? 1.0 : 0.0;
        clearMeters();
    }

    void setParameters (const Parameters& newParams) noexcept
    {
        params.slew = clamp01 (newParams.slew);
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

        const double threshold = std::pow ((1.0 - (double) params.slew), 4.0) / overallscale;

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

            double clamp = (lastSampleBL - lastSampleCL) * 0.381966011250105;
            clamp -= (lastSampleAL - lastSampleBL) * 0.6180339887498948482045;
            clamp += inputSampleL - lastSampleAL;

            lastSampleCL = lastSampleBL;
            lastSampleBL = lastSampleAL;
            lastSampleAL = inputSampleL;

            if (clamp > threshold)
                inputSampleL = lastSampleBL + threshold;
            if (-clamp > threshold)
                inputSampleL = lastSampleBL - threshold;

            lastSampleAL = (lastSampleAL * 0.381966011250105)
                         + (inputSampleL * 0.6180339887498948482045);

            clamp = (lastSampleBR - lastSampleCR) * 0.381966011250105;
            clamp -= (lastSampleAR - lastSampleBR) * 0.6180339887498948482045;
            clamp += inputSampleR - lastSampleAR;

            lastSampleCR = lastSampleBR;
            lastSampleBR = lastSampleAR;
            lastSampleAR = inputSampleR;

            if (clamp > threshold)
                inputSampleR = lastSampleBR + threshold;
            if (-clamp > threshold)
                inputSampleR = lastSampleBR - threshold;

            lastSampleAR = (lastSampleAR * 0.381966011250105)
                         + (inputSampleR * 0.6180339887498948482045);

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

    double lastSampleAL = 0.0, lastSampleBL = 0.0, lastSampleCL = 0.0;
    double lastSampleAR = 0.0, lastSampleBR = 0.0, lastSampleCR = 0.0;
    double bypassMix = 0.0;

    float inPeakL = 0.0f, inPeakR = 0.0f;
    float outPeakL = 0.0f, outPeakR = 0.0f;
    bool inClip = false, outClip = false;
};

} // namespace forja::dsp::rebarba

namespace forja::dsp
{
using rebarba::RebarbaDSP;
} // namespace forja::dsp
