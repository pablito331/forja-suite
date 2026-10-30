#pragma once

// Forja adapter for Airwindows IronOxide5 (MIT, Chris Johnson).
// Reference: Forja/Source/forja/vendor/airwindows/IronOxide5, commit
// b110fac3991fff7c4655cc2d25d6a3e221f9799c. processDoubleReplacing is
// transcribed literally: random flutter ring, alternating IIR highpass,
// sin() pre-gain, the Iron Oxide fast/slow IIR with the sparse tap sum,
// the second sin() limiter, the slew-based noise and the Inv/Dry/Wet block.
//
// Deviations: Forja adds metering and a 10 ms bypass crossfade. The
// original's unused iirSamplehA/B and fastIIH*/slowIIH* members are omitted;
// they never touch the signal. The commented-out 64-bit dither stays
// commented; only the RNG advances. No heap allocation, logging or locks.

#include "forja/dsp/ForjaDspCommon.h"

#include <cmath>
#include <cstdint>

namespace forja::dsp
{

class OxidoDSP
{
public:
    struct Parameters
    {
        float input = 0.5f;
        float tapeHigh = 0.562341325190349f;
        float tapeLow = 0.562341325190349f;
        float flutter = 0.5f;
        float noise = 0.5f;
        float output = 0.5f;
        float invDryWet = 1.0f;
        bool bypass = false;
    };

    OxidoDSP() noexcept { reset(); }

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
        iirSampleAL = iirSampleBL = 0.0;
        iirSampleAR = iirSampleBR = 0.0;

        for (double& value : dL)
            value = 0.0;
        for (double& value : dR)
            value = 0.0;

        fastIIRAL = fastIIRBL = slowIIRAL = slowIIRBL = 0.0;
        fastIIRAR = fastIIRBR = slowIIRAR = slowIIRBR = 0.0;
        prevInputSampleL = 0.0;
        prevInputSampleR = 0.0;

        gcount = 0;
        flip = false;

        for (double& value : flL)
            value = 0.0;
        for (double& value : flR)
            value = 0.0;

        fstoredcount = 0;
        rateof = 0.5;
        sweep = 0.0;
        nextmax = 0.5;

        fpdL = 22222u;
        fpdR = 22222u;

        bypassMix = params.bypass ? 1.0 : 0.0;
        clearMeters();
    }

    void setParameters (const Parameters& newParams) noexcept
    {
        params.input = clamp01 (newParams.input);
        params.tapeHigh = clamp01 (newParams.tapeHigh);
        params.tapeLow = clamp01 (newParams.tapeLow);
        params.flutter = clamp01 (newParams.flutter);
        params.noise = clamp01 (newParams.noise);
        params.output = clamp01 (newParams.output);
        params.invDryWet = clamp01 (newParams.invDryWet);
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

    // Iron Oxide has no output delay line of its own.
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

        const double inputgain = std::pow (10.0, (((double) params.input * 36.0) - 18.0) / 20.0);
        const double outputgain = std::pow (10.0, (((double) params.output * 36.0) - 18.0) / 20.0);

        // The original forms the fourth power in float before promoting.
        const float tapeHigh2 = params.tapeHigh * params.tapeHigh;
        const float tapeHigh4 = tapeHigh2 * tapeHigh2;
        double ips = (((double) tapeHigh4 * 148.5) + 1.5) * 1.1;
        if (ips < 1.0 || ips > 200.0) ips = 33.0;
        const double tempRandy = 0.04 + (0.11 / std::sqrt (ips));

        const float tapeLow2 = params.tapeLow * params.tapeLow;
        const float tapeLow4 = tapeLow2 * tapeLow2;
        double lps = (((double) tapeLow4 * 148.5) + 1.5) * 1.1;
        if (lps < 1.0 || lps > 200.0) lps = 33.0;

        double iirAmount = lps / 430.0;
        double fastTaper = ips / 15.0;
        double slowTaper = 2.0 / (lps * lps);
        double lowspeedscale = (5.0 / ips);

        const double depth = std::pow ((double) params.flutter, 2.0) * overallscale;
        const double fluttertrim = 0.00581 / overallscale;
        const double sweeptrim = (0.0005 * depth) / overallscale;
        const double tupi = kPi * 2.0;
        const double newrate = 0.006 / overallscale;
        const double oldrate = 1.0 - newrate;

        if (overallscale == 0.0)
        {
            fastTaper += 1.0;
            slowTaper += 1.0;
        }
        else
        {
            iirAmount /= overallscale;
            lowspeedscale *= overallscale;
            fastTaper = 1.0 + (fastTaper / overallscale);
            slowTaper = 1.0 + (slowTaper / overallscale);
        }

        const double noise = (double) params.noise * 0.5;
        const double invdrywet = (((double) params.invDryWet * 2.0) - 1.0);
        double dry = 1.0;
        if (invdrywet > 0.0) dry -= invdrywet;

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
            double drySampleL = inputSampleL;
            double drySampleR = inputSampleR;

            const double flutterrandy = ((double) fpdL / (double) UINT32_MAX);
            if (fstoredcount < 0 || fstoredcount > 30) fstoredcount = 30;
            int flutcount = fstoredcount;
            flL[flutcount + 31] = flL[flutcount] = inputSampleL;
            flR[flutcount + 31] = flR[flutcount] = inputSampleR;
            double offset = (1.0 + std::sin (sweep)) * depth;
            const double flooroffset = offset - std::floor (offset);
            flutcount += (int) std::floor (offset);
            double bridgerectifierL = (flL[flutcount] * (1 - flooroffset));
            double bridgerectifierR = (flR[flutcount] * (1 - flooroffset));
            bridgerectifierL += (flL[flutcount + 1] * flooroffset);
            bridgerectifierR += (flR[flutcount + 1] * flooroffset);
            rateof = (nextmax * newrate) + (rateof * oldrate);
            sweep += rateof * fluttertrim;
            sweep += sweep * sweeptrim;
            if (sweep >= tupi)
            {
                sweep = 0.0;
                nextmax = 0.02 + (flutterrandy * 0.98);
            }
            fstoredcount--;
            inputSampleL = bridgerectifierL;
            inputSampleR = bridgerectifierR;

            // IIR highpass for leaning out
            if (flip)
            {
                iirSampleAL = (iirSampleAL * (1 - iirAmount)) + (inputSampleL * iirAmount);
                iirSampleAR = (iirSampleAR * (1 - iirAmount)) + (inputSampleR * iirAmount);
                inputSampleL -= iirSampleAL;
                inputSampleR -= iirSampleAR;
            }
            else
            {
                iirSampleBL = (iirSampleBL * (1 - iirAmount)) + (inputSampleL * iirAmount);
                iirSampleBR = (iirSampleBR * (1 - iirAmount)) + (inputSampleR * iirAmount);
                inputSampleL -= iirSampleBL;
                inputSampleR -= iirSampleBR;
            }

            inputSampleL *= inputgain;
            inputSampleR *= inputgain;

            bridgerectifierL = std::fabs (inputSampleL);
            if (bridgerectifierL > 1.57079633) bridgerectifierL = 1.57079633;
            bridgerectifierL = std::sin (bridgerectifierL);
            if (inputSampleL > 0.0)
                inputSampleL = bridgerectifierL;
            else
                inputSampleL = -bridgerectifierL;

            bridgerectifierR = std::fabs (inputSampleR);
            if (bridgerectifierR > 1.57079633) bridgerectifierR = 1.57079633;
            bridgerectifierR = std::sin (bridgerectifierR);
            if (inputSampleR > 0.0)
                inputSampleR = bridgerectifierR;
            else
                inputSampleR = -bridgerectifierR;

            // Iron Oxide shaping
            if (gcount < 0 || gcount > 131) gcount = 131;
            int count = gcount;

            dL[count + 131] = dL[count] = inputSampleL;
            dR[count + 131] = dR[count] = inputSampleR;

            double temp;
            if (flip)
            {
                fastIIRAL = fastIIRAL / fastTaper;
                slowIIRAL = slowIIRAL / slowTaper;
                fastIIRAL += dL[count];

                fastIIRAR = fastIIRAR / fastTaper;
                slowIIRAR = slowIIRAR / slowTaper;
                fastIIRAR += dR[count];
                count += 3;

                temp = dL[count + 127];
                temp += dL[count + 113];
                temp += dL[count + 109];
                temp += dL[count + 107];
                temp += dL[count + 103];
                temp += dL[count + 101];
                temp += dL[count + 97];
                temp += dL[count + 89];
                temp += dL[count + 83];
                temp /= 2;
                temp += dL[count + 79];
                temp += dL[count + 73];
                temp += dL[count + 71];
                temp += dL[count + 67];
                temp += dL[count + 61];
                temp += dL[count + 59];
                temp += dL[count + 53];
                temp += dL[count + 47];
                temp += dL[count + 43];
                temp += dL[count + 41];
                temp += dL[count + 37];
                temp += dL[count + 31];
                temp += dL[count + 29];
                temp /= 2;
                temp += dL[count + 23];
                temp += dL[count + 19];
                temp += dL[count + 17];
                temp += dL[count + 13];
                temp += dL[count + 11];
                temp /= 2;
                temp += dL[count + 7];
                temp += dL[count + 5];
                temp += dL[count + 3];
                temp /= 2;
                temp += dL[count + 2];
                temp += dL[count + 1];
                slowIIRAL += (temp / 128);

                temp = dR[count + 127];
                temp += dR[count + 113];
                temp += dR[count + 109];
                temp += dR[count + 107];
                temp += dR[count + 103];
                temp += dR[count + 101];
                temp += dR[count + 97];
                temp += dR[count + 89];
                temp += dR[count + 83];
                temp /= 2;
                temp += dR[count + 79];
                temp += dR[count + 73];
                temp += dR[count + 71];
                temp += dR[count + 67];
                temp += dR[count + 61];
                temp += dR[count + 59];
                temp += dR[count + 53];
                temp += dR[count + 47];
                temp += dR[count + 43];
                temp += dR[count + 41];
                temp += dR[count + 37];
                temp += dR[count + 31];
                temp += dR[count + 29];
                temp /= 2;
                temp += dR[count + 23];
                temp += dR[count + 19];
                temp += dR[count + 17];
                temp += dR[count + 13];
                temp += dR[count + 11];
                temp /= 2;
                temp += dR[count + 7];
                temp += dR[count + 5];
                temp += dR[count + 3];
                temp /= 2;
                temp += dR[count + 2];
                temp += dR[count + 1];
                slowIIRAR += (temp / 128);

                inputSampleL = fastIIRAL - (slowIIRAL / slowTaper);
                inputSampleR = fastIIRAR - (slowIIRAR / slowTaper);
            }
            else
            {
                fastIIRBL = fastIIRBL / fastTaper;
                slowIIRBL = slowIIRBL / slowTaper;
                fastIIRBL += dL[count];

                fastIIRBR = fastIIRBR / fastTaper;
                slowIIRBR = slowIIRBR / slowTaper;
                fastIIRBR += dR[count];
                count += 3;

                temp = dL[count + 127];
                temp += dL[count + 113];
                temp += dL[count + 109];
                temp += dL[count + 107];
                temp += dL[count + 103];
                temp += dL[count + 101];
                temp += dL[count + 97];
                temp += dL[count + 89];
                temp += dL[count + 83];
                temp /= 2;
                temp += dL[count + 79];
                temp += dL[count + 73];
                temp += dL[count + 71];
                temp += dL[count + 67];
                temp += dL[count + 61];
                temp += dL[count + 59];
                temp += dL[count + 53];
                temp += dL[count + 47];
                temp += dL[count + 43];
                temp += dL[count + 41];
                temp += dL[count + 37];
                temp += dL[count + 31];
                temp += dL[count + 29];
                temp /= 2;
                temp += dL[count + 23];
                temp += dL[count + 19];
                temp += dL[count + 17];
                temp += dL[count + 13];
                temp += dL[count + 11];
                temp /= 2;
                temp += dL[count + 7];
                temp += dL[count + 5];
                temp += dL[count + 3];
                temp /= 2;
                temp += dL[count + 2];
                temp += dL[count + 1];
                slowIIRBL += (temp / 128);

                temp = dR[count + 127];
                temp += dR[count + 113];
                temp += dR[count + 109];
                temp += dR[count + 107];
                temp += dR[count + 103];
                temp += dR[count + 101];
                temp += dR[count + 97];
                temp += dR[count + 89];
                temp += dR[count + 83];
                temp /= 2;
                temp += dR[count + 79];
                temp += dR[count + 73];
                temp += dR[count + 71];
                temp += dR[count + 67];
                temp += dR[count + 61];
                temp += dR[count + 59];
                temp += dR[count + 53];
                temp += dR[count + 47];
                temp += dR[count + 43];
                temp += dR[count + 41];
                temp += dR[count + 37];
                temp += dR[count + 31];
                temp += dR[count + 29];
                temp /= 2;
                temp += dR[count + 23];
                temp += dR[count + 19];
                temp += dR[count + 17];
                temp += dR[count + 13];
                temp += dR[count + 11];
                temp /= 2;
                temp += dR[count + 7];
                temp += dR[count + 5];
                temp += dR[count + 3];
                temp /= 2;
                temp += dR[count + 2];
                temp += dR[count + 1];
                slowIIRBR += (temp / 128);

                inputSampleL = fastIIRBL - (slowIIRBL / slowTaper);
                inputSampleR = fastIIRBR - (slowIIRBR / slowTaper);
            }

            inputSampleL /= fastTaper;
            inputSampleR /= fastTaper;
            inputSampleL /= lowspeedscale;
            inputSampleR /= lowspeedscale;

            bridgerectifierL = std::fabs (inputSampleL);
            if (bridgerectifierL > 1.57079633) bridgerectifierL = 1.57079633;
            bridgerectifierL = std::sin (bridgerectifierL);
            if (inputSampleL > 0.0)
                inputSampleL = bridgerectifierL;
            else
                inputSampleL = -bridgerectifierL;

            bridgerectifierR = std::fabs (inputSampleR);
            if (bridgerectifierR > 1.57079633) bridgerectifierR = 1.57079633;
            bridgerectifierR = std::sin (bridgerectifierR);
            if (inputSampleR > 0.0)
                inputSampleR = bridgerectifierR;
            else
                inputSampleR = -bridgerectifierR;

            const double randy = (0.55 + tempRandy + (((double) fpdR / (double) UINT32_MAX) * tempRandy)) * noise;

            inputSampleL *= (1.0 - randy);
            inputSampleL += (prevInputSampleL * randy);
            prevInputSampleL = drySampleL;

            inputSampleR *= (1.0 - randy);
            inputSampleR += (prevInputSampleR * randy);
            prevInputSampleR = drySampleR;

            flip = !flip;

            if (outputgain != 1.0)
            {
                inputSampleL *= outputgain;
                inputSampleR *= outputgain;
            }
            if (invdrywet != 1.0)
            {
                inputSampleL *= invdrywet;
                inputSampleR *= invdrywet;
            }
            if (dry != 1.0)
            {
                drySampleL *= dry;
                drySampleR *= dry;
            }
            if (std::fabs (drySampleL) > 0.0)
                inputSampleL += drySampleL;
            if (std::fabs (drySampleR) > 0.0)
                inputSampleR += drySampleR;

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

    double iirSampleAL = 0.0, iirSampleBL = 0.0;
    double iirSampleAR = 0.0, iirSampleBR = 0.0;
    double dL[264] {}, dR[264] {};
    double fastIIRAL = 0.0, fastIIRBL = 0.0, slowIIRAL = 0.0, slowIIRBL = 0.0;
    double fastIIRAR = 0.0, fastIIRBR = 0.0, slowIIRAR = 0.0, slowIIRBR = 0.0;
    double prevInputSampleL = 0.0, prevInputSampleR = 0.0;

    int gcount = 0;
    bool flip = false;

    double flL[100] {}, flR[100] {};
    int fstoredcount = 0;
    double rateof = 0.5;
    double sweep = 0.0;
    double nextmax = 0.5;

    double bypassMix = 0.0;

    float inPeakL = 0.0f, inPeakR = 0.0f;
    float outPeakL = 0.0f, outPeakR = 0.0f;
    bool inClip = false, outClip = false;
};

} // namespace forja::dsp
