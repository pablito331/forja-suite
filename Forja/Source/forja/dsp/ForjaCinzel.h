#pragma once

// Forja adapter for Airwindows Parametric (MIT, Chris Johnson).
// Reference: Forja/Source/forja/vendor/airwindows/Parametric, commit
// b110fac3991fff7c4655cc2d25d6a3e221f9799c. The static path is a direct
// transcription of processDoubleReplacing: three stacked biquads per band,
// summed in parallel over the dry signal.
//
// Forja layer: a per-band dynamic detector (stereo linked, attack 10 ms,
// release 150 ms) that reduces boosts and deepens cuts by up to 12 dB.
// With dyn == 0 the detector and the gain smoothers are never touched, so the
// output is bit-identical to the original. The 32-bit dither of the float path
// is not reproduced here (the double path only advances the RNG as well).

#include "forja/dsp/ForjaDspCommon.h"

namespace forja::dsp
{

class CinzelDSP
{
public:
    struct Parameters
    {
        float highFreq = 0.5f;
        float highGain = 0.5f;
        float highReso = 0.5f;
        float highMidFreq = 0.5f;
        float highMidGain = 0.5f;
        float highMidReso = 0.5f;
        float lowMidFreq = 0.5f;
        float lowMidGain = 0.5f;
        float lowMidReso = 0.5f;
        float mix = 1.0f;
        float dyn = 0.0f;
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
        attackCoef = 1.0 - std::exp (-1.0 / (0.010 * sampleRate));
        releaseCoef = 1.0 - std::exp (-1.0 / (0.150 * sampleRate));
        dynSmoother.prepare (sampleRate, 0.010);
        for (auto& band : dynBands)
            band.gain.prepare (sampleRate, 0.010);
    }

    void reset() noexcept
    {
        high = Band {};
        highMid = Band {};
        lowMid = Band {};
        for (auto& band : dynBands)
        {
            band.env = 0.0;
            band.gain.reset (1.0);
        }
        dynSmoother.reset ((double) params.dyn);
        dynReductionDb = 0.0;
        fpdL = 22222u;
        fpdR = 22222u;
        bypassMix = params.bypass ? 1.0 : 0.0;
        clearMeters();
    }

    void setParameters (const Parameters& newParams) noexcept
    {
        params.highFreq = clamp01 (newParams.highFreq);
        params.highGain = clamp01 (newParams.highGain);
        params.highReso = clamp01 (newParams.highReso);
        params.highMidFreq = clamp01 (newParams.highMidFreq);
        params.highMidGain = clamp01 (newParams.highMidGain);
        params.highMidReso = clamp01 (newParams.highMidReso);
        params.lowMidFreq = clamp01 (newParams.lowMidFreq);
        params.lowMidGain = clamp01 (newParams.lowMidGain);
        params.lowMidReso = clamp01 (newParams.lowMidReso);
        params.mix = clamp01 (newParams.mix);
        params.dyn = clamp01 (newParams.dyn);
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
    struct Band
    {
        double freq = 0.0, reso = 0.0, level = 0.0, nonlin = 0.5;
        double a0 = 0.0, b1 = 0.0, b2 = 0.0;
        double c0 = 0.0, d1 = 0.0, d2 = 0.0;
        double e0 = 0.0, f1 = 0.0, f2 = 0.0;
        double aL1 = 0.0, aL2 = 0.0, aR1 = 0.0, aR2 = 0.0;
        double cL1 = 0.0, cL2 = 0.0, cR1 = 0.0, cR2 = 0.0;
        double eL1 = 0.0, eL2 = 0.0, eR1 = 0.0, eR2 = 0.0;
    };

    struct BandDynamics
    {
        double env = 0.0;
        ForjaSmoother gain;
    };

    static void configureBand (Band& band, double freq, double nonlin, double resoAmount,
                               double freqFloor) noexcept
    {
        if (freq < freqFloor)
            freq = freqFloor;

        band.freq = freq;
        band.nonlin = nonlin;
        band.level = (nonlin * 2.0) - 1.0;
        if (band.level > 0.0)
            band.level *= 2.0;
        band.reso = ((0.5 + (nonlin * 0.5) + std::sqrt (freq))
                     - (1.0 - std::pow (1.0 - resoAmount, 2.0)))
                    + 0.5 + (nonlin * 0.5);

        const double K = std::tan (kPi * band.freq);
        double norm = 1.0 / (1.0 + K / (band.reso * 1.93185165) + K * K);
        band.a0 = K / (band.reso * 1.93185165) * norm;
        band.b1 = 2.0 * (K * K - 1.0) * norm;
        band.b2 = (1.0 - K / (band.reso * 1.93185165) + K * K) * norm;
        norm = 1.0 / (1.0 + K / (band.reso * 0.70710678) + K * K);
        band.c0 = K / (band.reso * 0.70710678) * norm;
        band.d1 = 2.0 * (K * K - 1.0) * norm;
        band.d2 = (1.0 - K / (band.reso * 0.70710678) + K * K) * norm;
        norm = 1.0 / (1.0 + K / (band.reso * 0.51763809) + K * K);
        band.e0 = K / (band.reso * 0.51763809) * norm;
        band.f1 = 2.0 * (K * K - 1.0) * norm;
        band.f2 = (1.0 - K / (band.reso * 0.51763809) + K * K) * norm;
    }

    void configureBands() noexcept
    {
        configureBand (high,
                       (std::pow ((double) params.highFreq, 3.0) * 14500.0 + 1500.0) / sampleRate,
                       (double) params.highGain, (double) params.highReso, 0.0001);
        configureBand (highMid,
                       (std::pow ((double) params.highMidFreq, 3.0) * 6400.0 + 600.0) / sampleRate,
                       (double) params.highMidGain, (double) params.highMidReso, 0.0001);
        configureBand (lowMid,
                       (std::pow ((double) params.lowMidFreq, 3.0) * 2200.0 + 20.0) / sampleRate,
                       (double) params.lowMidGain, (double) params.lowMidReso, 0.00001);
    }

    static double processBand (Band& band, double inputSample, bool rightChannel) noexcept
    {
        double& a1 = rightChannel ? band.aR1 : band.aL1;
        double& a2 = rightChannel ? band.aR2 : band.aL2;
        double& c1 = rightChannel ? band.cR1 : band.cL1;
        double& c2 = rightChannel ? band.cR2 : band.cL2;
        double& e1 = rightChannel ? band.eR1 : band.eL1;
        double& e2 = rightChannel ? band.eR2 : band.eL2;

        double out = inputSample * std::fabs (band.level);

        double dis = std::fabs (band.a0 * (1.0 + (out * band.nonlin)));
        if (dis > 1.0)
            dis = 1.0;
        double temp = (out * dis) + a1;
        a1 = a2 - (temp * band.b1);
        a2 = (out * -dis) - (temp * band.b2);
        out = temp;

        dis = std::fabs (band.c0 * (1.0 + (out * band.nonlin)));
        if (dis > 1.0)
            dis = 1.0;
        temp = (out * dis) + c1;
        c1 = c2 - (temp * band.d1);
        c2 = (out * -dis) - (temp * band.d2);
        out = temp;

        dis = std::fabs (band.e0 * (1.0 + (out * band.nonlin)));
        if (dis > 1.0)
            dis = 1.0;
        temp = (out * dis) + e1;
        e1 = e2 - (temp * band.f1);
        e2 = (out * -dis) - (temp * band.f2);
        out = temp;

        out *= band.level;
        if (band.level > 1.0)
            out *= band.level;
        return out;
    }

    double updateBandGain (int index, double magnitude, double thresholdLinear) noexcept
    {
        BandDynamics& dynamics = dynBands[index];
        const double coef = (magnitude > dynamics.env) ? attackCoef : releaseCoef;
        dynamics.env += (magnitude - dynamics.env) * coef;

        double reductionDb = 0.0;
        if (dynamics.env > thresholdLinear)
            reductionDb = std::min (12.0, 20.0 * std::log10 (dynamics.env / thresholdLinear));

        // Boosts shrink toward 0 dB, cuts get deeper: both move the band
        // magnitude by the same amount in dB.
        const double direction = (bandAt (index).level < 0.0) ? 1.0 : -1.0;
        dynamics.gain.setTarget (std::pow (10.0, direction * reductionDb / 20.0));
        dynReductionDb = std::max (dynReductionDb, reductionDb);
        return dynamics.gain.next();
    }

    Band& bandAt (int index) noexcept
    {
        return index == 0 ? high : (index == 1 ? highMid : lowMid);
    }

    template <typename Sample>
    void processSamples (Sample* left, Sample* right, int numSamples) noexcept
    {
        configureBands();

        const double wet = (double) params.mix;
        const double targetBypass = params.bypass ? 1.0 : 0.0;
        const double smoothing = 1.0 - std::exp (-1.0 / (0.010 * sampleRate));

        dynSmoother.setTarget ((double) params.dyn);

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

            double highL = processBand (high, inputSampleL, false);
            double highMidL = processBand (highMid, inputSampleL, false);
            double lowMidL = processBand (lowMid, inputSampleL, false);
            double highR = processBand (high, inputSampleR, true);
            double highMidR = processBand (highMid, inputSampleR, true);
            double lowMidR = processBand (lowMid, inputSampleR, true);

            const double dyn = dynSmoother.next();
            if (dyn > 1.0e-9)
            {
                const double thresholdLinear = std::pow (10.0, (-6.0 - 30.0 * dyn) / 20.0);
                const double gHigh = updateBandGain (0, std::max (std::abs (highL), std::abs (highR)),
                                                     thresholdLinear);
                const double gHighMid = updateBandGain (1, std::max (std::abs (highMidL), std::abs (highMidR)),
                                                        thresholdLinear);
                const double gLowMid = updateBandGain (2, std::max (std::abs (lowMidL), std::abs (lowMidR)),
                                                       thresholdLinear);
                highL *= gHigh;      highR *= gHigh;
                highMidL *= gHighMid; highMidR *= gHighMid;
                lowMidL *= gLowMid;  lowMidR *= gLowMid;
            }

            const double parametricL = highL + highMidL + lowMidL;
            const double parametricR = highR + highMidR + lowMidR;
            inputSampleL += parametricL * wet;
            inputSampleR += parametricR * wet;

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
        frame.gainReductionDb = (float) -dynReductionDb;
        frame.inClip = inClip;
        frame.outClip = outClip;

        if (resetAfterRead)
        {
            dynReductionDb = 0.0;
            clearMeters();
        }

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

    Band high;
    Band highMid;
    Band lowMid;
    BandDynamics dynBands[3];

    std::uint32_t fpdL = 22222u;
    std::uint32_t fpdR = 22222u;

    double attackCoef = 0.0;
    double releaseCoef = 0.0;
    ForjaSmoother dynSmoother;
    double dynReductionDb = 0.0;

    double bypassMix = 0.0;

    float inPeakL = 0.0f, inPeakR = 0.0f;
    float outPeakL = 0.0f, outPeakR = 0.0f;
    bool inClip = false, outClip = false;
};

} // namespace forja::dsp
