#pragma once

// Forja adapter for Airwindows DeBess (MIT, Chris Johnson).
// Reference: Forja/Source/forja/vendor/airwindows/DeBess, commit
// b110fac3991fff7c4655cc2d25d6a3e221f9799c. processDoubleReplacing is
// transcribed literally, including the original's else-branch check on
// ratioA (it tests ratioAL/ratioAR instead of ratioBL/ratioBR). That quirk is
// part of how DeBess sounds and is preserved on purpose.
//
// Deviations: Forja adds bypass smoothing and metering. The 64-bit dither of
// the original is commented out there too; only the RNG advances. No heap
// allocation, logging or locks in process.

#include "forja/dsp/ForjaDspCommon.h"

namespace forja::dsp
{

class EscoriaDSP
{
public:
    struct Parameters
    {
        float intense = 0.0f;
        float sharp = 0.5f;
        float depth = 0.5f;
        float filter = 0.5f;
        float sense = 0.0f;
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
        for (int x = 0; x < 41; ++x)
        {
            sL[x] = sR[x] = 0.0;
            mL[x] = mR[x] = 0.0;
        }

        ratioAL = ratioBL = 1.0;
        ratioAR = ratioBR = 1.0;
        iirSampleAL = iirSampleBL = 0.0;
        iirSampleAR = iirSampleBR = 0.0;
        flip = false;
        fpdL = 22222u;
        fpdR = 22222u;
        bypassMix = params.bypass ? 1.0 : 0.0;
        clearMeters();
    }

    void setParameters (const Parameters& newParams) noexcept
    {
        params.intense = clamp01 (newParams.intense);
        params.sharp = clamp01 (newParams.sharp);
        params.depth = clamp01 (newParams.depth);
        params.filter = clamp01 (newParams.filter);
        params.sense = clamp01 (newParams.sense);
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

public:
    MeterFrame readMeters (bool resetAfterRead = true) noexcept
    {
        MeterFrame frame;
        frame.inPeakL = inPeakL;
        frame.inPeakR = inPeakR;
        frame.outPeakL = outPeakL;
        frame.outPeakR = outPeakR;
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

    // Test hook: keeps the parity harness deterministic.
    void setNoiseState (std::uint32_t newFpdL, std::uint32_t newFpdR) noexcept
    {
        fpdL = newFpdL;
        fpdR = newFpdR;
    }

private:
    template <typename Sample>
    void processSamples (Sample* left, Sample* right, int numSamples) noexcept
    {
        const double overallscale = sampleRate / 44100.0;

        // pow(float, int) promotes to the double overload, so the parameter
        // must not be rounded in float first.
        const double intensity = std::pow ((double) params.intense, 5.0) * (8192.0 / overallscale);
        double sharpness = (double) params.sharp * 40.0;
        if (sharpness < 2.0)
            sharpness = 2.0;
        const double speed = 0.1 / sharpness;
        const double depth = 1.0 / ((double) params.depth + 0.0001);
        const double iirAmount = (double) params.filter;
        const float monitoring = params.sense;

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

            sL[0] = inputSampleL;
            sR[0] = inputSampleR;
            for (int x = (int) sharpness; x > 0; --x)
            {
                sL[x] = sL[x - 1];
                sR[x] = sR[x - 1];
            }

            mL[1] = (sL[1] - sL[2]) * ((sL[1] - sL[2]) / 1.3);
            mR[1] = (sR[1] - sR[2]) * ((sR[1] - sR[2]) / 1.3);
            for (int x = (int) sharpness - 1; x > 1; --x)
            {
                mL[x] = (sL[x] - sL[x + 1]) * ((sL[x - 1] - sL[x]) / 1.3);
                mR[x] = (sR[x] - sR[x + 1]) * ((sR[x - 1] - sR[x]) / 1.3);
            }

            double senseL = std::fabs (mL[1] - mL[2]) * sharpness * sharpness;
            double senseR = std::fabs (mR[1] - mR[2]) * sharpness * sharpness;
            for (int x = (int) sharpness - 1; x > 0; --x)
            {
                double multL = std::fabs (mL[x] - mL[x + 1]) * sharpness * sharpness;
                if (multL < 1.0)
                    senseL *= multL;
                double multR = std::fabs (mR[x] - mR[x + 1]) * sharpness * sharpness;
                if (multR < 1.0)
                    senseR *= multR;
            }

            senseL = 1.0 + (intensity * intensity * senseL);
            if (senseL > intensity)
                senseL = intensity;
            senseR = 1.0 + (intensity * intensity * senseR);
            if (senseR > intensity)
                senseR = intensity;

            if (flip)
            {
                iirSampleAL = (iirSampleAL * (1.0 - iirAmount)) + (inputSampleL * iirAmount);
                iirSampleAR = (iirSampleAR * (1.0 - iirAmount)) + (inputSampleR * iirAmount);
                ratioAL = (ratioAL * (1.0 - speed)) + (senseL * speed);
                ratioAR = (ratioAR * (1.0 - speed)) + (senseR * speed);
                if (ratioAL > depth) ratioAL = depth;
                if (ratioAR > depth) ratioAR = depth;
                if (ratioAL > 1.0)
                    inputSampleL = iirSampleAL + ((inputSampleL - iirSampleAL) / ratioAL);
                if (ratioAR > 1.0)
                    inputSampleR = iirSampleAR + ((inputSampleR - iirSampleAR) / ratioAR);
            }
            else
            {
                iirSampleBL = (iirSampleBL * (1.0 - iirAmount)) + (inputSampleL * iirAmount);
                iirSampleBR = (iirSampleBR * (1.0 - iirAmount)) + (inputSampleR * iirAmount);
                ratioBL = (ratioBL * (1.0 - speed)) + (senseL * speed);
                ratioBR = (ratioBR * (1.0 - speed)) + (senseR * speed);
                if (ratioBL > depth) ratioBL = depth;
                if (ratioBR > depth) ratioBR = depth;
                // Original quirk: the B branch tests the A ratios.
                if (ratioAL > 1.0)
                    inputSampleL = iirSampleBL + ((inputSampleL - iirSampleBL) / ratioBL);
                if (ratioAR > 1.0)
                    inputSampleR = iirSampleBR + ((inputSampleR - iirSampleBR) / ratioBR);
            }
            flip = ! flip;

            if (monitoring > 0.49999f)
            {
                inputSampleL = dryInL - inputSampleL;
                inputSampleR = dryInR - inputSampleR;
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

    double sL[41] {}, mL[41] {};
    double sR[41] {}, mR[41] {};
    double ratioAL = 1.0, ratioBL = 1.0;
    double ratioAR = 1.0, ratioBR = 1.0;
    double iirSampleAL = 0.0, iirSampleBL = 0.0;
    double iirSampleAR = 0.0, iirSampleBR = 0.0;
    bool flip = false;

    std::uint32_t fpdL = 22222u;
    std::uint32_t fpdR = 22222u;

    double bypassMix = 0.0;

    float inPeakL = 0.0f, inPeakR = 0.0f;
    float outPeakL = 0.0f, outPeakR = 0.0f;
    bool inClip = false, outClip = false;
};

} // namespace forja::dsp
