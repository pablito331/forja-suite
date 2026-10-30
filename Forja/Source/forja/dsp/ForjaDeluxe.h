#pragma once

// Forja adapter for Airwindows ChimeyDeluxe (MIT, Chris Johnson).
// Reference: Forja/Source/forja/vendor/airwindows/ChimeyDeluxe, commit
// b110fac3991fff7c4655cc2d25d6a3e221f9799c. The double loop is transcribed
// literally: sixteen cascaded twelve-band structures, each followed by a
// per-band Bezier envelope follower that gates the signal through a sine
// curve. The original parameter names are kept in the DSP adapter; the UI
// explains them in Portuguese.
//
// Deviations: Forja adds metering and a 10 ms bypass crossfade. The original's
// commented-out 64-bit dither stays commented; only the RNG advances.

#include "forja/dsp/ForjaDspCommon.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace forja::dsp::deluxe
{

class DeluxeDSP
{
public:
    struct Parameters
    {
        float hell = 0.5f;
        float fizz = 0.5f;
        float pick = 0.5f;
        float satan = 0.5f;
        float danger = 0.5f;
        float critical = 0.5f;
        float hMeat = 0.5f;
        float lMeat = 0.5f;
        float swing = 0.5f;
        float rarely = 0.5f;
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
            for (int w = 0; w < bezTotal; ++w)
                bezComp[w][x] = 0.0;
            bezComp[bezCycle][x] = 1.0;

            for (int y = 0; y < 15; ++y)
            {
                angSL[x][y] = 0.0;
                angAL[x][y] = 0.0;
                angSR[x][y] = 0.0;
                angAR[x][y] = 0.0;
            }
        }
        for (int y = 0; y < 15; ++y)
            angG[y] = 0.0;

        fpdL = 22222u;
        fpdR = 22222u;
        bypassMix = params.bypass ? 1.0 : 0.0;
        clearMeters();
    }

    void setParameters (const Parameters& newParams) noexcept
    {
        params.hell = clamp01 (newParams.hell);
        params.fizz = clamp01 (newParams.fizz);
        params.pick = clamp01 (newParams.pick);
        params.satan = clamp01 (newParams.satan);
        params.danger = clamp01 (newParams.danger);
        params.critical = clamp01 (newParams.critical);
        params.hMeat = clamp01 (newParams.hMeat);
        params.lMeat = clamp01 (newParams.lMeat);
        params.swing = clamp01 (newParams.swing);
        params.rarely = clamp01 (newParams.rarely);
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
    enum
    {
        bezAL, bezBL, bezCL, bezInL, bezUnInL, bezSampL,
        bezAR, bezBR, bezCR, bezInR, bezUnInR, bezSampR,
        bezCycle,
        bezTotal
    };

    template <typename Sample>
    void processSamples (Sample* left, Sample* right, int numSamples) noexcept
    {
        const double overallscale = 1.0 / 44100.0 * sampleRate;

        double drive = 1.0;
        double pad = 1.0;
        angG[1] = ((double) params.hell + 1.5) * 0.5;
        if (pad > angG[1]) pad = angG[1];
        if (drive < angG[1]) drive = angG[1];
        angG[0] = (1.0 + angG[1]) * 0.5;
        if (angG[0] > angG[1]) angG[0] = angG[1];
        angG[2] = ((double) params.fizz + 1.5) * 0.5;
        if (pad > angG[2]) pad = angG[2];
        if (drive < angG[2]) drive = angG[2];
        angG[3] = ((double) params.pick + 1.5) * 0.5;
        if (pad > angG[3]) pad = angG[3];
        if (drive < angG[3]) drive = angG[3];
        angG[4] = ((double) params.satan + 1.5) * 0.5;
        if (pad > angG[4]) pad = angG[4];
        if (drive < angG[4]) drive = angG[4];
        angG[5] = ((double) params.danger + 1.5) * 0.5;
        if (pad > angG[5]) pad = angG[5];
        if (drive < angG[5]) drive = angG[5];
        angG[6] = ((double) params.critical + 1.5) * 0.5;
        if (pad > angG[6]) pad = angG[6];
        if (drive < angG[6]) drive = angG[6];
        angG[7] = ((double) params.hMeat + 1.5) * 0.5;
        if (pad > angG[7]) pad = angG[7];
        if (drive < angG[7]) drive = angG[7];
        angG[8] = ((double) params.lMeat + 1.5) * 0.5;
        if (pad > angG[8]) pad = angG[8];
        if (drive < angG[8]) drive = angG[8];
        angG[9] = ((double) params.swing + 1.5) * 0.5;
        if (pad > angG[9]) pad = angG[9];
        if (drive < angG[9]) drive = angG[9];
        angG[10] = ((double) params.rarely + 1.5) * 0.5;
        if (pad > angG[10]) pad = angG[10];
        if (drive < angG[10]) drive = angG[10];
        angG[12] = angG[11] = angG[10];
        if (drive > 1.0) drive = std::pow (drive, drive * 2.0);

        const double tune = 0.618 + (overallscale * 0.0055);
        double bezRez = (pad * drive * 0.0005) / overallscale;
        const int bezFreqFraction = (int) (1.0 / bezRez);
        double bezFreqTrim = (double) bezFreqFraction / (bezFreqFraction + 1.0);
        bezRez = 1.0 / bezFreqFraction;
        bezFreqTrim = 1.0 - (bezRez * bezFreqTrim);

        const double halfPi = 1.57079632679489661923;

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

            for (int x = 0; x < 16; ++x)
            {
                double fr = (0.92 / overallscale) + (overallscale * 0.01);
                double bandL = inputSampleL;
                inputSampleL = 0.0;
                double bandR = inputSampleR;
                inputSampleR = 0.0;

                for (int y = 0; y < 12; ++y)
                {
                    angAL[x][y] = (angAL[x][y] * (1.0 - fr)) + ((bandL - angSL[x][y]) * fr);
                    const double tempL = bandL;
                    bandL = ((angSL[x][y] + (angAL[x][y] * fr)) * (1.0 - fr)) + (bandL * fr);
                    angSL[x][y] = ((angSL[x][y] + (angAL[x][y] * fr)) * (1.0 - fr)) + (bandL * fr);
                    inputSampleL += ((tempL - bandL) * angG[y]);

                    angAR[x][y] = (angAR[x][y] * (1.0 - fr)) + ((bandR - angSR[x][y]) * fr);
                    const double tempR = bandR;
                    bandR = ((angSR[x][y] + (angAR[x][y] * fr)) * (1.0 - fr)) + (bandR * fr);
                    angSR[x][y] = ((angSR[x][y] + (angAR[x][y] * fr)) * (1.0 - fr)) + (bandR * fr);
                    inputSampleR += ((tempR - bandR) * angG[y]);

                    fr *= tune;
                }
                inputSampleL += bandL;
                inputSampleR += bandR;

                bezComp[bezCycle][x] += bezRez;
                bezComp[bezSampL][x] += (std::fabs (inputSampleL) * bezRez);
                bezComp[bezSampR][x] += (std::fabs (inputSampleR) * bezRez);
                if (bezComp[bezCycle][x] > 1.0)
                {
                    bezComp[bezCycle][x] = 0.0;
                    bezComp[bezCL][x] = bezComp[bezBL][x];
                    bezComp[bezBL][x] = bezComp[bezAL][x];
                    bezComp[bezAL][x] = bezComp[bezSampL][x];
                    bezComp[bezSampL][x] = 0.0;
                    bezComp[bezCR][x] = bezComp[bezBR][x];
                    bezComp[bezBR][x] = bezComp[bezAR][x];
                    bezComp[bezAR][x] = bezComp[bezSampR][x];
                    bezComp[bezSampR][x] = 0.0;
                }

                const double z = bezComp[bezCycle][x] * bezFreqTrim;
                const double CBL = (bezComp[bezCL][x] * (1.0 - z)) + (bezComp[bezBL][x] * z);
                const double BAL = (bezComp[bezBL][x] * (1.0 - z)) + (bezComp[bezAL][x] * z);
                double CBAL = (bezComp[bezBL][x] + (CBL * (1.0 - z)) + (BAL * z));

                const double CBR = (bezComp[bezCR][x] * (1.0 - z)) + (bezComp[bezBR][x] * z);
                const double BAR = (bezComp[bezBR][x] * (1.0 - z)) + (bezComp[bezAR][x] * z);
                double CBAR = (bezComp[bezBR][x] + (CBR * (1.0 - z)) + (BAR * z));

                CBAL = std::fmin (std::fmax (CBAL * drive * 0.23, 0.0), halfPi);
                CBAR = std::fmin (std::fmax (CBAR * drive * 0.23, 0.0), halfPi);
                inputSampleL *= 1.0 - std::sin (CBAL);
                inputSampleR *= 1.0 - std::sin (CBAR);
            }

            inputSampleL /= drive;
            inputSampleR /= drive;

            inputSampleL = std::fmin (std::fmax (inputSampleL * pad, -1.0), 1.0);
            inputSampleR = std::fmin (std::fmax (inputSampleR * pad, -1.0), 1.0);

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

    double angSL[18][15] {}, angAL[18][15] {};
    double angSR[18][15] {}, angAR[18][15] {};
    double angG[15] {};

    double bezComp[bezTotal][18] {};

    std::uint32_t fpdL = 22222u;
    std::uint32_t fpdR = 22222u;

    double bypassMix = 0.0;

    float inPeakL = 0.0f, inPeakR = 0.0f;
    float outPeakL = 0.0f, outPeakR = 0.0f;
    bool inClip = false, outClip = false;
};

} // namespace forja::dsp::deluxe

namespace forja::dsp
{
using deluxe::DeluxeDSP;
} // namespace forja::dsp
