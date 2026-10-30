#pragma once

// Forja adapter for Airwindows ToTape9 (MIT, Chris Johnson).
// Reference: Forja/Source/forja/vendor/airwindows/ToTape9, commit
// b110fac3991fff7c4655cc2d25d6a3e221f9799c. processDoubleReplacing is
// transcribed literally: Dubly encode, flutter ring, bias slew, hysteresis,
// the two TapeHack2 averaging passes, the TapeHack Taylor series, the
// head-bump biquad pair, Dubly decode and ClipOnly3.
//
// Deviations: Forja adds metering and a 10 ms bypass crossfade, and runs
// ClipOnly3 through the shared ClipOnly3Channel (larger ring; the original's
// 18/34 buffers are already in bounds because spacing stops at 16). The
// commented-out 64-bit dither of the original stays commented; only the RNG
// advances. No heap allocation, logging or locks in process.

#include "forja/dsp/ForjaDspCommon.h"

#include <cmath>
#include <cstdint>

namespace forja::dsp
{

class LaminadorDSP
{
public:
    struct Parameters
    {
        float input = 0.5f;
        float tilt = 0.5f;
        float shape = 0.5f;
        float flutter = 0.5f;
        float flutSpd = 0.5f;
        float bias = 0.5f;
        float headBmp = 0.5f;
        float headFrq = 0.5f;
        float output = 0.5f;
        bool bypass = false;
    };

    LaminadorDSP() noexcept { reset(); }

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
        iirEncL = iirDecL = 0.0;
        compEncL = compDecL = 1.0;
        avgEncL = avgDecL = 0.0;
        iirEncR = iirDecR = 0.0;
        compEncR = compDecR = 1.0;
        avgEncR = avgDecR = 0.0;

        for (double& value : dL)
            value = 0.0;
        for (double& value : dR)
            value = 0.0;

        sweepL = kPi;
        sweepR = kPi;
        nextmaxL = 0.5;
        nextmaxR = 0.5;
        gcount = 0;

        for (double& value : gslew)
            value = 0.0;

        hysteresisL = 0.0;
        hysteresisR = 0.0;

        headBumpL = 0.0;
        headBumpR = 0.0;
        for (double& value : hdbA)
            value = 0.0;
        for (double& value : hdbB)
            value = 0.0;

        for (double& value : avg32L) value = 0.0;
        for (double& value : avg32R) value = 0.0;
        for (double& value : avg16L) value = 0.0;
        for (double& value : avg16R) value = 0.0;
        for (double& value : avg8L) value = 0.0;
        for (double& value : avg8R) value = 0.0;
        for (double& value : avg4L) value = 0.0;
        for (double& value : avg4R) value = 0.0;
        for (double& value : avg2L) value = 0.0;
        for (double& value : avg2R) value = 0.0;
        for (double& value : post32L) value = 0.0;
        for (double& value : post32R) value = 0.0;
        for (double& value : post16L) value = 0.0;
        for (double& value : post16R) value = 0.0;
        for (double& value : post8L) value = 0.0;
        for (double& value : post8R) value = 0.0;
        for (double& value : post4L) value = 0.0;
        for (double& value : post4R) value = 0.0;
        for (double& value : post2L) value = 0.0;
        for (double& value : post2R) value = 0.0;

        avgPos = 0;
        lastDarkL = 0.0;
        lastDarkR = 0.0;

        clipL.reset();
        clipR.reset();

        fpdL = 22222u;
        fpdR = 22222u;

        bypassMix = params.bypass ? 1.0 : 0.0;
        clearMeters();
    }

    void setParameters (const Parameters& newParams) noexcept
    {
        params.input = clamp01 (newParams.input);
        params.tilt = clamp01 (newParams.tilt);
        params.shape = clamp01 (newParams.shape);
        params.flutter = clamp01 (newParams.flutter);
        params.flutSpd = clamp01 (newParams.flutSpd);
        params.bias = clamp01 (newParams.bias);
        params.headBmp = clamp01 (newParams.headBmp);
        params.headFrq = clamp01 (newParams.headFrq);
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

    // The ClipOnly3 ring holds one 44.1k sample at every rate.
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
        prevSampL1, prevSampR1, threshold1,
        prevSampL2, prevSampR2, threshold2,
        prevSampL3, prevSampR3, threshold3,
        prevSampL4, prevSampR4, threshold4,
        prevSampL5, prevSampR5, threshold5,
        prevSampL6, prevSampR6, threshold6,
        prevSampL7, prevSampR7, threshold7,
        prevSampL8, prevSampR8, threshold8,
        prevSampL9, prevSampR9, threshold9,
        gslewTotal
    };

    enum
    {
        hdbFreq, hdbReso, hdbA0, hdbA1, hdbA2, hdbB1, hdbB2,
        hdbSL1, hdbSL2, hdbSR1, hdbSR2, hdbTotal
    };

    template <typename Sample>
    void processSamples (Sample* left, Sample* right, int numSamples) noexcept
    {
        const double sampleRateNow = sampleRate;
        const double overallscale = sampleRateNow / 44100.0;

        int spacing = (int) std::floor (overallscale);
        if (spacing < 1) spacing = 1;
        if (spacing > 16) spacing = 16;
        int slewsing = (int) std::floor (overallscale * 2.0);
        if (slewsing < 2) slewsing = 2;
        if (slewsing > 32) slewsing = 32;

        const double inputGain = std::pow ((double) params.input * 2.0, 2.0);

        const double dublyAmount = (double) params.tilt * 2.0;
        double outlyAmount = (1.0 - (double) params.tilt) * -2.0;
        if (outlyAmount < -1.0) outlyAmount = -1.0;
        const double iirEncFreq = (1.0 - (double) params.shape) / overallscale;
        const double iirDecFreq = (double) params.shape / overallscale;

        double flutDepth = std::pow ((double) params.flutter, 6.0) * overallscale * 50.0;
        if (flutDepth > 498.0) flutDepth = 498.0;
        const double flutFrequency = (0.02 * std::pow ((double) params.flutSpd, 3.0)) / overallscale;
        const double bias = ((double) params.bias * 2.0) - 1.0;
        double underBias = (std::pow (bias, 4.0) * 0.25) / overallscale;
        double overBias = std::pow (1.0 - bias, 3.0) / overallscale;
        if (bias > 0.0) underBias = 0.0;
        if (bias < 0.0) overBias = 1.0 / overallscale;

        gslew[threshold9] = overBias;
        overBias *= 1.618033988749894848204586;
        gslew[threshold8] = overBias;
        overBias *= 1.618033988749894848204586;
        gslew[threshold7] = overBias;
        overBias *= 1.618033988749894848204586;
        gslew[threshold6] = overBias;
        overBias *= 1.618033988749894848204586;
        gslew[threshold5] = overBias;
        overBias *= 1.618033988749894848204586;
        gslew[threshold4] = overBias;
        overBias *= 1.618033988749894848204586;
        gslew[threshold3] = overBias;
        overBias *= 1.618033988749894848204586;
        gslew[threshold2] = overBias;
        overBias *= 1.618033988749894848204586;
        gslew[threshold1] = overBias;
        overBias *= 1.618033988749894848204586;

        const double headBumpDrive = ((double) params.headBmp * 0.1) / overallscale;
        const double headBumpMix = (double) params.headBmp * 0.5;

        // The original squares the float control, so keep the float rounding.
        const float headFrqSquared = params.headFrq * params.headFrq;
        hdbA[hdbFreq] = (((double) headFrqSquared * 175.0) + 25.0) / sampleRateNow;
        hdbB[hdbFreq] = hdbA[hdbFreq] * 0.9375;
        hdbB[hdbReso] = hdbA[hdbReso] = 0.618033988749894848204586;
        hdbB[hdbA1] = hdbA[hdbA1] = 0.0;

        double K = std::tan (kPi * hdbA[hdbFreq]);
        double norm = 1.0 / (1.0 + K / hdbA[hdbReso] + K * K);
        hdbA[hdbA0] = K / hdbA[hdbReso] * norm;
        hdbA[hdbA2] = -hdbA[hdbA0];
        hdbA[hdbB1] = 2.0 * (K * K - 1.0) * norm;
        hdbA[hdbB2] = (1.0 - K / hdbA[hdbReso] + K * K) * norm;
        K = std::tan (kPi * hdbB[hdbFreq]);
        norm = 1.0 / (1.0 + K / hdbB[hdbReso] + K * K);
        hdbB[hdbA0] = K / hdbB[hdbReso] * norm;
        hdbB[hdbA2] = -hdbB[hdbA0];
        hdbB[hdbB1] = 2.0 * (K * K - 1.0) * norm;
        hdbB[hdbB2] = (1.0 - K / hdbB[hdbReso] + K * K) * norm;

        const double outputGain = (double) params.output * 2.0;

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

            if (inputGain != 1.0)
            {
                inputSampleL *= inputGain;
                inputSampleR *= inputGain;
            }

            // Dubly encode
            iirEncL = (iirEncL * (1.0 - iirEncFreq)) + (inputSampleL * iirEncFreq);
            double highPart = ((inputSampleL - iirEncL) * 2.848);
            highPart += avgEncL;
            avgEncL = (inputSampleL - iirEncL) * 1.152;
            if (highPart > 1.0) highPart = 1.0;
            if (highPart < -1.0) highPart = -1.0;
            double dubly = std::fabs (highPart);
            if (dubly > 0.0)
            {
                double adjust = std::log (1.0 + (255.0 * dubly)) / 2.40823996531;
                if (adjust > 0.0) dubly /= adjust;
                compEncL = (compEncL * (1.0 - iirEncFreq)) + (dubly * iirEncFreq);
                inputSampleL += ((highPart * compEncL) * dublyAmount);
            }
            iirEncR = (iirEncR * (1.0 - iirEncFreq)) + (inputSampleR * iirEncFreq);
            highPart = ((inputSampleR - iirEncR) * 2.848);
            highPart += avgEncR;
            avgEncR = (inputSampleR - iirEncR) * 1.152;
            if (highPart > 1.0) highPart = 1.0;
            if (highPart < -1.0) highPart = -1.0;
            dubly = std::fabs (highPart);
            if (dubly > 0.0)
            {
                double adjust = std::log (1.0 + (255.0 * dubly)) / 2.40823996531;
                if (adjust > 0.0) dubly /= adjust;
                compEncR = (compEncR * (1.0 - iirEncFreq)) + (dubly * iirEncFreq);
                inputSampleR += ((highPart * compEncR) * dublyAmount);
            }

            // Flutter
            if (flutDepth > 0.0)
            {
                if (gcount < 0 || gcount > 999) gcount = 999;
                dL[gcount] = inputSampleL;
                int count = gcount;
                double offset = flutDepth + (flutDepth * std::sin (sweepL));
                sweepL += nextmaxL * flutFrequency;
                if (sweepL > (kPi * 2.0))
                {
                    sweepL -= kPi * 2.0;
                    const double flutA = 0.24 + ((double) fpdL / (double) UINT32_MAX * 0.74);
                    fpdL ^= fpdL << 13; fpdL ^= fpdL >> 17; fpdL ^= fpdL << 5;
                    const double flutB = 0.24 + ((double) fpdL / (double) UINT32_MAX * 0.74);
                    if (std::fabs (flutA - std::sin (sweepR + nextmaxR))
                        < std::fabs (flutB - std::sin (sweepR + nextmaxR)))
                        nextmaxL = flutA;
                    else
                        nextmaxL = flutB;
                }
                count += (int) std::floor (offset);
                inputSampleL = (dL[count - ((count > 999) ? 1000 : 0)] * (1 - (offset - std::floor (offset))));
                inputSampleL += (dL[count + 1 - ((count + 1 > 999) ? 1000 : 0)] * (offset - std::floor (offset)));
                dR[gcount] = inputSampleR;
                count = gcount;
                offset = flutDepth + (flutDepth * std::sin (sweepR));
                sweepR += nextmaxR * flutFrequency;
                if (sweepR > (kPi * 2.0))
                {
                    sweepR -= kPi * 2.0;
                    const double flutA = 0.24 + ((double) fpdR / (double) UINT32_MAX * 0.74);
                    fpdR ^= fpdR << 13; fpdR ^= fpdR >> 17; fpdR ^= fpdR << 5;
                    const double flutB = 0.24 + ((double) fpdR / (double) UINT32_MAX * 0.74);
                    if (std::fabs (flutA - std::sin (sweepL + nextmaxL))
                        < std::fabs (flutB - std::sin (sweepL + nextmaxL)))
                        nextmaxR = flutA;
                    else
                        nextmaxR = flutB;
                }
                count += (int) std::floor (offset);
                inputSampleR = (dR[count - ((count > 999) ? 1000 : 0)] * (1 - (offset - std::floor (offset))));
                inputSampleR += (dR[count + 1 - ((count + 1 > 999) ? 1000 : 0)] * (offset - std::floor (offset)));
                gcount--;
            }

            // Bias slew
            if (std::fabs (bias) > 0.001)
            {
                for (int x = 0; x < gslewTotal; x += 3)
                {
                    if (underBias > 0.0)
                    {
                        double stuck = std::fabs (inputSampleL - (gslew[x] / 0.975)) / underBias;
                        if (stuck < 1.0)
                            inputSampleL = (inputSampleL * stuck) + ((gslew[x] / 0.975) * (1.0 - stuck));
                        stuck = std::fabs (inputSampleR - (gslew[x + 1] / 0.975)) / underBias;
                        if (stuck < 1.0)
                            inputSampleR = (inputSampleR * stuck) + ((gslew[x + 1] / 0.975) * (1.0 - stuck));
                    }
                    if ((inputSampleL - gslew[x]) > gslew[x + 2]) inputSampleL = gslew[x] + gslew[x + 2];
                    if (-(inputSampleL - gslew[x]) > gslew[x + 2]) inputSampleL = gslew[x] - gslew[x + 2];
                    gslew[x] = inputSampleL * 0.975;
                    if ((inputSampleR - gslew[x + 1]) > gslew[x + 2]) inputSampleR = gslew[x + 1] + gslew[x + 2];
                    if (-(inputSampleR - gslew[x + 1]) > gslew[x + 2]) inputSampleR = gslew[x + 1] - gslew[x + 2];
                    gslew[x + 1] = inputSampleR * 0.975;
                }
            }

            // Tiny hysteresis
            double applyHysteresis = (1.0 - std::fabs (inputSampleL)) * (1.0 - std::fabs (inputSampleL)) * 0.012;
            hysteresisL = std::max (std::min (hysteresisL + ((inputSampleL * std::fabs (inputSampleL))), 0.011449), -0.011449) * 0.999;
            inputSampleL += (hysteresisL * applyHysteresis);
            applyHysteresis = (1.0 - std::fabs (inputSampleR)) * (1.0 - std::fabs (inputSampleR)) * 0.012;
            hysteresisR = std::max (std::min (hysteresisR + ((inputSampleR * std::fabs (inputSampleR))), 0.011449), -0.011449) * 0.999;
            inputSampleR += (hysteresisR * applyHysteresis);

            // TapeHack2, pre-distortion
            double darkSampleL = inputSampleL;
            double darkSampleR = inputSampleR;
            if (avgPos > 31) avgPos = 0;
            if (slewsing > 31)
            {
                avg32L[avgPos] = darkSampleL; avg32R[avgPos] = darkSampleR;
                darkSampleL = 0.0; darkSampleR = 0.0;
                for (int x = 0; x < 32; x++) { darkSampleL += avg32L[x]; darkSampleR += avg32R[x]; }
                darkSampleL /= 32.0; darkSampleR /= 32.0;
            }
            if (slewsing > 15)
            {
                avg16L[avgPos % 16] = darkSampleL; avg16R[avgPos % 16] = darkSampleR;
                darkSampleL = 0.0; darkSampleR = 0.0;
                for (int x = 0; x < 16; x++) { darkSampleL += avg16L[x]; darkSampleR += avg16R[x]; }
                darkSampleL /= 16.0; darkSampleR /= 16.0;
            }
            if (slewsing > 7)
            {
                avg8L[avgPos % 8] = darkSampleL; avg8R[avgPos % 8] = darkSampleR;
                darkSampleL = 0.0; darkSampleR = 0.0;
                for (int x = 0; x < 8; x++) { darkSampleL += avg8L[x]; darkSampleR += avg8R[x]; }
                darkSampleL /= 8.0; darkSampleR /= 8.0;
            }
            if (slewsing > 3)
            {
                avg4L[avgPos % 4] = darkSampleL; avg4R[avgPos % 4] = darkSampleR;
                darkSampleL = 0.0; darkSampleR = 0.0;
                for (int x = 0; x < 4; x++) { darkSampleL += avg4L[x]; darkSampleR += avg4R[x]; }
                darkSampleL /= 4.0; darkSampleR /= 4.0;
            }
            if (slewsing > 1)
            {
                avg2L[avgPos % 2] = darkSampleL; avg2R[avgPos % 2] = darkSampleR;
                darkSampleL = 0.0; darkSampleR = 0.0;
                for (int x = 0; x < 2; x++) { darkSampleL += avg2L[x]; darkSampleR += avg2R[x]; }
                darkSampleL /= 2.0; darkSampleR /= 2.0;
            }
            double avgSlewL = std::min (std::fabs (lastDarkL - inputSampleL) * 0.12 * overallscale, 1.0);
            avgSlewL = 1.0 - (1.0 - avgSlewL * 1.0 - avgSlewL);
            inputSampleL = (inputSampleL * (1.0 - avgSlewL)) + (darkSampleL * avgSlewL);
            lastDarkL = darkSampleL;
            double avgSlewR = std::min (std::fabs (lastDarkR - inputSampleR) * 0.12 * overallscale, 1.0);
            avgSlewR = 1.0 - (1.0 - avgSlewR * 1.0 - avgSlewR);
            inputSampleR = (inputSampleR * (1.0 - avgSlewR)) + (darkSampleR * avgSlewR);
            lastDarkR = darkSampleR;

            // TapeHack Taylor series
            inputSampleL = std::max (std::min (inputSampleL, 2.305929007734908), -2.305929007734908);
            double addtwo = inputSampleL * inputSampleL;
            double empower = inputSampleL * addtwo;
            inputSampleL -= (empower / 6.0);
            empower *= addtwo;
            inputSampleL += (empower / 69.0);
            empower *= addtwo;
            inputSampleL -= (empower / 2530.08);
            empower *= addtwo;
            inputSampleL += (empower / 224985.6);
            empower *= addtwo;
            inputSampleL -= (empower / 9979200.0);

            inputSampleR = std::max (std::min (inputSampleR, 2.305929007734908), -2.305929007734908);
            addtwo = inputSampleR * inputSampleR;
            empower = inputSampleR * addtwo;
            inputSampleR -= (empower / 6.0);
            empower *= addtwo;
            inputSampleR += (empower / 69.0);
            empower *= addtwo;
            inputSampleR -= (empower / 2530.08);
            empower *= addtwo;
            inputSampleR += (empower / 224985.6);
            empower *= addtwo;
            inputSampleR -= (empower / 9979200.0);

            // TapeHack2, post-distortion
            darkSampleL = inputSampleL;
            darkSampleR = inputSampleR;
            if (avgPos > 31) avgPos = 0;
            if (slewsing > 31)
            {
                post32L[avgPos] = darkSampleL; post32R[avgPos] = darkSampleR;
                darkSampleL = 0.0; darkSampleR = 0.0;
                for (int x = 0; x < 32; x++) { darkSampleL += post32L[x]; darkSampleR += post32R[x]; }
                darkSampleL /= 32.0; darkSampleR /= 32.0;
            }
            if (slewsing > 15)
            {
                post16L[avgPos % 16] = darkSampleL; post16R[avgPos % 16] = darkSampleR;
                darkSampleL = 0.0; darkSampleR = 0.0;
                for (int x = 0; x < 16; x++) { darkSampleL += post16L[x]; darkSampleR += post16R[x]; }
                darkSampleL /= 16.0; darkSampleR /= 16.0;
            }
            if (slewsing > 7)
            {
                post8L[avgPos % 8] = darkSampleL; post8R[avgPos % 8] = darkSampleR;
                darkSampleL = 0.0; darkSampleR = 0.0;
                for (int x = 0; x < 8; x++) { darkSampleL += post8L[x]; darkSampleR += post8R[x]; }
                darkSampleL /= 8.0; darkSampleR /= 8.0;
            }
            if (slewsing > 3)
            {
                post4L[avgPos % 4] = darkSampleL; post4R[avgPos % 4] = darkSampleR;
                darkSampleL = 0.0; darkSampleR = 0.0;
                for (int x = 0; x < 4; x++) { darkSampleL += post4L[x]; darkSampleR += post4R[x]; }
                darkSampleL /= 4.0; darkSampleR /= 4.0;
            }
            if (slewsing > 1)
            {
                post2L[avgPos % 2] = darkSampleL; post2R[avgPos % 2] = darkSampleR;
                darkSampleL = 0.0; darkSampleR = 0.0;
                for (int x = 0; x < 2; x++) { darkSampleL += post2L[x]; darkSampleR += post2R[x]; }
                darkSampleL /= 2.0; darkSampleR /= 2.0;
            }
            avgPos++;
            inputSampleL = (inputSampleL * (1.0 - avgSlewL)) + (darkSampleL * avgSlewL);
            inputSampleR = (inputSampleR * (1.0 - avgSlewR)) + (darkSampleR * avgSlewR);

            // Head bump
            double headBumpSampleL = 0.0;
            double headBumpSampleR = 0.0;
            if (headBumpMix > 0.0)
            {
                headBumpL += (inputSampleL * headBumpDrive);
                headBumpL -= (headBumpL * headBumpL * headBumpL * (0.0618 / std::sqrt (overallscale)));
                headBumpR += (inputSampleR * headBumpDrive);
                headBumpR -= (headBumpR * headBumpR * headBumpR * (0.0618 / std::sqrt (overallscale)));
                double headBiqSampleL = (headBumpL * hdbA[hdbA0]) + hdbA[hdbSL1];
                hdbA[hdbSL1] = (headBumpL * hdbA[hdbA1]) - (headBiqSampleL * hdbA[hdbB1]) + hdbA[hdbSL2];
                hdbA[hdbSL2] = (headBumpL * hdbA[hdbA2]) - (headBiqSampleL * hdbA[hdbB2]);
                headBumpSampleL = (headBiqSampleL * hdbB[hdbA0]) + hdbB[hdbSL1];
                hdbB[hdbSL1] = (headBiqSampleL * hdbB[hdbA1]) - (headBumpSampleL * hdbB[hdbB1]) + hdbB[hdbSL2];
                hdbB[hdbSL2] = (headBiqSampleL * hdbB[hdbA2]) - (headBumpSampleL * hdbB[hdbB2]);
                double headBiqSampleR = (headBumpR * hdbA[hdbA0]) + hdbA[hdbSR1];
                hdbA[hdbSR1] = (headBumpR * hdbA[hdbA1]) - (headBiqSampleR * hdbA[hdbB1]) + hdbA[hdbSR2];
                hdbA[hdbSR2] = (headBumpR * hdbA[hdbA2]) - (headBiqSampleR * hdbA[hdbB2]);
                headBumpSampleR = (headBiqSampleR * hdbB[hdbA0]) + hdbB[hdbSR1];
                hdbB[hdbSR1] = (headBiqSampleR * hdbB[hdbA1]) - (headBumpSampleR * hdbB[hdbB1]) + hdbB[hdbSR2];
                hdbB[hdbSR2] = (headBiqSampleR * hdbB[hdbA2]) - (headBumpSampleR * hdbB[hdbB2]);
            }

            inputSampleL += (headBumpSampleL * headBumpMix);
            inputSampleR += (headBumpSampleR * headBumpMix);

            // Dubly decode
            iirDecL = (iirDecL * (1.0 - iirDecFreq)) + (inputSampleL * iirDecFreq);
            highPart = ((inputSampleL - iirDecL) * 2.628);
            highPart += avgDecL;
            avgDecL = (inputSampleL - iirDecL) * 1.372;
            if (highPart > 1.0) highPart = 1.0;
            if (highPart < -1.0) highPart = -1.0;
            dubly = std::fabs (highPart);
            if (dubly > 0.0)
            {
                double adjust = std::log (1.0 + (255.0 * dubly)) / 2.40823996531;
                if (adjust > 0.0) dubly /= adjust;
                compDecL = (compDecL * (1.0 - iirDecFreq)) + (dubly * iirDecFreq);
                inputSampleL += ((highPart * compDecL) * outlyAmount);
            }
            iirDecR = (iirDecR * (1.0 - iirDecFreq)) + (inputSampleR * iirDecFreq);
            highPart = ((inputSampleR - iirDecR) * 2.628);
            highPart += avgDecR;
            avgDecR = (inputSampleR - iirDecR) * 1.372;
            if (highPart > 1.0) highPart = 1.0;
            if (highPart < -1.0) highPart = -1.0;
            dubly = std::fabs (highPart);
            if (dubly > 0.0)
            {
                double adjust = std::log (1.0 + (255.0 * dubly)) / 2.40823996531;
                if (adjust > 0.0) dubly /= adjust;
                compDecR = (compDecR * (1.0 - iirDecFreq)) + (dubly * iirDecFreq);
                inputSampleR += ((highPart * compDecR) * outlyAmount);
            }

            if (outputGain != 1.0)
            {
                inputSampleL *= outputGain;
                inputSampleR *= outputGain;
            }

            const double noiseL = 1.0 - ((double) fpdL / (double) UINT32_MAX * 0.076);
            inputSampleL = clipL.process (inputSampleL, spacing, noiseL);
            const double noiseR = 1.0 - ((double) fpdR / (double) UINT32_MAX * 0.076);
            inputSampleR = clipR.process (inputSampleR, spacing, noiseR);

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

    std::uint32_t fpdL = 22222u;
    std::uint32_t fpdR = 22222u;

    double iirEncL = 0.0, iirDecL = 0.0;
    double compEncL = 1.0, compDecL = 1.0;
    double avgEncL = 0.0, avgDecL = 0.0;
    double iirEncR = 0.0, iirDecR = 0.0;
    double compEncR = 1.0, compDecR = 1.0;
    double avgEncR = 0.0, avgDecR = 0.0;

    double dL[1002] {}, dR[1002] {};
    double sweepL = kPi, sweepR = kPi;
    double nextmaxL = 0.5, nextmaxR = 0.5;
    int gcount = 0;

    double gslew[gslewTotal] {};

    double hysteresisL = 0.0, hysteresisR = 0.0;

    double headBumpL = 0.0, headBumpR = 0.0;
    double hdbA[hdbTotal] {}, hdbB[hdbTotal] {};

    double avg32L[33] {}, avg32R[33] {};
    double avg16L[17] {}, avg16R[17] {};
    double avg8L[9] {}, avg8R[9] {};
    double avg4L[5] {}, avg4R[5] {};
    double avg2L[3] {}, avg2R[3] {};
    double post32L[33] {}, post32R[33] {};
    double post16L[17] {}, post16R[17] {};
    double post8L[9] {}, post8R[9] {};
    double post4L[5] {}, post4R[5] {};
    double post2L[3] {}, post2R[3] {};
    double lastDarkL = 0.0, lastDarkR = 0.0;
    int avgPos = 0;

    ClipOnly3Channel clipL, clipR;

    double bypassMix = 0.0;

    float inPeakL = 0.0f, inPeakR = 0.0f;
    float outPeakL = 0.0f, outPeakR = 0.0f;
    bool inClip = false, outClip = false;
};

} // namespace forja::dsp
