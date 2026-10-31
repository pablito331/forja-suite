#pragma once

// Forja adapter for Airwindows ADClip9 (MIT, Chris Johnson).
// Reference: Forja/Source/forja/vendor/airwindows/ADClip9, commit
// b110fac3991fff7c4655cc2d25d6a3e221f9799c. The input/output mode switch,
// ClipOnly3 stage, slew-dependent post-clip and dither sequence are a direct
// transcription of processDoubleReplacing.
//
// Deviations: bypass smoothing, metering and APVTS are Forja. Reported latency
// is 1 sample, matching the real output delay of the ClipOnly3 ring.

#include "forja/dsp/ForjaDspCommon.h"

namespace forja::dsp
{

class MuralhaDSP
{
public:
    struct Parameters
    {
        float boost = 0.0f;
        float match = 0.0f;
        float noise = 0.7f;
        float ceiling = 0.75f;
        float mode = 0.0f;
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
        lastSampleL = 0.0;
        lastDryL = 0.0;
        wasPosClipL = false;
        wasNegClipL = false;
        lastSampleR = 0.0;
        lastDryR = 0.0;
        wasPosClipR = false;
        wasNegClipR = false;

        for (double& value : intermediateL) value = 0.0;
        for (double& value : intermediateR) value = 0.0;
        for (double& value : slewL) value = 0.0;
        for (double& value : slewR) value = 0.0;

        fpdL = 22222u;
        fpdR = 22222u;

        bypassMix = params.bypass ? 1.0 : 0.0;
        dryDelayL = 0.0;
        dryDelayR = 0.0;
        grLinearMin = 1.0;
        clearMeters();
    }

    void setParameters (const Parameters& newParams) noexcept
    {
        params.boost = clamp01 (newParams.boost);
        params.match = clamp01 (newParams.match);
        params.noise = clamp01 (newParams.noise);
        params.ceiling = clamp01 (newParams.ceiling);
        params.mode = clamp01 (newParams.mode);
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
        int spacing = (int) std::floor (overallscale);
        if (spacing < 1) spacing = 1;
        if (spacing > 16) spacing = 16;

        const double inputGain = std::pow (10.0, ((double) params.boost * 18.0) / 20.0);
        const double match = std::pow (10.0, ((double) params.match * 18.0) / 20.0);
        const double noiseLevel = 1.0 - std::pow (1.0 - (double) params.noise, 2.0);
        const double ceiling = 1.0 - std::pow (1.0 - (double) params.ceiling, 2.0);
        const int mode = (int) ((double) params.mode * 2.999) + 1;

        const double targetBypass = params.bypass ? 1.0 : 0.0;
        const double smoothing = 1.0 - std::exp (-1.0 / (0.010 * sampleRate));

        for (int i = 0; i < numSamples; ++i)
        {
            const double dryInL = (double) left[i];
            const double dryInR = (double) right[i];

            inPeakL = std::max (inPeakL, (float) std::abs (left[i]));
            inPeakR = std::max (inPeakR, (float) std::abs (right[i]));
            if (std::abs (left[i]) >= 1.0f || std::abs (right[i]) >= 1.0f)
                inClip = true;

            double inputSampleL = dryInL;
            double inputSampleR = dryInR;
            if (std::abs (inputSampleL) < 1.18e-23)
                inputSampleL = (double) fpdL * 1.18e-17;
            if (std::abs (inputSampleR) < 1.18e-23)
                inputSampleR = (double) fpdR * 1.18e-17;

            switch (mode)
            {
                case 1: inputSampleL *= inputGain; inputSampleR *= inputGain; break;
                case 2: inputSampleL *= match; inputSampleR *= match; break;
                case 3: inputSampleL *= inputGain; inputSampleR *= inputGain; break;
                default: break;
            }

            const double overshootL = lastDryL;
            lastDryL = inputSampleL;
            const double overshootR = lastDryR;
            lastDryR = inputSampleR;

            double noise = 1.0 - (((double) fpdL / UINT32_MAX) * (1.0 - noiseLevel));
            if (wasPosClipL)
            {
                if (inputSampleL < lastSampleL)
                    lastSampleL = (noiseLevel * noise) + (inputSampleL * (1.0 - noise));
                else
                    lastSampleL = ceiling;
            }
            wasPosClipL = false;
            if (inputSampleL > noiseLevel)
            {
                wasPosClipL = true;
                inputSampleL = (noiseLevel * noise) + (lastSampleL * (1.0 - noise));
            }
            if (wasNegClipL)
            {
                if (inputSampleL > lastSampleL)
                    lastSampleL = (-noiseLevel * noise) + (inputSampleL * (1.0 - noise));
                else
                    lastSampleL = -ceiling;
            }
            wasNegClipL = false;
            if (inputSampleL < -noiseLevel)
            {
                wasNegClipL = true;
                inputSampleL = (-noiseLevel * noise) + (lastSampleL * (1.0 - noise));
            }
            slewL[spacing * 2] = std::abs (lastSampleL - inputSampleL);
            for (int x = spacing * 2; x > 0; x--)
                slewL[x - 1] = slewL[x];
            intermediateL[spacing] = inputSampleL;
            inputSampleL = lastSampleL;
            for (int x = spacing; x > 0; x--)
                intermediateL[x - 1] = intermediateL[x];
            lastSampleL = intermediateL[0];
            if (wasPosClipL || wasNegClipL)
            {
                for (int x = spacing; x > 0; x--)
                    lastSampleL += intermediateL[x];
                lastSampleL /= spacing;
            }
            double finalSlew = 0.0;
            for (int x = spacing * 2; x >= 0; x--)
                if (finalSlew < slewL[x]) finalSlew = slewL[x];
            double postclip = ceiling / (1.0 + (finalSlew * 1.3986013));
            if (inputSampleL > postclip) inputSampleL = postclip;
            if (inputSampleL < -postclip) inputSampleL = -postclip;

            noise = 1.0 - (((double) fpdR / UINT32_MAX) * (1.0 - noiseLevel));
            if (wasPosClipR)
            {
                if (inputSampleR < lastSampleR)
                    lastSampleR = (noiseLevel * noise) + (inputSampleR * (1.0 - noise));
                else
                    lastSampleR = ceiling;
            }
            wasPosClipR = false;
            if (inputSampleR > noiseLevel)
            {
                wasPosClipR = true;
                inputSampleR = (noiseLevel * noise) + (lastSampleR * (1.0 - noise));
            }
            if (wasNegClipR)
            {
                if (inputSampleR > lastSampleR)
                    lastSampleR = (-noiseLevel * noise) + (inputSampleR * (1.0 - noise));
                else
                    lastSampleR = -ceiling;
            }
            wasNegClipR = false;
            if (inputSampleR < -noiseLevel)
            {
                wasNegClipR = true;
                inputSampleR = (-noiseLevel * noise) + (lastSampleR * (1.0 - noise));
            }
            slewR[spacing * 2] = std::abs (lastSampleR - inputSampleR);
            for (int x = spacing * 2; x > 0; x--)
                slewR[x - 1] = slewR[x];
            intermediateR[spacing] = inputSampleR;
            inputSampleR = lastSampleR;
            for (int x = spacing; x > 0; x--)
                intermediateR[x - 1] = intermediateR[x];
            lastSampleR = intermediateR[0];
            if (wasPosClipR || wasNegClipR)
            {
                for (int x = spacing; x > 0; x--)
                    lastSampleR += intermediateR[x];
                lastSampleR /= spacing;
            }
            finalSlew = 0.0;
            for (int x = spacing * 2; x >= 0; x--)
                if (finalSlew < slewR[x]) finalSlew = slewR[x];
            postclip = ceiling / (1.0 + (finalSlew * 1.3986013));
            if (inputSampleR > postclip) inputSampleR = postclip;
            if (inputSampleR < -postclip) inputSampleR = -postclip;

            switch (mode)
            {
                case 1: break;
                case 2: inputSampleL /= match; inputSampleR /= match; break;
                case 3:
                    inputSampleL = (inputSampleL - overshootL) / inputGain;
                    inputSampleR = (inputSampleR - overshootR) / inputGain;
                    break;
                default: break;
            }

            fpdL ^= fpdL << 13; fpdL ^= fpdL >> 17; fpdL ^= fpdL << 5;
            fpdR ^= fpdR << 13; fpdR ^= fpdR >> 17; fpdR ^= fpdR << 5;
            if (fpdL - fpdR < 1073741824 || fpdR - fpdL < 1073741824)
            {
                fpdR ^= fpdR << 13; fpdR ^= fpdR >> 17; fpdR ^= fpdR << 5;
            }

            bypassMix += (targetBypass - bypassMix) * smoothing;

            const double delayedDryL = dryDelayL;
            const double delayedDryR = dryDelayR;
            dryDelayL = dryInL;
            dryDelayR = dryInR;

            const double outL = inputSampleL * (1.0 - bypassMix) + delayedDryL * bypassMix;
            const double outR = inputSampleR * (1.0 - bypassMix) + delayedDryR * bypassMix;

            outPeakL = std::max (outPeakL, (float) std::abs (outL));
            outPeakR = std::max (outPeakR, (float) std::abs (outR));
            if (std::abs (outL) >= 1.0 || std::abs (outR) >= 1.0)
                outClip = true;

            const double inAbsL = std::abs (dryInL);
            const double inAbsR = std::abs (dryInR);
            if (inAbsL > 1.0e-9)
                grLinearMin = std::min (grLinearMin, (float) std::min (1.0, std::abs (outL) / inAbsL));
            if (inAbsR > 1.0e-9)
                grLinearMin = std::min (grLinearMin, (float) std::min (1.0, std::abs (outR) / inAbsR));

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
        frame.gainReductionDb = std::min (0.0f, gainToDb (std::min (1.0f, grLinearMin)));
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

    int latencySamples() const noexcept { return 1; }

    void setNoiseState (std::uint32_t newFpdL, std::uint32_t newFpdR) noexcept
    {
        fpdL = newFpdL;
        fpdR = newFpdR;
    }

private:
    void clearMeters() noexcept
    {
        inPeakL = inPeakR = outPeakL = outPeakR = 0.0f;
        inClip = false;
        outClip = false;
    }

    double sampleRate = 48000.0;
    Parameters params {};

    double lastSampleL = 0.0;
    double lastDryL = 0.0;
    double intermediateL[18] {};
    double slewL[34] {};
    bool wasPosClipL = false;
    bool wasNegClipL = false;

    double lastSampleR = 0.0;
    double lastDryR = 0.0;
    double intermediateR[18] {};
    double slewR[34] {};
    bool wasPosClipR = false;
    bool wasNegClipR = false;

    std::uint32_t fpdL = 22222u;
    std::uint32_t fpdR = 22222u;

    double bypassMix = 0.0;
    double dryDelayL = 0.0;
    double dryDelayR = 0.0;
    float grLinearMin = 1.0f;

    float inPeakL = 0.0f, inPeakR = 0.0f;
    float outPeakL = 0.0f, outPeakR = 0.0f;
    bool inClip = false, outClip = false;
};

} // namespace forja::dsp
