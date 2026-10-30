#pragma once

// Forja adapter scaffold for @@PRODUCT@@ (Airwindows @@AIRWINDOWS@@, MIT).
// Replace the pass-through marked TODO with the literal transcription of the
// original processDoubleReplacing loop. Keep processDouble as the parity
// authority, move any `static` state into members, give the RNG per-instance
// state, size buffers for 192 kHz and never allocate inside process().
//
// Reference: Forja/Source/forja/vendor/airwindows/@@AIRWINDOWS@@, commit
// b110fac3991fff7c4655cc2d25d6a3e221f9799c.

#include "forja/dsp/ForjaDspCommon.h"

#include <algorithm>
#include <cmath>

namespace forja::dsp::@@SLUG@@
{

class @@MODEL@@DSP
{
public:
    struct Parameters
    {
        float amount = 0.5f;
        bool bypass = false;
    };

    void prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
        reset();
    }

    void reset() noexcept
    {
        bypassMix = params.bypass ? 1.0 : 0.0;
        clearMeters();
    }

    void setParameters (const Parameters& newParams) noexcept
    {
        params.amount = std::clamp (newParams.amount, 0.0f, 1.0f);
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

private:
    template <typename Sample>
    void processSamples (Sample* left, Sample* right, int numSamples) noexcept
    {
        const double targetBypass = params.bypass ? 1.0 : 0.0;
        const double smoothing = 1.0 - std::exp (-1.0 / (0.010 * sampleRate));

        for (int i = 0; i < numSamples; ++i)
        {
            const double dryL = (double) left[i];
            const double dryR = (double) right[i];

            inPeakL = std::max (inPeakL, (float) std::abs (dryL));
            inPeakR = std::max (inPeakR, (float) std::abs (dryR));
            if (std::abs (dryL) >= 1.0 || std::abs (dryR) >= 1.0)
                inClip = true;

            // TODO: replace this unity pass-through with the Airwindows port.
            const double wetL = dryL;
            const double wetR = dryR;

            bypassMix += (targetBypass - bypassMix) * smoothing;

            const double outL = wetL * (1.0 - bypassMix) + dryL * bypassMix;
            const double outR = wetR * (1.0 - bypassMix) + dryR * bypassMix;

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
    double bypassMix = 0.0;

    float inPeakL = 0.0f, inPeakR = 0.0f;
    float outPeakL = 0.0f, outPeakR = 0.0f;
    bool inClip = false, outClip = false;
};

} // namespace forja::dsp::@@SLUG@@

namespace forja::dsp
{
using @@SLUG@@::@@MODEL@@DSP;
} // namespace forja::dsp
