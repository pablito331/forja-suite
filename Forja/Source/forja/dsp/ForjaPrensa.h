#pragma once

// Forja adapter for Airwindows Pressure5 (MIT, Chris Johnson).
// Reference: Forja/Source/forja/vendor/airwindows/Pressure5, commit
// b110fac3991fff7c4655cc2d25d6a3e221f9799c. The vari-mu gain computer,
// alternating A/B detector, fixed ultrasonic biquads and ClipOnly2 are a
// direct transcription of processDoubleReplacing.
//
// Deviations: muNewSpeed/muAttack start at zero; ClipOnly2 has a 17th slot so
// spacing == 16 is safe; bypass smoothing, metering and APVTS are Forja.
// Reported latency is 1 sample, matching the real output delay.

#include "forja/dsp/ForjaDspCommon.h"

namespace forja::dsp
{

class PrensaDSP
{
public:
    struct Parameters
    {
        float pressure = 0.0f;
        float speed = 0.25f;
        float mewiness = 1.0f;
        float pawclaw = 0.5f;
        float output = 0.5f;
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
        configureFilters();
    }

    void reset() noexcept
    {
        muVary = 1.0;
        muAttack = 0.0;
        muNewSpeed = 0.0;
        muSpeedA = 10000.0;
        muSpeedB = 10000.0;
        muCoefficientA = 1.0;
        muCoefficientB = 1.0;
        flip = false;

        for (int x = 0; x < fix_total; ++x)
        {
            fixA[x] = 0.0;
            fixB[x] = 0.0;
        }
        configureFilters();

        clipL.reset();
        clipR.reset();
        slewMax = 0.0;

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
        params.pressure = clamp01 (newParams.pressure);
        params.speed = clamp01 (newParams.speed);
        params.mewiness = clamp01 (newParams.mewiness);
        params.pawclaw = clamp01 (newParams.pawclaw);
        params.output = clamp01 (newParams.output);
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

        const double threshold = 1.0 - ((double) params.pressure * 0.95);
        const double muMakeupGain = 1.0 / threshold;

        double release = std::pow (1.28 - (double) params.speed, 5) * 32768.0;
        double fastest = std::sqrt (release);
        release /= overallscale;
        fastest /= overallscale;

        const double mewinessRef = (double) params.mewiness;
        const double pawsClaws = -((double) params.pawclaw - 0.5) * 1.618033988749894848204586;
        const double outputGain = std::pow ((double) params.output * 2.0, 2);
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

            inputSampleL *= muMakeupGain;
            inputSampleR *= muMakeupGain;

            if (fixA[fix_freq] < 0.4999)
            {
                double temp = (inputSampleL * fixA[fix_a0]) + fixA[fix_sL1];
                fixA[fix_sL1] = (inputSampleL * fixA[fix_a1]) - (temp * fixA[fix_b1]) + fixA[fix_sL2];
                fixA[fix_sL2] = (inputSampleL * fixA[fix_a2]) - (temp * fixA[fix_b2]);
                inputSampleL = temp;
                temp = (inputSampleR * fixA[fix_a0]) + fixA[fix_sR1];
                fixA[fix_sR1] = (inputSampleR * fixA[fix_a1]) - (temp * fixA[fix_b1]) + fixA[fix_sR2];
                fixA[fix_sR2] = (inputSampleR * fixA[fix_a2]) - (temp * fixA[fix_b2]);
                inputSampleR = temp;
            }

            double inputSense = std::abs (inputSampleL);
            if (std::abs (inputSampleR) > inputSense)
                inputSense = std::abs (inputSampleR);

            double mewiness = std::sin (mewinessRef + (slewMax * pawsClaws));
            bool positivemu = true;
            if (mewiness < 0.0)
            {
                positivemu = false;
                mewiness = -mewiness;
            }

            if (flip)
            {
                if (inputSense > threshold)
                {
                    muVary = threshold / inputSense;
                    muAttack = std::sqrt (std::abs (muSpeedA));
                    muCoefficientA = muCoefficientA * (muAttack - 1.0);
                    if (muVary < threshold)
                        muCoefficientA += threshold;
                    else
                        muCoefficientA += muVary;
                    muCoefficientA /= muAttack;
                }
                else
                {
                    muCoefficientA = muCoefficientA * ((muSpeedA * muSpeedA) - 1.0);
                    muCoefficientA += 1.0;
                    muCoefficientA /= (muSpeedA * muSpeedA);
                }
                muNewSpeed = muSpeedA * (muSpeedA - 1.0);
                muNewSpeed += std::abs (inputSense * release) + fastest;
                muSpeedA = muNewSpeed / muSpeedA;
            }
            else
            {
                if (inputSense > threshold)
                {
                    muVary = threshold / inputSense;
                    muAttack = std::sqrt (std::abs (muSpeedB));
                    muCoefficientB = muCoefficientB * (muAttack - 1.0);
                    if (muVary < threshold)
                        muCoefficientB += threshold;
                    else
                        muCoefficientB += muVary;
                    muCoefficientB /= muAttack;
                }
                else
                {
                    muCoefficientB = muCoefficientB * ((muSpeedB * muSpeedB) - 1.0);
                    muCoefficientB += 1.0;
                    muCoefficientB /= (muSpeedB * muSpeedB);
                }
                muNewSpeed = muSpeedB * (muSpeedB - 1.0);
                muNewSpeed += std::abs (inputSense * release) + fastest;
                muSpeedB = muNewSpeed / muSpeedB;
            }

            double coefficient;
            if (flip)
            {
                if (positivemu)
                    coefficient = std::pow (muCoefficientA, 2);
                else
                    coefficient = std::sqrt (muCoefficientA);
                coefficient = (coefficient * mewiness) + (muCoefficientA * (1.0 - mewiness));
            }
            else
            {
                if (positivemu)
                    coefficient = std::pow (muCoefficientB, 2);
                else
                    coefficient = std::sqrt (muCoefficientB);
                coefficient = (coefficient * mewiness) + (muCoefficientB * (1.0 - mewiness));
            }

            inputSampleL *= coefficient;
            inputSampleR *= coefficient;
            grLinearMin = std::min (grLinearMin, coefficient);

            if (outputGain != 1.0)
            {
                inputSampleL *= outputGain;
                inputSampleR *= outputGain;
            }

            flip = !flip;

            if (fixB[fix_freq] < 0.49999)
            {
                double temp = (inputSampleL * fixB[fix_a0]) + fixB[fix_sL1];
                fixB[fix_sL1] = (inputSampleL * fixB[fix_a1]) - (temp * fixB[fix_b1]) + fixB[fix_sL2];
                fixB[fix_sL2] = (inputSampleL * fixB[fix_a2]) - (temp * fixB[fix_b2]);
                inputSampleL = temp;
                temp = (inputSampleR * fixB[fix_a0]) + fixB[fix_sR1];
                fixB[fix_sR1] = (inputSampleR * fixB[fix_a1]) - (temp * fixB[fix_b1]) + fixB[fix_sR2];
                fixB[fix_sR2] = (inputSampleR * fixB[fix_a2]) - (temp * fixB[fix_b2]);
                inputSampleR = temp;
            }

            if (wet != 1.0)
            {
                inputSampleL = (inputSampleL * wet) + (drySampleL * (1.0 - wet));
                inputSampleR = (inputSampleR * wet) + (drySampleR * (1.0 - wet));
            }

            slewMax = std::abs (inputSampleL - clipL.lastSample);
            if (slewMax < std::abs (inputSampleR - clipR.lastSample))
                slewMax = std::abs (inputSampleR - clipR.lastSample);

            inputSampleL = clipL.process (inputSampleL, spacing);
            inputSampleR = clipR.process (inputSampleR, spacing);

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
    enum
    {
        fix_freq,
        fix_reso,
        fix_a0,
        fix_a1,
        fix_a2,
        fix_b1,
        fix_b2,
        fix_sL1,
        fix_sL2,
        fix_sR1,
        fix_sR2,
        fix_lastSampleL,
        fix_lastSampleR,
        fix_total
    };

    void configureFilters() noexcept
    {
        fixA[fix_freq] = 24000.0 / sampleRate;
        fixA[fix_reso] = 0.7071;
        const double K = std::tan (3.14159265358979323846 * fixA[fix_freq]);
        const double norm = 1.0 / (1.0 + K / fixA[fix_reso] + K * K);
        fixA[fix_a0] = K * K * norm;
        fixA[fix_a1] = 2.0 * fixA[fix_a0];
        fixA[fix_a2] = fixA[fix_a0];
        fixA[fix_b1] = 2.0 * (K * K - 1.0) * norm;
        fixA[fix_b2] = (1.0 - K / fixA[fix_reso] + K * K) * norm;

        for (int x = 0; x < fix_sL1; ++x)
            fixB[x] = fixA[x];
    }

    void clearMeters() noexcept
    {
        inPeakL = inPeakR = outPeakL = outPeakR = 0.0f;
        inClip = false;
        outClip = false;
    }

    double sampleRate = 48000.0;
    Parameters params {};

    double muVary = 1.0;
    double muAttack = 0.0;
    double muNewSpeed = 0.0;
    double muSpeedA = 10000.0;
    double muSpeedB = 10000.0;
    double muCoefficientA = 1.0;
    double muCoefficientB = 1.0;
    bool flip = false;

    double fixA[fix_total] {};
    double fixB[fix_total] {};

    ClipOnly2Channel clipL;
    ClipOnly2Channel clipR;

    double slewMax = 0.0;
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
