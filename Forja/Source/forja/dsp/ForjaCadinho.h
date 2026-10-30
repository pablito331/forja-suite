#pragma once

// Forja adapter for Airwindows X2Buss (MIT, Chris Johnson).
// Reference: Forja/Source/forja/vendor/airwindows/X2Buss, commit
// b110fac3991fff7c4655cc2d25d6a3e221f9799c. processDoubleReplacing is
// transcribed literally: three interleaved crossover stages, the exponential
// IIR stage, the four-band gain and the SmoothEQ2/Dynamics2 block, then the
// fader and ClipOnly3.
//
// Deviations: Forja adds metering (peak, clip, gain reduction from
// Dynamics2), bypass smoothing and larger ClipOnly3 buffers. The original
// stores 18/34 slots for spacing up to 17, which goes out of bounds above
// ~372 kHz; ClipOnly3Channel keeps 33/65 so the port stays defined, and
// parity is verified up to 352.8 kHz where the original is in bounds.
// The 64-bit dither of the original is commented out there too; only the RNG
// advances. No heap allocation, logging or locks in process.

#include "forja/dsp/ForjaDspCommon.h"

namespace forja::dsp
{

class CadinhoDSP
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
        float thresh = 1.0f;
        float fader = 0.5f;
        bool bypass = false;
    };

    CadinhoDSP() noexcept
    {
        // X2Buss's constructor starts the bezier control voltages at 1.0.
        bezCompF[bezCycle] = 1.0;
        bezCompS[bezCycle] = 1.0;
    }

    void prepare (double newSampleRate) noexcept
    {
        setSampleRate (newSampleRate);
        reset();
    }

    void setSampleRate (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
        spacing = (int) std::floor ((sampleRate / 44100.0) * 2.0);
        if (spacing < 2) spacing = 2;
        if (spacing > ClipOnly3Channel::maxSpacing) spacing = ClipOnly3Channel::maxSpacing;
    }

    void reset() noexcept
    {
        for (double* filter : { highA, highB, highC, midA, midB, midC, lowA, lowB, lowC })
            for (int x = 0; x < biqTotal; ++x)
                filter[x] = 0.0;

        highLIIR = highRIIR = 0.0;
        midLIIR = midRIIR = 0.0;
        lowLIIR = lowRIIR = 0.0;

        for (double& value : bezCompF)
            value = 0.0;
        for (double& value : bezCompS)
            value = 0.0;
        bezCompF[bezCycle] = 1.0;
        bezCompS[bezCycle] = 1.0;
        bezMaxF = 0.0;

        clipL.reset();
        clipR.reset();

        inTrimA = 0.5;
        inTrimB = 0.5;
        fpdL = 22222u;
        fpdR = 22222u;

        bypassMix = params.bypass ? 1.0 : 0.0;
        grLinear = 1.0;
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
        params.thresh = clamp01 (newParams.thresh);
        params.fader = clamp01 (newParams.fader);
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

public:
    MeterFrame readMeters (bool resetAfterRead = true) noexcept
    {
        MeterFrame frame;
        frame.inPeakL = inPeakL;
        frame.inPeakR = inPeakR;
        frame.outPeakL = outPeakL;
        frame.outPeakR = outPeakR;
        frame.gainReductionDb = grLinear < 1.0 ? (float) (-20.0 * std::log10 (grLinear)) : 0.0f;
        frame.inClip = inClip;
        frame.outClip = outClip;

        if (resetAfterRead)
        {
            grLinear = 1.0;
            clearMeters();
        }

        return frame;
    }

    void resetClip() noexcept
    {
        inClip = false;
        outClip = false;
    }

    // ClipOnly3's interpolation ring collapses to a single-sample hold in the
    // original, so the measured impulse latency is 1 at every sample rate.
    int latencySamples() const noexcept { return 1; }

    // Test hook: keeps the parity harness deterministic.
    void setNoiseState (std::uint32_t newFpdL, std::uint32_t newFpdR) noexcept
    {
        fpdL = newFpdL;
        fpdR = newFpdR;
    }

private:
    enum
    {
        biqFreq, biqReso, biqA0, biqA1, biqA2, biqB1, biqB2,
        biqSL1, biqSL2, biqSR1, biqSR2, biqTotal
    };

    enum
    {
        bezAL, bezBL, bezCL, bezInL, bezUnInL, bezSampL,
        bezAR, bezBR, bezCR, bezInR, bezUnInR, bezSampR,
        bezCycle, bezTotal
    };

    template <typename Sample>
    void processSamples (Sample* left, Sample* right, int numSamples) noexcept
    {
        const double overallscale = sampleRate / 44100.0;
        const double sampleRateNow = sampleRate;

        const double trebleGain = gainLaw (params.high);
        const double highmidGain = gainLaw (params.highMid);
        const double lowmidGain = gainLaw (params.lowMid);
        const double bassGain = gainLaw (params.bass);

        const double trebleRef = (double) params.highFreq - 0.5;
        const double highmidRef = (double) params.highMidFreq - 0.5;
        const double lowmidRef = (double) params.lowMidFreq - 0.5;
        const double bassRef = (double) params.bassFreq - 0.5;
        const double highF = 0.75 + ((trebleRef + trebleRef + trebleRef + highmidRef) * 0.125);
        const double bassF = 0.25 + ((lowmidRef + bassRef + bassRef + bassRef) * 0.125);
        const double midF = (highF * 0.5) + (bassF * 0.5) + ((highmidRef + lowmidRef) * 0.125);

        const double highQ = std::max (std::min (1.0 + (highmidRef - trebleRef), 4.0), 0.125);
        const double midQ = std::max (std::min (1.0 + (lowmidRef - highmidRef), 4.0), 0.125);
        const double lowQ = std::max (std::min (1.0 + (bassRef - lowmidRef), 4.0), 0.125);

        highA[biqFreq] = (std::pow (highF, 3.0) * 20000.0) / sampleRateNow;
        highC[biqFreq] = highB[biqFreq] = highA[biqFreq] = std::max (std::min (highA[biqFreq], 0.4999), 0.00025);
        const double highFreq = std::pow (highF, 3.0) * 20000.0;
        double omega = 2.0 * kPi * (highFreq / sampleRateNow);
        double biqK = 2.0 - std::cos (omega);
        const double highCoef = -std::sqrt ((biqK * biqK) - 1.0) + biqK;
        highA[biqReso] = 2.24697960 * highQ;
        highB[biqReso] = 0.80193774 * highQ;
        highC[biqReso] = 0.55495813 * highQ;

        midA[biqFreq] = (std::pow (midF, 3.0) * 20000.0) / sampleRateNow;
        midC[biqFreq] = midB[biqFreq] = midA[biqFreq] = std::max (std::min (midA[biqFreq], 0.4999), 0.00025);
        const double midFreq = std::pow (midF, 3.0) * 20000.0;
        omega = 2.0 * kPi * (midFreq / sampleRateNow);
        biqK = 2.0 - std::cos (omega);
        const double midCoef = -std::sqrt ((biqK * biqK) - 1.0) + biqK;
        midA[biqReso] = 2.24697960 * midQ;
        midB[biqReso] = 0.80193774 * midQ;
        midC[biqReso] = 0.55495813 * midQ;

        lowA[biqFreq] = (std::pow (bassF, 3.0) * 20000.0) / sampleRateNow;
        lowC[biqFreq] = lowB[biqFreq] = lowA[biqFreq] = std::max (std::min (lowA[biqFreq], 0.4999), 0.00025);
        const double lowFreq = std::pow (bassF, 3.0) * 20000.0;
        omega = 2.0 * kPi * (lowFreq / sampleRateNow);
        biqK = 2.0 - std::cos (omega);
        const double lowCoef = -std::sqrt ((biqK * biqK) - 1.0) + biqK;
        lowA[biqReso] = 2.24697960 * lowQ;
        lowB[biqReso] = 0.80193774 * lowQ;
        lowC[biqReso] = 0.55495813 * lowQ;

        auto design = [] (double* filter) noexcept
        {
            double k = std::tan (kPi * filter[biqFreq]);
            double norm = 1.0 / (1.0 + k / filter[biqReso] + k * k);
            filter[biqA0] = k * k * norm;
            filter[biqA1] = 2.0 * filter[biqA0];
            filter[biqA2] = filter[biqA0];
            filter[biqB1] = 2.0 * (k * k - 1.0) * norm;
            filter[biqB2] = (1.0 - k / filter[biqReso] + k * k) * norm;
        };

        design (highA); design (highB); design (highC);
        design (midA); design (midB); design (midC);
        design (lowA); design (lowB); design (lowC);

        const double bezCThresh = std::pow (1.0 - (double) params.thresh, 6.0) * 8.0;
        double bezRez = std::pow (1.0 - (double) params.thresh, 12.360679774997898) / overallscale;
        bezRez = std::max (std::min (bezRez, 1.0), 0.00001);
        int stepped = 999999;
        if (bezRez > 0.000001)
            stepped = (int) (1.0 / bezRez);
        bezRez = 0.99999999 / stepped;
        const double bezTrim = 1.0 - (bezRez * ((double) stepped / (stepped + 1.0)));

        double sloRez = std::pow (1.0 - (double) params.thresh, 10.0) / overallscale;
        sloRez = std::max (std::min (sloRez, 1.0), 0.00001);
        stepped = 999999;
        if (sloRez > 0.000001)
            stepped = (int) (1.0 / sloRez);
        sloRez = 0.99999999 / stepped;
        const double sloTrim = 1.0 - (sloRez * ((double) stepped / (stepped + 1.0)));

        inTrimA = inTrimB;
        inTrimB = (double) params.fader * 2.0;

        const double targetBypass = params.bypass ? 1.0 : 0.0;
        const double smoothing = 1.0 - std::exp (-1.0 / (0.010 * sampleRateNow));

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

            // Console: golden-ratio exponential pad on the way in.
            if (inputSampleL > 1.0) inputSampleL = 1.0;
            else if (inputSampleL > 0.0)
                inputSampleL = -std::expm1 (std::log1p (-inputSampleL) * 0.6180339887498949);
            if (inputSampleL < -1.0) inputSampleL = -1.0;
            else if (inputSampleL < 0.0)
                inputSampleL = std::expm1 (std::log1p (inputSampleL) * 0.6180339887498949);

            if (inputSampleR > 1.0) inputSampleR = 1.0;
            else if (inputSampleR > 0.0)
                inputSampleR = -std::expm1 (std::log1p (-inputSampleR) * 0.6180339887498949);
            if (inputSampleR < -1.0) inputSampleR = -1.0;
            else if (inputSampleR < 0.0)
                inputSampleR = std::expm1 (std::log1p (inputSampleR) * 0.6180339887498949);

            const double crossoverL = processCrossovers (inputSampleL, trebleGain, highmidGain, lowmidGain,
                                                         bassGain, highCoef, midCoef, lowCoef, false);
            const double crossoverR = processCrossovers (inputSampleR, trebleGain, highmidGain, lowmidGain,
                                                         bassGain, highCoef, midCoef, lowCoef, true);
            inputSampleL = crossoverL;
            inputSampleR = crossoverR;

            if (bezCThresh > 0.0)
            {
                inputSampleL *= ((bezCThresh * 0.5) + 1.0);
                inputSampleR *= ((bezCThresh * 0.5) + 1.0);
            }

            bezCompF[bezCycle] += bezRez;
            bezCompF[bezSampL] += (std::fabs (inputSampleL) * bezRez);
            bezCompF[bezSampR] += (std::fabs (inputSampleR) * bezRez);
            bezMaxF = std::max (bezMaxF, std::max (std::fabs (inputSampleL), std::fabs (inputSampleR)));
            if (bezCompF[bezCycle] > bezTrim)
            {
                bezCompF[bezCycle] = 0.0;
                bezCompF[bezCL] = bezCompF[bezBL];
                bezCompF[bezBL] = bezCompF[bezAL];
                bezCompF[bezAL] = bezCompF[bezSampL];
                bezCompF[bezSampL] = 0.0;
                bezCompF[bezCR] = bezCompF[bezBR];
                bezCompF[bezBR] = bezCompF[bezAR];
                bezCompF[bezAR] = bezCompF[bezSampR];
                bezCompF[bezSampR] = 0.0;
                bezMaxF = 0.0;
            }

            bezCompS[bezCycle] += sloRez;
            bezCompS[bezSampL] += (std::fabs (inputSampleL) * sloRez);
            bezCompS[bezSampR] += (std::fabs (inputSampleR) * sloRez);
            if (bezCompS[bezCycle] > sloTrim)
            {
                bezCompS[bezCycle] = 0.0;
                bezCompS[bezCL] = bezCompS[bezBL];
                bezCompS[bezBL] = bezCompS[bezAL];
                bezCompS[bezAL] = bezCompS[bezSampL];
                bezCompS[bezSampL] = 0.0;
                bezCompS[bezCR] = bezCompS[bezBR];
                bezCompS[bezBR] = bezCompS[bezAR];
                bezCompS[bezAR] = bezCompS[bezSampR];
                bezCompS[bezSampR] = 0.0;
            }

            double X = bezCompF[bezCycle];
            const double CBAFL = (bezCompF[bezBL] + (bezCompF[bezCL] * (1.0 - X) * (1.0 - X))
                                  + (bezCompF[bezBL] * 2.0 * (1.0 - X) * X) + (bezCompF[bezAL] * X * X)) * 0.5;
            const double CBAFR = (bezCompF[bezBR] + (bezCompF[bezCR] * (1.0 - X) * (1.0 - X))
                                  + (bezCompF[bezBR] * 2.0 * (1.0 - X) * X) + (bezCompF[bezAR] * X * X)) * 0.5;
            X = bezCompS[bezCycle];
            const double CBASL = (bezCompS[bezBL] + (bezCompS[bezCL] * (1.0 - X) * (1.0 - X))
                                  + (bezCompS[bezBL] * 2.0 * (1.0 - X) * X) + (bezCompS[bezAL] * X * X)) * 0.5;
            const double CBASR = (bezCompS[bezBR] + (bezCompS[bezCR] * (1.0 - X) * (1.0 - X))
                                  + (bezCompS[bezBR] * 2.0 * (1.0 - X) * X) + (bezCompS[bezAR] * X * X)) * 0.5;

            double CBAMax = std::max (CBASL, CBAFL);
            if (CBAMax > 0.0)
                CBAMax = 1.0 / CBAMax;
            double CBAFade = ((CBASL * -CBAMax) + (CBAFL * CBAMax) + 1.0) * 0.5;
            if (bezCThresh > 0.0)
            {
                const double gainL = 1.0 - (std::min (((CBASL * (1.0 - CBAFade)) + (CBAFL * CBAFade)) * bezCThresh, 1.0));
                inputSampleL *= gainL;
                grLinear = std::min (grLinear, gainL);
            }

            CBAMax = std::max (CBASR, CBAFR);
            if (CBAMax > 0.0)
                CBAMax = 1.0 / CBAMax;
            CBAFade = ((CBASR * -CBAMax) + (CBAFR * CBAMax) + 1.0) * 0.5;
            if (bezCThresh > 0.0)
            {
                const double gainR = 1.0 - (std::min (((CBASR * (1.0 - CBAFade)) + (CBAFR * CBAFade)) * bezCThresh, 1.0));
                inputSampleR *= gainR;
                grLinear = std::min (grLinear, gainR);
            }

            const double temp = (double) (numSamples - 1 - i) / (double) numSamples;
            const double gain = (inTrimA * temp) + (inTrimB * (1.0 - temp));
            inputSampleL *= gain;
            inputSampleR *= gain;

            const double noiseL = 1.0 - ((double) fpdL / (double) UINT32_MAX) * 0.076;
            inputSampleL = clipL.process (inputSampleL, spacing, noiseL);

            const double noiseR = 1.0 - ((double) fpdR / (double) UINT32_MAX) * 0.076;
            inputSampleR = clipR.process (inputSampleR, spacing, noiseR);

            fpdL ^= fpdL << 13; fpdL ^= fpdL >> 17; fpdL ^= fpdL << 5;
            fpdR ^= fpdR << 13; fpdR ^= fpdR >> 17; fpdR ^= fpdR << 5;
            if (fpdL - fpdR < 1073741824u || fpdR - fpdL < 1073741824u)
            {
                fpdR ^= fpdR << 13; fpdR ^= fpdR >> 17; fpdR ^= fpdR << 5;
            }

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

    static double gainLaw (float value) noexcept
    {
        double gain = ((double) value - 0.5) * 2.0;
        return 1.0 + (gain * std::fabs (gain) * std::fabs (gain));
    }

    double processCrossovers (double input, double trebleGain, double highmidGain, double lowmidGain,
                              double bassGain, double highCoef, double midCoef, double lowCoef,
                              bool rightChannel) noexcept
    {
        double* const filtersA[3] = { highA, midA, lowA };
        double* const filtersB[3] = { highB, midB, lowB };
        double* const filtersC[3] = { highC, midC, lowC };

        double treble = input;

        auto runStage = [&] (double* const stage[3]) noexcept
        {
            double outSample = (treble * stage[0][biqA0]) + stage[0][rightChannel ? biqSR1 : biqSL1];
            stage[0][rightChannel ? biqSR1 : biqSL1] = (treble * stage[0][biqA1]) - (outSample * stage[0][biqB1])
                                                     + stage[0][rightChannel ? biqSR2 : biqSL2];
            stage[0][rightChannel ? biqSR2 : biqSL2] = (treble * stage[0][biqA2]) - (outSample * stage[0][biqB2]);
            double highmid = outSample;
            treble -= highmid;

            outSample = (highmid * stage[1][biqA0]) + stage[1][rightChannel ? biqSR1 : biqSL1];
            stage[1][rightChannel ? biqSR1 : biqSL1] = (highmid * stage[1][biqA1]) - (outSample * stage[1][biqB1])
                                                     + stage[1][rightChannel ? biqSR2 : biqSL2];
            stage[1][rightChannel ? biqSR2 : biqSL2] = (highmid * stage[1][biqA2]) - (outSample * stage[1][biqB2]);
            double lowmid = outSample;
            highmid -= lowmid;

            outSample = (lowmid * stage[2][biqA0]) + stage[2][rightChannel ? biqSR1 : biqSL1];
            stage[2][rightChannel ? biqSR1 : biqSL1] = (lowmid * stage[2][biqA1]) - (outSample * stage[2][biqB1])
                                                     + stage[2][rightChannel ? biqSR2 : biqSL2];
            stage[2][rightChannel ? biqSR2 : biqSL2] = (lowmid * stage[2][biqA2]) - (outSample * stage[2][biqB2]);
            const double bass = outSample;
            lowmid -= bass;

            treble = (bass * bassGain) + (lowmid * lowmidGain) + (highmid * highmidGain) + (treble * trebleGain);
        };

        runStage (filtersA);
        runStage (filtersB);
        runStage (filtersC);

        double& highIIR = rightChannel ? highRIIR : highLIIR;
        double& midIIR = rightChannel ? midRIIR : midLIIR;
        double& lowIIR = rightChannel ? lowRIIR : lowLIIR;

        highIIR = (highIIR * highCoef) + (treble * (1.0 - highCoef));
        double highmid = highIIR;
        treble -= highmid;

        midIIR = (midIIR * midCoef) + (highmid * (1.0 - midCoef));
        double lowmid = midIIR;
        highmid -= lowmid;

        lowIIR = (lowIIR * lowCoef) + (lowmid * (1.0 - lowCoef));
        const double bass = lowIIR;
        lowmid -= bass;

        return (bass * bassGain) + (lowmid * lowmidGain) + (highmid * highmidGain) + (treble * trebleGain);
    }

    void clearMeters() noexcept
    {
        inPeakL = inPeakR = outPeakL = outPeakR = 0.0f;
        inClip = false;
        outClip = false;
    }

    double sampleRate = 48000.0;
    int spacing = 2;
    Parameters params {};

    std::uint32_t fpdL = 22222u;
    std::uint32_t fpdR = 22222u;

    double highA[biqTotal] {}, highB[biqTotal] {}, highC[biqTotal] {};
    double midA[biqTotal] {}, midB[biqTotal] {}, midC[biqTotal] {};
    double lowA[biqTotal] {}, lowB[biqTotal] {}, lowC[biqTotal] {};

    double highLIIR = 0.0, highRIIR = 0.0;
    double midLIIR = 0.0, midRIIR = 0.0;
    double lowLIIR = 0.0, lowRIIR = 0.0;

    double bezCompF[bezTotal] {};
    double bezCompS[bezTotal] {};
    double bezMaxF = 0.0;

    ClipOnly3Channel clipL;
    ClipOnly3Channel clipR;

    double inTrimA = 0.5, inTrimB = 0.5;

    double bypassMix = 0.0;
    double grLinear = 1.0;

    float inPeakL = 0.0f, inPeakR = 0.0f;
    float outPeakL = 0.0f, outPeakR = 0.0f;
    bool inClip = false, outClip = false;
};

} // namespace forja::dsp
