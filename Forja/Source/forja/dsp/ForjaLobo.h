#pragma once

// Forja adapter for Airwindows Wolfbot (MIT, Chris Johnson).
// Reference: Forja/Source/forja/vendor/airwindows/Wolfbot, commit
// b110fac3991fff7c4655cc2d25d6a3e221f9799c. The double loop is transcribed
// literally: two chained Kalman-style predictors, one highpass and one lowpass,
// each feeding a slew-of-slew state machine into a sine output stage.
//
// Deviations: Forja adds metering and a 10 ms bypass crossfade. Wolfbot exposes
// no parameters in the original; the adapter keeps it that way and only adds
// the bypass. The original's commented-out 64-bit dither stays commented; only
// the RNG advances.

#include "forja/dsp/ForjaDspCommon.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace forja::dsp::lobo
{

class LoboDSP
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
        for (int x = 0; x < kalTotal; ++x)
        {
            kHP[x] = 0.0;
            kLP[x] = 0.0;
        }

        fpdL = 22222u;
        fpdR = 22222u;
        bypassMix = params.bypass ? 1.0 : 0.0;
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
        prevSampL1, prevSlewL1, accSlewL1,
        prevSampL2, prevSlewL2, accSlewL2,
        prevSampL3, prevSlewL3, accSlewL3,
        kalGainL, kalOutL,
        prevSampR1, prevSlewR1, accSlewR1,
        prevSampR2, prevSlewR2, accSlewR2,
        prevSampR3, prevSlewR3, accSlewR3,
        kalGainR, kalOutR,
        kalTotal
    };

    template <typename Sample>
    void processSamples (Sample* left, Sample* right, int numSamples) noexcept
    {
        const double overallscale = 1.0 / 44100.0 * sampleRate;
        const double kalHP = 1.0 - (0.004225 / overallscale);
        const double kalLP = 1.0 - (0.954529 / overallscale);

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
            const double drySampleL = inputSampleL;
            const double drySampleR = inputSampleR;

            // Kalman Filter L (highpass)
            double dryKal = inputSampleL = inputSampleL * (1.0 - kalHP) * 0.777;
            inputSampleL *= (1.0 - kalHP);
            kHP[prevSlewL3] += kHP[prevSampL3] - kHP[prevSampL2]; kHP[prevSlewL3] *= 0.5;
            kHP[prevSlewL2] += kHP[prevSampL2] - kHP[prevSampL1]; kHP[prevSlewL2] *= 0.5;
            kHP[prevSlewL1] += kHP[prevSampL1] - inputSampleL; kHP[prevSlewL1] *= 0.5;
            kHP[accSlewL2] += kHP[prevSlewL3] - kHP[prevSlewL2]; kHP[accSlewL2] *= 0.5;
            kHP[accSlewL1] += kHP[prevSlewL2] - kHP[prevSlewL1]; kHP[accSlewL1] *= 0.5;
            kHP[accSlewL3] += (kHP[accSlewL2] - kHP[accSlewL1]); kHP[accSlewL3] *= 0.5;
            kHP[kalOutL] += kHP[prevSampL1] + kHP[prevSlewL2] + kHP[accSlewL3]; kHP[kalOutL] *= 0.5;
            kHP[kalGainL] += std::fabs (dryKal - kHP[kalOutL]) * kalHP * 8.0; kHP[kalGainL] *= 0.5;
            if (kHP[kalGainL] > kalHP * 0.5) kHP[kalGainL] = kalHP * 0.5;
            kHP[kalOutL] += (dryKal * (1.0 - (0.68 + (kalHP * 0.157))));
            kHP[prevSampL3] = kHP[prevSampL2];
            kHP[prevSampL2] = kHP[prevSampL1];
            kHP[prevSampL1] = (kHP[kalGainL] * kHP[kalOutL]) + ((1.0 - kHP[kalGainL]) * dryKal);
            if (kHP[prevSampL1] > 1.0) kHP[prevSampL1] = 1.0;
            if (kHP[prevSampL1] < -1.0) kHP[prevSampL1] = -1.0;
            inputSampleL = drySampleL + (kHP[kalOutL] * -0.777);

            // Kalman Filter L (lowpass)
            dryKal = inputSampleL = inputSampleL * (1.0 - kalLP) * 0.777;
            inputSampleL *= (1.0 - kalLP);
            kLP[prevSlewL3] += kLP[prevSampL3] - kLP[prevSampL2]; kLP[prevSlewL3] *= 0.5;
            kLP[prevSlewL2] += kLP[prevSampL2] - kLP[prevSampL1]; kLP[prevSlewL2] *= 0.5;
            kLP[prevSlewL1] += kLP[prevSampL1] - inputSampleL; kLP[prevSlewL1] *= 0.5;
            kLP[accSlewL2] += kLP[prevSlewL3] - kLP[prevSlewL2]; kLP[accSlewL2] *= 0.5;
            kLP[accSlewL1] += kLP[prevSlewL2] - kLP[prevSlewL1]; kLP[accSlewL1] *= 0.5;
            kLP[accSlewL3] += (kLP[accSlewL2] - kLP[accSlewL1]); kLP[accSlewL3] *= 0.5;
            kLP[kalOutL] += kLP[prevSampL1] + kLP[prevSlewL2] + kLP[accSlewL3]; kLP[kalOutL] *= 0.5;
            kLP[kalGainL] += std::fabs (dryKal - kLP[kalOutL]) * kalLP * 8.0; kLP[kalGainL] *= 0.5;
            if (kLP[kalGainL] > kalLP * 0.5) kLP[kalGainL] = kalLP * 0.5;
            kLP[kalOutL] += (dryKal * (1.0 - (0.68 + (kalLP * 0.157))));
            kLP[prevSampL3] = kLP[prevSampL2];
            kLP[prevSampL2] = kLP[prevSampL1];
            kLP[prevSampL1] = (kLP[kalGainL] * kLP[kalOutL]) + ((1.0 - kLP[kalGainL]) * dryKal);
            if (kLP[prevSampL1] > 1.0) kLP[prevSampL1] = 1.0;
            if (kLP[prevSampL1] < -1.0) kLP[prevSampL1] = -1.0;
            inputSampleL = std::sin (kLP[kalOutL] * 0.7943) * 1.2589;

            // Kalman Filter R (highpass)
            dryKal = inputSampleR = inputSampleR * (1.0 - kalHP) * 0.777;
            inputSampleR *= (1.0 - kalHP);
            kHP[prevSlewR3] += kHP[prevSampR3] - kHP[prevSampR2]; kHP[prevSlewR3] *= 0.5;
            kHP[prevSlewR2] += kHP[prevSampR2] - kHP[prevSampR1]; kHP[prevSlewR2] *= 0.5;
            kHP[prevSlewR1] += kHP[prevSampR1] - inputSampleR; kHP[prevSlewR1] *= 0.5;
            kHP[accSlewR2] += kHP[prevSlewR3] - kHP[prevSlewR2]; kHP[accSlewR2] *= 0.5;
            kHP[accSlewR1] += kHP[prevSlewR2] - kHP[prevSlewR1]; kHP[accSlewR1] *= 0.5;
            kHP[accSlewR3] += (kHP[accSlewR2] - kHP[accSlewR1]); kHP[accSlewR3] *= 0.5;
            kHP[kalOutR] += kHP[prevSampR1] + kHP[prevSlewR2] + kHP[accSlewR3]; kHP[kalOutR] *= 0.5;
            kHP[kalGainR] += std::fabs (dryKal - kHP[kalOutR]) * kalHP * 8.0; kHP[kalGainR] *= 0.5;
            if (kHP[kalGainR] > kalHP * 0.5) kHP[kalGainR] = kalHP * 0.5;
            kHP[kalOutR] += (dryKal * (1.0 - (0.68 + (kalHP * 0.157))));
            kHP[prevSampR3] = kHP[prevSampR2];
            kHP[prevSampR2] = kHP[prevSampR1];
            kHP[prevSampR1] = (kHP[kalGainR] * kHP[kalOutR]) + ((1.0 - kHP[kalGainR]) * dryKal);
            if (kHP[prevSampR1] > 1.0) kHP[prevSampR1] = 1.0;
            if (kHP[prevSampR1] < -1.0) kHP[prevSampR1] = -1.0;
            inputSampleR = drySampleR + (kHP[kalOutR] * -0.777);

            // Kalman Filter R (lowpass)
            dryKal = inputSampleR = inputSampleR * (1.0 - kalLP) * 0.777;
            inputSampleR *= (1.0 - kalLP);
            kLP[prevSlewR3] += kLP[prevSampR3] - kLP[prevSampR2]; kLP[prevSlewR3] *= 0.5;
            kLP[prevSlewR2] += kLP[prevSampR2] - kLP[prevSampR1]; kLP[prevSlewR2] *= 0.5;
            kLP[prevSlewR1] += kLP[prevSampR1] - inputSampleR; kLP[prevSlewR1] *= 0.5;
            kLP[accSlewR2] += kLP[prevSlewR3] - kLP[prevSlewR2]; kLP[accSlewR2] *= 0.5;
            kLP[accSlewR1] += kLP[prevSlewR2] - kLP[prevSlewR1]; kLP[accSlewR1] *= 0.5;
            kLP[accSlewR3] += (kLP[accSlewR2] - kLP[accSlewR1]); kLP[accSlewR3] *= 0.5;
            kLP[kalOutR] += kLP[prevSampR1] + kLP[prevSlewR2] + kLP[accSlewR3]; kLP[kalOutR] *= 0.5;
            kLP[kalGainR] += std::fabs (dryKal - kLP[kalOutR]) * kalLP * 8.0; kLP[kalGainR] *= 0.5;
            if (kLP[kalGainR] > kalLP * 0.5) kLP[kalGainR] = kalLP * 0.5;
            kLP[kalOutR] += (dryKal * (1.0 - (0.68 + (kalLP * 0.157))));
            kLP[prevSampR3] = kLP[prevSampR2];
            kLP[prevSampR2] = kLP[prevSampR1];
            kLP[prevSampR1] = (kLP[kalGainR] * kLP[kalOutR]) + ((1.0 - kLP[kalGainR]) * dryKal);
            if (kLP[prevSampR1] > 1.0) kLP[prevSampR1] = 1.0;
            if (kLP[prevSampR1] < -1.0) kLP[prevSampR1] = -1.0;
            inputSampleR = std::sin (kLP[kalOutR] * 0.7943) * 1.2589;

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

    double kHP[kalTotal] {};
    double kLP[kalTotal] {};

    std::uint32_t fpdL = 22222u;
    std::uint32_t fpdR = 22222u;

    double bypassMix = 0.0;

    float inPeakL = 0.0f, inPeakR = 0.0f;
    float outPeakL = 0.0f, outPeakR = 0.0f;
    bool inClip = false, outClip = false;
};

} // namespace forja::dsp::lobo

namespace forja::dsp
{
using lobo::LoboDSP;
} // namespace forja::dsp
