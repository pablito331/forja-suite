#pragma once

// Forja adapter for Airwindows FinalClip (MIT, Chris Johnson).
// Reference: Forja/Source/forja/vendor/airwindows/FinalClip, commit
// b110fac3991fff7c4655cc2d25d6a3e221f9799c. The ±4 pre-clamp, slew limit and
// ClipOnly2 stage are a direct transcription of processDoubleReplacing.
//
// Deviations: the original intermediate[16] ring is written at index
// `spacing` (up to 16), so this adapter uses 18 slots. Bypass smoothing and
// metering are Forja. Reported latency is 1 sample, matching the output delay.

#include "forja/dsp/ForjaDspCommon.h"

namespace forja::dsp
{

class DiqueDSP
{
public:
    struct Parameters
    {
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
        intermediateL[0] = 0.0;
        wasPosClipL = false;
        wasNegClipL = false;
        lastSampleR = 0.0;
        intermediateR[0] = 0.0;
        wasPosClipR = false;
        wasNegClipR = false;

        for (double& value : intermediateL) value = 0.0;
        for (double& value : intermediateR) value = 0.0;

        bypassMix = params.bypass ? 1.0 : 0.0;
        dryDelayL = 0.0;
        dryDelayR = 0.0;
        grLinearMin = 1.0;
        clearMeters();
    }

    void setParameters (const Parameters& newParams) noexcept
    {
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

            if (inputSampleL > 4.0) inputSampleL = 4.0;
            if (inputSampleL < -4.0) inputSampleL = -4.0;
            if (inputSampleL - lastSampleL > 0.618033988749894)
                inputSampleL = lastSampleL + 0.618033988749894;
            if (inputSampleL - lastSampleL < -0.618033988749894)
                inputSampleL = lastSampleL - 0.618033988749894;

            if (wasPosClipL)
            {
                if (inputSampleL < lastSampleL)
                    lastSampleL = 1.0 + (inputSampleL * 0.381966011250105);
                else
                    lastSampleL = 0.618033988749894 + (lastSampleL * 0.618033988749894);
            }
            wasPosClipL = false;
            if (inputSampleL > 1.618033988749894)
            {
                wasPosClipL = true;
                inputSampleL = 1.0 + (lastSampleL * 0.381966011250105);
            }
            if (wasNegClipL)
            {
                if (inputSampleL > lastSampleL)
                    lastSampleL = -1.0 + (inputSampleL * 0.381966011250105);
                else
                    lastSampleL = -0.618033988749894 + (lastSampleL * 0.618033988749894);
            }
            wasNegClipL = false;
            if (inputSampleL < -1.618033988749894)
            {
                wasNegClipL = true;
                inputSampleL = -1.0 + (lastSampleL * 0.381966011250105);
            }
            intermediateL[spacing] = inputSampleL;
            inputSampleL = lastSampleL;
            for (int x = spacing; x > 0; x--)
                intermediateL[x - 1] = intermediateL[x];
            lastSampleL = intermediateL[0];

            if (inputSampleR > 4.0) inputSampleR = 4.0;
            if (inputSampleR < -4.0) inputSampleR = -4.0;
            if (inputSampleR - lastSampleR > 0.618033988749894)
                inputSampleR = lastSampleR + 0.618033988749894;
            if (inputSampleR - lastSampleR < -0.618033988749894)
                inputSampleR = lastSampleR - 0.618033988749894;

            if (wasPosClipR)
            {
                if (inputSampleR < lastSampleR)
                    lastSampleR = 1.0 + (inputSampleR * 0.381966011250105);
                else
                    lastSampleR = 0.618033988749894 + (lastSampleR * 0.618033988749894);
            }
            wasPosClipR = false;
            if (inputSampleR > 1.618033988749894)
            {
                wasPosClipR = true;
                inputSampleR = 1.0 + (lastSampleR * 0.381966011250105);
            }
            if (wasNegClipR)
            {
                if (inputSampleR > lastSampleR)
                    lastSampleR = -1.0 + (inputSampleR * 0.381966011250105);
                else
                    lastSampleR = -0.618033988749894 + (lastSampleR * 0.618033988749894);
            }
            wasNegClipR = false;
            if (inputSampleR < -1.618033988749894)
            {
                wasNegClipR = true;
                inputSampleR = -1.0 + (lastSampleR * 0.381966011250105);
            }
            intermediateR[spacing] = inputSampleR;
            inputSampleR = lastSampleR;
            for (int x = spacing; x > 0; x--)
                intermediateR[x - 1] = intermediateR[x];
            lastSampleR = intermediateR[0];

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
    double intermediateL[18] {};
    bool wasPosClipL = false;
    bool wasNegClipL = false;

    double lastSampleR = 0.0;
    double intermediateR[18] {};
    bool wasPosClipR = false;
    bool wasNegClipR = false;

    double bypassMix = 0.0;
    double dryDelayL = 0.0;
    double dryDelayR = 0.0;
    float grLinearMin = 1.0f;

    float inPeakL = 0.0f, inPeakR = 0.0f;
    float outPeakL = 0.0f, outPeakR = 0.0f;
    bool inClip = false, outClip = false;
};

} // namespace forja::dsp
