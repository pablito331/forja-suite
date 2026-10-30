#pragma once

// Forja adapter for Airwindows PointyGuitar (MIT, Chris Johnson).
// Reference: Forja/Source/forja/vendor/airwindows/PointyGuitar, commit
// b110fac3991fff7c4655cc2d25d6a3e221f9799c. The double loop is transcribed
// literally: a gated cascaded 9-band Bessel-ish structure whose pole count is
// driven by the Drive parameter, followed by speaker lowpass/highpass stacks.
//
// Deviations: Forja adds metering and a 10 ms bypass crossfade. The original's
// commented-out 64-bit dither stays commented; only the RNG advances. The
// original constructor leaves gateL/gateR uninitialised; the adapter starts
// them at 0.0 so every instance is deterministic.

#include "forja/dsp/ForjaDspCommon.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace forja::dsp::ponta
{

class PontaDSP
{
public:
    struct Parameters
    {
        float drive = 0.6f;
        float presence = 0.5f;
        float high = 0.8f;
        float mid = 0.6f;
        float low = 0.4f;
        float sub = 0.5f;
        float hspeaker = 0.7f;
        float lspeaker = 0.4f;
        float gate = 0.0f;
        float output = 0.7f;
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
        for (int x = 0; x < 18; ++x)
        {
            for (int y = 0; y < 12; ++y)
            {
                angSL[x][y] = 0.0;
                angAL[x][y] = 0.0;
                angSR[x][y] = 0.0;
                angAR[x][y] = 0.0;
            }
        }
        for (int y = 0; y < 12; ++y)
            angG[y] = 0.0;

        for (int count = 0; count < 37; ++count)
        {
            iirHPositionL[count] = 0.0;
            iirHAngleL[count] = 0.0;
            iirBPositionL[count] = 0.0;
            iirBAngleL[count] = 0.0;
            iirHPositionR[count] = 0.0;
            iirHAngleR[count] = 0.0;
            iirBPositionR[count] = 0.0;
            iirBAngleR[count] = 0.0;
        }

        WasNegativeL = false;
        ZeroCrossL = 0;
        gaterollerL = 0.0;
        gateL = 0.0;
        WasNegativeR = false;
        ZeroCrossR = 0;
        gaterollerR = 0.0;
        gateR = 0.0;

        fpdL = 22222u;
        fpdR = 22222u;
        bypassMix = params.bypass ? 1.0 : 0.0;
        clearMeters();
    }

    void setParameters (const Parameters& newParams) noexcept
    {
        params.drive = clamp01 (newParams.drive);
        params.presence = clamp01 (newParams.presence);
        params.high = clamp01 (newParams.high);
        params.mid = clamp01 (newParams.mid);
        params.low = clamp01 (newParams.low);
        params.sub = clamp01 (newParams.sub);
        params.hspeaker = clamp01 (newParams.hspeaker);
        params.lspeaker = clamp01 (newParams.lspeaker);
        params.gate = clamp01 (newParams.gate);
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

        const double drive = (double) params.drive + 0.618033988749894;
        angG[0] = std::sqrt ((double) params.presence * 2.0);
        angG[2] = std::sqrt ((double) params.high * 2.0);
        angG[4] = std::sqrt ((double) params.mid * 2.0);
        angG[6] = std::sqrt ((double) params.low * 2.0);
        angG[8] = std::sqrt ((double) params.sub * 2.0);
        angG[1] = (angG[0] + angG[2]) * 0.5;
        angG[3] = (angG[2] + angG[4]) * 0.5;
        angG[5] = (angG[4] + angG[6]) * 0.5;
        angG[7] = (angG[6] + angG[8]) * 0.5;
        angG[9] = angG[8];
        const int poles = (int) (drive * 10.0);
        const double hFreq = std::pow ((double) params.hspeaker, overallscale);
        const double lFreq = std::pow ((double) params.lspeaker, overallscale + 3.0);

        const double onthreshold = (std::pow ((double) params.gate, 3.0) / 3.0) + 0.00018;
        const double offthreshold = onthreshold * 1.1;
        const double release = 0.028331119964586;
        const double absmax = 220.9;

        const double output = params.output;

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

            // Gate L
            if (inputSampleL > 0.0)
            {
                if (WasNegativeL)
                    ZeroCrossL = (int) (absmax * 0.3);
                WasNegativeL = false;
            }
            else
            {
                ZeroCrossL += 1;
                WasNegativeL = true;
            }
            if (ZeroCrossL > absmax)
                ZeroCrossL = (int) absmax;

            if (gateL == 0.0)
            {
                if (std::fabs (inputSampleL) > onthreshold)
                {
                    if (gaterollerL == 0.0)
                        gaterollerL = ZeroCrossL;
                    else
                        gaterollerL -= release;
                }
                else
                {
                    gaterollerL -= release;
                }
            }
            else
            {
                if (std::fabs (inputSampleL) > offthreshold)
                {
                    if (gaterollerL < ZeroCrossL)
                        gaterollerL = ZeroCrossL;
                    else
                        gaterollerL -= release;
                }
                else
                {
                    gaterollerL -= release;
                }
            }
            if (gaterollerL < 0.0)
                gaterollerL = 0.0;

            for (int x = 0; x < poles; ++x)
            {
                double fr = 0.9 / overallscale;
                double band = inputSampleL;
                inputSampleL = 0.0;
                for (int y = 0; y < 9; ++y)
                {
                    angAL[x][y] = (angAL[x][y] * (1.0 - fr)) + ((band - angSL[x][y]) * fr);
                    const double temp = band;
                    band = ((angSL[x][y] + (angAL[x][y] * fr)) * (1.0 - fr)) + (band * fr);
                    angSL[x][y] = ((angSL[x][y] + (angAL[x][y] * fr)) * (1.0 - fr)) + (band * fr);
                    inputSampleL += ((temp - band) * angG[y]);
                    fr *= 0.618033988749894;
                }
                inputSampleL += (band * angG[9]);
                inputSampleL *= drive;
                inputSampleL -= std::fmin (std::fmax ((inputSampleL
                                                       * (std::fabs (inputSampleL) * 0.654)
                                                       * (std::fabs (inputSampleL) * 0.654)),
                                                      -1.0),
                                           1.0);
            }

            if (gaterollerL < 1.0)
            {
                gateL = gaterollerL;
                const double bridgerectifier = 1.0 - std::cos (std::fabs (inputSampleL));
                if (inputSampleL > 0.0)
                    inputSampleL = (inputSampleL * gateL) + (bridgerectifier * (1.0 - gateL));
                else
                    inputSampleL = (inputSampleL * gateL) - (bridgerectifier * (1.0 - gateL));
                if (gateL == 0.0)
                    inputSampleL = 0.0;
            }
            else
            {
                gateL = 1.0;
            }

            double lowSample = inputSampleL;
            for (int count = 0; count < (3.0 + (lFreq * 32.0)); ++count)
            {
                iirBAngleL[count] = (iirBAngleL[count] * (1.0 - lFreq))
                                    + ((lowSample - iirBPositionL[count]) * lFreq);
                lowSample = ((iirBPositionL[count] + (iirBAngleL[count] * lFreq))
                             * (1.0 - lFreq)) + (lowSample * lFreq);
                iirBPositionL[count] = ((iirBPositionL[count] + (iirBAngleL[count] * lFreq))
                                        * (1.0 - lFreq)) + (lowSample * lFreq);
                inputSampleL -= (lowSample * (1.0 / (3.0 + (lFreq * 32.0))));
            }

            for (int count = 0; count < (3.0 + (hFreq * 32.0)); ++count)
            {
                iirHAngleL[count] = (iirHAngleL[count] * (1.0 - hFreq))
                                    + ((inputSampleL - iirHPositionL[count]) * hFreq);
                inputSampleL = ((iirHPositionL[count] + (iirHAngleL[count] * hFreq))
                                * (1.0 - hFreq)) + (inputSampleL * hFreq);
                iirHPositionL[count] = ((iirHPositionL[count] + (iirHAngleL[count] * hFreq))
                                        * (1.0 - hFreq)) + (inputSampleL * hFreq);
            }

            inputSampleL *= output;

            // Gate R
            if (inputSampleR > 0.0)
            {
                if (WasNegativeR)
                    ZeroCrossR = (int) (absmax * 0.3);
                WasNegativeR = false;
            }
            else
            {
                ZeroCrossR += 1;
                WasNegativeR = true;
            }
            if (ZeroCrossR > absmax)
                ZeroCrossR = (int) absmax;

            if (gateR == 0.0)
            {
                if (std::fabs (inputSampleR) > onthreshold)
                {
                    if (gaterollerR == 0.0)
                        gaterollerR = ZeroCrossR;
                    else
                        gaterollerR -= release;
                }
                else
                {
                    gaterollerR -= release;
                }
            }
            else
            {
                if (std::fabs (inputSampleR) > offthreshold)
                {
                    if (gaterollerR < ZeroCrossR)
                        gaterollerR = ZeroCrossR;
                    else
                        gaterollerR -= release;
                }
                else
                {
                    gaterollerR -= release;
                }
            }
            if (gaterollerR < 0.0)
                gaterollerR = 0.0;

            for (int x = 0; x < poles; ++x)
            {
                double fr = 0.9 / overallscale;
                double band = inputSampleR;
                inputSampleR = 0.0;
                for (int y = 0; y < 9; ++y)
                {
                    angAR[x][y] = (angAR[x][y] * (1.0 - fr)) + ((band - angSR[x][y]) * fr);
                    const double temp = band;
                    band = ((angSR[x][y] + (angAR[x][y] * fr)) * (1.0 - fr)) + (band * fr);
                    angSR[x][y] = ((angSR[x][y] + (angAR[x][y] * fr)) * (1.0 - fr)) + (band * fr);
                    inputSampleR += ((temp - band) * angG[y]);
                    fr *= 0.618033988749894;
                }
                inputSampleR += (band * angG[9]);
                inputSampleR *= drive;
                inputSampleR -= std::fmin (std::fmax ((inputSampleR
                                                       * (std::fabs (inputSampleR) * 0.654)
                                                       * (std::fabs (inputSampleR) * 0.654)),
                                                      -1.0),
                                           1.0);
            }

            if (gaterollerR < 1.0)
            {
                gateR = gaterollerR;
                const double bridgerectifier = 1.0 - std::cos (std::fabs (inputSampleR));
                if (inputSampleR > 0.0)
                    inputSampleR = (inputSampleR * gateR) + (bridgerectifier * (1.0 - gateR));
                else
                    inputSampleR = (inputSampleR * gateR) - (bridgerectifier * (1.0 - gateR));
                if (gateR == 0.0)
                    inputSampleR = 0.0;
            }
            else
            {
                gateR = 1.0;
            }

            lowSample = inputSampleR;
            for (int count = 0; count < (3.0 + (lFreq * 32.0)); ++count)
            {
                iirBAngleR[count] = (iirBAngleR[count] * (1.0 - lFreq))
                                    + ((lowSample - iirBPositionR[count]) * lFreq);
                lowSample = ((iirBPositionR[count] + (iirBAngleR[count] * lFreq))
                             * (1.0 - lFreq)) + (lowSample * lFreq);
                iirBPositionR[count] = ((iirBPositionR[count] + (iirBAngleR[count] * lFreq))
                                        * (1.0 - lFreq)) + (lowSample * lFreq);
                inputSampleR -= (lowSample * (1.0 / (3.0 + (lFreq * 32.0))));
            }

            for (int count = 0; count < (3.0 + (hFreq * 32.0)); ++count)
            {
                iirHAngleR[count] = (iirHAngleR[count] * (1.0 - hFreq))
                                    + ((inputSampleR - iirHPositionR[count]) * hFreq);
                inputSampleR = ((iirHPositionR[count] + (iirHAngleR[count] * hFreq))
                                * (1.0 - hFreq)) + (inputSampleR * hFreq);
                iirHPositionR[count] = ((iirHPositionR[count] + (iirHAngleR[count] * hFreq))
                                        * (1.0 - hFreq)) + (inputSampleR * hFreq);
            }

            inputSampleR *= output;

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

    double angSL[18][12] {}, angAL[18][12] {};
    double angSR[18][12] {}, angAR[18][12] {};
    double angG[12] {};

    double iirHPositionL[37] {}, iirHAngleL[37] {};
    double iirBPositionL[37] {}, iirBAngleL[37] {};
    double iirHPositionR[37] {}, iirHAngleR[37] {};
    double iirBPositionR[37] {}, iirBAngleR[37] {};

    bool WasNegativeL = false;
    int ZeroCrossL = 0;
    double gaterollerL = 0.0;
    double gateL = 0.0;

    bool WasNegativeR = false;
    int ZeroCrossR = 0;
    double gaterollerR = 0.0;
    double gateR = 0.0;

    std::uint32_t fpdL = 22222u;
    std::uint32_t fpdR = 22222u;

    double bypassMix = 0.0;

    float inPeakL = 0.0f, inPeakR = 0.0f;
    float outPeakL = 0.0f, outPeakR = 0.0f;
    bool inClip = false, outClip = false;
};

} // namespace forja::dsp::ponta

namespace forja::dsp
{
using ponta::PontaDSP;
} // namespace forja::dsp
