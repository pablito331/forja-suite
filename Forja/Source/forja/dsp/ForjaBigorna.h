#pragma once

// Forja adapter for Airwindows CStrip2 (MIT, Chris Johnson).
// Reference: Forja/Source/forja/vendor/airwindows/CStrip2, commit
// b110fac3991fff7c4655cc2d25d6a3e221f9799c. processDoubleReplacing is
// transcribed stage by stage: gearbox highpass/lowpass, ButterComp, the
// three-stage interleaved EQ with its drive section, and the Spiral output
// stage. All `static`/member state lives per instance.
//
// Deviations: Forja adds bypass smoothing, metering and a gain-reduction
// readout taken from the compressor's totalmultiplier. The 64-bit dither of
// the original is commented out there too; only the RNG advances. No heap
// allocation, logging or locks in process.

#include "forja/dsp/ForjaDspCommon.h"

namespace forja::dsp
{

class BigornaDSP
{
public:
    struct Parameters
    {
        float treble = 0.5f;
        float mid = 0.5f;
        float bass = 0.5f;
        float trebFreq = 0.5f;
        float bassFreq = 0.5f;
        float lowCap = 1.0f;
        float hiCap = 0.0f;
        float compres = 0.0f;
        float compSpd = 0.0f;
        float output = 0.33f;
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
        iirHighpassAL = iirHighpassBL = iirHighpassCL = 0.0;
        iirHighpassDL = iirHighpassEL = iirHighpassFL = 0.0;
        iirLowpassAL = iirLowpassBL = iirLowpassCL = 0.0;
        iirLowpassDL = iirLowpassEL = iirLowpassFL = 0.0;
        iirHighpassAR = iirHighpassBR = iirHighpassCR = 0.0;
        iirHighpassDR = iirHighpassER = iirHighpassFR = 0.0;
        iirLowpassAR = iirLowpassBR = iirLowpassCR = 0.0;
        iirLowpassDR = iirLowpassER = iirLowpassFR = 0.0;
        count = 0;

        lastSampleL = last2SampleL = 0.0;
        lastSampleR = last2SampleR = 0.0;

        iirHighSampleLA = iirHighSampleLB = iirHighSampleLC = 0.0;
        iirHighSampleLD = iirHighSampleLE = 0.0;
        iirLowSampleLA = iirLowSampleLB = iirLowSampleLC = 0.0;
        iirLowSampleLD = iirLowSampleLE = 0.0;
        iirHighSampleL = iirLowSampleL = 0.0;

        iirHighSampleRA = iirHighSampleRB = iirHighSampleRC = 0.0;
        iirHighSampleRD = iirHighSampleRE = 0.0;
        iirLowSampleRA = iirLowSampleRB = iirLowSampleRC = 0.0;
        iirLowSampleRD = iirLowSampleRE = 0.0;
        iirHighSampleR = iirLowSampleR = 0.0;

        tripletLA = tripletLB = tripletLC = tripletFactorL = 0.0;
        tripletRA = tripletRB = tripletRC = tripletFactorR = 0.0;
        flip = false;
        flipthree = 0;

        controlAposL = controlAnegL = controlBposL = controlBnegL = 1.0;
        targetposL = targetnegL = 1.0;
        avgLA = avgLB = nvgLA = nvgLB = 0.0;
        controlAposR = controlAnegR = controlBposR = controlBnegR = 1.0;
        targetposR = targetnegR = 1.0;
        avgRA = avgRB = nvgRA = nvgRB = 0.0;

        fpdL = 22222u;
        fpdR = 22222u;
        bypassMix = params.bypass ? 1.0 : 0.0;
        grLinear = 1.0;
        clearMeters();
    }

    void setParameters (const Parameters& newParams) noexcept
    {
        params.treble = clamp01 (newParams.treble);
        params.mid = clamp01 (newParams.mid);
        params.bass = clamp01 (newParams.bass);
        params.trebFreq = clamp01 (newParams.trebFreq);
        params.bassFreq = clamp01 (newParams.bassFreq);
        params.lowCap = clamp01 (newParams.lowCap);
        params.hiCap = clamp01 (newParams.hiCap);
        params.compres = clamp01 (newParams.compres);
        params.compSpd = clamp01 (newParams.compSpd);
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

    int latencySamples() const noexcept { return 0; }

    // Test hook: keeps the parity harness deterministic.
    void setNoiseState (std::uint32_t newFpdL, std::uint32_t newFpdR) noexcept
    {
        fpdL = newFpdL;
        fpdR = newFpdR;
    }

private:
    template <typename Sample>
    void processSamples (Sample* left, Sample* right, int numSamples) noexcept
    {
        // Original: overallscale is the raw sample rate at the IIR stage and
        // compscale is sr/44100. The two are easy to conflate, so keep both.
        const double overallscale = sampleRate;
        const double compscale = sampleRate / 44100.0;
        const double fpOld = 0.618033988749894848204586;
        const double fpNew = 1.0 - fpOld;

        double highSampleL = 0.0, midSampleL = 0.0, bassSampleL = 0.0;
        double highSampleR = 0.0, midSampleR = 0.0, bassSampleR = 0.0;

        double densityA = ((double) params.treble * 12.0) - 6.0;
        double densityB = ((double) params.mid * 12.0) - 6.0;
        double densityC = ((double) params.bass * 12.0) - 6.0;
        bool engageEQ = true;
        if (densityA == 0.0 && densityB == 0.0 && densityC == 0.0)
            engageEQ = false;

        densityA = std::pow (10.0, densityA / 20.0) - 1.0;
        densityB = std::pow (10.0, densityB / 20.0) - 1.0;
        densityC = std::pow (10.0, densityC / 20.0) - 1.0;

        const double tripletIntensity = -densityA;
        const float trebFreqSquared = params.trebFreq * params.trebFreq;
        const float bassFreqSquared = params.bassFreq * params.bassFreq;
        const double iirAmountA = ((((double) trebFreqSquared * 15.0) + 1.0) * 1000.0) / overallscale;
        const double iirAmountB = ((((double) bassFreqSquared * 1570.0) + 30.0) * 10.0) / overallscale;
        double bridgerectifier = 0.0;
        const double outA = std::fabs (densityA);
        const double outB = std::fabs (densityB);
        const double outC = std::fabs (densityC);

        // pow(float, int) promotes to the double overload; keep the products in
        // double instead of rounding the parameter in float first.
        const double lowpassAmount = std::pow ((double) params.lowCap, 2.0);
        const double highpassAmount = std::pow ((double) params.hiCap, 2.0);
        const bool engageCapacitor = (lowpassAmount < 1.0) || (highpassAmount > 0.0);

        double inputpos = 0.0, inputneg = 0.0, calcpos = 0.0, calcneg = 0.0;
        double outputpos = 0.0, outputneg = 0.0, totalmultiplier = 1.0;
        const double inputgain = (std::pow ((double) params.compres, 4.0) * 35.0) + 1.0;
        double compoutgain = inputgain;
        compoutgain -= 1.0;
        compoutgain /= 1.2;
        compoutgain += 1.0;
        double divisor = (0.008 * std::pow ((double) params.compSpd, 2.0)) + 0.0004;
        divisor /= compscale;
        const double remainder = divisor;
        divisor = 1.0 - divisor;
        const bool engageComp = inputgain > 1.0;

        const double outputgain = (double) params.output * 3.0;
        double density = outputgain - 1.0;
        if (density < 0.0)
            density = 0.0;
        double phattity = density - 1.0;
        if (density > 1.0)
            density = 1.0;
        if (phattity < 0.0)
            phattity = 0.0;

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

            if (engageCapacitor)
            {
                count++;
                if (count > 5)
                    count = 0;

                switch (count)
                {
                    case 0:
                        gearbox (inputSampleL, inputSampleR, highpassAmount, lowpassAmount,
                                 iirHighpassAL, iirLowpassAL, iirHighpassAR, iirLowpassAR);
                        gearbox (inputSampleL, inputSampleR, highpassAmount, lowpassAmount,
                                 iirHighpassBL, iirLowpassBL, iirHighpassBR, iirLowpassBR);
                        gearbox (inputSampleL, inputSampleR, highpassAmount, lowpassAmount,
                                 iirHighpassDL, iirLowpassDL, iirHighpassDR, iirLowpassDR);
                        break;
                    case 1:
                        gearbox (inputSampleL, inputSampleR, highpassAmount, lowpassAmount,
                                 iirHighpassAL, iirLowpassAL, iirHighpassAR, iirLowpassAR);
                        gearbox (inputSampleL, inputSampleR, highpassAmount, lowpassAmount,
                                 iirHighpassCL, iirLowpassCL, iirHighpassCR, iirLowpassCR);
                        gearbox (inputSampleL, inputSampleR, highpassAmount, lowpassAmount,
                                 iirHighpassEL, iirLowpassEL, iirHighpassER, iirLowpassER);
                        break;
                    case 2:
                        gearbox (inputSampleL, inputSampleR, highpassAmount, lowpassAmount,
                                 iirHighpassAL, iirLowpassAL, iirHighpassAR, iirLowpassAR);
                        gearbox (inputSampleL, inputSampleR, highpassAmount, lowpassAmount,
                                 iirHighpassBL, iirLowpassBL, iirHighpassBR, iirLowpassBR);
                        gearbox (inputSampleL, inputSampleR, highpassAmount, lowpassAmount,
                                 iirHighpassFL, iirLowpassFL, iirHighpassFR, iirLowpassFR);
                        break;
                    case 3:
                        gearbox (inputSampleL, inputSampleR, highpassAmount, lowpassAmount,
                                 iirHighpassAL, iirLowpassAL, iirHighpassAR, iirLowpassAR);
                        gearbox (inputSampleL, inputSampleR, highpassAmount, lowpassAmount,
                                 iirHighpassCL, iirLowpassCL, iirHighpassCR, iirLowpassCR);
                        gearbox (inputSampleL, inputSampleR, highpassAmount, lowpassAmount,
                                 iirHighpassDL, iirLowpassDL, iirHighpassDR, iirLowpassDR);
                        break;
                    case 4:
                        gearbox (inputSampleL, inputSampleR, highpassAmount, lowpassAmount,
                                 iirHighpassAL, iirLowpassAL, iirHighpassAR, iirLowpassAR);
                        gearbox (inputSampleL, inputSampleR, highpassAmount, lowpassAmount,
                                 iirHighpassBL, iirLowpassBL, iirHighpassBR, iirLowpassBR);
                        gearbox (inputSampleL, inputSampleR, highpassAmount, lowpassAmount,
                                 iirHighpassEL, iirLowpassEL, iirHighpassER, iirLowpassER);
                        break;
                    case 5:
                        gearbox (inputSampleL, inputSampleR, highpassAmount, lowpassAmount,
                                 iirHighpassAL, iirLowpassAL, iirHighpassAR, iirLowpassAR);
                        gearbox (inputSampleL, inputSampleR, highpassAmount, lowpassAmount,
                                 iirHighpassCL, iirLowpassCL, iirHighpassCR, iirLowpassCR);
                        gearbox (inputSampleL, inputSampleR, highpassAmount, lowpassAmount,
                                 iirHighpassFL, iirLowpassFL, iirHighpassFR, iirLowpassFR);
                        break;
                    default:
                        break;
                }
            }

            if (engageComp)
            {
                flip = ! flip;

                inputSampleL *= inputgain;

                inputpos = (inputSampleL * fpOld) + (avgLA * fpNew) + 1.0;
                avgLA = inputSampleL;

                if (inputpos < 0.0) inputpos = 0.0;
                outputpos = inputpos / 2.0;
                if (outputpos > 1.0) outputpos = 1.0;
                inputpos *= inputpos;
                targetposL *= divisor;
                targetposL += (inputpos * remainder);
                calcpos = std::pow ((1.0 / targetposL), 2);

                inputneg = (-inputSampleL * fpOld) + (nvgLA * fpNew) + 1.0;
                nvgLA = -inputSampleL;

                if (inputneg < 0.0) inputneg = 0.0;
                outputneg = inputneg / 2.0;
                if (outputneg > 1.0) outputneg = 1.0;
                inputneg *= inputneg;
                targetnegL *= divisor;
                targetnegL += (inputneg * remainder);
                calcneg = std::pow ((1.0 / targetnegL), 2);

                if (inputSampleL > 0)
                {
                    if (flip)
                    {
                        controlAposL *= divisor;
                        controlAposL += (calcpos * remainder);
                    }
                    else
                    {
                        controlBposL *= divisor;
                        controlBposL += (calcpos * remainder);
                    }
                }
                else
                {
                    if (flip)
                    {
                        controlAnegL *= divisor;
                        controlAnegL += (calcneg * remainder);
                    }
                    else
                    {
                        controlBnegL *= divisor;
                        controlBnegL += (calcneg * remainder);
                    }
                }

                if (flip)
                    totalmultiplier = (controlAposL * outputpos) + (controlAnegL * outputneg);
                else
                    totalmultiplier = (controlBposL * outputpos) + (controlBnegL * outputneg);

                grLinear = std::min (grLinear, totalmultiplier);
                inputSampleL *= totalmultiplier;
                inputSampleL /= compoutgain;

                inputSampleR *= inputgain;

                inputpos = (inputSampleR * fpOld) + (avgRA * fpNew) + 1.0;
                avgRA = inputSampleR;

                if (inputpos < 0.0) inputpos = 0.0;
                outputpos = inputpos / 2.0;
                if (outputpos > 1.0) outputpos = 1.0;
                inputpos *= inputpos;
                targetposR *= divisor;
                targetposR += (inputpos * remainder);
                calcpos = std::pow ((1.0 / targetposR), 2);

                inputneg = (-inputSampleR * fpOld) + (nvgRA * fpNew) + 1.0;
                nvgRA = -inputSampleR;

                if (inputneg < 0.0) inputneg = 0.0;
                outputneg = inputneg / 2.0;
                if (outputneg > 1.0) outputneg = 1.0;
                inputneg *= inputneg;
                targetnegR *= divisor;
                targetnegR += (inputneg * remainder);
                calcneg = std::pow ((1.0 / targetnegR), 2);

                if (inputSampleR > 0)
                {
                    if (flip)
                    {
                        controlAposR *= divisor;
                        controlAposR += (calcpos * remainder);
                    }
                    else
                    {
                        controlBposR *= divisor;
                        controlBposR += (calcpos * remainder);
                    }
                }
                else
                {
                    if (flip)
                    {
                        controlAnegR *= divisor;
                        controlAnegR += (calcneg * remainder);
                    }
                    else
                    {
                        controlBnegR *= divisor;
                        controlBnegR += (calcneg * remainder);
                    }
                }

                if (flip)
                    totalmultiplier = (controlAposR * outputpos) + (controlAnegR * outputneg);
                else
                    totalmultiplier = (controlBposR * outputpos) + (controlBnegR * outputneg);

                grLinear = std::min (grLinear, totalmultiplier);
                inputSampleR *= totalmultiplier;
                inputSampleR /= compoutgain;
            }

            if (engageEQ)
            {
                last2SampleL = lastSampleL;
                lastSampleL = inputSampleL;
                last2SampleR = lastSampleR;
                lastSampleR = inputSampleR;
                flipthree++;
                if (flipthree < 1 || flipthree > 3)
                    flipthree = 1;

                switch (flipthree)
                {
                    case 1:
                        tripletFactorL = last2SampleL - inputSampleL;
                        tripletLA += tripletFactorL;
                        tripletLC -= tripletFactorL;
                        tripletFactorL = tripletLA * tripletIntensity;
                        iirHighSampleLC = (iirHighSampleLC * (1.0 - iirAmountA)) + (inputSampleL * iirAmountA);
                        highSampleL = inputSampleL - iirHighSampleLC;
                        iirLowSampleLC = (iirLowSampleLC * (1.0 - iirAmountB)) + (inputSampleL * iirAmountB);
                        bassSampleL = iirLowSampleLC;

                        tripletFactorR = last2SampleR - inputSampleR;
                        tripletRA += tripletFactorR;
                        tripletRC -= tripletFactorR;
                        tripletFactorR = tripletRA * tripletIntensity;
                        iirHighSampleRC = (iirHighSampleRC * (1.0 - iirAmountA)) + (inputSampleR * iirAmountA);
                        highSampleR = inputSampleR - iirHighSampleRC;
                        iirLowSampleRC = (iirLowSampleRC * (1.0 - iirAmountB)) + (inputSampleR * iirAmountB);
                        bassSampleR = iirLowSampleRC;
                        break;
                    case 2:
                        tripletFactorL = last2SampleL - inputSampleL;
                        tripletLB += tripletFactorL;
                        tripletLA -= tripletFactorL;
                        tripletFactorL = tripletLB * tripletIntensity;
                        iirHighSampleLD = (iirHighSampleLD * (1.0 - iirAmountA)) + (inputSampleL * iirAmountA);
                        highSampleL = inputSampleL - iirHighSampleLD;
                        iirLowSampleLD = (iirLowSampleLD * (1.0 - iirAmountB)) + (inputSampleL * iirAmountB);
                        bassSampleL = iirLowSampleLD;

                        tripletFactorR = last2SampleR - inputSampleR;
                        tripletRB += tripletFactorR;
                        tripletRA -= tripletFactorR;
                        tripletFactorR = tripletRB * tripletIntensity;
                        iirHighSampleRD = (iirHighSampleRD * (1.0 - iirAmountA)) + (inputSampleR * iirAmountA);
                        highSampleR = inputSampleR - iirHighSampleRD;
                        iirLowSampleRD = (iirLowSampleRD * (1.0 - iirAmountB)) + (inputSampleR * iirAmountB);
                        bassSampleR = iirLowSampleRD;
                        break;
                    case 3:
                        tripletFactorL = last2SampleL - inputSampleL;
                        tripletLC += tripletFactorL;
                        tripletLB -= tripletFactorL;
                        tripletFactorL = tripletLC * tripletIntensity;
                        iirHighSampleLE = (iirHighSampleLE * (1.0 - iirAmountA)) + (inputSampleL * iirAmountA);
                        highSampleL = inputSampleL - iirHighSampleLE;
                        iirLowSampleLE = (iirLowSampleLE * (1.0 - iirAmountB)) + (inputSampleL * iirAmountB);
                        bassSampleL = iirLowSampleLE;

                        tripletFactorR = last2SampleR - inputSampleR;
                        tripletRC += tripletFactorR;
                        tripletRB -= tripletFactorR;
                        tripletFactorR = tripletRC * tripletIntensity;
                        iirHighSampleRE = (iirHighSampleRE * (1.0 - iirAmountA)) + (inputSampleR * iirAmountA);
                        highSampleR = inputSampleR - iirHighSampleRE;
                        iirLowSampleRE = (iirLowSampleRE * (1.0 - iirAmountB)) + (inputSampleR * iirAmountB);
                        bassSampleR = iirLowSampleRE;
                        break;
                    default:
                        break;
                }

                tripletLA /= 2.0;
                tripletLB /= 2.0;
                tripletLC /= 2.0;
                highSampleL = highSampleL + tripletFactorL;

                tripletRA /= 2.0;
                tripletRB /= 2.0;
                tripletRC /= 2.0;
                highSampleR = highSampleR + tripletFactorR;

                if (flip)
                {
                    iirHighSampleLA = (iirHighSampleLA * (1.0 - iirAmountA)) + (highSampleL * iirAmountA);
                    highSampleL -= iirHighSampleLA;
                    iirLowSampleLA = (iirLowSampleLA * (1.0 - iirAmountB)) + (bassSampleL * iirAmountB);
                    bassSampleL = iirLowSampleLA;

                    iirHighSampleRA = (iirHighSampleRA * (1.0 - iirAmountA)) + (highSampleR * iirAmountA);
                    highSampleR -= iirHighSampleRA;
                    iirLowSampleRA = (iirLowSampleRA * (1.0 - iirAmountB)) + (bassSampleR * iirAmountB);
                    bassSampleR = iirLowSampleRA;
                }
                else
                {
                    iirHighSampleLB = (iirHighSampleLB * (1.0 - iirAmountA)) + (highSampleL * iirAmountA);
                    highSampleL -= iirHighSampleLB;
                    iirLowSampleLB = (iirLowSampleLB * (1.0 - iirAmountB)) + (bassSampleL * iirAmountB);
                    bassSampleL = iirLowSampleLB;

                    iirHighSampleRB = (iirHighSampleRB * (1.0 - iirAmountA)) + (highSampleR * iirAmountA);
                    highSampleR -= iirHighSampleRB;
                    iirLowSampleRB = (iirLowSampleRB * (1.0 - iirAmountB)) + (bassSampleR * iirAmountB);
                    bassSampleR = iirLowSampleRB;
                }

                iirHighSampleL = (iirHighSampleL * (1.0 - iirAmountA)) + (highSampleL * iirAmountA);
                highSampleL -= iirHighSampleL;
                iirLowSampleL = (iirLowSampleL * (1.0 - iirAmountB)) + (bassSampleL * iirAmountB);
                bassSampleL = iirLowSampleL;

                iirHighSampleR = (iirHighSampleR * (1.0 - iirAmountA)) + (highSampleR * iirAmountA);
                highSampleR -= iirHighSampleR;
                iirLowSampleR = (iirLowSampleR * (1.0 - iirAmountB)) + (bassSampleR * iirAmountB);
                bassSampleR = iirLowSampleR;

                midSampleL = (inputSampleL - bassSampleL) - highSampleL;
                midSampleR = (inputSampleR - bassSampleR) - highSampleR;

                highSampleL *= (densityA + 1.0);
                bridgerectifier = std::fabs (highSampleL) * 1.57079633;
                if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
                if (densityA > 0) bridgerectifier = std::sin (bridgerectifier);
                else bridgerectifier = 1.0 - std::cos (bridgerectifier);
                if (highSampleL > 0) highSampleL = (highSampleL * (1.0 - outA)) + (bridgerectifier * outA);
                else highSampleL = (highSampleL * (1.0 - outA)) - (bridgerectifier * outA);

                highSampleR *= (densityA + 1.0);
                bridgerectifier = std::fabs (highSampleR) * 1.57079633;
                if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
                if (densityA > 0) bridgerectifier = std::sin (bridgerectifier);
                else bridgerectifier = 1.0 - std::cos (bridgerectifier);
                if (highSampleR > 0) highSampleR = (highSampleR * (1.0 - outA)) + (bridgerectifier * outA);
                else highSampleR = (highSampleR * (1.0 - outA)) - (bridgerectifier * outA);

                midSampleL *= (densityB + 1.0);
                bridgerectifier = std::fabs (midSampleL) * 1.57079633;
                if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
                if (densityB > 0) bridgerectifier = std::sin (bridgerectifier);
                else bridgerectifier = 1.0 - std::cos (bridgerectifier);
                if (midSampleL > 0) midSampleL = (midSampleL * (1.0 - outB)) + (bridgerectifier * outB);
                else midSampleL = (midSampleL * (1.0 - outB)) - (bridgerectifier * outB);

                midSampleR *= (densityB + 1.0);
                bridgerectifier = std::fabs (midSampleR) * 1.57079633;
                if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
                if (densityB > 0) bridgerectifier = std::sin (bridgerectifier);
                else bridgerectifier = 1.0 - std::cos (bridgerectifier);
                if (midSampleR > 0) midSampleR = (midSampleR * (1.0 - outB)) + (bridgerectifier * outB);
                else midSampleR = (midSampleR * (1.0 - outB)) - (bridgerectifier * outB);

                bassSampleL *= (densityC + 1.0);
                bridgerectifier = std::fabs (bassSampleL) * 1.57079633;
                if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
                if (densityC > 0) bridgerectifier = std::sin (bridgerectifier);
                else bridgerectifier = 1.0 - std::cos (bridgerectifier);
                if (bassSampleL > 0) bassSampleL = (bassSampleL * (1.0 - outC)) + (bridgerectifier * outC);
                else bassSampleL = (bassSampleL * (1.0 - outC)) - (bridgerectifier * outC);

                bassSampleR *= (densityC + 1.0);
                bridgerectifier = std::fabs (bassSampleR) * 1.57079633;
                if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
                if (densityC > 0) bridgerectifier = std::sin (bridgerectifier);
                else bridgerectifier = 1.0 - std::cos (bridgerectifier);
                if (bassSampleR > 0) bassSampleR = (bassSampleR * (1.0 - outC)) + (bridgerectifier * outC);
                else bassSampleR = (bassSampleR * (1.0 - outC)) - (bridgerectifier * outC);

                inputSampleL = midSampleL;
                inputSampleL += highSampleL;
                inputSampleL += bassSampleL;

                inputSampleR = midSampleR;
                inputSampleR += highSampleR;
                inputSampleR += bassSampleR;
            }

            if (outputgain != 1.0)
            {
                inputSampleL *= outputgain;
                inputSampleR *= outputgain;
                const double rawSampleL = inputSampleL;
                const double rawSampleR = inputSampleR;
                if (inputSampleL > 1.0) inputSampleL = 1.0;
                if (inputSampleL < -1.0) inputSampleL = -1.0;
                if (inputSampleR > 1.0) inputSampleR = 1.0;
                if (inputSampleR < -1.0) inputSampleR = -1.0;
                const double phatSampleL = std::sin (inputSampleL * 1.57079633);
                const double phatSampleR = std::sin (inputSampleR * 1.57079633);
                inputSampleL *= 1.2533141373155;
                inputSampleR *= 1.2533141373155;
                const double distSampleL = std::sin (inputSampleL * std::fabs (inputSampleL))
                                         / ((std::fabs (inputSampleL) == 0.0) ? 1.0 : std::fabs (inputSampleL));
                const double distSampleR = std::sin (inputSampleR * std::fabs (inputSampleR))
                                         / ((std::fabs (inputSampleR) == 0.0) ? 1.0 : std::fabs (inputSampleR));
                inputSampleL = distSampleL;
                inputSampleR = distSampleR;
                if (density < 1.0)
                {
                    inputSampleL = (rawSampleL * (1.0 - density)) + (distSampleL * density);
                    inputSampleR = (rawSampleR * (1.0 - density)) + (distSampleR * density);
                }
                if (phattity > 0.0)
                {
                    inputSampleL = (inputSampleL * (1.0 - phattity)) + (phatSampleL * phattity);
                    inputSampleR = (inputSampleR * (1.0 - phattity)) + (phatSampleR * phattity);
                }
            }

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

    static void gearbox (double& inputL, double& inputR,
                         double highpassAmount, double lowpassAmount,
                         double& hpL, double& lpL, double& hpR, double& lpR) noexcept
    {
        hpL = (hpL * (1.0 - highpassAmount)) + (inputL * highpassAmount);
        inputL -= hpL;
        lpL = (lpL * (1.0 - lowpassAmount)) + (inputL * lowpassAmount);
        inputL = lpL;

        hpR = (hpR * (1.0 - highpassAmount)) + (inputR * highpassAmount);
        inputR -= hpR;
        lpR = (lpR * (1.0 - lowpassAmount)) + (inputR * lowpassAmount);
        inputR = lpR;
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

    double lastSampleL = 0.0, last2SampleL = 0.0;
    double lastSampleR = 0.0, last2SampleR = 0.0;

    double iirHighpassAL = 0.0, iirHighpassBL = 0.0, iirHighpassCL = 0.0;
    double iirHighpassDL = 0.0, iirHighpassEL = 0.0, iirHighpassFL = 0.0;
    double iirLowpassAL = 0.0, iirLowpassBL = 0.0, iirLowpassCL = 0.0;
    double iirLowpassDL = 0.0, iirLowpassEL = 0.0, iirLowpassFL = 0.0;
    double iirHighpassAR = 0.0, iirHighpassBR = 0.0, iirHighpassCR = 0.0;
    double iirHighpassDR = 0.0, iirHighpassER = 0.0, iirHighpassFR = 0.0;
    double iirLowpassAR = 0.0, iirLowpassBR = 0.0, iirLowpassCR = 0.0;
    double iirLowpassDR = 0.0, iirLowpassER = 0.0, iirLowpassFR = 0.0;
    int count = 0;

    double iirHighSampleLA = 0.0, iirHighSampleLB = 0.0, iirHighSampleLC = 0.0;
    double iirHighSampleLD = 0.0, iirHighSampleLE = 0.0;
    double iirLowSampleLA = 0.0, iirLowSampleLB = 0.0, iirLowSampleLC = 0.0;
    double iirLowSampleLD = 0.0, iirLowSampleLE = 0.0;
    double iirHighSampleL = 0.0, iirLowSampleL = 0.0;

    double iirHighSampleRA = 0.0, iirHighSampleRB = 0.0, iirHighSampleRC = 0.0;
    double iirHighSampleRD = 0.0, iirHighSampleRE = 0.0;
    double iirLowSampleRA = 0.0, iirLowSampleRB = 0.0, iirLowSampleRC = 0.0;
    double iirLowSampleRD = 0.0, iirLowSampleRE = 0.0;
    double iirHighSampleR = 0.0, iirLowSampleR = 0.0;

    double tripletLA = 0.0, tripletLB = 0.0, tripletLC = 0.0, tripletFactorL = 0.0;
    double tripletRA = 0.0, tripletRB = 0.0, tripletRC = 0.0, tripletFactorR = 0.0;

    bool flip = false;
    int flipthree = 0;

    double controlAposL = 1.0, controlAnegL = 1.0;
    double controlBposL = 1.0, controlBnegL = 1.0;
    double targetposL = 1.0, targetnegL = 1.0;
    double avgLA = 0.0, avgLB = 0.0, nvgLA = 0.0, nvgLB = 0.0;

    double controlAposR = 1.0, controlAnegR = 1.0;
    double controlBposR = 1.0, controlBnegR = 1.0;
    double targetposR = 1.0, targetnegR = 1.0;
    double avgRA = 0.0, avgRB = 0.0, nvgRA = 0.0, nvgRB = 0.0;

    double bypassMix = 0.0;
    double grLinear = 1.0;

    float inPeakL = 0.0f, inPeakR = 0.0f;
    float outPeakL = 0.0f, outPeakR = 0.0f;
    bool inClip = false, outClip = false;
};

} // namespace forja::dsp
