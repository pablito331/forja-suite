#pragma once

// Forja adapter for Airwindows Echojam (MIT, Chris Johnson).
// Reference: Forja/Source/forja/vendor/airwindows/Echojam, commit
// b110fac3991fff7c4655cc2d25d6a3e221f9799c. processDoubleReplacing is
// transcribed literally: the 16-tap feedback network per channel with the
// Hadamard-style sums, the 17 hard-coded spaces selected by the first
// parameter, the Bezier reconstruction between undersampled reverb samples and
// the dry/wet mix.
//
// Deviations: Forja adds metering, a 10 ms bypass crossfade and an optional
// host-tempo sync that aligns the longest wall of the selected space to the
// chosen division (sync off is the original speed path). The original's
// commented-out 64-bit dither stays commented; only the RNG advances (including
// the asymmetric extra step on the right channel). Delay lines live on the
// heap (std::vector) because the type is also created on the stack by the test
// harness. No allocation, logging or locks inside process().

#include "forja/dsp/ForjaDspCommon.h"
#include "forja/dsp/ForjaSync.h"

#include <cmath>
#include <cstdint>
#include <vector>

namespace forja::dsp
{

class EcoDSP
{
public:
    struct Parameters
    {
        float select = 0.5f;
        float regen = 0.5f;
        float speed = 0.5f;
        float dryWet = 0.5f;
        bool sync = false;
        float division = 0.5f;
        double hostBpm = 0.0;
        bool bypass = false;
    };

    EcoDSP()
        : lA (d4A + 5, 0.0), lB (d4B + 5, 0.0), lC (d4C + 5, 0.0), lD (d4D + 5, 0.0),
          lE (d4E + 5, 0.0), lF (d4F + 5, 0.0), lG (d4G + 5, 0.0), lH (d4H + 5, 0.0),
          lI (d4I + 5, 0.0), lJ (d4J + 5, 0.0), lK (d4K + 5, 0.0), lL (d4L + 5, 0.0),
          lM (d4M + 5, 0.0), lN (d4N + 5, 0.0), lO (d4O + 5, 0.0), lP (d4P + 5, 0.0),
          rA (d4A + 5, 0.0), rB (d4B + 5, 0.0), rC (d4C + 5, 0.0), rD (d4D + 5, 0.0),
          rE (d4E + 5, 0.0), rF (d4F + 5, 0.0), rG (d4G + 5, 0.0), rH (d4H + 5, 0.0),
          rI (d4I + 5, 0.0), rJ (d4J + 5, 0.0), rK (d4K + 5, 0.0), rL (d4L + 5, 0.0),
          rM (d4M + 5, 0.0), rN (d4N + 5, 0.0), rO (d4O + 5, 0.0), rP (d4P + 5, 0.0)
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
        for (auto* line : { &lA, &lB, &lC, &lD, &lE, &lF, &lG, &lH,
                            &lI, &lJ, &lK, &lL, &lM, &lN, &lO, &lP,
                            &rA, &rB, &rC, &rD, &rE, &rF, &rG, &rH,
                            &rI, &rJ, &rK, &rL, &rM, &rN, &rO, &rP })
        {
            for (double& value : *line)
                value = 0.0;
        }

        cLA = cLB = cLC = cLD = cLE = cLF = cLG = cLH = 1;
        cLI = cLJ = cLK = cLL = cLM = cLN = cLO = cLP = 1;
        cRA = cRB = cRC = cRD = cRE = cRF = cRG = cRH = 1;
        cRI = cRJ = cRK = cRL = cRM = cRN = cRO = cRP = 1;

        fAL = fBL = fCL = fDL = 0.0;
        hAL = hBL = hCL = hDL = hEL = 0.0;
        fDR = fHR = fLR = fPR = 0.0;
        hAR = hBR = hCR = hDR = hER = 0.0;

        for (double& value : bez)
            value = 0.0;
        bez[bez_cycle] = 1.0;

        shortA = 17; shortB = 19; shortC = 23; shortD = 29; shortE = 19; shortF = 106;
        shortG = 41; shortH = 6361; shortI = 47; shortJ = 53; shortK = 59; shortL = 61;
        shortM = 67; shortN = 5824; shortO = 6257; shortP = 653;

        prevclearcoat = -1;

        fpdL = 22222u;
        fpdR = 22222u;

        bypassMix = params.bypass ? 1.0 : 0.0;
        clearMeters();
    }

    void setParameters (const Parameters& newParams) noexcept
    {
        params.select = clamp01 (newParams.select);
        params.regen = clamp01 (newParams.regen);
        params.speed = clamp01 (newParams.speed);
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

    // The spaces and the Bezier reconstruction are musical, never PDC.
    int latencySamples() const noexcept { return 0; }

    // Test hook: keeps the parity harness deterministic.
    void setNoiseState (std::uint32_t newFpdL, std::uint32_t newFpdR) noexcept
    {
        fpdL = newFpdL;
        fpdR = newFpdR;
    }

    static int spaceCount() noexcept { return 17; }

    // Longest wall (in 44.1 kHz samples) of each of the 17 spaces. Used by the
    // host-tempo mapping: sync aligns that longest loop to the chosen division.
    static int maxWallFor (int clearcoat) noexcept
    {
        static constexpr int walls[17] = {
            6361, 7450, 6822, 7230, 6854, 6998, 6852, 6906, 7069,
            7355, 7203, 7187, 7253, 7221, 7443, 6324, 6819
        };
        return walls[clearcoat < 0 ? 0 : (clearcoat > 16 ? 16 : clearcoat)];
    }

    // `derez` before the original's clamp/quantization. With sync off this is
    // exactly the original pow(speed, sampleRate / 44100).
    static double resolveDerez (float speed, bool sync, float division, double hostBpm,
                                double sampleRate, int clearcoat) noexcept
    {
        if (sync)
        {
            const double seconds = syncDivisionSeconds (division, hostBpm);
            if (seconds > 0.0 && sampleRate > 0.0)
                return std::clamp ((double) maxWallFor (clearcoat) / (sampleRate * seconds),
                                   0.0001, 1.0);
        }

        return std::pow ((double) speed, sampleRate / 44100.0);
    }

    // Resolved delay of the longest loop, shared with the editor readout.
    static double resolveDelaySeconds (float speed, bool sync, float division, double hostBpm,
                                       double sampleRate, int clearcoat) noexcept
    {
        double derez = resolveDerez (speed, sync, division, hostBpm, sampleRate, clearcoat);
        derez = std::fmin (std::fmax (derez, 0.0001), 1.0);
        const int bezFraction = (int) (1.0 / derez);
        derez = 1.0 / (double) bezFraction;
        return (double) maxWallFor (clearcoat) / (derez * sampleRate);
    }

private:
    enum
    {
        bez_AL,
        bez_AR,
        bez_BL,
        bez_BR,
        bez_CL,
        bez_CR,
        bez_SampL,
        bez_SampR,
        bez_cycle,
        bez_total
    };

    static constexpr int d4A = 6654;
    static constexpr int d4B = 7450;
    static constexpr int d4C = 6998;
    static constexpr int d4D = 6650;
    static constexpr int d4E = 7136;
    static constexpr int d4F = 7198;
    static constexpr int d4G = 7253;
    static constexpr int d4H = 7221;
    static constexpr int d4I = 7355;
    static constexpr int d4J = 4838;
    static constexpr int d4K = 7069;
    static constexpr int d4L = 7175;
    static constexpr int d4M = 7307;
    static constexpr int d4N = 6819;
    static constexpr int d4O = 7104;
    static constexpr int d4P = 7443;

    void selectSpace (int clearcoat) noexcept
    {
        switch (clearcoat)
        {
            case 0:
                shortA = 17; shortB = 19; shortC = 23; shortD = 29; shortE = 19; shortF = 106; shortG = 41; shortH = 6361; shortI = 47; shortJ = 53; shortK = 59; shortL = 61; shortM = 67; shortN = 5824; shortO = 6257; shortP = 653; break;
            case 1:
                shortA = 17; shortB = 7450; shortC = 1692; shortD = 29; shortE = 64; shortF = 37; shortG = 41; shortH = 43; shortI = 47; shortJ = 4838; shortK = 776; shortL = 61; shortM = 67; shortN = 6307; shortO = 1158; shortP = 79; break;
            case 2:
                shortA = 17; shortB = 19; shortC = 23; shortD = 29; shortE = 6111; shortF = 6237; shortG = 41; shortH = 5973; shortI = 47; shortJ = 53; shortK = 59; shortL = 768; shortM = 67; shortN = 71; shortO = 73; shortP = 6822; break;
            case 3:
                shortA = 17; shortB = 19; shortC = 23; shortD = 29; shortE = 5841; shortF = 37; shortG = 41; shortH = 43; shortI = 794; shortJ = 53; shortK = 59; shortL = 6160; shortM = 7230; shortN = 71; shortO = 73; shortP = 6473; break;
            case 4:
                shortA = 17; shortB = 3; shortC = 6854; shortD = 6650; shortE = 31; shortF = 6854; shortG = 41; shortH = 765; shortI = 47; shortJ = 53; shortK = 59; shortL = 61; shortM = 67; shortN = 71; shortO = 73; shortP = 809; break;
            case 5:
                shortA = 756; shortB = 19; shortC = 6998; shortD = 29; shortE = 31; shortF = 37; shortG = 6808; shortH = 43; shortI = 47; shortJ = 53; shortK = 59; shortL = 61; shortM = 6978; shortN = 71; shortO = 269; shortP = 79; break;
            case 6:
                shortA = 5824; shortB = 652; shortC = 739; shortD = 6086; shortE = 31; shortF = 37; shortG = 41; shortH = 6852; shortI = 47; shortJ = 75; shortK = 59; shortL = 61; shortM = 67; shortN = 71; shortO = 73; shortP = 6589; break;
            case 7:
                shortA = 17; shortB = 19; shortC = 6906; shortD = 29; shortE = 6361; shortF = 37; shortG = 6098; shortH = 43; shortI = 47; shortJ = 53; shortK = 59; shortL = 765; shortM = 67; shortN = 71; shortO = 6133; shortP = 79; break;
            case 8:
                shortA = 6617; shortB = 19; shortC = 315; shortD = 29; shortE = 31; shortF = 5982; shortG = 41; shortH = 676; shortI = 47; shortJ = 53; shortK = 7069; shortL = 61; shortM = 67; shortN = 71; shortO = 73; shortP = 79; break;
            case 9:
                shortA = 17; shortB = 19; shortC = 6171; shortD = 29; shortE = 31; shortF = 37; shortG = 41; shortH = 43; shortI = 7355; shortJ = 53; shortK = 906; shortL = 61; shortM = 7307; shortN = 71; shortO = 1911; shortP = 79; break;
            case 10:
                shortA = 744; shortB = 649; shortC = 433; shortD = 29; shortE = 31; shortF = 37; shortG = 41; shortH = 6974; shortI = 47; shortJ = 53; shortK = 59; shortL = 61; shortM = 7203; shortN = 71; shortO = 6478; shortP = 79; break;
            case 11:
                shortA = 848; shortB = 19; shortC = 23; shortD = 29; shortE = 7136; shortF = 7187; shortG = 41; shortH = 43; shortI = 726; shortJ = 53; shortK = 59; shortL = 7175; shortM = 67; shortN = 71; shortO = 73; shortP = 79; break;
            case 12:
                shortA = 17; shortB = 19; shortC = 23; shortD = 6428; shortE = 31; shortF = 7198; shortG = 7253; shortH = 7087; shortI = 47; shortJ = 53; shortK = 59; shortL = 878; shortM = 67; shortN = 71; shortO = 13; shortP = 79; break;
            case 13:
                shortA = 962; shortB = 19; shortC = 23; shortD = 29; shortE = 31; shortF = 37; shortG = 41; shortH = 7221; shortI = 47; shortJ = 53; shortK = 59; shortL = 6437; shortM = 67; shortN = 71; shortO = 7104; shortP = 79; break;
            case 14:
                shortA = 17; shortB = 19; shortC = 4995; shortD = 29; shortE = 20; shortF = 37; shortG = 41; shortH = 5153; shortI = 47; shortJ = 53; shortK = 5325; shortL = 61; shortM = 67; shortN = 71; shortO = 958; shortP = 7443; break;
            case 15:
                shortA = 17; shortB = 19; shortC = 23; shortD = 5994; shortE = 5965; shortF = 37; shortG = 41; shortH = 6324; shortI = 47; shortJ = 664; shortK = 59; shortL = 61; shortM = 5693; shortN = 71; shortO = 73; shortP = 79; break;
            case 16:
            default:
                shortA = 6654; shortB = 19; shortC = 23; shortD = 29; shortE = 31; shortF = 37; shortG = 41; shortH = 43; shortI = 47; shortJ = 21; shortK = 59; shortL = 740; shortM = 67; shortN = 6819; shortO = 6662; shortP = 79; break;
        }
    }

    template <typename Sample>
    void processSamples (Sample* left, Sample* right, int numSamples) noexcept
    {
        const double sampleRateNow = sampleRate;

        const int clearcoat = (int) ((double) params.select * 16.999);
        if (clearcoat != prevclearcoat)
        {
            for (auto* line : { &lA, &lB, &lC, &lD, &lE, &lF, &lG, &lH,
                                &lI, &lJ, &lK, &lL, &lM, &lN, &lO, &lP,
                                &rA, &rB, &rC, &rD, &rE, &rF, &rG, &rH,
                                &rI, &rJ, &rK, &rL, &rM, &rN, &rO, &rP })
            {
                for (double& value : *line)
                    value = 0.0;
            }

            cLA = cLB = cLC = cLD = cLE = cLF = cLG = cLH = 1;
            cLI = cLJ = cLK = cLL = cLM = cLN = cLO = cLP = 1;
            cRA = cRB = cRC = cRD = cRE = cRF = cRG = cRH = 1;
            cRI = cRJ = cRK = cRL = cRM = cRN = cRO = cRP = 1;

            selectSpace (clearcoat);
            prevclearcoat = clearcoat;
        }

        const double regen = (1.0 - std::pow (1.0 - (double) params.regen, 2.0)) * 0.0625f;
        double derez = resolveDerez (params.speed, params.sync, params.division,
                                     params.hostBpm, sampleRateNow, clearcoat);
        derez = std::fmin (std::fmax (derez, 0.0001), 1.0);
        const int bezFraction = (int) (1.0 / derez);
        double bezTrim = (double) bezFraction / (bezFraction + 1.0);
        derez = 1.0 / bezFraction;
        bezTrim = 1.0 - (derez * bezTrim);
        const double wet = 1.0 - std::pow (1.0 - (double) params.dryWet, 2.0);

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
            const double drySampleL = inputSampleL;
            const double drySampleR = inputSampleR;

            bez[bez_cycle] += derez;
            bez[bez_SampL] += inputSampleL * derez;
            bez[bez_SampR] += inputSampleR * derez;
            if (bez[bez_cycle] > 1.0)
            {
                bez[bez_cycle] = 0.0;
                inputSampleL = bez[bez_SampL];
                inputSampleR = bez[bez_SampR];

                lA[(std::size_t) cLA] = fAL * regen + inputSampleL;
                lB[(std::size_t) cLB] = fBL * regen + inputSampleL;
                lC[(std::size_t) cLC] = fCL * regen + inputSampleL;
                lD[(std::size_t) cLD] = fDL * regen + inputSampleL;
                rD[(std::size_t) cRD] = fDR * regen + inputSampleR;
                rH[(std::size_t) cRH] = fHR * regen + inputSampleR;
                rL[(std::size_t) cRL] = fLR * regen + inputSampleR;
                rP[(std::size_t) cRP] = fPR * regen + inputSampleR;
                cLA++; cLB++; cLC++; cLD++;
                cRD++; cRH++; cRL++; cRP++;
                if (cLA > shortA) cLA = 0;
                if (cLB > shortB) cLB = 0;
                if (cLC > shortC) cLC = 0;
                if (cLD > shortD) cLD = 0;
                if (cRD > shortD) cRD = 0;
                if (cRH > shortH) cRH = 0;
                if (cRL > shortL) cRL = 0;
                if (cRP > shortP) cRP = 0;
                hAL = lA[(std::size_t) cLA]; hBL = lB[(std::size_t) cLB];
                hCL = lC[(std::size_t) cLC]; hDL = lD[(std::size_t) cLD];
                hAR = rD[(std::size_t) cRD]; hBR = rH[(std::size_t) cRH];
                hCR = rL[(std::size_t) cRL]; hDR = rP[(std::size_t) cRP];
                hEL = -(hAL + hBL + hCL + hDL);
                hER = -(hAR + hBR + hCR + hDR);
                lE[(std::size_t) cLE] = hAL * 2.0f + hEL;
                lF[(std::size_t) cLF] = hBL * 2.0f + hEL;
                lG[(std::size_t) cLG] = hCL * 2.0f + hEL;
                lH[(std::size_t) cLH] = hDL * 2.0f + hEL;
                rC[(std::size_t) cRC] = hAR * 2.0f + hER;
                rG[(std::size_t) cRG] = hBR * 2.0f + hER;
                rK[(std::size_t) cRK] = hCR * 2.0f + hER;
                rO[(std::size_t) cRO] = hDR * 2.0f + hER;
                cLE++; cLF++; cLG++; cLH++;
                cRC++; cRG++; cRK++; cRO++;
                if (cLE > shortE) cLE = 0;
                if (cLF > shortF) cLF = 0;
                if (cLG > shortG) cLG = 0;
                if (cLH > shortH) cLH = 0;
                if (cRC > shortC) cRC = 0;
                if (cRG > shortG) cRG = 0;
                if (cRK > shortK) cRK = 0;
                if (cRO > shortO) cRO = 0;
                hAL = lE[(std::size_t) cLE]; hBL = lF[(std::size_t) cLF];
                hCL = lG[(std::size_t) cLG]; hDL = lH[(std::size_t) cLH];
                hAR = rC[(std::size_t) cRC]; hBR = rG[(std::size_t) cRG];
                hCR = rK[(std::size_t) cRK]; hDR = rO[(std::size_t) cRO];
                hEL = -(hAL + hBL + hCL + hDL);
                hER = -(hAR + hBR + hCR + hDR);
                lI[(std::size_t) cLI] = hAL * 2.0f + hEL;
                lJ[(std::size_t) cLJ] = hBL * 2.0f + hEL;
                lK[(std::size_t) cLK] = hCL * 2.0f + hEL;
                lL[(std::size_t) cLL] = hDL * 2.0f + hEL;
                rB[(std::size_t) cRB] = hAR * 2.0f + hER;
                rF[(std::size_t) cRF] = hBR * 2.0f + hER;
                rJ[(std::size_t) cRJ] = hCR * 2.0f + hER;
                rN[(std::size_t) cRN] = hDR * 2.0f + hER;
                cLI++; cLJ++; cLK++; cLL++;
                cRB++; cRF++; cRJ++; cRN++;
                if (cLI > shortI) cLI = 0;
                if (cLJ > shortJ) cLJ = 0;
                if (cLK > shortK) cLK = 0;
                if (cLL > shortL) cLL = 0;
                if (cRB > shortB) cRB = 0;
                if (cRF > shortF) cRF = 0;
                if (cRJ > shortJ) cRJ = 0;
                if (cRN > shortN) cRN = 0;
                hAL = lI[(std::size_t) cLI]; hBL = lJ[(std::size_t) cLJ];
                hCL = lK[(std::size_t) cLK]; hDL = lL[(std::size_t) cLL];
                hAR = rB[(std::size_t) cRB]; hBR = rF[(std::size_t) cRF];
                hCR = rJ[(std::size_t) cRJ]; hDR = rN[(std::size_t) cRN];
                hEL = -(hAL + hBL + hCL + hDL);
                hER = -(hAR + hBR + hCR + hDR);
                lM[(std::size_t) cLM] = hAL * 2.0f + hEL;
                lN[(std::size_t) cLN] = hBL * 2.0f + hEL;
                lO[(std::size_t) cLO] = hCL * 2.0f + hEL;
                lP[(std::size_t) cLP] = hDL * 2.0f + hEL;
                rA[(std::size_t) cRA] = hAR * 2.0f + hER;
                rE[(std::size_t) cRE] = hBR * 2.0f + hER;
                rI[(std::size_t) cRI] = hCR * 2.0f + hER;
                rM[(std::size_t) cRM] = hDR * 2.0f + hER;
                cLM++; cLN++; cLO++; cLP++;
                cRA++; cRE++; cRI++; cRM++;
                if (cLM > shortM) cLM = 0;
                if (cLN > shortN) cLN = 0;
                if (cLO > shortO) cLO = 0;
                if (cLP > shortP) cLP = 0;
                if (cRA > shortA) cRA = 0;
                if (cRE > shortE) cRE = 0;
                if (cRI > shortI) cRI = 0;
                if (cRM > shortM) cRM = 0;
                hAL = lM[(std::size_t) cLM]; hBL = lN[(std::size_t) cLN];
                hCL = lO[(std::size_t) cLO]; hDL = lP[(std::size_t) cLP];
                hAR = rA[(std::size_t) cRA]; hBR = rE[(std::size_t) cRE];
                hCR = rI[(std::size_t) cRI]; hDR = rM[(std::size_t) cRM];
                hEL = -(hAL + hBL + hCL + hDL);
                hER = -(hAR + hBR + hCR + hDR);
                fAL = hAR * 2.0f + hER;
                fBL = hBR * 2.0f + hER;
                fCL = hCR * 2.0f + hER;
                fDL = hDR * 2.0f + hER;
                fDR = hAL * 2.0f + hEL;
                fHR = hBL * 2.0f + hEL;
                fLR = hCL * 2.0f + hEL;
                fPR = hDL * 2.0f + hEL;
                inputSampleL = (hAL + hBL + hCL + hDL) * 0.0625f;
                inputSampleR = (hAR + hBR + hCR + hDR) * 0.0625f;

                bez[bez_CL] = bez[bez_BL];
                bez[bez_BL] = bez[bez_AL];
                bez[bez_AL] = inputSampleL;
                bez[bez_SampL] = 0.0;

                bez[bez_CR] = bez[bez_BR];
                bez[bez_BR] = bez[bez_AR];
                bez[bez_AR] = inputSampleR;
                bez[bez_SampR] = 0.0;
            }
            const double X = bez[bez_cycle] * bezTrim;
            const double CBL = (bez[bez_CL] * (1.0 - X)) + (bez[bez_BL] * X);
            const double CBR = (bez[bez_CR] * (1.0 - X)) + (bez[bez_BR] * X);
            const double BAL = (bez[bez_BL] * (1.0 - X)) + (bez[bez_AL] * X);
            const double BAR = (bez[bez_BR] * (1.0 - X)) + (bez[bez_AR] * X);
            inputSampleL = (bez[bez_BL] + (CBL * (1.0 - X)) + (BAL * X)) * 0.125;
            inputSampleR = (bez[bez_BR] + (CBR * (1.0 - X)) + (BAR * X)) * 0.125;

            inputSampleL = (inputSampleL * wet) + (drySampleL * (1.0 - wet));
            inputSampleR = (inputSampleR * wet) + (drySampleR * (1.0 - wet));

            fpdL ^= fpdL << 13; fpdL ^= fpdL >> 17; fpdL ^= fpdL << 5;
            fpdR ^= fpdR << 13; fpdR ^= fpdR >> 17; fpdR ^= fpdR << 5;
            if ((std::uint32_t) (fpdL - fpdR) < 1073741824u
                || (std::uint32_t) (fpdR - fpdL) < 1073741824u)
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

    std::vector<double> lA, lB, lC, lD, lE, lF, lG, lH;
    std::vector<double> lI, lJ, lK, lL, lM, lN, lO, lP;
    std::vector<double> rA, rB, rC, rD, rE, rF, rG, rH;
    std::vector<double> rI, rJ, rK, rL, rM, rN, rO, rP;

    int cLA = 1, cLB = 1, cLC = 1, cLD = 1, cLE = 1, cLF = 1, cLG = 1, cLH = 1;
    int cLI = 1, cLJ = 1, cLK = 1, cLL = 1, cLM = 1, cLN = 1, cLO = 1, cLP = 1;
    int cRA = 1, cRB = 1, cRC = 1, cRD = 1, cRE = 1, cRF = 1, cRG = 1, cRH = 1;
    int cRI = 1, cRJ = 1, cRK = 1, cRL = 1, cRM = 1, cRN = 1, cRO = 1, cRP = 1;

    double fAL = 0.0, fBL = 0.0, fCL = 0.0, fDL = 0.0;
    double hAL = 0.0, hBL = 0.0, hCL = 0.0, hDL = 0.0, hEL = 0.0;
    double fDR = 0.0, fHR = 0.0, fLR = 0.0, fPR = 0.0;
    double hAR = 0.0, hBR = 0.0, hCR = 0.0, hDR = 0.0, hER = 0.0;

    double bez[bez_total] {};

    int shortA = 17, shortB = 19, shortC = 23, shortD = 29, shortE = 19, shortF = 106;
    int shortG = 41, shortH = 6361, shortI = 47, shortJ = 53, shortK = 59, shortL = 61;
    int shortM = 67, shortN = 5824, shortO = 6257, shortP = 653;

    int prevclearcoat = -1;

    double bypassMix = 0.0;

    float inPeakL = 0.0f, inPeakR = 0.0f;
    float outPeakL = 0.0f, outPeakR = 0.0f;
    bool inClip = false, outClip = false;
};

} // namespace forja::dsp
