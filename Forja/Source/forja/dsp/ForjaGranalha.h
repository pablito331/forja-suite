#pragma once

// Forja adapter for Airwindows DeRez5 (MIT, Chris Johnson).
// Reference: Forja/Source/forja/vendor/airwindows/DeRez5, commit
// b110fac3991fff7c4655cc2d25d6a3e221f9799c. processDoubleReplacing is
// transcribed literally: the direction counter that limits the sample rate,
// the two Bezier undersamplers (bright and bassis), the waveform point
// reconstruction and the output trim.
//
// Deviations: Forja adds metering and a 10 ms bypass crossfade. The original's
// commented-out 64-bit dither stays commented; only the RNG advances (including
// the asymmetric extra step on the right channel). No allocation, logging or
// locks inside process().

#include "forja/dsp/ForjaDspCommon.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace forja::dsp::granalha
{

enum
{
    bez_HMAL, bez_HMBL, bez_HMCL, bez_HMDL,
    bez_LMAL, bez_LMBL, bez_LMCL, bez_LMDL,
    bez_HMAR, bez_HMBR, bez_HMCR, bez_HMDR, bez_HMcycle,
    bez_LMAR, bez_LMBR, bez_LMCR, bez_LMDR, bez_LMcycle,
    bez_total
};

class GranalhaDSP
{
public:
    struct Parameters
    {
        float derez = 1.0f;
        float trim = 1.0f;
        float square = 0.0f;
        float bright = 1.0f;
        float bassis = 0.0f;
        float output = 1.0f;
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
        for (int x = 0; x < bez_total; ++x)
            for (int y = 0; y < 2; ++y)
                bezEQ[x][y] = 0.0;

        pointCycleL = pointAL = pointBL = 0.0;
        sDerezL = 1.0;
        pointCycleR = pointAR = pointBR = 0.0;
        sDerezR = 1.0;
        lastSampleL = 0.0;
        directionCountL = 1.0;
        goingUpL = false;
        lastSampleR = 0.0;
        directionCountR = 1.0;
        goingUpR = false;

        fpdL = 22222u;
        fpdR = 22222u;

        bypassMix = params.bypass ? 1.0 : 0.0;
        clearMeters();
    }

    void setParameters (const Parameters& newParams) noexcept
    {
        params.derez = clamp01 (newParams.derez);
        params.trim = clamp01 (newParams.trim);
        params.square = clamp01 (newParams.square);
        params.bright = clamp01 (newParams.bright);
        params.bassis = clamp01 (newParams.bassis);
        params.output = clamp01 (newParams.output);
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
        const double sampleRateNow = sampleRate;
        const double overallscale = sampleRateNow / 44100.0;

        const double applyDerez = 1.0 - std::pow ((double) params.derez, 2.0 + overallscale);
        const double trimDerez = (1.0 - (double) params.trim) * 16.0 * overallscale;
        const double square = 1.0 - std::pow ((double) params.square, 3.0);
        double brightBez = (1.0 - std::pow ((double) params.bright, 2.0)) * 4.0 * overallscale;
        int stepped = 999999;
        if (brightBez > 1.0)
        {
            stepped = (int) brightBez;
            brightBez = 0.99999999 / stepped;
        }
        else
        {
            brightBez = 1.1;
        }
        const double brightTrim = 1.0 - (brightBez * ((double) stepped / (stepped + 1.0)));
        const double bassRef = std::pow ((double) params.bassis, 4.0);
        double bassisBez = (bassRef * 0.25) / overallscale;
        stepped = 999999;
        if (bassisBez > 0.000001)
            stepped = (int) (1.0 / bassisBez);
        if (bassisBez > 0.0)
            bassisBez = 0.99999999 / stepped;
        const double bassisTrim = 1.0 - (bassisBez * ((double) stepped / (stepped + 1.0)));
        const double output = params.output;

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

            if (inputSampleL > lastSampleL == goingUpL) directionCountL += (applyDerez + trimDerez);
            else directionCountL = trimDerez + 1.0;
            goingUpL = (inputSampleL > lastSampleL); lastSampleL = inputSampleL;

            if (inputSampleR > lastSampleR == goingUpR) directionCountR += (applyDerez + trimDerez);
            else directionCountR = trimDerez + 1.0;
            goingUpR = (inputSampleR > lastSampleR); lastSampleR = inputSampleR;

            if (brightBez < 1.0)
            {
                bezEQ[bez_HMcycle][0] += brightBez;
                bezEQ[bez_HMAL][0] += (inputSampleL * 0.5 * brightBez);
                bezEQ[bez_HMAR][0] += (inputSampleR * 0.5 * brightBez);
                if (bezEQ[bez_HMcycle][0] > brightTrim)
                {
                    bezEQ[bez_HMDL][0] = bezEQ[bez_HMCL][0]; bezEQ[bez_HMCL][0] = bezEQ[bez_HMBL][0];
                    bezEQ[bez_HMBL][0] = bezEQ[bez_HMAL][0] * (0.5 - (brightBez * 0.082));
                    bezEQ[bez_HMAL][0] = 0.0;
                    bezEQ[bez_HMDR][0] = bezEQ[bez_HMCR][0]; bezEQ[bez_HMCR][0] = bezEQ[bez_HMBR][0];
                    bezEQ[bez_HMBR][0] = bezEQ[bez_HMAR][0] * (0.5 - (brightBez * 0.082));
                    bezEQ[bez_HMAR][0] = 0.0;
                    bezEQ[bez_HMcycle][0] = 0.0;
                }
                double X = bezEQ[bez_HMcycle][0];
                inputSampleL = bezEQ[bez_HMCL][0] + (bezEQ[bez_HMDL][0] * (1.0 - X) * (1.0 - X));
                inputSampleL += (bezEQ[bez_HMCL][0] * 2.0 * (1.0 - X) * X) + (bezEQ[bez_HMBL][0] * X * X);
                inputSampleL /= brightTrim;
                inputSampleR = bezEQ[bez_HMCR][0] + (bezEQ[bez_HMDR][0] * (1.0 - X) * (1.0 - X));
                inputSampleR += (bezEQ[bez_HMCR][0] * 2.0 * (1.0 - X) * X) + (bezEQ[bez_HMBR][0] * X * X);
                inputSampleR /= brightTrim;
            }
            if (bassisBez > 0.0)
            {
                bezEQ[bez_LMcycle][0] += bassisBez;
                bezEQ[bez_LMAL][0] += (inputSampleL * bassisBez);
                bezEQ[bez_LMAR][0] += (inputSampleR * bassisBez);
                if (bezEQ[bez_LMcycle][0] > bassisTrim)
                {
                    bezEQ[bez_LMDL][0] = bezEQ[bez_LMCL][0]; bezEQ[bez_LMCL][0] = bezEQ[bez_LMBL][0];
                    bezEQ[bez_LMBL][0] = bezEQ[bez_LMAL][0] * (0.5 - (bassisBez * 0.082));
                    bezEQ[bez_LMAL][0] = 0.0;
                    bezEQ[bez_LMDR][0] = bezEQ[bez_LMCR][0]; bezEQ[bez_LMCR][0] = bezEQ[bez_LMBR][0];
                    bezEQ[bez_LMBR][0] = bezEQ[bez_LMAR][0] * (0.5 - (bassisBez * 0.082));
                    bezEQ[bez_LMAR][0] = 0.0;
                    bezEQ[bez_LMcycle][0] = 0.0;
                }
                double X = bezEQ[bez_LMcycle][0];
                inputSampleL -= (bezEQ[bez_LMCL][0] + (bezEQ[bez_LMDL][0] * (1.0 - X) * (1.0 - X))) * 0.5;
                inputSampleL -= ((bezEQ[bez_LMCL][0] * 2.0 * (1.0 - X) * X) + (bezEQ[bez_LMBL][0] * X * X)) * 0.5;
                inputSampleL /= bassisTrim;
                inputSampleR -= (bezEQ[bez_LMCR][0] + (bezEQ[bez_LMDR][0] * (1.0 - X) * (1.0 - X))) * 0.5;
                inputSampleR -= ((bezEQ[bez_LMCR][0] * 2.0 * (1.0 - X) * X) + (bezEQ[bez_LMBR][0] * X * X)) * 0.5;
                inputSampleR /= bassisTrim;
            }
            if (applyDerez + trimDerez > 0.0)
            {
                pointCycleL += sDerezL;
                if (pointCycleL > 0.9999999)
                {
                    pointAL = pointBL;
                    pointBL = inputSampleL;
                    pointCycleL = 0.0;

                    stepped = 999999;
                    if (applyDerez < 0.999999) stepped = (int) (1.0 / (1.0 - applyDerez));
                    if (stepped > directionCountL) stepped = (int) directionCountL;
                    if (stepped < 1) stepped = 1;
                    sDerezL = 0.99999999 / stepped;
                }
                double X = pointCycleL * square;
                if (sDerezL > 0.0)
                {
                    inputSampleL = pointAL + ((pointBL - pointAL) * X);
                }
                else
                {
                    inputSampleL = 0.0;
                }
                pointCycleR += sDerezR;
                if (pointCycleR > 0.9999999)
                {
                    pointAR = pointBR;
                    pointBR = inputSampleR;
                    pointCycleR = 0.0;

                    stepped = 999999;
                    if (applyDerez < 0.999999) stepped = (int) (1.0 / (1.0 - applyDerez));
                    if (stepped > directionCountR) stepped = (int) directionCountR;
                    if (stepped < 1) stepped = 1;
                    sDerezR = 0.99999999 / stepped;
                }
                X = pointCycleR * square;
                if (sDerezR > 0.0)
                {
                    inputSampleR = pointAR + ((pointBR - pointAR) * X);
                }
                else
                {
                    inputSampleR = 0.0;
                }
            }
            if (brightBez < 1.0)
            {
                bezEQ[bez_HMcycle][1] += brightBez;
                bezEQ[bez_HMAL][1] += (inputSampleL * brightBez);
                bezEQ[bez_HMAR][1] += (inputSampleR * brightBez);
                if (bezEQ[bez_HMcycle][1] > brightTrim)
                {
                    bezEQ[bez_HMDL][1] = bezEQ[bez_HMCL][1]; bezEQ[bez_HMCL][1] = bezEQ[bez_HMBL][1];
                    bezEQ[bez_HMBL][1] = bezEQ[bez_HMAL][1] * (0.5 - (brightBez * 0.082));
                    bezEQ[bez_HMAL][1] = 0.0;
                    bezEQ[bez_HMDR][1] = bezEQ[bez_HMCR][1]; bezEQ[bez_HMCR][1] = bezEQ[bez_HMBR][1];
                    bezEQ[bez_HMBR][1] = bezEQ[bez_HMAR][1] * (0.5 - (brightBez * 0.082));
                    bezEQ[bez_HMAR][1] = 0.0; bezEQ[bez_HMcycle][1] = 0.0;
                }
                double X = bezEQ[bez_HMcycle][1];
                inputSampleL = bezEQ[bez_HMCL][1] + (bezEQ[bez_HMDL][1] * (1.0 - X) * (1.0 - X));
                inputSampleL += (bezEQ[bez_HMCL][1] * 2.0 * (1.0 - X) * X) + (bezEQ[bez_HMBL][1] * X * X);
                inputSampleL /= brightTrim;
                inputSampleR = bezEQ[bez_HMCR][1] + (bezEQ[bez_HMDR][1] * (1.0 - X) * (1.0 - X));
                inputSampleR += (bezEQ[bez_HMCR][1] * 2.0 * (1.0 - X) * X) + (bezEQ[bez_HMBR][1] * X * X);
                inputSampleR /= brightTrim;
            }
            if (bassisBez > 0.0)
            {
                bezEQ[bez_LMcycle][1] += bassisBez;
                bezEQ[bez_LMAL][1] += (inputSampleL * bassisBez);
                bezEQ[bez_LMAR][1] += (inputSampleR * bassisBez);
                if (bezEQ[bez_LMcycle][1] > bassisTrim)
                {
                    bezEQ[bez_LMDL][1] = bezEQ[bez_LMCL][1]; bezEQ[bez_LMCL][1] = bezEQ[bez_LMBL][1];
                    bezEQ[bez_LMBL][1] = bezEQ[bez_LMAL][1] * (0.5 - (bassisBez * 0.082));
                    bezEQ[bez_LMAL][1] = 0.0;
                    bezEQ[bez_LMDR][1] = bezEQ[bez_LMCR][1]; bezEQ[bez_LMCR][1] = bezEQ[bez_LMBR][1];
                    bezEQ[bez_LMBR][1] = bezEQ[bez_LMAR][1] * (0.5 - (bassisBez * 0.082));
                    bezEQ[bez_LMAR][1] = 0.0; bezEQ[bez_LMcycle][1] = 0.0;
                }
                double X = bezEQ[bez_LMcycle][1];
                inputSampleL -= (bezEQ[bez_LMCL][1] + (bezEQ[bez_LMDL][1] * (1.0 - X) * (1.0 - X))) * 0.5;
                inputSampleL -= ((bezEQ[bez_LMCL][1] * 2.0 * (1.0 - X) * X) + (bezEQ[bez_LMBL][1] * X * X)) * 0.5;
                inputSampleL /= bassisTrim;
                inputSampleR -= (bezEQ[bez_LMCR][1] + (bezEQ[bez_LMDR][1] * (1.0 - X) * (1.0 - X))) * 0.5;
                inputSampleR -= ((bezEQ[bez_LMCR][1] * 2.0 * (1.0 - X) * X) + (bezEQ[bez_LMBR][1] * X * X)) * 0.5;
                inputSampleR /= bassisTrim;
            }
            if (output < 1.0)
            {
                inputSampleL *= output;
                inputSampleR *= output;
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

    double bezEQ[bez_total][2] {};
    double pointCycleL = 0.0, pointAL = 0.0, pointBL = 0.0, sDerezL = 1.0;
    double pointCycleR = 0.0, pointAR = 0.0, pointBR = 0.0, sDerezR = 1.0;
    double lastSampleL = 0.0, directionCountL = 1.0;
    double lastSampleR = 0.0, directionCountR = 1.0;
    bool goingUpL = false, goingUpR = false;

    double bypassMix = 0.0;

    float inPeakL = 0.0f, inPeakR = 0.0f;
    float outPeakL = 0.0f, outPeakR = 0.0f;
    bool inClip = false, outClip = false;
};

} // namespace forja::dsp::granalha

namespace forja::dsp
{
using granalha::GranalhaDSP;
} // namespace forja::dsp
