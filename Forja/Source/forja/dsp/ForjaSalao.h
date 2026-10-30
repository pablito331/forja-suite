#pragma once

// Forja adapter for Airwindows kRockstar2 (MIT, Chris Johnson).
// Reference: Forja/Source/forja/vendor/airwindows/kRockstar2, commit
// b110fac3991fff7c4655cc2d25d6a3e221f9799c. processDoubleReplacing is
// transcribed literally: the nine-line early-reflection network, the 36-line
// feedback reverb per channel, the room-noise generators, the undersampled
// Bezier reconstruction and the distance filter (25-stage averaging stack).
//
// Deviations: Forja adds metering and a 10 ms bypass crossfade. The original's
// commented-out 64-bit dither stays commented; only the RNG advances (including
// the asymmetric extra step on the right channel). The 90 delay lines live on
// the heap (std::vector) because the type is also created on the stack by the
// test harness. No allocation, logging or locks inside process().

#include "forja/dsp/ForjaDspCommon.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace forja::dsp::salao
{

inline constexpr int d3A = 1090; inline constexpr int d3B = 1682; inline constexpr int d3C = 1695;
inline constexpr int d3D = 938; inline constexpr int d3E = 1558; inline constexpr int d3F = 1645;
inline constexpr int d3G = 600; inline constexpr int d3H = 800; inline constexpr int d3I = 1580;
inline constexpr int d6A = 17; inline constexpr int d6B = 204; inline constexpr int d6C = 173; inline constexpr int d6D = 29; inline constexpr int d6E = 151; inline constexpr int d6F = 372; inline constexpr int d6G = 1090; inline constexpr int d6H = 132; inline constexpr int d6I = 71; inline constexpr int d6J = 215; inline constexpr int d6K = 1558; inline constexpr int d6L = 61; inline constexpr int d6M = 32; inline constexpr int d6N = 71; inline constexpr int d6O = 938; inline constexpr int d6P = 1695; inline constexpr int d6Q = 129; inline constexpr int d6R = 11; inline constexpr int d6S = 131; inline constexpr int d6T = 1682; inline constexpr int d6U = 103; inline constexpr int d6V = 107; inline constexpr int d6W = 109; inline constexpr int d6X = 113; inline constexpr int d6Y = 800; inline constexpr int d6ZA = 60; inline constexpr int d6ZB = 40; inline constexpr int d6ZC = 157; inline constexpr int d6ZD = 149; inline constexpr int d6ZE = 1580; inline constexpr int d6ZF = 157; inline constexpr int d6ZG = 1645; inline constexpr int d6ZH = 28; inline constexpr int d6ZI = 6; inline constexpr int d6ZJ = 179; inline constexpr int d6ZK = 600; //2 to 193 ms, 1137 seat hall  
inline constexpr int early[] = {6, 11, 17, 28, 29, 32, 40, 60, 61, 71, 71, 103, 107, 109, 113, 129, 131, 132, 149, 151, 157, 157, 173, 179, 204, 215, 372, 600, 800, 938, 1090, 1558, 1580, 1645, 1682, 1695};

class SalaoDSP
{
public:
    struct Parameters
    {
        float regen = 0.5f;
        float position = 0.75f;
        float downRez = 1.0f;
        float distance = 0.25f;
        float dryWet = 0.5f;
        bool bypass = false;
    };

    SalaoDSP() { reset(); }

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
        std::fill (a3AL.begin(), a3AL.end(), 0.0);
        std::fill (a3AR.begin(), a3AR.end(), 0.0);
        std::fill (a3BL.begin(), a3BL.end(), 0.0);
        std::fill (a3BR.begin(), a3BR.end(), 0.0);
        std::fill (a3CL.begin(), a3CL.end(), 0.0);
        std::fill (a3CR.begin(), a3CR.end(), 0.0);
        std::fill (a3DL.begin(), a3DL.end(), 0.0);
        std::fill (a3DR.begin(), a3DR.end(), 0.0);
        std::fill (a3EL.begin(), a3EL.end(), 0.0);
        std::fill (a3ER.begin(), a3ER.end(), 0.0);
        std::fill (a3FL.begin(), a3FL.end(), 0.0);
        std::fill (a3FR.begin(), a3FR.end(), 0.0);
        std::fill (a3GL.begin(), a3GL.end(), 0.0);
        std::fill (a3GR.begin(), a3GR.end(), 0.0);
        std::fill (a3HL.begin(), a3HL.end(), 0.0);
        std::fill (a3HR.begin(), a3HR.end(), 0.0);
        std::fill (a3IL.begin(), a3IL.end(), 0.0);
        std::fill (a3IR.begin(), a3IR.end(), 0.0);
        std::fill (a6AL.begin(), a6AL.end(), 0.0);
        std::fill (a6AR.begin(), a6AR.end(), 0.0);
        std::fill (a6BL.begin(), a6BL.end(), 0.0);
        std::fill (a6BR.begin(), a6BR.end(), 0.0);
        std::fill (a6CL.begin(), a6CL.end(), 0.0);
        std::fill (a6CR.begin(), a6CR.end(), 0.0);
        std::fill (a6DL.begin(), a6DL.end(), 0.0);
        std::fill (a6DR.begin(), a6DR.end(), 0.0);
        std::fill (a6EL.begin(), a6EL.end(), 0.0);
        std::fill (a6ER.begin(), a6ER.end(), 0.0);
        std::fill (a6FL.begin(), a6FL.end(), 0.0);
        std::fill (a6FR.begin(), a6FR.end(), 0.0);
        std::fill (a6GL.begin(), a6GL.end(), 0.0);
        std::fill (a6GR.begin(), a6GR.end(), 0.0);
        std::fill (a6HL.begin(), a6HL.end(), 0.0);
        std::fill (a6HR.begin(), a6HR.end(), 0.0);
        std::fill (a6IL.begin(), a6IL.end(), 0.0);
        std::fill (a6IR.begin(), a6IR.end(), 0.0);
        std::fill (a6JL.begin(), a6JL.end(), 0.0);
        std::fill (a6JR.begin(), a6JR.end(), 0.0);
        std::fill (a6KL.begin(), a6KL.end(), 0.0);
        std::fill (a6KR.begin(), a6KR.end(), 0.0);
        std::fill (a6LL.begin(), a6LL.end(), 0.0);
        std::fill (a6LR.begin(), a6LR.end(), 0.0);
        std::fill (a6ML.begin(), a6ML.end(), 0.0);
        std::fill (a6MR.begin(), a6MR.end(), 0.0);
        std::fill (a6NL.begin(), a6NL.end(), 0.0);
        std::fill (a6NR.begin(), a6NR.end(), 0.0);
        std::fill (a6OL.begin(), a6OL.end(), 0.0);
        std::fill (a6OR.begin(), a6OR.end(), 0.0);
        std::fill (a6PL.begin(), a6PL.end(), 0.0);
        std::fill (a6PR.begin(), a6PR.end(), 0.0);
        std::fill (a6QL.begin(), a6QL.end(), 0.0);
        std::fill (a6QR.begin(), a6QR.end(), 0.0);
        std::fill (a6RL.begin(), a6RL.end(), 0.0);
        std::fill (a6RR.begin(), a6RR.end(), 0.0);
        std::fill (a6SL.begin(), a6SL.end(), 0.0);
        std::fill (a6SR.begin(), a6SR.end(), 0.0);
        std::fill (a6TL.begin(), a6TL.end(), 0.0);
        std::fill (a6TR.begin(), a6TR.end(), 0.0);
        std::fill (a6UL.begin(), a6UL.end(), 0.0);
        std::fill (a6UR.begin(), a6UR.end(), 0.0);
        std::fill (a6VL.begin(), a6VL.end(), 0.0);
        std::fill (a6VR.begin(), a6VR.end(), 0.0);
        std::fill (a6WL.begin(), a6WL.end(), 0.0);
        std::fill (a6WR.begin(), a6WR.end(), 0.0);
        std::fill (a6XL.begin(), a6XL.end(), 0.0);
        std::fill (a6XR.begin(), a6XR.end(), 0.0);
        std::fill (a6YL.begin(), a6YL.end(), 0.0);
        std::fill (a6YR.begin(), a6YR.end(), 0.0);
        std::fill (a6ZAL.begin(), a6ZAL.end(), 0.0);
        std::fill (a6ZAR.begin(), a6ZAR.end(), 0.0);
        std::fill (a6ZBL.begin(), a6ZBL.end(), 0.0);
        std::fill (a6ZBR.begin(), a6ZBR.end(), 0.0);
        std::fill (a6ZCL.begin(), a6ZCL.end(), 0.0);
        std::fill (a6ZCR.begin(), a6ZCR.end(), 0.0);
        std::fill (a6ZDL.begin(), a6ZDL.end(), 0.0);
        std::fill (a6ZDR.begin(), a6ZDR.end(), 0.0);
        std::fill (a6ZEL.begin(), a6ZEL.end(), 0.0);
        std::fill (a6ZER.begin(), a6ZER.end(), 0.0);
        std::fill (a6ZFL.begin(), a6ZFL.end(), 0.0);
        std::fill (a6ZFR.begin(), a6ZFR.end(), 0.0);
        std::fill (a6ZGL.begin(), a6ZGL.end(), 0.0);
        std::fill (a6ZGR.begin(), a6ZGR.end(), 0.0);
        std::fill (a6ZHL.begin(), a6ZHL.end(), 0.0);
        std::fill (a6ZHR.begin(), a6ZHR.end(), 0.0);
        std::fill (a6ZIL.begin(), a6ZIL.end(), 0.0);
        std::fill (a6ZIR.begin(), a6ZIR.end(), 0.0);
        std::fill (a6ZJL.begin(), a6ZJL.end(), 0.0);
        std::fill (a6ZJR.begin(), a6ZJR.end(), 0.0);
        std::fill (a6ZKL.begin(), a6ZKL.end(), 0.0);
        std::fill (a6ZKR.begin(), a6ZKR.end(), 0.0);

        c3AL = 1, c3BL = 1, c3CL = 1, c3DL = 1, c3EL = 1, c3FL = 1, c3GL = 1, c3HL = 1, c3IL = 1;
        c3AR = 1, c3BR = 1, c3CR = 1, c3DR = 1, c3ER = 1, c3FR = 1, c3GR = 1, c3HR = 1, c3IR = 1;
        c6AL = 1, c6BL = 1, c6CL = 1, c6DL = 1, c6EL = 1, c6FL = 1, c6GL = 1, c6HL = 1, c6IL = 1;
        c6JL = 1, c6KL = 1, c6LL = 1, c6ML = 1, c6NL = 1, c6OL = 1, c6PL = 1, c6QL = 1, c6RL = 1;
        c6SL = 1, c6TL = 1, c6UL = 1, c6VL = 1, c6WL = 1, c6XL = 1, c6YL = 1, c6ZAL = 1, c6ZBL = 1;
        c6ZCL = 1, c6ZDL = 1, c6ZEL = 1, c6ZFL = 1, c6ZGL = 1, c6ZHL = 1, c6ZIL = 1, c6ZJL = 1, c6ZKL = 1;
        c6AR = 1, c6BR = 1, c6CR = 1, c6DR = 1, c6ER = 1, c6FR = 1, c6GR = 1, c6HR = 1, c6IR = 1;
        c6JR = 1, c6KR = 1, c6LR = 1, c6MR = 1, c6NR = 1, c6OR = 1, c6PR = 1, c6QR = 1, c6RR = 1;
        c6SR = 1, c6TR = 1, c6UR = 1, c6VR = 1, c6WR = 1, c6XR = 1, c6YR = 1, c6ZAR = 1, c6ZBR = 1;
        c6ZCR = 1, c6ZDR = 1, c6ZER = 1, c6ZFR = 1, c6ZGR = 1, c6ZHR = 1, c6ZIR = 1, c6ZJR = 1, c6ZKR = 1;

        earlyZero = 0;

        ld3G = early[27];
        ld3H = early[28];
        ld3D = early[29];
        ld3A = early[30];
        ld3E = early[31];
        ld3I = early[32];
        ld3F = early[33];
        ld3B = early[34];
        ld3C = early[35];

        f6AL = f6BL = f6CL = f6DL = f6EL = f6FL = 0.0;
        f6FR = f6LR = f6RR = f6XR = f6ZER = f6ZKR = 0.0;
        fhAL = fhBL = fhCL = fhDL = fhEL = fhFL = 0.0;
        flAL = flBL = flCL = flDL = flEL = flFL = 0.0;
        fhAR = fhBR = fhCR = fhDR = fhER = fhFR = 0.0;
        flAR = flBR = flCR = flDR = flER = flFR = 0.0;
        hA = hB = hC = hD = hE = hF = 0.0;

        for (int x = 0; x < bez_total; ++x)
            bez[x] = 0.0;
        bez[bez_cycle] = 1.0;

        firstDryL = firstAvgL = 0.0;
        lsA = lsB = lsC = lsD = lsE = lsF = lsG = lsH = lsI = lsJ = lsK = lsL = lsM = 0.0;
        lsN = lsO = lsP = lsQ = lsR = lsS = lsT = lsU = lsV = lsW = lsX = lsY = lsZ = 0.0;
        lsa = lsb = lsc = lsd = lse = lsf = lsg = lsh = lsi = lsj = lsk = lsl = lsm = 0.0;
        lsn = lso = lsp = lsq = lsr = lss = lst = lsu = lsv = lsw = lsx = lsy = lsz = 0.0;
        rsA = rsB = rsC = rsD = rsE = rsF = rsG = rsH = rsI = rsJ = rsK = rsL = rsM = 0.0;
        rsN = rsO = rsP = rsQ = rsR = rsS = rsT = rsU = rsV = rsW = rsX = rsY = rsZ = 0.0;
        rsa = rsb = rsc = rsd = rse = rsf = rsg = rsh = rsi = rsj = rsk = rsl = rsm = 0.0;
        rsn = rso = rsp = rsq = rsr = rss = rst = rsu = rsv = rsw = rsx = rsy = rsz = 0.0;
        firstDryR = firstAvgR = 0.0;

        firstdryl = firstavgl = 0.0;
        firstdryr = firstavgr = 0.0;

        prevDistance = 0;

        roomNoiseL = 0.0;
        roomNoiseR = 0.0006;
        roomTimerL = 0;
        roomTimerR = 0;

        fpdL = 22222u;
        fpdR = 22222u;

        bypassMix = params.bypass ? 1.0 : 0.0;
        clearMeters();
    }

    void setParameters (const Parameters& newParams) noexcept
    {
        params.regen = clamp01 (newParams.regen);
        params.position = clamp01 (newParams.position);
        params.downRez = clamp01 (newParams.downRez);
        params.distance = clamp01 (newParams.distance);
        params.dryWet = clamp01 (newParams.dryWet);
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

    // The space and the Bezier reconstruction are musical, never PDC.
    int latencySamples() const noexcept { return 0; }

    // Test hook: keeps the parity harness deterministic.
    void setNoiseState (std::uint32_t newFpdL, std::uint32_t newFpdR) noexcept
    {
        fpdL = newFpdL;
        fpdR = newFpdR;
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

    template <typename Sample>
    void processSamples (Sample* left, Sample* right, int numSamples) noexcept
    {
        using std::fabs;
        using std::fmin;
        using std::fmax;
        using std::pow;

        double overallscale = 1.0;
        overallscale /= 48000.0;
        overallscale *= sampleRate;

        const float A = params.regen;
        const float B = params.position;
        const float C = params.downRez;
        const float D = params.distance;
        const float E = params.dryWet;

        double reg6n = (1.0-pow(1.0-A,3.0))*0.0078125;
        double regenMax = 0.0078125;
        int start = (int)(B * 27.0);

        double downRez = ((C*(overallscale/(overallscale+0.99999)))/overallscale);
        downRez = fmin(fmax(downRez,0.0005),1.0/overallscale);

        int bezFraction = (int)(1.0/downRez);
        double bezTrim = (double)bezFraction/(bezFraction+1.0);
        downRez = 0.99999999 / bezFraction;
        bezTrim = 1.0-(downRez*bezTrim);
        float distance = pow(D*5.0f,2.0f);
        int distanceSteps = (int)distance;
        distance -= (double)distanceSteps;
        double wet = E;

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

		if (fabs(inputSampleL)<1.18e-23) inputSampleL = fpdL * 1.18e-17;
		if (fabs(inputSampleR)<1.18e-23) inputSampleR = fpdR * 1.18e-17;
		double drySampleL = inputSampleL;
		double drySampleR = inputSampleR;
		
		bez[bez_cycle] += downRez;
		bez[bez_SampL] += (inputSampleL * downRez);
		bez[bez_SampR] += (inputSampleR * downRez);
		if (bez[bez_cycle] > bezTrim) { //hit the end point and we do a reverb sample
			bez[bez_cycle] = 0.0;
			
			double earlyFloor = fabs(inputSampleL) + fabs(inputSampleR);
			switch(earlyZero){
				case 0:
					earlyFloor = earlyFloor + fabs(a3AL[c3AL]) + fabs(a3AR[c3AR]);
					if (ld3A != early[start+3] && earlyFloor < 0.06125)
					{
						for(int x = 0; x < d3A+2; x++) {a3AL[x] = 0.0; a3AR[x] = 0.0;}
						ld3A = early[start+3];
					} break;
				case 1:
					earlyFloor = earlyFloor + fabs(a3BL[c3BL]) + fabs(a3BR[c3BR]);
					if (ld3B != early[start+7] && earlyFloor < 0.06125)
					{
						for(int x = 0; x < d3B+2; x++) {a3BL[x] = 0.0; a3BR[x] = 0.0;}
						ld3B = early[start+7];
					} break;
				case 2:
					earlyFloor = earlyFloor + fabs(a3CL[c3CL]) + fabs(a3CR[c3CR]);
					if (ld3C != early[start+8] && earlyFloor < 0.06125)
					{
						for(int x = 0; x < d3C+2; x++) {a3CL[x] = 0.0; a3CR[x] = 0.0;}
						ld3C = early[start+8];
					} break;
				case 3:
					earlyFloor = earlyFloor + fabs(a3DL[c3DL]) + fabs(a3DR[c3DR]);
					if (ld3D != early[start+2] && earlyFloor < 0.06125)
					{
						for(int x = 0; x < d3D+2; x++) {a3DL[x] = 0.0; a3DR[x] = 0.0;}
						ld3D = early[start+2];
					} break;
				case 4:
					earlyFloor = earlyFloor + fabs(a3EL[c3EL]) + fabs(a3ER[c3ER]);
					if (ld3E != early[start+4] && earlyFloor < 0.06125)
					{
						for(int x = 0; x < d3E+2; x++) {a3EL[x] = 0.0; a3ER[x] = 0.0;}
						ld3E = early[start+4];
					} break;
				case 5:
					earlyFloor = earlyFloor + fabs(a3FL[c3FL]) + fabs(a3FR[c3FR]);
					if (ld3F != early[start+6] && earlyFloor < 0.06125)
					{
						for(int x = 0; x < d3F+2; x++) {a3FL[x] = 0.0; a3FR[x] = 0.0;}
						ld3F = early[start+6];
					} break;
				case 6:
					earlyFloor = earlyFloor + fabs(a3GL[c3GL]) + fabs(a3GR[c3GR]);
					if (ld3G != early[start] && earlyFloor < 0.06125)
					{
						for(int x = 0; x < d3G+2; x++) {a3GL[x] = 0.0; a3GR[x] = 0.0;}
						ld3G = early[start];
					} break;
				case 7:
					earlyFloor = earlyFloor + fabs(a3HL[c3HL]) + fabs(a3HR[c3HR]);
					if (ld3H != early[start+1] && earlyFloor < 0.06125)
					{
						for(int x = 0; x < d3H+2; x++) {a3HL[x] = 0.0; a3HR[x] = 0.0;}
						ld3H = early[start+1];
					} break;
				case 8:
					earlyFloor = earlyFloor + fabs(a3IL[c3IL]) + fabs(a3IR[c3IR]);
					if (ld3I != early[start+5] && earlyFloor < 0.06125)
					{
						for(int x = 0; x < d3I+2; x++) {a3IL[x] = 0.0; a3IR[x] = 0.0;}
						ld3I = early[start+5];
					} break;
			}
			earlyZero++; if (earlyZero > 8) earlyZero = 0;
			
			inputSampleL = (bez[bez_SampL]);
			inputSampleR = (bez[bez_SampR]);
			
			a3AL[c3AL] = inputSampleL;
			a3BL[c3BL] = inputSampleL;
			a3CL[c3CL] = inputSampleL;
			a3CR[c3CR] = inputSampleR;
			a3FR[c3FR] = inputSampleR;
			a3IR[c3IR] = inputSampleR;
			
			c3AL++; c3BL++; c3CL++; c3CR++; c3FR++; c3IR++; 
			
			if (c3AL > ld3A) c3AL = 0;
			if (c3BL > ld3B) c3BL = 0;
			if (c3CL > ld3C) c3CL = 0;
			if (c3CR > ld3C) c3CR = 0;
			if (c3FR > ld3F) c3FR = 0;
			if (c3IR > ld3I) c3IR = 0;
			
			hA = a3AL[c3AL-((c3AL > ld3A)?c3AL+1:0)];
			hB = a3BL[c3BL-((c3BL > ld3B)?c3BL+1:0)];
			hC = a3CL[c3CL-((c3CL > ld3C)?c3CL+1:0)];
			hD = a3CR[c3CR-((c3CR > ld3C)?c3CR+1:0)];
			hE = a3FR[c3FR-((c3FR > ld3F)?c3FR+1:0)];
			hF = a3IR[c3IR-((c3IR > ld3I)?c3IR+1:0)];
			
			a3DL[c3DL] = hB + hC;
			a3EL[c3EL] = hA + hC;
			a3FL[c3FL] = hA + hB;
			a3BR[c3BR] = hE + hF;
			a3ER[c3ER] = hD + hF;
			a3HR[c3HR] = hD + hE;
			
			a3DL[c3DL] = (((hB + hC) * -2.0) + hA);
			a3EL[c3EL] = (((hA + hC) * -2.0) + hB);
			a3FL[c3FL] = (((hA + hB) * -2.0) + hC);
			a3BR[c3BR] = (((hE + hF) * -2.0) + hD);
			a3ER[c3ER] = (((hD + hF) * -2.0) + hE);
			a3HR[c3HR] = (((hD + hE) * -2.0) + hF);
			
			c3DL++; c3EL++; c3FL++; c3BR++; c3ER++; c3HR++; 
			
			if (c3DL > ld3D) c3DL = 0;
			if (c3EL > ld3E) c3EL = 0;
			if (c3FL > ld3F) c3FL = 0;
			if (c3BR > ld3B) c3BR = 0;
			if (c3ER > ld3E) c3ER = 0;
			if (c3HR > ld3H) c3HR = 0;
			
			hA = a3DL[c3DL-((c3DL > ld3D)?c3DL+1:0)];
			hB = a3EL[c3EL-((c3EL > ld3E)?c3EL+1:0)];
			hC = a3FL[c3FL-((c3FL > ld3F)?c3FL+1:0)];
			hD = a3BR[c3BR-((c3BR > ld3B)?c3BR+1:0)];
			hE = a3ER[c3ER-((c3ER > ld3E)?c3ER+1:0)];
			hF = a3HR[c3HR-((c3HR > ld3H)?c3HR+1:0)];
			
			a3GL[c3GL] = (((hB + hC) * -2.0) + hA);
			a3HL[c3HL] = (((hA + hC) * -2.0) + hB);
			a3IL[c3IL] = (((hA + hB) * -2.0) + hC);
			a3AR[c3AR] = (((hE + hF) * -2.0) + hD);
			a3DR[c3DR] = (((hD + hF) * -2.0) + hE);
			a3GR[c3GR] = (((hD + hE) * -2.0) + hF);
			
			c3GL++; c3HL++; c3IL++; c3AR++; c3DR++; c3GR++; 
			
			if (c3GL > ld3G) c3GL = 0;
			if (c3HL > ld3H) c3HL = 0;
			if (c3IL > ld3I) c3IL = 0;
			if (c3AR > ld3A) c3AR = 0;
			if (c3DR > ld3D) c3DR = 0;
			if (c3GR > ld3G) c3GR = 0;
			
			hA = a3GL[c3GL-((c3GL > ld3G)?c3GL+1:0)];
			hB = a3HL[c3HL-((c3HL > ld3H)?c3HL+1:0)];
			hC = a3IL[c3IL-((c3IL > ld3I)?c3IL+1:0)];
			hD = a3AR[c3AR-((c3AR > ld3A)?c3AR+1:0)];
			hE = a3DR[c3DR-((c3DR > ld3D)?c3DR+1:0)];
			hF = a3GR[c3GR-((c3GR > ld3G)?c3GR+1:0)];
			
			double earlyReflectionL = (((hB + hC) * -2.0) + hA)*-0.0625;
			double earlyReflectionR = (((hE + hF) * -2.0) + hD)*-0.0625;
			
			double nonlin = 1.0-fmin(fmax(fabs(inputSampleL),fabs(inputSampleR)),1.0);
			nonlin = 1.0-(nonlin*nonlin*nonlin);
			
			inputSampleL -= earlyReflectionL;
			inputSampleR -= earlyReflectionR;
			
			roomTimerL++;
			if (roomTimerL > 13) {
				roomTimerL = 0;
				if (roomNoiseL > 0.0) roomNoiseL -= fabs((fpdL / (double)0xffffffffu)-0.5)*0.125;
				else roomNoiseL += fabs((fpdL / (double)0xffffffffu)-0.5)*0.125;
			} else roomNoiseL += ((fpdL / (double)0xffffffffu)-0.5)*0.125;
			//roomNoiseL governs the added 'room tone' noise. 0.125 gives you not an overwhelming
			//volume of it, less will be quieter. The 13 is part of VoiceOfTheStarship, and
			//larger allows more subs into the noise: it'll determine the apparent room scale,
			//with subs content implying a HUGE space, no lows implying a small space.
			
			roomTimerR++;
			if (roomTimerR > 4) {
				roomTimerR = 0;
				if (roomNoiseR > fmin(reg6n+(nonlin*0.0002),regenMax)) roomNoiseR -= fabs((fpdR / (double)0xffffffffu)-0.5)*0.000007629;
				else roomNoiseR += fabs((fpdR / (double)0xffffffffu)-0.5)*0.000007629;
			} else roomNoiseR += ((fpdR / (double)0xffffffffu)-0.5)*0.000007629;
			//roomNoiseR governs the activity of the air in this space. Consider it in terms of
			//winds at MPH: the RoomTimer number is (int)sqrt(wind MPH) which governs the intensity
			//of gusts and fluctuations (avoid hurricane force inside realistic rooms)
			//and the scaling of roomNoiseR (as added to reg6n) is sqrt(wind MPH)*0.00002
			//but check against pure tones, don't let it dirty them up too much!
			f6BL += roomNoiseL;
			a6AL[c6AL] = f6BL*roomNoiseR + inputSampleL;
			a6BL[c6BL] = f6CL*roomNoiseR + inputSampleL;
			a6CL[c6CL] = f6DL*roomNoiseR + inputSampleL;
			a6DL[c6DL] = f6EL*roomNoiseR + inputSampleL;
			a6EL[c6EL] = f6FL*roomNoiseR + inputSampleL;
			a6FL[c6FL] = f6AL*roomNoiseR + inputSampleL;
			
			f6LR += roomNoiseL;
			a6FR[c6FR] = f6LR*roomNoiseR + inputSampleR;
			a6LR[c6LR] = f6RR*roomNoiseR + inputSampleR;
			a6RR[c6RR] = f6XR*roomNoiseR + inputSampleR;
			a6XR[c6XR] = f6ZER*roomNoiseR + inputSampleR;
			a6ZER[c6ZER] = f6ZKR*roomNoiseR + inputSampleR;
			a6ZKR[c6ZKR] = f6FR*roomNoiseR + inputSampleR;
			
			//left verb
			
			c6AL++; c6BL++; c6CL++; c6DL++; c6EL++; c6FL++; 
			
			if (c6AL > d6A) c6AL = 0;
			if (c6BL > d6B) c6BL = 0;
			if (c6CL > d6C) c6CL = 0;
			if (c6DL > d6D) c6DL = 0;
			if (c6EL > d6E) c6EL = 0;
			if (c6FL > d6F) c6FL = 0;
			
			hA = a6AL[c6AL-((c6AL > d6A)?d6A+1:0)];
			hB = a6BL[c6BL-((c6BL > d6B)?d6B+1:0)];
			hC = a6CL[c6CL-((c6CL > d6C)?d6C+1:0)];
			hD = a6DL[c6DL-((c6DL > d6D)?d6D+1:0)];
			hE = a6EL[c6EL-((c6EL > d6E)?d6E+1:0)];
			hF = a6FL[c6FL-((c6FL > d6F)?d6F+1:0)];
			
			a6GL[c6GL] = hB + hC + hD + hE + hF;
			a6HL[c6HL] = (hA + hC + hF) - (hD + hE);
			a6IL[c6IL] = (hA + hB + hD) - (hE + hF);
			a6JL[c6JL] = (hA + hC + hE) - (hB + hF);
			a6KL[c6KL] = (hA + hD + hF) - (hB + hC);
			a6LL[c6LL] = (hA + hB + hE) - (hC + hD);
			
			c6GL++; c6HL++; c6IL++; c6JL++; c6KL++; c6LL++; 
			
			if (c6GL > d6G) c6GL = 0;
			if (c6HL > d6H) c6HL = 0;
			if (c6IL > d6I) c6IL = 0;
			if (c6JL > d6J) c6JL = 0;
			if (c6KL > d6K) c6KL = 0;
			if (c6LL > d6L) c6LL = 0;
			
			hA = a6GL[c6GL-((c6GL > d6G)?d6G+1:0)];
			hB = a6HL[c6HL-((c6HL > d6H)?d6H+1:0)];
			hC = a6IL[c6IL-((c6IL > d6I)?d6I+1:0)];
			hD = a6JL[c6JL-((c6JL > d6J)?d6J+1:0)];
			hE = a6KL[c6KL-((c6KL > d6K)?d6K+1:0)];
			hF = a6LL[c6LL-((c6LL > d6L)?d6L+1:0)];
			
			a6ML[c6ML] = hB + hC + hD + hE + hF;
			a6NL[c6NL] = (hA + hC + hF) - (hD + hE);
			a6OL[c6OL] = (hA + hB + hD) - (hE + hF);
			a6PL[c6PL] = (hA + hC + hE) - (hB + hF);
			a6QL[c6QL] = (hA + hD + hF) - (hB + hC);
			a6RL[c6RL] = (hA + hB + hE) - (hC + hD);
			
			
			c6ML++; c6NL++; c6OL++; c6PL++; c6QL++; c6RL++; 
			
			if (c6ML > d6M) c6ML = 0;
			if (c6NL > d6N) c6NL = 0;
			if (c6OL > d6O) c6OL = 0;
			if (c6PL > d6P) c6PL = 0;
			if (c6QL > d6Q) c6QL = 0;
			if (c6RL > d6R) c6RL = 0;
			
			hA = a6ML[c6ML-((c6ML > d6M)?d6M+1:0)];
			hB = a6NL[c6NL-((c6NL > d6N)?d6N+1:0)];
			hC = a6OL[c6OL-((c6OL > d6O)?d6O+1:0)];
			hD = a6PL[c6PL-((c6PL > d6P)?d6P+1:0)];
			hE = a6QL[c6QL-((c6QL > d6Q)?d6Q+1:0)];
			hF = a6RL[c6RL-((c6RL > d6R)?d6R+1:0)];
			
			a6SL[c6SL] = hB + hC + hD + hE + hF;
			a6TL[c6TL] = (hA + hC + hF) - (hD + hE);
			a6UL[c6UL] = (hA + hB + hD) - (hE + hF);
			a6VL[c6VL] = (hA + hC + hE) - (hB + hF);
			a6WL[c6WL] = (hA + hD + hF) - (hB + hC);
			a6XL[c6XL] = (hA + hB + hE) - (hC + hD);
			
			
			c6SL++; c6TL++; c6UL++; c6VL++; c6WL++; c6XL++; 
			
			if (c6SL > d6S) c6SL = 0;
			if (c6TL > d6T) c6TL = 0;
			if (c6UL > d6U) c6UL = 0;
			if (c6VL > d6V) c6VL = 0;
			if (c6WL > d6W) c6WL = 0;
			if (c6XL > d6X) c6XL = 0;
			
			hA = a6SL[c6SL-((c6SL > d6S)?d6S+1:0)];
			hB = a6TL[c6TL-((c6TL > d6T)?d6T+1:0)];
			hC = a6UL[c6UL-((c6UL > d6U)?d6U+1:0)];
			hD = a6VL[c6VL-((c6VL > d6V)?d6V+1:0)];
			hE = a6WL[c6WL-((c6WL > d6W)?d6W+1:0)];
			hF = a6XL[c6XL-((c6XL > d6X)?d6X+1:0)];
			
			a6YL[c6YL] = hB + hC + hD + hE + hF;
			a6ZAL[c6ZAL] = (hA + hC + hF) - (hD + hE);
			a6ZBL[c6ZBL] = (hA + hB + hD) - (hE + hF);
			a6ZCL[c6ZCL] = (hA + hC + hE) - (hB + hF);
			a6ZDL[c6ZDL] = (hA + hD + hF) - (hB + hC);
			a6ZEL[c6ZEL] = (hA + hB + hE) - (hC + hD);
			
			c6YL++; c6ZAL++; c6ZBL++; c6ZCL++; c6ZDL++; c6ZEL++; 
			
			if (c6YL > d6Y) c6YL = 0;
			if (c6ZAL > d6ZA) c6ZAL = 0;
			if (c6ZBL > d6ZB) c6ZBL = 0;
			if (c6ZCL > d6ZC) c6ZCL = 0;
			if (c6ZDL > d6ZD) c6ZDL = 0;
			if (c6ZEL > d6ZE) c6ZEL = 0;
			
			hA = a6YL[c6YL-((c6YL > d6Y)?d6Y+1:0)];
			hB = a6ZAL[c6ZAL-((c6ZAL > d6ZA)?d6ZA+1:0)];
			hC = a6ZBL[c6ZBL-((c6ZBL > d6ZB)?d6ZB+1:0)];
			hD = a6ZCL[c6ZCL-((c6ZCL > d6ZC)?d6ZC+1:0)];
			hE = a6ZDL[c6ZDL-((c6ZDL > d6ZD)?d6ZD+1:0)];
			hF = a6ZEL[c6ZEL-((c6ZEL > d6ZE)?d6ZE+1:0)];
			
			a6ZFL[c6ZFL] = hB + hC + hD + hE + hF;
			a6ZGL[c6ZGL] = (hA + hC + hF) - (hD + hE);
			a6ZHL[c6ZHL] = (hA + hB + hD) - (hE + hF);
			a6ZIL[c6ZIL] = (hA + hC + hE) - (hB + hF);
			a6ZJL[c6ZJL] = (hA + hD + hF) - (hB + hC);
			a6ZKL[c6ZKL] = (hA + hB + hE) - (hC + hD);
			
			c6ZFL++; c6ZGL++; c6ZHL++; c6ZIL++; c6ZJL++; c6ZKL++; 
			
			if (c6ZFL > d6ZF) c6ZFL = 0;
			if (c6ZGL > d6ZG) c6ZGL = 0;
			if (c6ZHL > d6ZH) c6ZHL = 0;
			if (c6ZIL > d6ZI) c6ZIL = 0;
			if (c6ZJL > d6ZJ) c6ZJL = 0;
			if (c6ZKL > d6ZK) c6ZKL = 0;
			
			hA = a6ZFL[c6ZFL-((c6ZFL > d6ZF)?d6ZF+1:0)];
			hB = a6ZGL[c6ZGL-((c6ZGL > d6ZG)?d6ZG+1:0)];
			hC = a6ZHL[c6ZHL-((c6ZHL > d6ZH)?d6ZH+1:0)];
			hD = a6ZIL[c6ZIL-((c6ZIL > d6ZI)?d6ZI+1:0)];
			hE = a6ZJL[c6ZJL-((c6ZJL > d6ZJ)?d6ZJ+1:0)];
			hF = a6ZKL[c6ZKL-((c6ZKL > d6ZK)?d6ZK+1:0)];
			
			f6FR = hB + hC + hD + hE + hF;
			f6LR = (hA + hC + hF) - (hD + hE);
			f6RR = (hA + hB + hD) - (hE + hF);
			f6XR = (hA + hC + hE) - (hB + hF);
			f6ZER = (hA + hD + hF) - (hB + hC);
			f6ZKR = (hA + hB + hE) - (hC + hD);
			
			inputSampleL = (hB + hC + hD + hE + hF)*0.015625;
			
			//right verb
			
			c6FR++; c6LR++; c6RR++; c6XR++; c6ZER++; c6ZKR++; 
			
			if (c6FR > d6F) c6FR = 0;
			if (c6LR > d6L) c6LR = 0;
			if (c6RR > d6R) c6RR = 0;
			if (c6XR > d6X) c6XR = 0;
			if (c6ZER > d6ZE) c6ZER = 0;
			if (c6ZKR > d6ZK) c6ZKR = 0;
			
			hA = a6FR[c6FR-((c6FR > d6F)?d6F+1:0)];
			hB = a6LR[c6LR-((c6LR > d6L)?d6L+1:0)];
			hC = a6RR[c6RR-((c6RR > d6R)?d6R+1:0)];
			hD = a6XR[c6XR-((c6XR > d6X)?d6X+1:0)];
			hE = a6ZER[c6ZER-((c6ZER > d6ZE)?d6ZE+1:0)];
			hF = a6ZKR[c6ZKR-((c6ZKR > d6ZK)?d6ZK+1:0)];
			
			a6ER[c6ER] = hB + hC + hD + hE + hF;
			a6KR[c6KR] = (hA + hC + hF) - (hD + hE);
			a6QR[c6QR] = (hA + hB + hD) - (hE + hF);
			a6WR[c6WR] = (hA + hC + hE) - (hB + hF);
			a6ZDR[c6ZDR] = (hA + hD + hF) - (hB + hC);
			a6ZJR[c6ZJR] = (hA + hB + hE) - (hC + hD);
			
			c6ER++; c6KR++; c6QR++; c6WR++; c6ZDR++; c6ZJR++; 
			
			if (c6ER > d6E) c6ER = 0;
			if (c6KR > d6K) c6KR = 0;
			if (c6QR > d6Q) c6QR = 0;
			if (c6WR > d6W) c6WR = 0;
			if (c6ZDR > d6ZD) c6ZDR = 0;
			if (c6ZJR > d6ZJ) c6ZJR = 0;
			
			hA = a6ER[c6ER-((c6ER > d6E)?d6E+1:0)];
			hB = a6KR[c6KR-((c6KR > d6K)?d6K+1:0)];
			hC = a6QR[c6QR-((c6QR > d6Q)?d6Q+1:0)];
			hD = a6WR[c6WR-((c6WR > d6W)?d6W+1:0)];
			hE = a6ZDR[c6ZDR-((c6ZDR > d6ZD)?d6ZD+1:0)];
			hF = a6ZJR[c6ZJR-((c6ZJR > d6ZJ)?d6ZJ+1:0)];
			
			a6DR[c6DR] = hB + hC + hD + hE + hF;
			a6JR[c6JR] = (hA + hC + hF) - (hD + hE);
			a6PR[c6PR] = (hA + hB + hD) - (hE + hF);
			a6VR[c6VR] = (hA + hC + hE) - (hB + hF);
			a6ZCR[c6ZCR] = (hA + hD + hF) - (hB + hC);
			a6ZIR[c6ZIR] = (hA + hB + hE) - (hC + hD);
			
			c6DR++; c6JR++; c6PR++; c6VR++; c6ZCR++; c6ZIR++; 
			
			if (c6DR > d6D) c6DR = 0;
			if (c6JR > d6J) c6JR = 0;
			if (c6PR > d6P) c6PR = 0;
			if (c6VR > d6V) c6VR = 0;
			if (c6ZCR > d6ZC) c6ZCR = 0;
			if (c6ZIR > d6ZI) c6ZIR = 0;
			
			hA = a6DR[c6DR-((c6DR > d6D)?d6D+1:0)];
			hB = a6JR[c6JR-((c6JR > d6J)?d6J+1:0)];
			hC = a6PR[c6PR-((c6PR > d6P)?d6P+1:0)];
			hD = a6VR[c6VR-((c6VR > d6V)?d6V+1:0)];
			hE = a6ZCR[c6ZCR-((c6ZCR > d6ZC)?d6ZC+1:0)];
			hF = a6ZIR[c6ZIR-((c6ZIR > d6ZI)?d6ZI+1:0)];
			
			a6CR[c6CR] = hB + hC + hD + hE + hF;
			a6IR[c6IR] = (hA + hC + hF) - (hD + hE);
			a6OR[c6OR] = (hA + hB + hD) - (hE + hF);
			a6UR[c6UR] = (hA + hC + hE) - (hB + hF);
			a6ZBR[c6ZBR] = (hA + hD + hF) - (hB + hC);
			a6ZHR[c6ZHR] = (hA + hB + hE) - (hC + hD);
			
			c6CR++; c6IR++; c6OR++; c6UR++; c6ZBR++; c6ZHR++; 
			
			if (c6CR > d6C) c6CR = 0;
			if (c6IR > d6I) c6IR = 0;
			if (c6OR > d6O) c6OR = 0;
			if (c6UR > d6U) c6UR = 0;
			if (c6ZBR > d6ZB) c6ZBR = 0;
			if (c6ZHR > d6ZH) c6ZHR = 0;
			
			hA = a6CR[c6CR-((c6CR > d6C)?d6C+1:0)];
			hB = a6IR[c6IR-((c6IR > d6I)?d6I+1:0)];
			hC = a6OR[c6OR-((c6OR > d6O)?d6O+1:0)];
			hD = a6UR[c6UR-((c6UR > d6U)?d6U+1:0)];
			hE = a6ZBR[c6ZBR-((c6ZBR > d6ZB)?d6ZB+1:0)];
			hF = a6ZHR[c6ZHR-((c6ZHR > d6ZH)?d6ZH+1:0)];
			
			a6BR[c6BR] = hB + hC + hD + hE + hF;
			a6HR[c6HR] = (hA + hC + hF) - (hD + hE);
			a6NR[c6NR] = (hA + hB + hD) - (hE + hF);
			a6TR[c6TR] = (hA + hC + hE) - (hB + hF);
			a6ZAR[c6ZAR] = (hA + hD + hF) - (hB + hC);
			a6ZGR[c6ZGR] = (hA + hB + hE) - (hC + hD);
			
			c6BR++; c6HR++; c6NR++; c6TR++; c6ZBR++; c6ZGR++; 
			
			if (c6BR > d6B) c6BR = 0;
			if (c6HR > d6H) c6HR = 0;
			if (c6NR > d6N) c6NR = 0;
			if (c6TR > d6T) c6TR = 0;
			if (c6ZBR > d6ZB) c6ZBR = 0;
			if (c6ZGR > d6ZG) c6ZGR = 0;
			
			hA = a6BR[c6BR-((c6BR > d6B)?d6B+1:0)];
			hB = a6HR[c6HR-((c6HR > d6H)?d6H+1:0)];
			hC = a6NR[c6NR-((c6NR > d6N)?d6N+1:0)];
			hD = a6TR[c6TR-((c6TR > d6T)?d6T+1:0)];
			hE = a6ZAR[c6ZAR-((c6ZAR > d6ZA)?d6ZA+1:0)];
			hF = a6ZGR[c6ZGR-((c6ZGR > d6ZG)?d6ZG+1:0)];
			
			a6AR[c6AR] = hB + hC + hD + hE + hF;
			a6GR[c6GR] = (hA + hC + hF) - (hD + hE);
			a6MR[c6MR] = (hA + hB + hD) - (hE + hF);
			a6SR[c6SR] = (hA + hC + hE) - (hB + hF);
			a6YR[c6YR] = (hA + hD + hF) - (hB + hC);
			a6ZFR[c6ZFR] = (hA + hB + hE) - (hC + hD);
			
			c6AR++; c6GR++; c6MR++; c6SR++; c6YR++; c6ZFR++; 
			
			if (c6AR > d6A) c6AR = 0;
			if (c6GR > d6G) c6GR = 0;
			if (c6MR > d6M) c6MR = 0;
			if (c6SR > d6S) c6SR = 0;
			if (c6YR > d6Y) c6YR = 0;
			if (c6ZFR > d6ZF) c6ZFR = 0;
			
			hA = a6AR[c6AR-((c6AR > d6A)?d6A+1:0)];
			hB = a6GR[c6GR-((c6GR > d6G)?d6G+1:0)];
			hC = a6MR[c6MR-((c6MR > d6M)?d6M+1:0)];
			hD = a6SR[c6SR-((c6SR > d6S)?d6S+1:0)];
			hE = a6YR[c6YR-((c6YR > d6Y)?d6Y+1:0)];
			hF = a6ZFR[c6ZFR-((c6ZFR > d6ZF)?d6ZF+1:0)];
			
			f6AL = hB + hC + hD + hE + hF;
			f6BL = (hA + hC + hF) - (hD + hE);
			f6CL = (hA + hB + hD) - (hE + hF);
			f6DL = (hA + hC + hE) - (hB + hF);
			f6EL = (hA + hD + hF) - (hB + hC);
			f6FL = (hA + hB + hE) - (hC + hD);
			
			fhDL *= 0.25f; f6DL -= fhDL*0.125f; fhDL += f6DL;
			fhDR *= 0.25f; f6XR -= fhDR*0.125f; fhDR += f6XR;
			fhEL *= 0.25f; f6EL -= fhEL*0.125f; fhEL += f6EL;
			fhER *= 0.25f; f6ZER -= fhER*0.125f; fhER += f6ZER;
			fhFL *= 0.25f; f6FL -= fhFL*0.125f; fhFL += f6FL;
			fhFR *= 0.25f; f6ZKR -= fhFR*0.125f; fhFR += f6ZKR;
			f6BL = (f6BL+flBL)*0.5f; flBL = f6BL;
			f6LR = (f6LR+flBR)*0.5f; flBR = f6LR;
			f6AL = (f6AL+flAL)*0.5f; flAL = f6AL;
			f6FR = (f6FR+flAR)*0.5f; flAR = f6FR;
			
			inputSampleR = (hB + hC + hD + hE + hF)*0.015625;
			
			//begin just the distance filter, lowercase for inside the undersampling
			firstdryl = inputSampleL; //start by doing the interpolation
			inputSampleL += firstavgl; inputSampleL *= 0.5f; firstavgl = inputSampleL;
			inputSampleL = (firstdryl*(1.0f-distance)) + (inputSampleL*distance);
			firstdryr = inputSampleR; //start by doing the interpolation
			inputSampleR += firstavgr; inputSampleR *= 0.5f; firstavgr = inputSampleR;
			inputSampleR = (firstdryr*(1.0f-distance)) + (inputSampleR*distance);
			//having done at least one interpolation we can now do the integer number of stages
			switch (25-distanceSteps)
			{ //apply the stack of filter steps to produce the distance filter
				case  0: inputSampleL += lsz; inputSampleL *= 0.5; lsz = inputSampleL; inputSampleR += rsz; inputSampleR *= 0.5; rsz = inputSampleR;
				case  1: inputSampleL += lsy; inputSampleL *= 0.5; lsy = inputSampleL; inputSampleR += rsy; inputSampleR *= 0.5; rsy = inputSampleR;
				case  2: inputSampleL += lsx; inputSampleL *= 0.5; lsx = inputSampleL; inputSampleR += rsx; inputSampleR *= 0.5; rsx = inputSampleR;
				case  3: inputSampleL += lsw; inputSampleL *= 0.5; lsw = inputSampleL; inputSampleR += rsw; inputSampleR *= 0.5; rsw = inputSampleR;
				case  4: inputSampleL += lsv; inputSampleL *= 0.5; lsv = inputSampleL; inputSampleR += rsv; inputSampleR *= 0.5; rsv = inputSampleR;
				case  5: inputSampleL += lsu; inputSampleL *= 0.5; lsu = inputSampleL; inputSampleR += rsu; inputSampleR *= 0.5; rsu = inputSampleR;
				case  6: inputSampleL += lst; inputSampleL *= 0.5; lst = inputSampleL; inputSampleR += rst; inputSampleR *= 0.5; rst = inputSampleR;
				case  7: inputSampleL += lss; inputSampleL *= 0.5; lss = inputSampleL; inputSampleR += rss; inputSampleR *= 0.5; rss = inputSampleR;
				case  8: inputSampleL += lsr; inputSampleL *= 0.5; lsr = inputSampleL; inputSampleR += rsr; inputSampleR *= 0.5; rsr = inputSampleR;
				case  9: inputSampleL += lsq; inputSampleL *= 0.5; lsq = inputSampleL; inputSampleR += rsq; inputSampleR *= 0.5; rsq = inputSampleR;
				case 10: inputSampleL += lsp; inputSampleL *= 0.5; lsp = inputSampleL; inputSampleR += rsp; inputSampleR *= 0.5; rsp = inputSampleR;
				case 11: inputSampleL += lso; inputSampleL *= 0.5; lso = inputSampleL; inputSampleR += rso; inputSampleR *= 0.5; rso = inputSampleR;
				case 12: inputSampleL += lsn; inputSampleL *= 0.5; lsn = inputSampleL; inputSampleR += rsn; inputSampleR *= 0.5; rsn = inputSampleR;
				case 13: inputSampleL += lsm; inputSampleL *= 0.5; lsm = inputSampleL; inputSampleR += rsm; inputSampleR *= 0.5; rsm = inputSampleR;
				case 14: inputSampleL += lsl; inputSampleL *= 0.5; lsl = inputSampleL; inputSampleR += rsl; inputSampleR *= 0.5; rsl = inputSampleR;
				case 15: inputSampleL += lsk; inputSampleL *= 0.5; lsk = inputSampleL; inputSampleR += rsk; inputSampleR *= 0.5; rsk = inputSampleR;
				case 16: inputSampleL += lsj; inputSampleL *= 0.5; lsj = inputSampleL; inputSampleR += rsj; inputSampleR *= 0.5; rsj = inputSampleR;
				case 17: inputSampleL += lsi; inputSampleL *= 0.5; lsi = inputSampleL; inputSampleR += rsi; inputSampleR *= 0.5; rsi = inputSampleR;
				case 18: inputSampleL += lsh; inputSampleL *= 0.5; lsh = inputSampleL; inputSampleR += rsh; inputSampleR *= 0.5; rsh = inputSampleR;
				case 19: inputSampleL += lsg; inputSampleL *= 0.5; lsg = inputSampleL; inputSampleR += rsg; inputSampleR *= 0.5; rsg = inputSampleR;
				case 20: inputSampleL += lsf; inputSampleL *= 0.5; lsf = inputSampleL; inputSampleR += rsf; inputSampleR *= 0.5; rsf = inputSampleR;
				case 21: inputSampleL += lse; inputSampleL *= 0.5; lse = inputSampleL; inputSampleR += rse; inputSampleR *= 0.5; rse = inputSampleR;
				case 22: inputSampleL += lsd; inputSampleL *= 0.5; lsd = inputSampleL; inputSampleR += rsd; inputSampleR *= 0.5; rsd = inputSampleR;
				case 23: inputSampleL += lsc; inputSampleL *= 0.5; lsc = inputSampleL; inputSampleR += rsc; inputSampleR *= 0.5; rsc = inputSampleR;
				case 24: inputSampleL += lsb; inputSampleL *= 0.5; lsb = inputSampleL; inputSampleR += rsb; inputSampleR *= 0.5; rsb = inputSampleR;
				case 25: inputSampleL += lsa; inputSampleL *= 0.5; lsa = inputSampleL; inputSampleR += rsa; inputSampleR *= 0.5; rsa = inputSampleR;
				case 26: break;
			}
			//end distance filter
			
			inputSampleL += earlyReflectionL;
			inputSampleR += earlyReflectionR;
			
			bez[bez_CL] = bez[bez_BL];
			bez[bez_BL] = bez[bez_AL];
			bez[bez_AL] = inputSampleL;
			bez[bez_SampL] = 0.0;
			
			bez[bez_CR] = bez[bez_BR];
			bez[bez_BR] = bez[bez_AR];
			bez[bez_AR] = inputSampleR;
			bez[bez_SampR] = 0.0;
		}
		double X = bez[bez_cycle];
		inputSampleL = (bez[bez_BL]+(bez[bez_CL]*(1.0-X)*(1.0-X))+(bez[bez_BL]*2.0*(1.0-X)*X)+(bez[bez_AL]*X*X))*-0.0625;
		inputSampleR = (bez[bez_BR]+(bez[bez_CR]*(1.0-X)*(1.0-X))+(bez[bez_BR]*2.0*(1.0-X)*X)+(bez[bez_AR]*X*X))*-0.0625;
		
		if (prevDistance < distanceSteps) {
			switch (prevDistance)
			{
				case  0: rsa = lsa = 0.0f; rsA = inputSampleR; lsA = inputSampleL; 
				case  1: rsb = lsb = 0.0f; rsB = inputSampleR; lsB = inputSampleL; 
				case  2: rsc = lsc = 0.0f; rsC = inputSampleR; lsC = inputSampleL;
				case  3: rsd = lsd = 0.0f; rsD = inputSampleR; lsD = inputSampleL;
				case  4: rse = lse = 0.0f; rsE = inputSampleR; lsE = inputSampleL; 
				case  5: rsf = lsf = 0.0f; rsF = inputSampleR; lsF = inputSampleL; 
				case  6: rsg = lsg = 0.0f; rsG = inputSampleR; lsG = inputSampleL; 
				case  7: rsh = lsh = 0.0f; rsH = inputSampleR; lsH = inputSampleL;
				case  8: rsi = lsi = 0.0f; rsI = inputSampleR; lsI = inputSampleL;
				case  9: rsj = lsj = 0.0f; rsJ = inputSampleR; lsJ = inputSampleL;
				case 10: rsk = lsk = 0.0f; rsK = inputSampleR; lsK = inputSampleL;
				case 11: rsl = lsl = 0.0f; rsL = inputSampleR; lsL = inputSampleL;
				case 12: rsm = lsm = 0.0f; rsM = inputSampleR; lsM = inputSampleL;
				case 13: rsn = lsn = 0.0f; rsN = inputSampleR; lsN = inputSampleL;
				case 14: rso = lso = 0.0f; rsO = inputSampleR; lsO = inputSampleL;
				case 15: rsp = lsp = 0.0f; rsP = inputSampleR; lsP = inputSampleL;
				case 16: rsq = lsq = 0.0f; rsQ = inputSampleR; lsQ = inputSampleL;
				case 17: rsr = lsr = 0.0f; rsR = inputSampleR; lsR = inputSampleL;
				case 18: rss = lss = 0.0f; rsS = inputSampleR; lsS = inputSampleL;
				case 19: rst = lst = 0.0f; rsT = inputSampleR; lsT = inputSampleL;
				case 20: rsu = lsu = 0.0f; rsU = inputSampleR; lsU = inputSampleL;
				case 21: rsv = lsv = 0.0f; rsV = inputSampleR; lsV = inputSampleL;
				case 22: rsw = lsw = 0.0f; rsW = inputSampleR; lsW = inputSampleL;
				case 23: rsx = lsx = 0.0f; rsX = inputSampleR; lsX = inputSampleL;
				case 24: rsy = lsy = 0.0f; rsY = inputSampleR; lsY = inputSampleL;
				case 25: rsz = lsz = 0.0f; rsZ = inputSampleR; lsZ = inputSampleL;
				case 26: break;
			}
			prevDistance = distanceSteps;
		}
		
		//begin just the distance filter capitalized for outside the undersampling
		firstDryL = inputSampleL; //start by doing the interpolation
		inputSampleL += firstAvgL; inputSampleL *= 0.5f; firstAvgL = inputSampleL;
		inputSampleL = (firstDryL*(1.0f-distance)) + (inputSampleL*distance);
		firstDryR = inputSampleR; //start by doing the interpolation
		inputSampleR += firstAvgR; inputSampleR *= 0.5f; firstAvgR = inputSampleR;
		inputSampleR = (firstDryR*(1.0f-distance)) + (inputSampleR*distance);
		//having done at least one interpolation we can now do the integer number of stages				
		switch (25-distanceSteps)
		{ //apply the stack of filter steps to produce the distance filter
			case 0: inputSampleL += lsZ; inputSampleL *= 0.5; lsZ = inputSampleL; inputSampleR += rsZ; inputSampleR *= 0.5; rsZ = inputSampleR;
			case 1: inputSampleL += lsY; inputSampleL *= 0.5; lsY = inputSampleL; inputSampleR += rsY; inputSampleR *= 0.5; rsY = inputSampleR;
			case 2: inputSampleL += lsX; inputSampleL *= 0.5; lsX = inputSampleL; inputSampleR += rsX; inputSampleR *= 0.5; rsX = inputSampleR;
			case 3: inputSampleL += lsW; inputSampleL *= 0.5; lsW = inputSampleL; inputSampleR += rsW; inputSampleR *= 0.5; rsW = inputSampleR;
			case 4: inputSampleL += lsV; inputSampleL *= 0.5; lsV = inputSampleL; inputSampleR += rsV; inputSampleR *= 0.5; rsV = inputSampleR;
			case 5: inputSampleL += lsU; inputSampleL *= 0.5; lsU = inputSampleL; inputSampleR += rsU; inputSampleR *= 0.5; rsU = inputSampleR;
			case 6: inputSampleL += lsT; inputSampleL *= 0.5; lsT = inputSampleL; inputSampleR += rsT; inputSampleR *= 0.5; rsT = inputSampleR;
			case 7: inputSampleL += lsS; inputSampleL *= 0.5; lsS = inputSampleL; inputSampleR += rsS; inputSampleR *= 0.5; rsS = inputSampleR;
			case 8: inputSampleL += lsR; inputSampleL *= 0.5; lsR = inputSampleL; inputSampleR += rsR; inputSampleR *= 0.5; rsR = inputSampleR;
			case 9: inputSampleL += lsQ; inputSampleL *= 0.5; lsQ = inputSampleL; inputSampleR += rsQ; inputSampleR *= 0.5; rsQ = inputSampleR;
			case 10: inputSampleL += lsP; inputSampleL *= 0.5; lsP = inputSampleL; inputSampleR += rsP; inputSampleR *= 0.5; rsP = inputSampleR;
			case 11: inputSampleL += lsO; inputSampleL *= 0.5; lsO = inputSampleL; inputSampleR += rsO; inputSampleR *= 0.5; rsO = inputSampleR;
			case 12: inputSampleL += lsN; inputSampleL *= 0.5; lsN = inputSampleL; inputSampleR += rsN; inputSampleR *= 0.5; rsN = inputSampleR;
			case 13: inputSampleL += lsM; inputSampleL *= 0.5; lsM = inputSampleL; inputSampleR += rsM; inputSampleR *= 0.5; rsM = inputSampleR;
			case 14: inputSampleL += lsL; inputSampleL *= 0.5; lsL = inputSampleL; inputSampleR += rsL; inputSampleR *= 0.5; rsL = inputSampleR;
			case 15: inputSampleL += lsK; inputSampleL *= 0.5; lsK = inputSampleL; inputSampleR += rsK; inputSampleR *= 0.5; rsK = inputSampleR;
			case 16: inputSampleL += lsJ; inputSampleL *= 0.5; lsJ = inputSampleL; inputSampleR += rsJ; inputSampleR *= 0.5; rsJ = inputSampleR;
			case 17: inputSampleL += lsI; inputSampleL *= 0.5; lsI = inputSampleL; inputSampleR += rsI; inputSampleR *= 0.5; rsI = inputSampleR;
			case 18: inputSampleL += lsH; inputSampleL *= 0.5; lsH = inputSampleL; inputSampleR += rsH; inputSampleR *= 0.5; rsH = inputSampleR;
			case 19: inputSampleL += lsG; inputSampleL *= 0.5; lsG = inputSampleL; inputSampleR += rsG; inputSampleR *= 0.5; rsG = inputSampleR;
			case 20: inputSampleL += lsF; inputSampleL *= 0.5; lsF = inputSampleL; inputSampleR += rsF; inputSampleR *= 0.5; rsF = inputSampleR;
			case 21: inputSampleL += lsE; inputSampleL *= 0.5; lsE = inputSampleL; inputSampleR += rsE; inputSampleR *= 0.5; rsE = inputSampleR;
			case 22: inputSampleL += lsD; inputSampleL *= 0.5; lsD = inputSampleL; inputSampleR += rsD; inputSampleR *= 0.5; rsD = inputSampleR;
			case 23: inputSampleL += lsC; inputSampleL *= 0.5; lsC = inputSampleL; inputSampleR += rsC; inputSampleR *= 0.5; rsC = inputSampleR;
			case 24: inputSampleL += lsB; inputSampleL *= 0.5; lsB = inputSampleL; inputSampleR += rsB; inputSampleR *= 0.5; rsB = inputSampleR;
			case 25: inputSampleL += lsA; inputSampleL *= 0.5; lsA = inputSampleL; inputSampleR += rsA; inputSampleR *= 0.5; rsA = inputSampleR;
			case 26: break;
		}
		//end distance filter
		
		inputSampleL = (inputSampleL * wet)+(drySampleL * (1.0-wet));
		inputSampleR = (inputSampleR * wet)+(drySampleR * (1.0-wet));
		
		//begin 64 bit stereo floating point dither
		//int expon; frexp((double)inputSampleL, &expon);
		fpdL ^= fpdL << 13; fpdL ^= fpdL >> 17; fpdL ^= fpdL << 5;
		//inputSampleL += ((double(fpdL)-uint32_t(0x7fffffff)) * 3.553e-44l * pow(2,expon+62));
		//frexp((double)inputSampleR, &expon);
		fpdR ^= fpdR << 13; fpdR ^= fpdR >> 17; fpdR ^= fpdR << 5;
		if (fpdL-fpdR < 1073741824 || fpdR-fpdL < 1073741824) {
			fpdR ^= fpdR << 13; fpdR ^= fpdR >> 17; fpdR ^= fpdR << 5;}
		//inputSampleR += ((double(fpdR)-uint32_t(0x7fffffff)) * 3.553e-44l * pow(2,expon+62));
		//end 64 bit stereo floating point dither

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

    std::vector<double> a3AL = std::vector<double>((std::size_t) d3A + 5, 0.0);
    std::vector<double> a3AR = std::vector<double>((std::size_t) d3A + 5, 0.0);
    std::vector<double> a3BL = std::vector<double>((std::size_t) d3B + 5, 0.0);
    std::vector<double> a3BR = std::vector<double>((std::size_t) d3B + 5, 0.0);
    std::vector<double> a3CL = std::vector<double>((std::size_t) d3C + 5, 0.0);
    std::vector<double> a3CR = std::vector<double>((std::size_t) d3C + 5, 0.0);
    std::vector<double> a3DL = std::vector<double>((std::size_t) d3D + 5, 0.0);
    std::vector<double> a3DR = std::vector<double>((std::size_t) d3D + 5, 0.0);
    std::vector<double> a3EL = std::vector<double>((std::size_t) d3E + 5, 0.0);
    std::vector<double> a3ER = std::vector<double>((std::size_t) d3E + 5, 0.0);
    std::vector<double> a3FL = std::vector<double>((std::size_t) d3F + 5, 0.0);
    std::vector<double> a3FR = std::vector<double>((std::size_t) d3F + 5, 0.0);
    std::vector<double> a3GL = std::vector<double>((std::size_t) d3G + 5, 0.0);
    std::vector<double> a3GR = std::vector<double>((std::size_t) d3G + 5, 0.0);
    std::vector<double> a3HL = std::vector<double>((std::size_t) d3H + 5, 0.0);
    std::vector<double> a3HR = std::vector<double>((std::size_t) d3H + 5, 0.0);
    std::vector<double> a3IL = std::vector<double>((std::size_t) d3I + 5, 0.0);
    std::vector<double> a3IR = std::vector<double>((std::size_t) d3I + 5, 0.0);
    std::vector<double> a6AL = std::vector<double>((std::size_t) d6A + 5, 0.0);
    std::vector<double> a6AR = std::vector<double>((std::size_t) d6A + 5, 0.0);
    std::vector<double> a6BL = std::vector<double>((std::size_t) d6B + 5, 0.0);
    std::vector<double> a6BR = std::vector<double>((std::size_t) d6B + 5, 0.0);
    std::vector<double> a6CL = std::vector<double>((std::size_t) d6C + 5, 0.0);
    std::vector<double> a6CR = std::vector<double>((std::size_t) d6C + 5, 0.0);
    std::vector<double> a6DL = std::vector<double>((std::size_t) d6D + 5, 0.0);
    std::vector<double> a6DR = std::vector<double>((std::size_t) d6D + 5, 0.0);
    std::vector<double> a6EL = std::vector<double>((std::size_t) d6E + 5, 0.0);
    std::vector<double> a6ER = std::vector<double>((std::size_t) d6E + 5, 0.0);
    std::vector<double> a6FL = std::vector<double>((std::size_t) d6F + 5, 0.0);
    std::vector<double> a6FR = std::vector<double>((std::size_t) d6F + 5, 0.0);
    std::vector<double> a6GL = std::vector<double>((std::size_t) d6G + 5, 0.0);
    std::vector<double> a6GR = std::vector<double>((std::size_t) d6G + 5, 0.0);
    std::vector<double> a6HL = std::vector<double>((std::size_t) d6H + 5, 0.0);
    std::vector<double> a6HR = std::vector<double>((std::size_t) d6H + 5, 0.0);
    std::vector<double> a6IL = std::vector<double>((std::size_t) d6I + 5, 0.0);
    std::vector<double> a6IR = std::vector<double>((std::size_t) d6I + 5, 0.0);
    std::vector<double> a6JL = std::vector<double>((std::size_t) d6J + 5, 0.0);
    std::vector<double> a6JR = std::vector<double>((std::size_t) d6J + 5, 0.0);
    std::vector<double> a6KL = std::vector<double>((std::size_t) d6K + 5, 0.0);
    std::vector<double> a6KR = std::vector<double>((std::size_t) d6K + 5, 0.0);
    std::vector<double> a6LL = std::vector<double>((std::size_t) d6L + 5, 0.0);
    std::vector<double> a6LR = std::vector<double>((std::size_t) d6L + 5, 0.0);
    std::vector<double> a6ML = std::vector<double>((std::size_t) d6M + 5, 0.0);
    std::vector<double> a6MR = std::vector<double>((std::size_t) d6M + 5, 0.0);
    std::vector<double> a6NL = std::vector<double>((std::size_t) d6N + 5, 0.0);
    std::vector<double> a6NR = std::vector<double>((std::size_t) d6N + 5, 0.0);
    std::vector<double> a6OL = std::vector<double>((std::size_t) d6O + 5, 0.0);
    std::vector<double> a6OR = std::vector<double>((std::size_t) d6O + 5, 0.0);
    std::vector<double> a6PL = std::vector<double>((std::size_t) d6P + 5, 0.0);
    std::vector<double> a6PR = std::vector<double>((std::size_t) d6P + 5, 0.0);
    std::vector<double> a6QL = std::vector<double>((std::size_t) d6Q + 5, 0.0);
    std::vector<double> a6QR = std::vector<double>((std::size_t) d6Q + 5, 0.0);
    std::vector<double> a6RL = std::vector<double>((std::size_t) d6R + 5, 0.0);
    std::vector<double> a6RR = std::vector<double>((std::size_t) d6R + 5, 0.0);
    std::vector<double> a6SL = std::vector<double>((std::size_t) d6S + 5, 0.0);
    std::vector<double> a6SR = std::vector<double>((std::size_t) d6S + 5, 0.0);
    std::vector<double> a6TL = std::vector<double>((std::size_t) d6T + 5, 0.0);
    std::vector<double> a6TR = std::vector<double>((std::size_t) d6T + 5, 0.0);
    std::vector<double> a6UL = std::vector<double>((std::size_t) d6U + 5, 0.0);
    std::vector<double> a6UR = std::vector<double>((std::size_t) d6U + 5, 0.0);
    std::vector<double> a6VL = std::vector<double>((std::size_t) d6V + 5, 0.0);
    std::vector<double> a6VR = std::vector<double>((std::size_t) d6V + 5, 0.0);
    std::vector<double> a6WL = std::vector<double>((std::size_t) d6W + 5, 0.0);
    std::vector<double> a6WR = std::vector<double>((std::size_t) d6W + 5, 0.0);
    std::vector<double> a6XL = std::vector<double>((std::size_t) d6X + 5, 0.0);
    std::vector<double> a6XR = std::vector<double>((std::size_t) d6X + 5, 0.0);
    std::vector<double> a6YL = std::vector<double>((std::size_t) d6Y + 5, 0.0);
    std::vector<double> a6YR = std::vector<double>((std::size_t) d6Y + 5, 0.0);
    std::vector<double> a6ZAL = std::vector<double>((std::size_t) d6ZA + 5, 0.0);
    std::vector<double> a6ZAR = std::vector<double>((std::size_t) d6ZA + 5, 0.0);
    std::vector<double> a6ZBL = std::vector<double>((std::size_t) d6ZB + 5, 0.0);
    std::vector<double> a6ZBR = std::vector<double>((std::size_t) d6ZB + 5, 0.0);
    std::vector<double> a6ZCL = std::vector<double>((std::size_t) d6ZC + 5, 0.0);
    std::vector<double> a6ZCR = std::vector<double>((std::size_t) d6ZC + 5, 0.0);
    std::vector<double> a6ZDL = std::vector<double>((std::size_t) d6ZD + 5, 0.0);
    std::vector<double> a6ZDR = std::vector<double>((std::size_t) d6ZD + 5, 0.0);
    std::vector<double> a6ZEL = std::vector<double>((std::size_t) d6ZE + 5, 0.0);
    std::vector<double> a6ZER = std::vector<double>((std::size_t) d6ZE + 5, 0.0);
    std::vector<double> a6ZFL = std::vector<double>((std::size_t) d6ZF + 5, 0.0);
    std::vector<double> a6ZFR = std::vector<double>((std::size_t) d6ZF + 5, 0.0);
    std::vector<double> a6ZGL = std::vector<double>((std::size_t) d6ZG + 5, 0.0);
    std::vector<double> a6ZGR = std::vector<double>((std::size_t) d6ZG + 5, 0.0);
    std::vector<double> a6ZHL = std::vector<double>((std::size_t) d6ZH + 5, 0.0);
    std::vector<double> a6ZHR = std::vector<double>((std::size_t) d6ZH + 5, 0.0);
    std::vector<double> a6ZIL = std::vector<double>((std::size_t) d6ZI + 5, 0.0);
    std::vector<double> a6ZIR = std::vector<double>((std::size_t) d6ZI + 5, 0.0);
    std::vector<double> a6ZJL = std::vector<double>((std::size_t) d6ZJ + 5, 0.0);
    std::vector<double> a6ZJR = std::vector<double>((std::size_t) d6ZJ + 5, 0.0);
    std::vector<double> a6ZKL = std::vector<double>((std::size_t) d6ZK + 5, 0.0);
    std::vector<double> a6ZKR = std::vector<double>((std::size_t) d6ZK + 5, 0.0);

    int c3AL = 1, c3BL = 1, c3CL = 1, c3DL = 1, c3EL = 1, c3FL = 1, c3GL = 1, c3HL = 1, c3IL = 1;
    int c3AR = 1, c3BR = 1, c3CR = 1, c3DR = 1, c3ER = 1, c3FR = 1, c3GR = 1, c3HR = 1, c3IR = 1;
    int c6AL = 1, c6BL = 1, c6CL = 1, c6DL = 1, c6EL = 1, c6FL = 1, c6GL = 1, c6HL = 1, c6IL = 1;
    int c6JL = 1, c6KL = 1, c6LL = 1, c6ML = 1, c6NL = 1, c6OL = 1, c6PL = 1, c6QL = 1, c6RL = 1;
    int c6SL = 1, c6TL = 1, c6UL = 1, c6VL = 1, c6WL = 1, c6XL = 1, c6YL = 1, c6ZAL = 1, c6ZBL = 1;
    int c6ZCL = 1, c6ZDL = 1, c6ZEL = 1, c6ZFL = 1, c6ZGL = 1, c6ZHL = 1, c6ZIL = 1, c6ZJL = 1, c6ZKL = 1;
    int c6AR = 1, c6BR = 1, c6CR = 1, c6DR = 1, c6ER = 1, c6FR = 1, c6GR = 1, c6HR = 1, c6IR = 1;
    int c6JR = 1, c6KR = 1, c6LR = 1, c6MR = 1, c6NR = 1, c6OR = 1, c6PR = 1, c6QR = 1, c6RR = 1;
    int c6SR = 1, c6TR = 1, c6UR = 1, c6VR = 1, c6WR = 1, c6XR = 1, c6YR = 1, c6ZAR = 1, c6ZBR = 1;
    int c6ZCR = 1, c6ZDR = 1, c6ZER = 1, c6ZFR = 1, c6ZGR = 1, c6ZHR = 1, c6ZIR = 1, c6ZJR = 1, c6ZKR = 1;

    int ld3A = 0, ld3B = 0, ld3C = 0, ld3D = 0, ld3E = 0, ld3F = 0, ld3G = 0, ld3H = 0, ld3I = 0;
    int earlyZero = 0;

    double f6AL = 0.0, f6BL = 0.0, f6CL = 0.0, f6DL = 0.0, f6EL = 0.0, f6FL = 0.0;
    double f6FR = 0.0, f6LR = 0.0, f6RR = 0.0, f6XR = 0.0, f6ZER = 0.0, f6ZKR = 0.0;
    double fhAL = 0.0, fhBL = 0.0, fhCL = 0.0, fhDL = 0.0, fhEL = 0.0, fhFL = 0.0;
    double flAL = 0.0, flBL = 0.0, flCL = 0.0, flDL = 0.0, flEL = 0.0, flFL = 0.0;
    double fhAR = 0.0, fhBR = 0.0, fhCR = 0.0, fhDR = 0.0, fhER = 0.0, fhFR = 0.0;
    double flAR = 0.0, flBR = 0.0, flCR = 0.0, flDR = 0.0, flER = 0.0, flFR = 0.0;
    double hA = 0.0, hB = 0.0, hC = 0.0, hD = 0.0, hE = 0.0, hF = 0.0;

    double bez[bez_total] {};

    double lsA = 0.0, lsB = 0.0, lsC = 0.0, lsD = 0.0, lsE = 0.0, lsF = 0.0, lsG = 0.0, lsH = 0.0, lsI = 0.0, lsJ = 0.0, lsK = 0.0, lsL = 0.0, lsM = 0.0;
    double lsN = 0.0, lsO = 0.0, lsP = 0.0, lsQ = 0.0, lsR = 0.0, lsS = 0.0, lsT = 0.0, lsU = 0.0, lsV = 0.0, lsW = 0.0, lsX = 0.0, lsY = 0.0, lsZ = 0.0;
    double lsa = 0.0, lsb = 0.0, lsc = 0.0, lsd = 0.0, lse = 0.0, lsf = 0.0, lsg = 0.0, lsh = 0.0, lsi = 0.0, lsj = 0.0, lsk = 0.0, lsl = 0.0, lsm = 0.0;
    double lsn = 0.0, lso = 0.0, lsp = 0.0, lsq = 0.0, lsr = 0.0, lss = 0.0, lst = 0.0, lsu = 0.0, lsv = 0.0, lsw = 0.0, lsx = 0.0, lsy = 0.0, lsz = 0.0;
    double rsA = 0.0, rsB = 0.0, rsC = 0.0, rsD = 0.0, rsE = 0.0, rsF = 0.0, rsG = 0.0, rsH = 0.0, rsI = 0.0, rsJ = 0.0, rsK = 0.0, rsL = 0.0, rsM = 0.0;
    double rsN = 0.0, rsO = 0.0, rsP = 0.0, rsQ = 0.0, rsR = 0.0, rsS = 0.0, rsT = 0.0, rsU = 0.0, rsV = 0.0, rsW = 0.0, rsX = 0.0, rsY = 0.0, rsZ = 0.0;
    double rsa = 0.0, rsb = 0.0, rsc = 0.0, rsd = 0.0, rse = 0.0, rsf = 0.0, rsg = 0.0, rsh = 0.0, rsi = 0.0, rsj = 0.0, rsk = 0.0, rsl = 0.0, rsm = 0.0;
    double rsn = 0.0, rso = 0.0, rsp = 0.0, rsq = 0.0, rsr = 0.0, rss = 0.0, rst = 0.0, rsu = 0.0, rsv = 0.0, rsw = 0.0, rsx = 0.0, rsy = 0.0, rsz = 0.0;

    double firstDryL = 0.0, firstAvgL = 0.0;
    double firstDryR = 0.0, firstAvgR = 0.0;
    double firstdryl = 0.0, firstavgl = 0.0;
    double firstdryr = 0.0, firstavgr = 0.0;

    int prevDistance = 0;

    double roomNoiseL = 0.0;
    double roomNoiseR = 0.0006;
    int roomTimerL = 0;
    int roomTimerR = 0;

    double bypassMix = 0.0;

    float inPeakL = 0.0f, inPeakR = 0.0f;
    float outPeakL = 0.0f, outPeakR = 0.0f;
    bool inClip = false, outClip = false;
};

} // namespace forja::dsp::salao

namespace forja::dsp
{
using salao::SalaoDSP;
} // namespace forja::dsp