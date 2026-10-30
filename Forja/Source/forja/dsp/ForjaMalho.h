#pragma once

// Forja adapter for Airwindows Pop2 (MIT, Chris Johnson).
// Reference: Forja/Source/forja/vendor/airwindows/Pop2, commit
// b110fac3991fff7c4655cc2d25d6a3e221f9799c. The dual envelopes, attack and
// release mapping, drive gain and ClipOnly2 are a direct transcription of
// processDoubleReplacing.
//
// Deviations: ClipOnly2 has a 17th slot so spacing == 16 is safe; bypass
// smoothing, metering and APVTS are Forja. Reported latency is 1 sample,
// matching the real output delay.

#include "forja/dsp/ForjaDspCommon.h"

namespace forja::dsp
{

class MalhoDSP
{
public:
    struct Parameters
    {
        float compress = 0.5f;
        float attack = 0.5f;
        float release = 0.5f;
        float drive = 0.5f;
        float mix = 1.0f;
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
        muVaryL = 0.0;
        muAttackL = 0.0;
        muNewSpeedL = 1000.0;
        muSpeedAL = 1000.0;
        muSpeedBL = 1000.0;
        muCoefficientAL = 1.0;
        muCoefficientBL = 1.0;

        muVaryR = 0.0;
        muAttackR = 0.0;
        muNewSpeedR = 1000.0;
        muSpeedAR = 1000.0;
        muSpeedBR = 1000.0;
        muCoefficientAR = 1.0;
        muCoefficientBR = 1.0;

        flip = false;

        clipL.reset();
        clipR.reset();

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
        params.compress = clamp01 (newParams.compress);
        params.attack = clamp01 (newParams.attack);
        params.release = clamp01 (newParams.release);
        params.drive = clamp01 (newParams.drive);
        params.mix = clamp01 (newParams.mix);
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

        const double threshold = 1.0 - ((1.0 - std::pow (1.0 - (double) params.compress, 2)) * 0.9);
        const double attack = ((std::pow ((double) params.attack, 4) * 100000.0) + 10.0) * overallscale;
        const double release = ((std::pow ((double) params.release, 5) * 2000000.0) + 20.0) * overallscale;
        const double maxRelease = release * 4.0;
        const double muPreGain = 1.0 / threshold;
        const double muMakeupGain = std::sqrt (1.0 / threshold) * (double) params.drive;
        const double wet = (double) params.mix;

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

            const double drySampleL = inputSampleL;
            const double drySampleR = inputSampleR;

            inputSampleL *= muPreGain;
            inputSampleR *= muPreGain;

            if (flip)
            {
                if (std::abs (inputSampleL) > threshold)
                {
                    muVaryL = threshold / std::abs (inputSampleL);
                    muAttackL = std::sqrt (std::abs (muSpeedAL));
                    muCoefficientAL = muCoefficientAL * (muAttackL - 1.0);
                    if (muVaryL < threshold)
                        muCoefficientAL += threshold;
                    else
                        muCoefficientAL += muVaryL;
                    muCoefficientAL /= muAttackL;
                    muNewSpeedL = muSpeedAL * (muSpeedAL - 1.0);
                    muNewSpeedL += release;
                    muSpeedAL = muNewSpeedL / muSpeedAL;
                    if (muSpeedAL > maxRelease) muSpeedAL = maxRelease;
                }
                else
                {
                    muCoefficientAL = muCoefficientAL * ((muSpeedAL * muSpeedAL) - 1.0);
                    muCoefficientAL += 1.0;
                    muCoefficientAL /= (muSpeedAL * muSpeedAL);
                    muNewSpeedL = muSpeedAL * (muSpeedAL - 1.0);
                    muNewSpeedL += attack;
                    muSpeedAL = muNewSpeedL / muSpeedAL;
                }
            }
            else
            {
                if (std::abs (inputSampleL) > threshold)
                {
                    muVaryL = threshold / std::abs (inputSampleL);
                    muAttackL = std::sqrt (std::abs (muSpeedBL));
                    muCoefficientBL = muCoefficientBL * (muAttackL - 1.0);
                    if (muVaryL < threshold)
                        muCoefficientBL += threshold;
                    else
                        muCoefficientBL += muVaryL;
                    muCoefficientBL /= muAttackL;
                    muNewSpeedL = muSpeedBL * (muSpeedBL - 1.0);
                    muNewSpeedL += release;
                    muSpeedBL = muNewSpeedL / muSpeedBL;
                    if (muSpeedBL > maxRelease) muSpeedBL = maxRelease;
                }
                else
                {
                    muCoefficientBL = muCoefficientBL * ((muSpeedBL * muSpeedBL) - 1.0);
                    muCoefficientBL += 1.0;
                    muCoefficientBL /= (muSpeedBL * muSpeedBL);
                    muNewSpeedL = muSpeedBL * (muSpeedBL - 1.0);
                    muNewSpeedL += attack;
                    muSpeedBL = muNewSpeedL / muSpeedBL;
                }
            }

            if (flip)
            {
                if (std::abs (inputSampleR) > threshold)
                {
                    muVaryR = threshold / std::abs (inputSampleR);
                    muAttackR = std::sqrt (std::abs (muSpeedAR));
                    muCoefficientAR = muCoefficientAR * (muAttackR - 1.0);
                    if (muVaryR < threshold)
                        muCoefficientAR += threshold;
                    else
                        muCoefficientAR += muVaryR;
                    muCoefficientAR /= muAttackR;
                    muNewSpeedR = muSpeedAR * (muSpeedAR - 1.0);
                    muNewSpeedR += release;
                    muSpeedAR = muNewSpeedR / muSpeedAR;
                    if (muSpeedAR > maxRelease) muSpeedAR = maxRelease;
                }
                else
                {
                    muCoefficientAR = muCoefficientAR * ((muSpeedAR * muSpeedAR) - 1.0);
                    muCoefficientAR += 1.0;
                    muCoefficientAR /= (muSpeedAR * muSpeedAR);
                    muNewSpeedR = muSpeedAR * (muSpeedAR - 1.0);
                    muNewSpeedR += attack;
                    muSpeedAR = muNewSpeedR / muSpeedAR;
                }
            }
            else
            {
                if (std::abs (inputSampleR) > threshold)
                {
                    muVaryR = threshold / std::abs (inputSampleR);
                    muAttackR = std::sqrt (std::abs (muSpeedBR));
                    muCoefficientBR = muCoefficientBR * (muAttackR - 1.0);
                    if (muVaryR < threshold)
                        muCoefficientBR += threshold;
                    else
                        muCoefficientBR += muVaryR;
                    muCoefficientBR /= muAttackR;
                    muNewSpeedR = muSpeedBR * (muSpeedBR - 1.0);
                    muNewSpeedR += release;
                    muSpeedBR = muNewSpeedR / muSpeedBR;
                    if (muSpeedBR > maxRelease) muSpeedBR = maxRelease;
                }
                else
                {
                    muCoefficientBR = muCoefficientBR * ((muSpeedBR * muSpeedBR) - 1.0);
                    muCoefficientBR += 1.0;
                    muCoefficientBR /= (muSpeedBR * muSpeedBR);
                    muNewSpeedR = muSpeedBR * (muSpeedBR - 1.0);
                    muNewSpeedR += attack;
                    muSpeedBR = muNewSpeedR / muSpeedBR;
                }
            }

            double gainL;
            double gainR;
            if (flip)
            {
                gainL = std::pow (muCoefficientAL, 2);
                gainR = std::pow (muCoefficientAR, 2);
            }
            else
            {
                gainL = std::pow (muCoefficientBL, 2);
                gainR = std::pow (muCoefficientBR, 2);
            }

            grLinearMin = std::min (grLinearMin, std::min (gainL, gainR));

            inputSampleL *= gainL;
            inputSampleR *= gainR;
            inputSampleL *= muMakeupGain;
            inputSampleR *= muMakeupGain;
            flip = !flip;

            inputSampleL = clipL.process (inputSampleL, spacing);
            inputSampleR = clipR.process (inputSampleR, spacing);

            if (wet < 1.0)
            {
                inputSampleL = (drySampleL * (1.0 - wet)) + (inputSampleL * wet);
                inputSampleR = (drySampleR * (1.0 - wet)) + (inputSampleR * wet);
            }

            fpdL ^= fpdL << 13; fpdL ^= fpdL >> 17; fpdL ^= fpdL << 5;
            fpdR ^= fpdR << 13; fpdR ^= fpdR >> 17; fpdR ^= fpdR << 5;

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
        frame.gainReductionDb = std::min (0.0f, gainToDb (std::min (1.0f, (float) grLinearMin)));
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

    double muVaryL = 0.0, muAttackL = 0.0, muNewSpeedL = 1000.0;
    double muSpeedAL = 1000.0, muSpeedBL = 1000.0;
    double muCoefficientAL = 1.0, muCoefficientBL = 1.0;

    double muVaryR = 0.0, muAttackR = 0.0, muNewSpeedR = 1000.0;
    double muSpeedAR = 1000.0, muSpeedBR = 1000.0;
    double muCoefficientAR = 1.0, muCoefficientBR = 1.0;

    bool flip = false;

    ClipOnly2Channel clipL;
    ClipOnly2Channel clipR;

    std::uint32_t fpdL = 22222u;
    std::uint32_t fpdR = 22222u;

    double bypassMix = 0.0;
    double dryDelayL = 0.0;
    double dryDelayR = 0.0;
    double grLinearMin = 1.0;

    float inPeakL = 0.0f, inPeakR = 0.0f;
    float outPeakL = 0.0f, outPeakR = 0.0f;
    bool inClip = false, outClip = false;
};

} // namespace forja::dsp
