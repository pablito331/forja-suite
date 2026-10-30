#pragma once

// Forja adapter for Airwindows TapeDelay2 (MIT, Chris Johnson).
// Reference: Forja/Source/forja/vendor/airwindows/TapeDelay2, commit
// b110fac3991fff7c4655cc2d25d6a3e221f9799c. processDoubleReplacing is
// transcribed literally: the 88200-sample ring that fills backwards with an
// interpolated slope, the regenerative state-variable filter, the output
// filter tuned 1.618x wider, the input-driven flutter sweep and the submix
// dry/wet (0.5 is full dry plus full wet, not a crossfade).
//
// Deviations: Forja adds metering, a 10 ms bypass crossfade and an optional
// host-tempo sync that resolves the division into a target speed (sync off is
// the original time path). The original's
// commented-out 64-bit dither stays commented; only the RNG advances. Ring
// buffers live on the heap (std::vector) instead of inline members because the
// type is also created on the stack by the test harness. No allocation,
// logging or locks inside process().

#include "forja/dsp/ForjaDspCommon.h"
#include "forja/dsp/ForjaSync.h"

#include <cmath>
#include <cstdint>
#include <vector>

namespace forja::dsp
{

class RepiqueDSP
{
public:
    struct Parameters
    {
        float time = 1.0f;
        float regen = 0.0f;
        float freq = 0.5f;
        float reso = 0.0f;
        float flutter = 0.0f;
        float dryWet = 1.0f;
        bool sync = false;
        float division = 0.5f;
        double hostBpm = 0.0;
        bool bypass = false;
    };

    RepiqueDSP()
        : dL (kRingSize, 0.0), dR (kRingSize, 0.0)
    {
        reset();
    }

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
        for (double& value : dL)
            value = 0.0;
        for (double& value : dR)
            value = 0.0;

        prevSampleL = prevSampleR = 0.0;
        delayL = delayR = 0.0;
        sweepL = sweepR = 0.0;

        for (int i = 0; i < 9; ++i)
        {
            regenFilterL[i] = regenFilterR[i] = 0.0;
            outFilterL[i] = outFilterR[i] = 0.0;
        }

        for (int i = 0; i < 10; ++i)
            lastRefL[i] = lastRefR[i] = 0.0;

        cycle = 0;

        fpdL = 22222u;
        fpdR = 22222u;

        bypassMix = params.bypass ? 1.0 : 0.0;
        clearMeters();
    }

    void setParameters (const Parameters& newParams) noexcept
    {
        params.time = clamp01 (newParams.time);
        params.regen = clamp01 (newParams.regen);
        params.freq = clamp01 (newParams.freq);
        params.reso = clamp01 (newParams.reso);
        params.flutter = clamp01 (newParams.flutter);
        params.dryWet = clamp01 (newParams.dryWet);
        params.sync = newParams.sync;
        params.division = clamp01 (newParams.division);
        params.hostBpm = newParams.hostBpm;
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

    // The ring delay is musical, never PDC: the host must not compensate it.
    int latencySamples() const noexcept { return 0; }

    // Test hook: keeps the parity harness deterministic.
    void setNoiseState (std::uint32_t newFpdL, std::uint32_t newFpdR) noexcept
    {
        fpdL = newFpdL;
        fpdR = newFpdR;
    }

    // Host-tempo resolution shared by the audio path and the editor readout.
    // With sync off this is exactly the original 25 * time^4 + 1.
    static double resolveSpeed (float time, bool sync, float division,
                                double hostBpm, double sampleRate) noexcept
    {
        double timeSetting = (double) time;

        if (sync)
        {
            const double seconds = syncDivisionSeconds (division, hostBpm);
            if (seconds > 0.0 && sampleRate > 0.0)
            {
                const double speed = std::clamp (88200.0 / (sampleRate * seconds), 1.0, 26.0);
                timeSetting = std::pow ((speed - 1.0) / 25.0, 0.25);
            }
        }

        return (std::pow (timeSetting, 4.0) * 25.0) + 1.0;
    }

    static double resolveDelaySeconds (float time, bool sync, float division,
                                       double hostBpm, double sampleRate) noexcept
    {
        return 88200.0 / (resolveSpeed (time, sync, division, hostBpm, sampleRate) * sampleRate);
    }

private:
    template <typename Sample>
    void processSamples (Sample* left, Sample* right, int numSamples) noexcept
    {
        const double sampleRateNow = sampleRate;
        const double overallscale = sampleRateNow / 44100.0;

        int cycleEnd = (int) std::floor (overallscale);
        if (cycleEnd < 1) cycleEnd = 1;
        if (cycleEnd > 4) cycleEnd = 4;
        if (cycle > cycleEnd - 1) cycle = cycleEnd - 1;

        const double baseSpeed = resolveSpeed (params.time, params.sync, params.division,
                                               params.hostBpm, sampleRateNow);
        const double feedback = std::pow ((double) params.regen, 2.0);

        regenFilterL[0] = regenFilterR[0] = (std::pow ((double) params.freq, 3.0) * 0.4) + 0.0001;
        regenFilterL[1] = regenFilterR[1] = std::pow ((double) params.reso, 2.0) + 0.01;
        double K = std::tan (kPi * regenFilterR[0]);
        double norm = 1.0 / (1.0 + (K / regenFilterR[1]) + (K * K));
        regenFilterL[2] = regenFilterR[2] = K / regenFilterR[1] * norm;
        regenFilterL[4] = regenFilterR[4] = -regenFilterR[2];
        regenFilterL[5] = regenFilterR[5] = 2.0 * ((K * K) - 1.0) * norm;
        regenFilterL[6] = regenFilterR[6] = (1.0 - (K / regenFilterR[1]) + (K * K)) * norm;

        outFilterL[0] = outFilterR[0] = regenFilterR[0];
        outFilterL[1] = outFilterR[1] = regenFilterR[1] * 1.618033988749894848204586;
        K = std::tan (kPi * outFilterR[0]);
        norm = 1.0 / (1.0 + (K / outFilterR[1]) + (K * K));
        outFilterL[2] = outFilterR[2] = K / outFilterR[1] * norm;
        outFilterL[4] = outFilterR[4] = -outFilterR[2];
        outFilterL[5] = outFilterR[5] = 2.0 * ((K * K) - 1.0) * norm;
        outFilterL[6] = outFilterR[6] = (1.0 - (K / outFilterR[1]) + (K * K)) * norm;

        const double vibSpeed = std::pow ((double) params.flutter, 5.0) * baseSpeed
                              * ((regenFilterR[0] * 0.09) + 0.025);

        double wet = (double) params.dryWet * 2.0;
        double dry = 2.0 - wet;
        if (wet > 1.0) wet = 1.0;
        if (wet < 0.0) wet = 0.0;
        if (dry > 1.0) dry = 1.0;
        if (dry < 0.0) dry = 0.0;

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
            double drySampleL = inputSampleL;
            double drySampleR = inputSampleR;

            cycle++;
            if (cycle == cycleEnd)
            {
                const double speedL = baseSpeed + (vibSpeed * (std::sin (sweepL) + 1.0));
                const double speedR = baseSpeed + (vibSpeed * (std::sin (sweepR) + 1.0));
                sweepL += 0.05 * inputSampleL * inputSampleL;
                if (sweepL > 6.283185307179586) sweepL -= 6.283185307179586;
                sweepR += 0.05 * inputSampleR * inputSampleR;
                if (sweepR > 6.283185307179586) sweepR -= 6.283185307179586;

                // begin left channel
                int pos = (int) std::floor (delayL);
                double newSample = inputSampleL + (dL[(std::size_t) pos] * feedback);
                double tempSample = (newSample * regenFilterL[2]) + regenFilterL[7];
                regenFilterL[7] = -(tempSample * regenFilterL[5]) + regenFilterL[8];
                regenFilterL[8] = (newSample * regenFilterL[4]) - (tempSample * regenFilterL[6]);
                newSample = tempSample;

                delayL -= speedL;
                if (delayL < 0.0) delayL += 88200.0;
                const double increment = (newSample - prevSampleL) / speedL;
                dL[(std::size_t) pos] = prevSampleL;
                while (pos != (int) std::floor (delayL))
                {
                    dL[(std::size_t) pos] = prevSampleL;
                    prevSampleL += increment;
                    pos--;
                    if (pos < 0) pos += 88200;
                }
                prevSampleL = newSample;
                pos = (int) std::floor (delayL);
                inputSampleL = dL[(std::size_t) pos];
                tempSample = (inputSampleL * outFilterL[2]) + outFilterL[7];
                outFilterL[7] = -(tempSample * outFilterL[5]) + outFilterL[8];
                outFilterL[8] = (inputSampleL * outFilterL[4]) - (tempSample * outFilterL[6]);
                inputSampleL = tempSample;
                // end left channel
                // begin right channel
                pos = (int) std::floor (delayR);
                newSample = inputSampleR + (dR[(std::size_t) pos] * feedback);
                tempSample = (newSample * regenFilterR[2]) + regenFilterR[7];
                regenFilterR[7] = -(tempSample * regenFilterR[5]) + regenFilterR[8];
                regenFilterR[8] = (newSample * regenFilterR[4]) - (tempSample * regenFilterR[6]);
                newSample = tempSample;

                delayR -= speedR;
                if (delayR < 0.0) delayR += 88200.0;
                const double incrementR = (newSample - prevSampleR) / speedR;
                dR[(std::size_t) pos] = prevSampleR;
                while (pos != (int) std::floor (delayR))
                {
                    dR[(std::size_t) pos] = prevSampleR;
                    prevSampleR += incrementR;
                    pos--;
                    if (pos < 0) pos += 88200;
                }
                prevSampleR = newSample;
                pos = (int) std::floor (delayR);
                inputSampleR = dR[(std::size_t) pos];
                tempSample = (inputSampleR * outFilterR[2]) + outFilterR[7];
                outFilterR[7] = -(tempSample * outFilterR[5]) + outFilterR[8];
                outFilterR[8] = (inputSampleR * outFilterR[4]) - (tempSample * outFilterR[6]);
                inputSampleR = tempSample;
                // end right channel

                if (cycleEnd == 4)
                {
                    lastRefL[0] = lastRefL[4];
                    lastRefL[2] = (lastRefL[0] + inputSampleL) / 2;
                    lastRefL[1] = (lastRefL[0] + lastRefL[2]) / 2;
                    lastRefL[3] = (lastRefL[2] + inputSampleL) / 2;
                    lastRefL[4] = inputSampleL;
                    lastRefR[0] = lastRefR[4];
                    lastRefR[2] = (lastRefR[0] + inputSampleR) / 2;
                    lastRefR[1] = (lastRefR[0] + lastRefR[2]) / 2;
                    lastRefR[3] = (lastRefR[2] + inputSampleR) / 2;
                    lastRefR[4] = inputSampleR;
                }
                if (cycleEnd == 3)
                {
                    lastRefL[0] = lastRefL[3];
                    lastRefL[2] = (lastRefL[0] + lastRefL[0] + inputSampleL) / 3;
                    lastRefL[1] = (lastRefL[0] + inputSampleL + inputSampleL) / 3;
                    lastRefL[3] = inputSampleL;
                    lastRefR[0] = lastRefR[3];
                    lastRefR[2] = (lastRefR[0] + lastRefR[0] + inputSampleR) / 3;
                    lastRefR[1] = (lastRefR[0] + inputSampleR + inputSampleR) / 3;
                    lastRefR[3] = inputSampleR;
                }
                if (cycleEnd == 2)
                {
                    lastRefL[0] = lastRefL[2];
                    lastRefL[1] = (lastRefL[0] + inputSampleL) / 2;
                    lastRefL[2] = inputSampleL;
                    lastRefR[0] = lastRefR[2];
                    lastRefR[1] = (lastRefR[0] + inputSampleR) / 2;
                    lastRefR[2] = inputSampleR;
                }
                if (cycleEnd == 1)
                {
                    lastRefL[0] = inputSampleL;
                    lastRefR[0] = inputSampleR;
                }
                cycle = 0;
                inputSampleL = lastRefL[cycle];
                inputSampleR = lastRefR[cycle];
            }
            else
            {
                inputSampleL = lastRefL[cycle];
                inputSampleR = lastRefR[cycle];
            }

            switch (cycleEnd)
            {
                case 4:
                    lastRefL[8] = inputSampleL; inputSampleL = (inputSampleL + lastRefL[7]) * 0.5;
                    lastRefL[7] = lastRefL[8];
                    lastRefR[8] = inputSampleR; inputSampleR = (inputSampleR + lastRefR[7]) * 0.5;
                    lastRefR[7] = lastRefR[8];
                    [[fallthrough]];
                case 3:
                    lastRefL[8] = inputSampleL; inputSampleL = (inputSampleL + lastRefL[6]) * 0.5;
                    lastRefL[6] = lastRefL[8];
                    lastRefR[8] = inputSampleR; inputSampleR = (inputSampleR + lastRefR[6]) * 0.5;
                    lastRefR[6] = lastRefR[8];
                    [[fallthrough]];
                case 2:
                    lastRefL[8] = inputSampleL; inputSampleL = (inputSampleL + lastRefL[5]) * 0.5;
                    lastRefL[5] = lastRefL[8];
                    lastRefR[8] = inputSampleR; inputSampleR = (inputSampleR + lastRefR[5]) * 0.5;
                    lastRefR[5] = lastRefR[8];
                    [[fallthrough]];
                case 1:
                    break;
            }

            if (wet < 1.0)
            {
                inputSampleL *= wet;
                inputSampleR *= wet;
            }
            if (dry < 1.0)
            {
                drySampleL *= dry;
                drySampleR *= dry;
            }
            inputSampleL += drySampleL;
            inputSampleR += drySampleR;

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

    static constexpr int kRingSize = 88211;

    double sampleRate = 48000.0;
    Parameters params {};

    std::uint32_t fpdL = 22222u;
    std::uint32_t fpdR = 22222u;

    std::vector<double> dL;
    std::vector<double> dR;
    double prevSampleL = 0.0, prevSampleR = 0.0;
    double delayL = 0.0, delayR = 0.0;
    double sweepL = 0.0, sweepR = 0.0;
    double regenFilterL[9] {}, regenFilterR[9] {};
    double outFilterL[9] {}, outFilterR[9] {};
    double lastRefL[10] {}, lastRefR[10] {};
    int cycle = 0;

    double bypassMix = 0.0;

    float inPeakL = 0.0f, inPeakR = 0.0f;
    float outPeakL = 0.0f, outPeakR = 0.0f;
    bool inClip = false, outClip = false;
};

} // namespace forja::dsp
