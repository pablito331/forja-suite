#pragma once

// Forja adapter for Airwindows ClearCoat (MIT, Chris Johnson).
// Reference: Forja/Source/forja/vendor/airwindows/ClearCoat, master branch.
// processDoubleReplacing is transcribed literally: the 16-line parallel comb
// network with 4-stage Householder matrix mixing, the SubTight nonlinear
// distortion, and the upsampled interpolation (cycle 1-4). 17 room presets.
//
// Deviations: Forja adds metering and a 10 ms bypass crossfade. The original's
// commented-out 64-bit dither stays commented; only the RNG advances. The
// 32 delay-line buffers live on the heap (std::vector) because the type is
// also created on the stack by the test harness. No allocation inside process().

#include "forja/dsp/ForjaDspCommon.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace forja::dsp::clearcoat
{

// Maximum buffer sizes from the original header.
inline constexpr int kshortA = 350;
inline constexpr int kshortB = 1710;
inline constexpr int kshortC = 1610;
inline constexpr int kshortD = 835;
inline constexpr int kshortE = 700;
inline constexpr int kshortF = 1260;
inline constexpr int kshortG = 1110;
inline constexpr int kshortH = 1768;
inline constexpr int kshortI = 280;
inline constexpr int kshortJ = 2645;
inline constexpr int kshortK = 1410;
inline constexpr int kshortL = 1175;
inline constexpr int kshortM = 12;
inline constexpr int kshortN = 3110;
inline constexpr int kshortO = 120;
inline constexpr int kshortP = 2370;

class ClearCoatDSP
{
public:
    struct Parameters
    {
        float select = 0.5f;   // room index 0..16
        float dryWet = 1.0f;   // wet level
        bool bypass = false;
    };

    ClearCoatDSP() { reset(); }

    void prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
        allocateBuffers();
        reset();
    }

    void setParameters (const Parameters& p) noexcept
    {
        params.select = clamp01 (p.select);
        params.dryWet = clamp01 (p.dryWet);
        params.bypass = p.bypass;
    }

    void process (float* left, float* right, int numSamples) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
        {
            double l = static_cast<double> (left[i]);
            double r = static_cast<double> (right[i]);
            processSample (l, r);
            left[i] = static_cast<float> (l);
            right[i] = static_cast<float> (r);
        }
    }

    void processDouble (double* left, double* right, int numSamples) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
            processSample (left[i], right[i]);
    }

    MeterFrame readMeters (bool resetAfterRead = true) noexcept
    {
        MeterFrame frame;
        frame.inPeakL = inPeakL; frame.inPeakR = inPeakR;
        frame.outPeakL = outPeakL; frame.outPeakR = outPeakR;
        frame.gainReductionDb = 0.0f;
        frame.inClip = inClip; frame.outClip = outClip;
        if (resetAfterRead) clearMeters();
        return frame;
    }

    int latencySamples() const noexcept { return 0; }

private:
    void allocateBuffers()
    {
        aAL.resize(kshortA + 5, 0.0); aAR.resize(kshortA + 5, 0.0);
        aBL.resize(kshortB + 5, 0.0); aBR.resize(kshortB + 5, 0.0);
        aCL.resize(kshortC + 5, 0.0); aCR.resize(kshortC + 5, 0.0);
        aDL.resize(kshortD + 5, 0.0); aDR.resize(kshortD + 5, 0.0);
        aEL.resize(kshortE + 5, 0.0); aER.resize(kshortE + 5, 0.0);
        aFL.resize(kshortF + 5, 0.0); aFR.resize(kshortF + 5, 0.0);
        aGL.resize(kshortG + 5, 0.0); aGR.resize(kshortG + 5, 0.0);
        aHL.resize(kshortH + 5, 0.0); aHR.resize(kshortH + 5, 0.0);
        aIL.resize(kshortI + 5, 0.0); aIR.resize(kshortI + 5, 0.0);
        aJL.resize(kshortJ + 5, 0.0); aJR.resize(kshortJ + 5, 0.0);
        aKL.resize(kshortK + 5, 0.0); aKR.resize(kshortK + 5, 0.0);
        aLL.resize(kshortL + 5, 0.0); aLR.resize(kshortL + 5, 0.0);
        aML.resize(kshortM + 5, 0.0); aMR.resize(kshortM + 5, 0.0);
        aNL.resize(kshortN + 5, 0.0); aNR.resize(kshortN + 5, 0.0);
        aOL.resize(kshortO + 5, 0.0); aOR.resize(kshortO + 5, 0.0);
        aPL.resize(kshortP + 5, 0.0); aPR.resize(kshortP + 5, 0.0);
    }

    void reset() noexcept
    {
        for (auto& v : {&aAL,&aAR,&aBL,&aBR,&aCL,&aCR,&aDL,&aDR,
                        &aEL,&aER,&aFL,&aFR,&aGL,&aGR,&aHL,&aHR,
                        &aIL,&aIR,&aJL,&aJR,&aKL,&aKR,&aLL,&aLR,
                        &aML,&aMR,&aNL,&aNR,&aOL,&aOR,&aPL,&aPR})
            std::fill(v->begin(), v->end(), 0.0);

        feedbackAL = feedbackBL = feedbackCL = feedbackDL = 0.0;
        feedbackDR = feedbackHR = feedbackLR = feedbackPR = 0.0;
        prevMulchAL = prevMulchAR = 0.0;
        tailL = tailR = 0.0;
        for (int i = 0; i < 7; ++i) { lastRefL[i] = 0.0; lastRefR[i] = 0.0; }
        subAL = subAR = subBL = subBR = subCL = subCR = subDL = subDR = 0.0;

        countAL = countBL = countCL = countDL = 1;
        countEL = countFL = countGL = countHL = 1;
        countIL = countJL = countKL = countLL = 1;
        countML = countNL = countOL = countPL = 1;
        countAR = countBR = countCR = countDR = 1;
        countER = countFR = countGR = countHR = 1;
        countIR = countJR = countKR = countLR = 1;
        countMR = countNR = countOR = countPR = 1;

        cycle = 0;
        prevclearcoat = -1;
        fpdL = 22222u; fpdR = 22222u;
        bypassMix = params.bypass ? 1.0 : 0.0;
        clearMeters();
    }

    void setRoom (int clearcoat)
    {
        switch (clearcoat)
        {
            case 0:
                shortA=65; shortB=124; shortC=83; shortD=180; shortE=200; shortF=291; shortG=108; shortH=189; shortI=73; shortJ=410; shortK=479; shortL=310; shortM=11; shortN=928; shortO=23; shortP=654; break;
            case 1:
                shortA=114; shortB=205; shortC=498; shortD=195; shortE=205; shortF=318; shortG=143; shortH=254; shortI=64; shortJ=721; shortK=512; shortL=324; shortM=11; shortN=782; shortO=26; shortP=394; break;
            case 2:
                shortA=118; shortB=272; shortC=292; shortD=145; shortE=200; shortF=241; shortG=204; shortH=504; shortI=50; shortJ=678; shortK=424; shortL=412; shortM=11; shortN=1124; shortO=47; shortP=766; break;
            case 3:
                shortA=19; shortB=474; shortC=301; shortD=275; shortE=260; shortF=321; shortG=371; shortH=571; shortI=50; shortJ=410; shortK=697; shortL=414; shortM=11; shortN=986; shortO=47; shortP=522; break;
            case 4:
                shortA=112; shortB=387; shortC=452; shortD=289; shortE=173; shortF=476; shortG=321; shortH=593; shortI=73; shortJ=343; shortK=829; shortL=91; shortM=11; shortN=1055; shortO=43; shortP=862; break;
            case 5:
                shortA=60; shortB=368; shortC=295; shortD=272; shortE=210; shortF=284; shortG=326; shortH=830; shortI=125; shortJ=236; shortK=737; shortL=486; shortM=11; shortN=1178; shortO=75; shortP=902; break;
            case 6:
                shortA=73; shortB=311; shortC=472; shortD=251; shortE=134; shortF=509; shortG=393; shortH=591; shortI=124; shortJ=1070; shortK=340; shortL=525; shortM=11; shortN=1367; shortO=75; shortP=816; break;
            case 7:
                shortA=159; shortB=518; shortC=514; shortD=165; shortE=275; shortF=494; shortG=296; shortH=667; shortI=75; shortJ=1101; shortK=116; shortL=414; shortM=11; shortN=1261; shortO=79; shortP=998; break;
            case 8:
                shortA=41; shortB=741; shortC=274; shortD=59; shortE=306; shortF=332; shortG=291; shortH=767; shortI=42; shortJ=881; shortK=959; shortL=422; shortM=11; shortN=1237; shortO=45; shortP=958; break;
            case 9:
                shortA=251; shortB=437; shortC=783; shortD=189; shortE=130; shortF=272; shortG=244; shortH=761; shortI=128; shortJ=1190; shortK=320; shortL=491; shortM=11; shortN=1409; shortO=58; shortP=455; break;
            case 10:
                shortA=316; shortB=510; shortC=1087; shortD=349; shortE=359; shortF=74; shortG=79; shortH=1269; shortI=34; shortJ=693; shortK=749; shortL=511; shortM=11; shortN=1751; shortO=93; shortP=403; break;
            case 11:
                shortA=254; shortB=651; shortC=845; shortD=316; shortE=373; shortF=267; shortG=182; shortH=857; shortI=215; shortJ=1535; shortK=1127; shortL=315; shortM=11; shortN=1649; shortO=97; shortP=829; break;
            case 12:
                shortA=113; shortB=101; shortC=673; shortD=357; shortE=340; shortF=229; shortG=278; shortH=1008; shortI=265; shortJ=1890; shortK=155; shortL=267; shortM=11; shortN=2233; shortO=116; shortP=600; break;
            case 13:
                shortA=218; shortB=1058; shortC=862; shortD=505; shortE=297; shortF=580; shortG=532; shortH=1387; shortI=120; shortJ=576; shortK=1409; shortL=473; shortM=11; shortN=1991; shortO=76; shortP=685; break;
            case 14:
                shortA=78; shortB=760; shortC=982; shortD=528; shortE=445; shortF=1128; shortG=130; shortH=708; shortI=22; shortJ=2144; shortK=354; shortL=1169; shortM=11; shortN=2782; shortO=58; shortP=1515; break;
            case 15:
                shortA=330; shortB=107; shortC=1110; shortD=371; shortE=620; shortF=143; shortG=1014; shortH=1763; shortI=184; shortJ=2068; shortK=1406; shortL=595; shortM=11; shortN=2639; shortO=33; shortP=1594; break;
            case 16: default:
                shortA=336; shortB=1660; shortC=386; shortD=623; shortE=693; shortF=1079; shortG=891; shortH=1574; shortI=24; shortJ=2641; shortK=1239; shortL=775; shortM=11; shortN=3104; shortO=55; shortP=2366; break;
        }
    }

    void processSample (double& ioL, double& ioR) noexcept
    {
        const double inputL = ioL;
        const double inputR = ioR;

        meterInput (inputL, inputR);

        double inL = inputL;
        double inR = inputR;
        if (std::fabs(inL) < 1.18e-23) inL = static_cast<double>(fpdL) * 1.18e-17;
        if (std::fabs(inR) < 1.18e-23) inR = static_cast<double>(fpdR) * 1.18e-17;
        double dryL = inL;
        double dryR = inR;

        const double overallscale = sampleRate / 44100.0;
        int cycleEnd = static_cast<int> (std::floor (overallscale));
        if (cycleEnd < 1) cycleEnd = 1;
        if (cycleEnd > 4) cycleEnd = 4;
        if (cycle > cycleEnd - 1) cycle = cycleEnd - 1;

        const int clearcoat = static_cast<int> (params.select * 16.999);

        if (clearcoat != prevclearcoat)
        {
            for (int c = 0; c < kshortA+2; ++c) { aAL[c]=0.0; aAR[c]=0.0; }
            for (int c = 0; c < kshortB+2; ++c) { aBL[c]=0.0; aBR[c]=0.0; }
            for (int c = 0; c < kshortC+2; ++c) { aCL[c]=0.0; aCR[c]=0.0; }
            for (int c = 0; c < kshortD+2; ++c) { aDL[c]=0.0; aDR[c]=0.0; }
            for (int c = 0; c < kshortE+2; ++c) { aEL[c]=0.0; aER[c]=0.0; }
            for (int c = 0; c < kshortF+2; ++c) { aFL[c]=0.0; aFR[c]=0.0; }
            for (int c = 0; c < kshortG+2; ++c) { aGL[c]=0.0; aGR[c]=0.0; }
            for (int c = 0; c < kshortH+2; ++c) { aHL[c]=0.0; aHR[c]=0.0; }
            for (int c = 0; c < kshortI+2; ++c) { aIL[c]=0.0; aIR[c]=0.0; }
            for (int c = 0; c < kshortJ+2; ++c) { aJL[c]=0.0; aJR[c]=0.0; }
            for (int c = 0; c < kshortK+2; ++c) { aKL[c]=0.0; aKR[c]=0.0; }
            for (int c = 0; c < kshortL+2; ++c) { aLL[c]=0.0; aLR[c]=0.0; }
            for (int c = 0; c < kshortM+2; ++c) { aML[c]=0.0; aMR[c]=0.0; }
            for (int c = 0; c < kshortN+2; ++c) { aNL[c]=0.0; aNR[c]=0.0; }
            for (int c = 0; c < kshortO+2; ++c) { aOL[c]=0.0; aOR[c]=0.0; }
            for (int c = 0; c < kshortP+2; ++c) { aPL[c]=0.0; aPR[c]=0.0; }
            countAL=countBL=countCL=countDL=1;
            countEL=countFL=countGL=countHL=1;
            countIL=countJL=countKL=countLL=1;
            countML=countNL=countOL=countPL=1;
            countAR=countBR=countCR=countDR=1;
            countER=countFR=countGR=countHR=1;
            countIR=countJR=countKR=countLR=1;
            countMR=countNR=countOR=countPR=1;
            setRoom (clearcoat);
            prevclearcoat = clearcoat;
        }

        const double subRate = 0.001 / overallscale;
        double wet = params.dryWet * 2.0;
        double dry = 2.0 - wet;
        if (wet > 1.0) wet = 1.0;
        if (wet < 0.0) wet = 0.0;
        if (dry > 1.0) dry = 1.0;
        if (dry < 0.0) dry = 0.0;

        cycle++;
        if (cycle == cycleEnd)
        {
            doReverb (inL, inR);
            cycle = 0;
            inL = lastRefL[cycle];
            inR = lastRefR[cycle];
        }
        else
        {
            inL = lastRefL[cycle];
            inR = lastRefR[cycle];
        }

        // SubTight
        double subSampleL = inL * subRate;
        double subSampleR = inR * subRate;
        double sc;
        sc=0.5+std::fabs(subSampleL*0.5); subSampleL=(subAL+(std::sin(subAL-subSampleL)*sc)); subAL=subSampleL*sc;
        sc=0.5+std::fabs(subSampleR*0.5); subSampleR=(subAR+(std::sin(subAR-subSampleR)*sc)); subAR=subSampleR*sc;
        sc=0.5+std::fabs(subSampleL*0.5); subSampleL=(subBL+(std::sin(subBL-subSampleL)*sc)); subBL=subSampleL*sc;
        sc=0.5+std::fabs(subSampleR*0.5); subSampleR=(subBR+(std::sin(subBR-subSampleR)*sc)); subBR=subSampleR*sc;
        sc=0.5+std::fabs(subSampleL*0.5); subSampleL=(subCL+(std::sin(subCL-subSampleL)*sc)); subCL=subSampleL*sc;
        sc=0.5+std::fabs(subSampleR*0.5); subSampleR=(subCR+(std::sin(subCR-subSampleR)*sc)); subCR=subSampleR*sc;
        sc=0.5+std::fabs(subSampleL*0.5); subSampleL=(subDL+(std::sin(subDL-subSampleL)*sc)); subDL=subSampleL*sc;
        sc=0.5+std::fabs(subSampleR*0.5); subSampleR=(subDR+(std::sin(subDR-subSampleR)*sc)); subDR=subSampleR*sc;
        if (subSampleL > 0.25) subSampleL = 0.25;
        if (subSampleL < -0.25) subSampleL = -0.25;
        if (subSampleR > 0.25) subSampleR = 0.25;
        if (subSampleR < -0.25) subSampleR = -0.25;
        inL -= (subSampleL * 16.0);
        inR -= (subSampleR * 16.0);

        if (cycleEnd > 1)
        {
            double o = (inL + tailL) * 0.5; tailL = inL; inL = o;
            o = (inR + tailR) * 0.5; tailR = inR; inR = o;
        }

        if (wet < 1.0) { inL *= wet; inR *= wet; }
        if (dry < 1.0) { dryL *= dry; dryR *= dry; }
        inL += dryL; inR += dryR;

        // RNG advance (dither commented out, matching original double path)
        fpdL ^= fpdL << 13; fpdL ^= fpdL >> 17; fpdL ^= fpdL << 5;
        fpdR ^= fpdR << 13; fpdR ^= fpdR >> 17; fpdR ^= fpdR << 5;

        // Bypass crossfade
        const double targetMix = params.bypass ? 1.0 : 0.0;
        bypassMix += (targetMix - bypassMix) * bypassRate;
        if (std::fabs(bypassMix - targetMix) < 1e-7) bypassMix = targetMix;
        ioL = inL * (1.0 - bypassMix) + inputL * bypassMix;
        ioR = inR * (1.0 - bypassMix) + inputR * bypassMix;

        meterOutput (ioL, ioR);
    }

    void doReverb (double& inputSampleL, double& inputSampleR) noexcept
    {
        const double fb = 0.04166666666;
        aAL[countAL] = inputSampleL + feedbackAL * fb;
        aBL[countBL] = inputSampleL + feedbackBL * fb;
        aCL[countCL] = inputSampleL + feedbackCL * fb;
        aDL[countDL] = inputSampleL + feedbackDL * fb;
        aDR[countDR] = inputSampleR + feedbackDR * fb;
        aHR[countHR] = inputSampleR + feedbackHR * fb;
        aLR[countLR] = inputSampleR + feedbackLR * fb;
        aPR[countPR] = inputSampleR + feedbackPR * fb;

        countAL++; if (countAL < 0 || countAL > shortA) countAL = 0;
        countBL++; if (countBL < 0 || countBL > shortB) countBL = 0;
        countCL++; if (countCL < 0 || countCL > shortC) countCL = 0;
        countDL++; if (countDL < 0 || countDL > shortD) countDL = 0;
        countDR++; if (countDR < 0 || countDR > shortD) countDR = 0;
        countHR++; if (countHR < 0 || countHR > shortH) countHR = 0;
        countLR++; if (countLR < 0 || countLR > shortL) countLR = 0;
        countPR++; if (countPR < 0 || countPR > shortP) countPR = 0;

        double outAL = aAL[countAL-((countAL>shortA)?shortA+1:0)];
        double outBL = aBL[countBL-((countBL>shortB)?shortB+1:0)];
        double outCL = aCL[countCL-((countCL>shortC)?shortC+1:0)];
        double outDL = aDL[countDL-((countDL>shortD)?shortD+1:0)];
        double outDR = aDR[countDR-((countDR>shortD)?shortD+1:0)];
        double outHR = aHR[countHR-((countHR>shortH)?shortH+1:0)];
        double outLR = aLR[countLR-((countLR>shortL)?shortL+1:0)];
        double outPR = aPR[countPR-((countPR>shortP)?shortP+1:0)];

        aEL[countEL] = outAL-(outBL+outCL+outDL);
        aFL[countFL] = outBL-(outAL+outCL+outDL);
        aGL[countGL] = outCL-(outAL+outBL+outDL);
        aHL[countHL] = outDL-(outAL+outBL+outCL);
        aCR[countCR] = outDR-(outHR+outLR+outPR);
        aGR[countGR] = outHR-(outDR+outLR+outPR);
        aKR[countKR] = outLR-(outDR+outHR+outPR);
        aOR[countOR] = outPR-(outDR+outHR+outLR);

        countEL++; if (countEL<0||countEL>shortE) countEL=0;
        countFL++; if (countFL<0||countFL>shortF) countFL=0;
        countGL++; if (countGL<0||countGL>shortG) countGL=0;
        countHL++; if (countHL<0||countHL>shortH) countHL=0;
        countCR++; if (countCR<0||countCR>shortC) countCR=0;
        countGR++; if (countGR<0||countGR>shortG) countGR=0;
        countKR++; if (countKR<0||countKR>shortK) countKR=0;
        countOR++; if (countOR<0||countOR>shortO) countOR=0;

        double outEL=aEL[countEL-((countEL>shortE)?shortE+1:0)];
        double outFL=aFL[countFL-((countFL>shortF)?shortF+1:0)];
        double outGL=aGL[countGL-((countGL>shortG)?shortG+1:0)];
        double outHL=aHL[countHL-((countHL>shortH)?shortH+1:0)];
        double outCR=aCR[countCR-((countCR>shortC)?shortC+1:0)];
        double outGR=aGR[countGR-((countGR>shortG)?shortG+1:0)];
        double outKR=aKR[countKR-((countKR>shortK)?shortK+1:0)];
        double outOR=aOR[countOR-((countOR>shortO)?shortO+1:0)];

        aIL[countIL]=outEL-(outFL+outGL+outHL);
        aJL[countJL]=outFL-(outEL+outGL+outHL);
        aKL[countKL]=outGL-(outEL+outFL+outHL);
        aLL[countLL]=outHL-(outEL+outFL+outGL);
        aBR[countBR]=outCR-(outGR+outKR+outOR);
        aFR[countFR]=outGR-(outCR+outKR+outOR);
        aJR[countJR]=outKR-(outCR+outGR+outOR);
        aNR[countNR]=outOR-(outCR+outGR+outKR);

        countIL++; if(countIL<0||countIL>shortI) countIL=0;
        countJL++; if(countJL<0||countJL>shortJ) countJL=0;
        countKL++; if(countKL<0||countKL>shortK) countKL=0;
        countLL++; if(countLL<0||countLL>shortL) countLL=0;
        countBR++; if(countBR<0||countBR>shortB) countBR=0;
        countFR++; if(countFR<0||countFR>shortF) countFR=0;
        countJR++; if(countJR<0||countJR>shortJ) countJR=0;
        countNR++; if(countNR<0||countNR>shortN) countNR=0;

        double outIL=aIL[countIL-((countIL>shortI)?shortI+1:0)];
        double outJL=aJL[countJL-((countJL>shortJ)?shortJ+1:0)];
        double outKL=aKL[countKL-((countKL>shortK)?shortK+1:0)];
        double outLL=aLL[countLL-((countLL>shortL)?shortL+1:0)];
        double outBR=aBR[countBR-((countBR>shortB)?shortB+1:0)];
        double outFR=aFR[countFR-((countFR>shortF)?shortF+1:0)];
        double outJR=aJR[countJR-((countJR>shortJ)?shortJ+1:0)];
        double outNR=aNR[countNR-((countNR>shortN)?shortN+1:0)];

        aML[countML]=outIL-(outJL+outKL+outLL);
        aNL[countNL]=outJL-(outIL+outKL+outLL);
        aOL[countOL]=outKL-(outIL+outJL+outLL);
        aPL[countPL]=outLL-(outIL+outJL+outKL);
        aAR[countAR]=outBR-(outFR+outJR+outNR);
        aER[countER]=outFR-(outBR+outJR+outNR);
        aIR[countIR]=outJR-(outBR+outFR+outNR);
        aMR[countMR]=outNR-(outBR+outFR+outJR);

        countML++; if(countML<0||countML>shortM) countML=0;
        countNL++; if(countNL<0||countNL>shortN) countNL=0;
        countOL++; if(countOL<0||countOL>shortO) countOL=0;
        countPL++; if(countPL<0||countPL>shortP) countPL=0;
        countAR++; if(countAR<0||countAR>shortA) countAR=0;
        countER++; if(countER<0||countER>shortE) countER=0;
        countIR++; if(countIR<0||countIR>shortI) countIR=0;
        countMR++; if(countMR<0||countMR>shortM) countMR=0;

        double outML=aML[countML-((countML>shortM)?shortM+1:0)];
        double outNL=aNL[countNL-((countNL>shortN)?shortN+1:0)];
        double outOL=aOL[countOL-((countOL>shortO)?shortO+1:0)];
        double outPL=aPL[countPL-((countPL>shortP)?shortP+1:0)];
        double outAR=aAR[countAR-((countAR>shortA)?shortA+1:0)];
        double outER=aER[countER-((countER>shortE)?shortE+1:0)];
        double outIR=aIR[countIR-((countIR>shortI)?shortI+1:0)];
        double outMR=aMR[countMR-((countMR>shortM)?shortM+1:0)];

        double o=(outML+outML+outML+prevMulchAL)*0.25; prevMulchAL=outML; outML=o;
        o=(outAR+outAR+outAR+prevMulchAR)*0.25; prevMulchAR=outAR; outAR=o;

        feedbackAL=outML-(outNL+outOL+outPL);
        feedbackDR=outAR-(outER+outIR+outMR);
        feedbackBL=outNL-(outML+outOL+outPL);
        feedbackHR=outER-(outAR+outIR+outMR);
        feedbackCL=outOL-(outML+outNL+outPL);
        feedbackLR=outIR-(outAR+outER+outMR);
        feedbackDL=outPL-(outML+outNL+outOL);
        feedbackPR=outMR-(outAR+outER+outIR);

        inputSampleL = (outML+outNL+outOL+outPL)/8.0;
        inputSampleR = (outAR+outER+outIR+outMR)/8.0;

        if (inputSampleL > 1.0) inputSampleL = 1.0;
        if (inputSampleL < -1.0) inputSampleL = -1.0;
        if (inputSampleR > 1.0) inputSampleR = 1.0;
        if (inputSampleR < -1.0) inputSampleR = -1.0;

        // Interpolation
        const double os = sampleRate / 44100.0;
        int cycleEnd = static_cast<int>(std::floor(os));
        if (cycleEnd < 1) cycleEnd = 1;
        if (cycleEnd > 4) cycleEnd = 4;

        if (cycleEnd == 4) {
            lastRefL[0]=lastRefL[4]; lastRefL[2]=(lastRefL[0]+inputSampleL)/2; lastRefL[1]=(lastRefL[0]+lastRefL[2])/2; lastRefL[3]=(lastRefL[2]+inputSampleL)/2; lastRefL[4]=inputSampleL;
            lastRefR[0]=lastRefR[4]; lastRefR[2]=(lastRefR[0]+inputSampleR)/2; lastRefR[1]=(lastRefR[0]+lastRefR[2])/2; lastRefR[3]=(lastRefR[2]+inputSampleR)/2; lastRefR[4]=inputSampleR;
        } else if (cycleEnd == 3) {
            lastRefL[0]=lastRefL[3]; lastRefL[2]=(lastRefL[0]+lastRefL[0]+inputSampleL)/3; lastRefL[1]=(lastRefL[0]+inputSampleL+inputSampleL)/3; lastRefL[3]=inputSampleL;
            lastRefR[0]=lastRefR[3]; lastRefR[2]=(lastRefR[0]+lastRefR[0]+inputSampleR)/3; lastRefR[1]=(lastRefR[0]+inputSampleR+inputSampleR)/3; lastRefR[3]=inputSampleR;
        } else if (cycleEnd == 2) {
            lastRefL[0]=lastRefL[2]; lastRefL[1]=(lastRefL[0]+inputSampleL)/2; lastRefL[2]=inputSampleL;
            lastRefR[0]=lastRefR[2]; lastRefR[1]=(lastRefR[0]+inputSampleR)/2; lastRefR[2]=inputSampleR;
        } else {
            lastRefL[0]=inputSampleL; lastRefR[0]=inputSampleR;
        }
    }

    Parameters params;
    double sampleRate = 44100.0;
    uint32_t fpdL = 22222u, fpdR = 22222u;
    int cycle = 0, prevclearcoat = -1;
    int shortA=336, shortB=1660, shortC=386, shortD=623, shortE=693, shortF=1079, shortG=891, shortH=1574;
    int shortI=24, shortJ=2641, shortK=1239, shortL=775, shortM=11, shortN=3104, shortO=55, shortP=2366;
    int countAL=1,countBL=1,countCL=1,countDL=1,countEL=1,countFL=1,countGL=1,countHL=1;
    int countIL=1,countJL=1,countKL=1,countLL=1,countML=1,countNL=1,countOL=1,countPL=1;
    int countAR=1,countBR=1,countCR=1,countDR=1,countER=1,countFR=1,countGR=1,countHR=1;
    int countIR=1,countJR=1,countKR=1,countLR=1,countMR=1,countNR=1,countOR=1,countPR=1;
    double feedbackAL=0,feedbackBL=0,feedbackCL=0,feedbackDL=0;
    double feedbackDR=0,feedbackHR=0,feedbackLR=0,feedbackPR=0;
    double prevMulchAL=0,prevMulchAR=0,tailL=0,tailR=0;
    double lastRefL[7]={},lastRefR[7]={};
    double subAL=0,subAR=0,subBL=0,subBR=0,subCL=0,subCR=0,subDL=0,subDR=0;
    double bypassMix = 0.0;
    double bypassRate = 1.0 - std::exp(-1.0 / (44100.0 * 0.010));

    std::vector<double> aAL,aAR,aBL,aBR,aCL,aCR,aDL,aDR;
    std::vector<double> aEL,aER,aFL,aFR,aGL,aGR,aHL,aHR;
    std::vector<double> aIL,aIR,aJL,aJR,aKL,aKR,aLL,aLR;
    std::vector<double> aML,aMR,aNL,aNR,aOL,aOR,aPL,aPR;

    float inPeakL=0,inPeakR=0,outPeakL=0,outPeakR=0;
    bool inClip=false,outClip=false;

    void meterInput(double l,double r) noexcept {
        float al=static_cast<float>(std::fabs(l)),ar=static_cast<float>(std::fabs(r));
        if(al>inPeakL){inPeakL=al;inClip=al>=1.0f;} if(ar>inPeakR){inPeakR=ar;inClip=inClip||ar>=1.0f;}
    }
    void meterOutput(double l,double r) noexcept {
        float al=static_cast<float>(std::fabs(l)),ar=static_cast<float>(std::fabs(r));
        if(al>outPeakL){outPeakL=al;outClip=al>=1.0f;} if(ar>outPeakR){outPeakR=ar;outClip=outClip||ar>=1.0f;}
    }
    void clearMeters() noexcept { inPeakL=inPeakR=outPeakL=outPeakR=0.0f; inClip=outClip=false; }
};

} // namespace forja::dsp::clearcoat
