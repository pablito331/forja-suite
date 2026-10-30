#pragma once

// Forja adapter for Airwindows Baxandall3 (MIT, Chris Johnson).
// Reference: Forja/Source/forja/vendor/airwindows/Baxandall3, commit
// b110fac3991fff7c4655cc2d25d6a3e221f9799c. Interleaved Bessel shelving pair
// with Console5 encode/decode, transcribed from processDoubleReplacing.
//
// The algorithm is only neutral at Input = Treble = Bass = 0.5; that coupling
// is part of the original character and is not "fixed" here.

#include "forja/dsp/ForjaDspCommon.h"

namespace forja::dsp
{

class TornoDSP
{
public:
    struct Parameters
    {
        float input = 0.5f;
        float treble = 0.5f;
        float bass = 0.5f;
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
        baxHigh = BaxFilter {};
        baxLow = BaxFilter {};
        flip = false;
        fpdL = 22222u;
        fpdR = 22222u;
        bypassMix = params.bypass ? 1.0 : 0.0;
        clearMeters();
    }

    void setParameters (const Parameters& newParams) noexcept
    {
        params.input = clamp01 (newParams.input);
        params.treble = clamp01 (newParams.treble);
        params.bass = clamp01 (newParams.bass);
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
    struct BaxFilter
    {
        double freq = 0.0, reso = 0.0;
        double a0 = 0.0, a1 = 0.0, a2 = 0.0, b1 = 0.0, b2 = 0.0;
        double sLA1 = 0.0, sLA2 = 0.0, sRA1 = 0.0, sRA2 = 0.0;
        double sLB1 = 0.0, sLB2 = 0.0, sRB1 = 0.0, sRB2 = 0.0;
    };

    static void configureBax (BaxFilter& filter, double freq, double sr) noexcept
    {
        if (freq > 0.45)
            freq = 0.45;
        filter.freq = freq;
        filter.reso = 0.57735026919; // Bessel second order

        const double K = std::tan (kPi * filter.freq);
        const double norm = 1.0 / (1.0 + K / filter.reso + K * K);
        filter.a0 = K * K * norm;
        filter.a1 = 2.0 * filter.a0;
        filter.a2 = filter.a0;
        filter.b1 = 2.0 * (K * K - 1.0) * norm;
        filter.b2 = (1.0 - K / filter.reso + K * K) * norm;
    }

    template <typename Sample>
    void processSamples (Sample* left, Sample* right, int numSamples) noexcept
    {
        const double inputGain = std::pow ((double) params.input * 2.0, 2.0);
        const double trebleGain = std::pow ((double) params.treble * 2.0, 2.0);
        const double bassGain = std::pow ((double) params.bass * 2.0, 2.0);
        const double trebleFreq = ((2000.0 * trebleGain) + 200.0) / sampleRate;
        double bassFreq = std::pow ((1.0 - (double) params.bass) * 2.0, 2.0);
        bassFreq = ((2000.0 * bassFreq) + 200.0) / sampleRate;

        configureBax (baxHigh, trebleFreq, sampleRate);
        configureBax (baxLow, bassFreq, sampleRate);

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

            inputSampleL = std::sin (std::fmax (std::fmin (inputSampleL * inputGain, kPi / 2.0), -kPi / 2.0));
            inputSampleR = std::sin (std::fmax (std::fmin (inputSampleR * inputGain, kPi / 2.0), -kPi / 2.0));
            // encode Console5

            double trebleSampleL, trebleSampleR, bassSampleL, bassSampleR;
            if (flip)
            {
                trebleSampleL = (inputSampleL * baxHigh.a0) + baxHigh.sLA1;
                baxHigh.sLA1 = (inputSampleL * baxHigh.a1) - (trebleSampleL * baxHigh.b1) + baxHigh.sLA2;
                baxHigh.sLA2 = (inputSampleL * baxHigh.a2) - (trebleSampleL * baxHigh.b2);
                trebleSampleL = inputSampleL - trebleSampleL;
                trebleSampleR = (inputSampleR * baxHigh.a0) + baxHigh.sRA1;
                baxHigh.sRA1 = (inputSampleR * baxHigh.a1) - (trebleSampleR * baxHigh.b1) + baxHigh.sRA2;
                baxHigh.sRA2 = (inputSampleR * baxHigh.a2) - (trebleSampleR * baxHigh.b2);
                trebleSampleR = inputSampleR - trebleSampleR;
                bassSampleL = (inputSampleL * baxLow.a0) + baxLow.sLA1;
                baxLow.sLA1 = (inputSampleL * baxLow.a1) - (bassSampleL * baxLow.b1) + baxLow.sLA2;
                baxLow.sLA2 = (inputSampleL * baxLow.a2) - (bassSampleL * baxLow.b2);
                bassSampleR = (inputSampleR * baxLow.a0) + baxLow.sRA1;
                baxLow.sRA1 = (inputSampleR * baxLow.a1) - (bassSampleR * baxLow.b1) + baxLow.sRA2;
                baxLow.sRA2 = (inputSampleR * baxLow.a2) - (bassSampleR * baxLow.b2);
            }
            else
            {
                trebleSampleL = (inputSampleL * baxHigh.a0) + baxHigh.sLB1;
                baxHigh.sLB1 = (inputSampleL * baxHigh.a1) - (trebleSampleL * baxHigh.b1) + baxHigh.sLB2;
                baxHigh.sLB2 = (inputSampleL * baxHigh.a2) - (trebleSampleL * baxHigh.b2);
                trebleSampleL = inputSampleL - trebleSampleL;
                trebleSampleR = (inputSampleR * baxHigh.a0) + baxHigh.sRB1;
                baxHigh.sRB1 = (inputSampleR * baxHigh.a1) - (trebleSampleR * baxHigh.b1) + baxHigh.sRB2;
                baxHigh.sRB2 = (inputSampleR * baxHigh.a2) - (trebleSampleR * baxHigh.b2);
                trebleSampleR = inputSampleR - trebleSampleR;
                bassSampleL = (inputSampleL * baxLow.a0) + baxLow.sLB1;
                baxLow.sLB1 = (inputSampleL * baxLow.a1) - (bassSampleL * baxLow.b1) + baxLow.sLB2;
                baxLow.sLB2 = (inputSampleL * baxLow.a2) - (bassSampleL * baxLow.b2);
                bassSampleR = (inputSampleR * baxLow.a0) + baxLow.sRB1;
                baxLow.sRB1 = (inputSampleR * baxLow.a1) - (bassSampleR * baxLow.b1) + baxLow.sRB2;
                baxLow.sRB2 = (inputSampleR * baxLow.a2) - (bassSampleR * baxLow.b2);
            }
            flip = !flip;

            trebleSampleL *= trebleGain;
            trebleSampleR *= trebleGain;
            bassSampleL *= bassGain;
            bassSampleR *= bassGain;
            inputSampleL = bassSampleL + trebleSampleL;
            inputSampleR = bassSampleR + trebleSampleR;

            inputSampleL = std::asin (std::fmax (std::fmin (inputSampleL, 0.99999), -0.99999));
            inputSampleR = std::asin (std::fmax (std::fmin (inputSampleR, 0.99999), -0.99999));
            // amplitude aspect

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

public:
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

    // Test hook: keeps the parity harness deterministic.
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

    BaxFilter baxHigh;
    BaxFilter baxLow;
    bool flip = false;

    std::uint32_t fpdL = 22222u;
    std::uint32_t fpdR = 22222u;

    double bypassMix = 0.0;

    float inPeakL = 0.0f, inPeakR = 0.0f;
    float outPeakL = 0.0f, outPeakR = 0.0f;
    bool inClip = false, outClip = false;
};

} // namespace forja::dsp
