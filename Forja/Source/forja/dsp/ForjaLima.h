#pragma once

// Forja adapter for Airwindows SmoothEQ2 (MIT, Chris Johnson).
// Reference: Forja/Source/forja/vendor/airwindows/SmoothEQ2, commit
// b110fac3991fff7c4655cc2d25d6a3e221f9799c. Four broad bands split by three
// cascaded crossover stages plus one exponential (one-pole) stage, all summed
// back with the four gain controls. Direct transcription of
// processDoubleReplacing; no saturation anywhere.

#include "forja/dsp/ForjaDspCommon.h"

namespace forja::dsp
{

class LimaDSP
{
public:
    struct Parameters
    {
        float high = 0.5f;
        float highMid = 0.5f;
        float lowMid = 0.5f;
        float bass = 0.5f;
        float highFreq = 0.5f;
        float highMidFreq = 0.5f;
        float lowMidFreq = 0.5f;
        float bassFreq = 0.5f;
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
        stageA = Stage {};
        stageB = Stage {};
        stageC = Stage {};
        highIIRL = highIIRR = 0.0;
        midIIRL = midIIRR = 0.0;
        lowIIRL = lowIIRR = 0.0;
        highCoef = midCoef = lowCoef = 1.0;
        fpdL = 22222u;
        fpdR = 22222u;
        bypassMix = params.bypass ? 1.0 : 0.0;
        clearMeters();
    }

    void setParameters (const Parameters& newParams) noexcept
    {
        params.high = clamp01 (newParams.high);
        params.highMid = clamp01 (newParams.highMid);
        params.lowMid = clamp01 (newParams.lowMid);
        params.bass = clamp01 (newParams.bass);
        params.highFreq = clamp01 (newParams.highFreq);
        params.highMidFreq = clamp01 (newParams.highMidFreq);
        params.lowMidFreq = clamp01 (newParams.lowMidFreq);
        params.bassFreq = clamp01 (newParams.bassFreq);
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
    struct Biquad
    {
        double freq = 0.0, reso = 0.0;
        double a0 = 0.0, a1 = 0.0, a2 = 0.0, b1 = 0.0, b2 = 0.0;
        double sL1 = 0.0, sL2 = 0.0, sR1 = 0.0, sR2 = 0.0;

        double process (double input, bool rightChannel) noexcept
        {
            double& s1 = rightChannel ? sR1 : sL1;
            double& s2 = rightChannel ? sR2 : sL2;
            const double out = (input * a0) + s1;
            s1 = (input * a1) - (out * b1) + s2;
            s2 = (input * a2) - (out * b2);
            return out;
        }

        void configure (double newFreq, double newReso) noexcept
        {
            freq = newFreq;
            reso = newReso;
            const double K = std::tan (kPi * freq);
            const double norm = 1.0 / (1.0 + K / reso + K * K);
            a0 = K * K * norm;
            a1 = 2.0 * a0;
            a2 = a0;
            b1 = 2.0 * (K * K - 1.0) * norm;
            b2 = (1.0 - K / reso + K * K) * norm;
        }
    };

    struct Stage
    {
        Biquad high;
        Biquad mid;
        Biquad low;
    };

    static double splitStage (Stage& stage, double input, bool rightChannel,
                              double highGain, double highMidGain, double lowMidGain,
                              double bassGain) noexcept
    {
        double treble = input;
        double highMid = stage.high.process (treble, rightChannel);
        treble -= highMid;
        double lowMid = stage.mid.process (highMid, rightChannel);
        highMid -= lowMid;
        double bass = stage.low.process (lowMid, rightChannel);
        lowMid -= bass;

        return (bass * bassGain) + (lowMid * lowMidGain) + (highMid * highMidGain) + (treble * highGain);
    }

    void configure() noexcept
    {
        const double trebleRef = (double) params.highFreq - 0.5;
        const double highmidRef = (double) params.highMidFreq - 0.5;
        const double lowmidRef = (double) params.lowMidFreq - 0.5;
        const double bassRef = (double) params.bassFreq - 0.5;

        const double highF = 0.75 + ((trebleRef + trebleRef + trebleRef + highmidRef) * 0.125);
        const double bassF = 0.25 + ((lowmidRef + bassRef + bassRef + bassRef) * 0.125);
        const double midF = (highF * 0.5) + (bassF * 0.5) + ((highmidRef + lowmidRef) * 0.125);

        const double highQ = std::fmax (std::fmin (1.0 + (highmidRef - trebleRef), 4.0), 0.125);
        const double midQ = std::fmax (std::fmin (1.0 + (lowmidRef - highmidRef), 4.0), 0.125);
        const double lowQ = std::fmax (std::fmin (1.0 + (bassRef - lowmidRef), 4.0), 0.125);

        const double highFreqNorm = std::fmax (std::fmin ((std::pow (highF, 3.0) * 20000.0) / sampleRate,
                                                          0.4999), 0.00025);
        const double midFreqNorm = std::fmax (std::fmin ((std::pow (midF, 3.0) * 20000.0) / sampleRate,
                                                         0.4999), 0.00025);
        const double lowFreqNorm = std::fmax (std::fmin ((std::pow (bassF, 3.0) * 20000.0) / sampleRate,
                                                         0.4999), 0.00025);

        stageA.high.configure (highFreqNorm, 2.24697960 * highQ);
        stageA.mid.configure (midFreqNorm, 2.24697960 * midQ);
        stageA.low.configure (lowFreqNorm, 2.24697960 * lowQ);
        stageB.high.configure (highFreqNorm, 0.80193774 * highQ);
        stageB.mid.configure (midFreqNorm, 0.80193774 * midQ);
        stageB.low.configure (lowFreqNorm, 0.80193774 * lowQ);
        stageC.high.configure (highFreqNorm, 0.55495813 * highQ);
        stageC.mid.configure (midFreqNorm, 0.55495813 * midQ);
        stageC.low.configure (lowFreqNorm, 0.55495813 * lowQ);

        const double highFreq = std::pow (highF, 3.0) * 20000.0;
        double omega = 2.0 * kPi * (highFreq / sampleRate);
        double K = 2.0 - std::cos (omega);
        highCoef = -std::sqrt ((K * K) - 1.0) + K;

        const double midFreq = std::pow (midF, 3.0) * 20000.0;
        omega = 2.0 * kPi * (midFreq / sampleRate);
        K = 2.0 - std::cos (omega);
        midCoef = -std::sqrt ((K * K) - 1.0) + K;

        const double lowFreq = std::pow (bassF, 3.0) * 20000.0;
        omega = 2.0 * kPi * (lowFreq / sampleRate);
        K = 2.0 - std::cos (omega);
        lowCoef = -std::sqrt ((K * K) - 1.0) + K;
    }

    static double bandGain (float value) noexcept
    {
        const double linear = ((double) value - 0.5) * 2.0;
        return 1.0 + (linear * std::fabs (linear) * std::fabs (linear));
    }

    template <typename Sample>
    void processSamples (Sample* left, Sample* right, int numSamples) noexcept
    {
        configure();

        const double highGain = bandGain (params.high);
        const double highMidGain = bandGain (params.highMid);
        const double lowMidGain = bandGain (params.lowMid);
        const double bassGain = bandGain (params.bass);

        const double targetBypass = params.bypass ? 1.0 : 0.0;
        const double smoothing = 1.0 - std::exp (-1.0 / (0.010 * sampleRate));
        const double highDry = 1.0 - highCoef;
        const double midDry = 1.0 - midCoef;
        const double lowDry = 1.0 - lowCoef;

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

            // stage A
            double trebleL = splitStage (stageA, inputSampleL, false, highGain, highMidGain, lowMidGain, bassGain);
            // stage B
            trebleL = splitStage (stageB, trebleL, false, highGain, highMidGain, lowMidGain, bassGain);
            // stage C
            trebleL = splitStage (stageC, trebleL, false, highGain, highMidGain, lowMidGain, bassGain);

            highIIRL = (highIIRL * highCoef) + (trebleL * highDry);
            double highMidL = highIIRL; trebleL -= highMidL;
            midIIRL = (midIIRL * midCoef) + (highMidL * midDry);
            double lowMidL = midIIRL; highMidL -= lowMidL;
            lowIIRL = (lowIIRL * lowCoef) + (lowMidL * lowDry);
            double bassL = lowIIRL; lowMidL -= bassL;
            inputSampleL = (bassL * bassGain) + (lowMidL * lowMidGain) + (highMidL * highMidGain) + (trebleL * highGain);

            double trebleR = splitStage (stageA, inputSampleR, true, highGain, highMidGain, lowMidGain, bassGain);
            trebleR = splitStage (stageB, trebleR, true, highGain, highMidGain, lowMidGain, bassGain);
            trebleR = splitStage (stageC, trebleR, true, highGain, highMidGain, lowMidGain, bassGain);

            highIIRR = (highIIRR * highCoef) + (trebleR * highDry);
            double highMidR = highIIRR; trebleR -= highMidR;
            midIIRR = (midIIRR * midCoef) + (highMidR * midDry);
            double lowMidR = midIIRR; highMidR -= lowMidR;
            lowIIRR = (lowIIRR * lowCoef) + (lowMidR * lowDry);
            double bassR = lowIIRR; lowMidR -= bassR;
            inputSampleR = (bassR * bassGain) + (lowMidR * lowMidGain) + (highMidR * highMidGain) + (trebleR * highGain);

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

    Stage stageA;
    Stage stageB;
    Stage stageC;

    double highIIRL = 0.0, highIIRR = 0.0;
    double midIIRL = 0.0, midIIRR = 0.0;
    double lowIIRL = 0.0, lowIIRR = 0.0;
    double highCoef = 1.0, midCoef = 1.0, lowCoef = 1.0;

    std::uint32_t fpdL = 22222u;
    std::uint32_t fpdR = 22222u;

    double bypassMix = 0.0;

    float inPeakL = 0.0f, inPeakR = 0.0f;
    float outPeakL = 0.0f, outPeakR = 0.0f;
    bool inClip = false, outClip = false;
};

} // namespace forja::dsp
