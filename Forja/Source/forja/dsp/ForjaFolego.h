#pragma once

// Forja adapter for Airwindows kStation (MIT, Chris Johnson).
// Reference: Forja/Source/forja/vendor/airwindows/kStation, commit
// b110fac3991fff7c4655cc2d25d6a3e221f9799c. processDoubleReplacing is
// transcribed literally: the 3x3 early-reflection
// network with its nine delay lines, the six-stage 6x6 feedback reverb per
// channel, the undersampled frequency Bezier inside the loop, the outer
// Bezier reconstruction and the early-reflection blend.
//
// Deviations: Forja adds metering and a 10 ms bypass crossfade. The original's
// commented-out 64-bit dither stays commented; only the RNG advances. The 88
// delay lines live on the heap (std::vector) because the type is also created
// on the stack by the test harness. The reverb RNG state is per instance and
// seeded through setNoiseState() by the parity harness. No allocation, logging
// or locks inside process().

#include "forja/dsp/ForjaDspCommon.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace forja::dsp::folego
{

inline constexpr int d3A = 636; inline constexpr int d3B = 787; inline constexpr int d3C = 816;
inline constexpr int d3D = 624; inline constexpr int d3E = 650; inline constexpr int d3F = 750;
inline constexpr int d3G = 445; inline constexpr int d3H = 517; inline constexpr int d3I = 656;
inline constexpr int d6A = 118; inline constexpr int d6B = 27; inline constexpr int d6C = 38; inline constexpr int d6D = 139; inline constexpr int d6E = 750; inline constexpr int d6F = 32; inline constexpr int d6G = 285; inline constexpr int d6H = 36; inline constexpr int d6I = 206; inline constexpr int d6J = 517; inline constexpr int d6K = 72; inline constexpr int d6L = 650; inline constexpr int d6M = 43; inline constexpr int d6N = 59; inline constexpr int d6O = 816; inline constexpr int d6P = 787; inline constexpr int d6Q = 73; inline constexpr int d6R = 91; inline constexpr int d6S = 34; inline constexpr int d6T = 9; inline constexpr int d6U = 656; inline constexpr int d6V = 445; inline constexpr int d6W = 6; inline constexpr int d6X = 47; inline constexpr int d6Y = 91; inline constexpr int d6ZA = 48; inline constexpr int d6ZB = 91; inline constexpr int d6ZC = 8; inline constexpr int d6ZD = 636; inline constexpr int d6ZE = 41; inline constexpr int d6ZF = 432; inline constexpr int d6ZG = 30; inline constexpr int d6ZH = 293; inline constexpr int d6ZI = 7; inline constexpr int d6ZJ = 115; inline constexpr int d6ZK = 624; //1 to 93 ms, 272 seat club  
inline constexpr int early[] = {6, 7, 8, 9, 27, 30, 32, 34, 36, 38, 41, 43, 47, 48, 59, 72, 73, 91, 91, 91, 115, 118, 139, 206, 285, 293, 432, 445, 517, 624, 636, 650, 656, 750, 787, 816};

class FolegoDSP
{
public:
    struct Parameters
    {
        float regen = 0.5f;
        float derez = 0.5f;
        float filter = 0.5f;
        float earlyRf = 0.5f;
        float position = 0.9f;
        float dryWet = 0.5f;
        bool bypass = false;
    };

    FolegoDSP() { reset(); }

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

        f6AL = f6BL = f6CL = f6DL = f6EL = f6FL = 0.0;
        f6FR = f6LR = f6RR = f6XR = f6ZER = f6ZKR = 0.0;
        avg6L = avg6R = 0.0;

        for (int x = 0; x < bez_total; ++x)
        {
            bez[x] = 0.0;
            bezF[x] = 0.0;
        }
        bez[bez_cycle] = 1.0;
        bezF[bez_cycle] = 1.0;

        fpdL = 22222u;
        fpdR = 22222u;

        bypassMix = params.bypass ? 1.0 : 0.0;
        clearMeters();
    }

    void setParameters (const Parameters& newParams) noexcept
    {
        params.regen = clamp01 (newParams.regen);
        params.derez = clamp01 (newParams.derez);
        params.filter = clamp01 (newParams.filter);
        params.earlyRf = clamp01 (newParams.earlyRf);
        params.position = clamp01 (newParams.position);
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

    // The space, the early reflections and both Bezier stages are musical.
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
        bez_InL,
        bez_InR,
        bez_UnInL,
        bez_UnInR,
        bez_SampL,
        bez_SampR,
        bez_AvgInSampL,
        bez_AvgInSampR,
        bez_AvgOutSampL,
        bez_AvgOutSampR,
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
        overallscale /= 44100.0;
        overallscale *= sampleRate;

        const float A = params.regen;
        const float B = params.derez;
        const float C = params.filter;
        const float D = params.earlyRf;
        const float E = params.position;
        const float F = params.dryWet;

        double fdb6ck = (0.0009765625+0.0009765625+0.001953125)*0.3333333;
        double reg6n = (1.0-pow(1.0-A,2.0))*fdb6ck;

        double derez = B*2.0;
        bool stepped = true;
        if (derez > 1.0) {
            stepped = false;
            derez = 1.0-(derez-1.0);
        }
        derez = fmin(fmax(derez/overallscale,0.0005),1.0);
        int bezFraction = (int)(1.0/derez);
        double bezTrim = (double)bezFraction/(bezFraction+1.0);
        if (stepped) {
            derez = 1.0 / bezFraction;
            bezTrim = 1.0-(derez*bezTrim);
        } else {
            derez /= (2.0/pow(overallscale,0.5-((overallscale-1.0)*0.0375)));
            bezTrim = 1.0-pow(derez*0.5,1.0/(derez*0.5));
        }

        double derezFreq = C*2.0;
        bool steppedFreq = true;
        if (derezFreq > 1.0) {
            steppedFreq = false;
            derezFreq = 1.0-(derezFreq-1.0);
        }
        derezFreq = fmin(fmax(derezFreq,0.0005),1.0);
        int bezFreqFraction = (int)(1.0/derezFreq);
        double bezFreqTrim = (double)bezFreqFraction/(bezFreqFraction+1.0);
        if (steppedFreq) {
            derezFreq = 1.0 / bezFreqFraction;
            bezFreqTrim = 1.0-(derezFreq*bezFreqTrim);
        } else {
            bezFreqTrim = 1.0-pow(derezFreq*0.5,1.0/(derezFreq*0.5));
        }

        double earlyLoudness = D;
        int start = (int)(E * 27.0);
        int ld3G = early[start];
        int ld3H = early[start+1];
        int ld3D = early[start+2];
        int ld3A = early[start+3];
        int ld3E = early[start+4];
        int ld3I = early[start+5];
        int ld3F = early[start+6];
        int ld3B = early[start+7];
        int ld3C = early[start+8];
        double wet = F;

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
		
		bez[bez_cycle] += derez;
		bez[bez_SampL] += ((inputSampleL+bez[bez_InL]) * derez);
		bez[bez_SampR] += ((inputSampleR+bez[bez_InR]) * derez);
		bez[bez_InL] = inputSampleL; bez[bez_InR] = inputSampleR;
		if (bez[bez_cycle] > 1.0) { //hit the end point and we do a reverb sample
			if (stepped) bez[bez_cycle] = 0.0;
			else bez[bez_cycle] -= 1.0;
			
			inputSampleL = (bez[bez_SampL]+bez[bez_AvgInSampL])*0.5;
			bez[bez_AvgInSampL] = bez[bez_SampL];
			inputSampleR = (bez[bez_SampR]+bez[bez_AvgInSampR])*0.5;
			bez[bez_AvgInSampR] = bez[bez_SampR];
			
			a3AL[c3AL] = inputSampleL;// + (f3AL * reg3n);
			a3BL[c3BL] = inputSampleL;// + (f3BL * reg3n);
			a3CL[c3CL] = inputSampleL;// + (f3CL * reg3n);
			
			a3CR[c3CR] = inputSampleR;// + (f3CR * reg3n);
			a3FR[c3FR] = inputSampleR;// + (f3FR * reg3n);
			a3IR[c3IR] = inputSampleR;// + (f3IR * reg3n);
			
			c3AL++; if (c3AL < 0 || c3AL > ld3A) c3AL = 0;
			c3BL++; if (c3BL < 0 || c3BL > ld3B) c3BL = 0;
			c3CL++; if (c3CL < 0 || c3CL > ld3C) c3CL = 0;
			c3CR++; if (c3CR < 0 || c3CR > ld3C) c3CR = 0;
			c3FR++; if (c3FR < 0 || c3FR > ld3F) c3FR = 0;
			c3IR++; if (c3IR < 0 || c3IR > ld3I) c3IR = 0;
			
			double o3AL = a3AL[c3AL-((c3AL > ld3A)?c3AL+1:0)];
			double o3BL = a3BL[c3BL-((c3BL > ld3B)?c3BL+1:0)];
			double o3CL = a3CL[c3CL-((c3CL > ld3C)?c3CL+1:0)];
			double o3CR = a3CR[c3CR-((c3CR > ld3C)?c3CR+1:0)];
			double o3FR = a3FR[c3FR-((c3FR > ld3F)?c3FR+1:0)];
			double o3IR = a3IR[c3IR-((c3IR > ld3I)?c3IR+1:0)];
			
			a3DL[c3DL] = (((o3BL + o3CL) * -2.0) + o3AL);
			a3EL[c3EL] = (((o3AL + o3CL) * -2.0) + o3BL);
			a3FL[c3FL] = (((o3AL + o3BL) * -2.0) + o3CL);
			a3BR[c3BR] = (((o3FR + o3IR) * -2.0) + o3CR);
			a3ER[c3ER] = (((o3CR + o3IR) * -2.0) + o3FR);
			a3HR[c3HR] = (((o3CR + o3FR) * -2.0) + o3IR);
			
			c3DL++; if (c3DL < 0 || c3DL > ld3D) c3DL = 0;
			c3EL++; if (c3EL < 0 || c3EL > ld3E) c3EL = 0;
			c3FL++; if (c3FL < 0 || c3FL > ld3F) c3FL = 0;
			c3BR++; if (c3BR < 0 || c3BR > ld3B) c3BR = 0;
			c3ER++; if (c3ER < 0 || c3ER > ld3E) c3ER = 0;
			c3HR++; if (c3HR < 0 || c3HR > ld3H) c3HR = 0;
			
			double o3DL = a3DL[c3DL-((c3DL > ld3D)?c3DL+1:0)];
			double o3EL = a3EL[c3EL-((c3EL > ld3E)?c3EL+1:0)];
			double o3FL = a3FL[c3FL-((c3FL > ld3F)?c3FL+1:0)];
			double o3BR = a3BR[c3BR-((c3BR > ld3B)?c3BR+1:0)];
			double o3ER = a3ER[c3ER-((c3ER > ld3E)?c3ER+1:0)];
			double o3HR = a3HR[c3HR-((c3HR > ld3H)?c3HR+1:0)];
			
			a3GL[c3GL] = (((o3EL + o3FL) * -2.0) + o3DL);
			a3HL[c3HL] = (((o3DL + o3FL) * -2.0) + o3EL);
			a3IL[c3IL] = (((o3DL + o3EL) * -2.0) + o3FL);
			a3AR[c3AR] = (((o3ER + o3HR) * -2.0) + o3BR);
			a3DR[c3DR] = (((o3BR + o3HR) * -2.0) + o3ER);
			a3GR[c3GR] = (((o3BR + o3ER) * -2.0) + o3HR);
			
			c3GL++; if (c3GL < 0 || c3GL > ld3G) c3GL = 0;
			c3HL++; if (c3HL < 0 || c3HL > ld3H) c3HL = 0;
			c3IL++; if (c3IL < 0 || c3IL > ld3I) c3IL = 0;
			c3AR++; if (c3AR < 0 || c3AR > ld3A) c3AR = 0;
			c3DR++; if (c3DR < 0 || c3DR > ld3D) c3DR = 0;
			c3GR++; if (c3GR < 0 || c3GR > ld3G) c3GR = 0;
			
			double o3GL = a3GL[c3GL-((c3GL > ld3G)?c3GL+1:0)];
			double o3HL = a3HL[c3HL-((c3HL > ld3H)?c3HL+1:0)];
			double o3IL = a3IL[c3IL-((c3IL > ld3I)?c3IL+1:0)];
			double o3AR = a3AR[c3AR-((c3AR > ld3A)?c3AR+1:0)];
			double o3DR = a3DR[c3DR-((c3DR > ld3D)?c3DR+1:0)];
			double o3GR = a3GR[c3GR-((c3GR > ld3G)?c3GR+1:0)];
			
			double inputSampleL = (o3GL + o3HL + o3IL)*0.03125;
			double inputSampleR = (o3AR + o3DR + o3GR)*0.03125;
			
			double earlyReflectionL = inputSampleL;
			double earlyReflectionR = inputSampleR;
			
			bezF[bez_cycle] += derezFreq;
			bezF[bez_SampL] += ((inputSampleL+bezF[bez_InL]) * derezFreq);
			bezF[bez_SampR] += ((inputSampleL+bezF[bez_InR]) * derezFreq);
			bezF[bez_InL] = inputSampleL; bezF[bez_InR] = inputSampleR;
			if (bezF[bez_cycle] > 1.0) { //hit the end point and we do a filter sample
				if (steppedFreq) bezF[bez_cycle] = 0.0;
				else bezF[bez_cycle] -= 1.0;
				bezF[bez_CL] = bezF[bez_BL];
				bezF[bez_BL] = bezF[bez_AL];
				bezF[bez_AL] = (bezF[bez_SampL]+bezF[bez_AvgInSampL])*0.5;
				bezF[bez_AvgInSampL] = bezF[bez_SampL]; bezF[bez_SampL] = 0.0;
				bezF[bez_CR] = bezF[bez_BR];
				bezF[bez_BR] = bezF[bez_AR];
				bezF[bez_AR] = (bezF[bez_SampR]+bezF[bez_AvgInSampR])*0.5;
				bezF[bez_AvgInSampR] = bezF[bez_SampR]; bezF[bez_SampR] = 0.0;
			}
			double X = bezF[bez_cycle]*bezFreqTrim;
			double CBLfreq = (bezF[bez_CL]*(1.0-X))+(bezF[bez_BL]*X);
			double BALfreq = (bezF[bez_BL]*(1.0-X))+(bezF[bez_AL]*X);
			double CBALfreq = (bezF[bez_BL]+(CBLfreq*(1.0-X))+(BALfreq*X))*0.0625;
			double CBRfreq = (bezF[bez_CR]*(1.0-X))+(bezF[bez_BR]*X);
			double BARfreq = (bezF[bez_BR]*(1.0-X))+(bezF[bez_AR]*X);
			double CBARfreq = (bezF[bez_BR]+(CBRfreq*(1.0-X))+(BARfreq*X))*0.0625;
			inputSampleL = CBALfreq+bezF[bez_AvgOutSampL];
			bezF[bez_AvgOutSampL] = CBALfreq;
			inputSampleR = CBARfreq+bezF[bez_AvgOutSampR];
			bezF[bez_AvgOutSampR] = CBARfreq;
			
			a6AL[c6AL] = inputSampleL + (f6BL * reg6n);
			a6BL[c6BL] = inputSampleL + (f6CL * reg6n);
			a6CL[c6CL] = inputSampleL + (f6DL * reg6n);
			a6DL[c6DL] = inputSampleL + (f6EL * reg6n);
			a6EL[c6EL] = inputSampleL + (f6FL * reg6n);
			a6FL[c6FL] = inputSampleL + (f6AL * reg6n);
			
			c6AL++; if (c6AL < 0 || c6AL > d6A) c6AL = 0;
			c6BL++; if (c6BL < 0 || c6BL > d6B) c6BL = 0;
			c6CL++; if (c6CL < 0 || c6CL > d6C) c6CL = 0;
			c6DL++; if (c6DL < 0 || c6DL > d6D) c6DL = 0;
			c6EL++; if (c6EL < 0 || c6EL > d6E) c6EL = 0;
			c6FL++; if (c6FL < 0 || c6FL > d6F) c6FL = 0;
			
			double o6AL = a6AL[c6AL-((c6AL > d6A)?d6A+1:0)];
			double o6BL = a6BL[c6BL-((c6BL > d6B)?d6B+1:0)];
			double o6CL = a6CL[c6CL-((c6CL > d6C)?d6C+1:0)];
			double o6DL = a6DL[c6DL-((c6DL > d6D)?d6D+1:0)];
			double o6EL = a6EL[c6EL-((c6EL > d6E)?d6E+1:0)];
			double o6FL = a6FL[c6FL-((c6FL > d6F)?d6F+1:0)];
			
			a6FR[c6FR] = inputSampleR + (f6LR * reg6n);
			a6LR[c6LR] = inputSampleR + (f6RR * reg6n);
			a6RR[c6RR] = inputSampleR + (f6XR * reg6n);
			a6XR[c6XR] = inputSampleR + (f6ZER * reg6n);
			a6ZER[c6ZER] = inputSampleR + (f6ZKR * reg6n);
			a6ZKR[c6ZKR] = inputSampleR + (f6FR * reg6n);
			
			c6FR++; if (c6FR < 0 || c6FR > d6F) c6FR = 0;
			c6LR++; if (c6LR < 0 || c6LR > d6L) c6LR = 0;
			c6RR++; if (c6RR < 0 || c6RR > d6R) c6RR = 0;
			c6XR++; if (c6XR < 0 || c6XR > d6X) c6XR = 0;
			c6ZER++; if (c6ZER < 0 || c6ZER > d6ZE) c6ZER = 0;
			c6ZKR++; if (c6ZKR < 0 || c6ZKR > d6ZK) c6ZKR = 0;
			
			double o6FR = a6FR[c6FR-((c6FR > d6F)?d6F+1:0)];
			double o6LR = a6LR[c6LR-((c6LR > d6L)?d6L+1:0)];
			double o6RR = a6RR[c6RR-((c6RR > d6R)?d6R+1:0)];
			double o6XR = a6XR[c6XR-((c6XR > d6X)?d6X+1:0)];
			double o6ZER = a6ZER[c6ZER-((c6ZER > d6ZE)?d6ZE+1:0)];
			double o6ZKR = a6ZKR[c6ZKR-((c6ZKR > d6ZK)?d6ZK+1:0)];
			
			//-------- one
			
			a6GL[c6GL] = ((o6AL*2.0) - (o6BL + o6CL + o6DL + o6EL + o6FL));
			a6HL[c6HL] = ((o6BL*2.0) - (o6AL + o6CL + o6DL + o6EL + o6FL));
			a6IL[c6IL] = ((o6CL*2.0) - (o6AL + o6BL + o6DL + o6EL + o6FL));
			a6JL[c6JL] = ((o6DL*2.0) - (o6AL + o6BL + o6CL + o6EL + o6FL));
			a6KL[c6KL] = ((o6EL*2.0) - (o6AL + o6BL + o6CL + o6DL + o6FL));
			a6LL[c6LL] = ((o6FL*2.0) - (o6AL + o6BL + o6CL + o6DL + o6EL));
			
			c6GL++; if (c6GL < 0 || c6GL > d6G) c6GL = 0;
			c6HL++; if (c6HL < 0 || c6HL > d6H) c6HL = 0;
			c6IL++; if (c6IL < 0 || c6IL > d6I) c6IL = 0;
			c6JL++; if (c6JL < 0 || c6JL > d6J) c6JL = 0;
			c6KL++; if (c6KL < 0 || c6KL > d6K) c6KL = 0;
			c6LL++; if (c6LL < 0 || c6LL > d6L) c6LL = 0;
			
			double o6GL = a6GL[c6GL-((c6GL > d6G)?d6G+1:0)];
			double o6HL = a6HL[c6HL-((c6HL > d6H)?d6H+1:0)];
			double o6IL = a6IL[c6IL-((c6IL > d6I)?d6I+1:0)];
			double o6JL = a6JL[c6JL-((c6JL > d6J)?d6J+1:0)];
			double o6KL = a6KL[c6KL-((c6KL > d6K)?d6K+1:0)];
			double o6LL = a6LL[c6LL-((c6LL > d6L)?d6L+1:0)];
			
			a6ER[c6ER] = ((o6FR*2.0) - (o6LR + o6RR + o6XR + o6ZER + o6ZKR));
			a6KR[c6KR] = ((o6LR*2.0) - (o6FR + o6RR + o6XR + o6ZER + o6ZKR));
			a6QR[c6QR] = ((o6RR*2.0) - (o6FR + o6LR + o6XR + o6ZER + o6ZKR));
			a6WR[c6WR] = ((o6XR*2.0) - (o6FR + o6LR + o6RR + o6ZER + o6ZKR));
			a6ZDR[c6ZDR] = ((o6ZER*2.0) - (o6FR + o6LR + o6RR + o6XR + o6ZKR));			
			a6ZJR[c6ZJR] = ((o6ZKR*2.0) - (o6FR + o6LR + o6RR + o6XR + o6ZER));			
			
			c6ER++; if (c6ER < 0 || c6ER > d6E) c6ER = 0;
			c6KR++; if (c6KR < 0 || c6KR > d6K) c6KR = 0;
			c6QR++; if (c6QR < 0 || c6QR > d6Q) c6QR = 0;
			c6WR++; if (c6WR < 0 || c6WR > d6W) c6WR = 0;
			c6ZDR++; if (c6ZDR < 0 || c6ZDR > d6ZD) c6ZDR = 0;
			c6ZJR++; if (c6ZJR < 0 || c6ZJR > d6ZJ) c6ZJR = 0;
			
			double o6ER = a6ER[c6ER-((c6ER > d6E)?d6E+1:0)];
			double o6KR = a6KR[c6KR-((c6KR > d6K)?d6K+1:0)];
			double o6QR = a6QR[c6QR-((c6QR > d6Q)?d6Q+1:0)];
			double o6WR = a6WR[c6WR-((c6WR > d6W)?d6W+1:0)];
			double o6ZDR = a6ZDR[c6ZDR-((c6ZDR > d6ZD)?d6ZD+1:0)];
			double o6ZJR = a6ZJR[c6ZJR-((c6ZJR > d6ZJ)?d6ZJ+1:0)];
			
			//-------- two
			
			a6ML[c6ML] = ((o6GL*2.0) - (o6HL + o6IL + o6JL + o6KL + o6LL));
			a6NL[c6NL] = ((o6HL*2.0) - (o6GL + o6IL + o6JL + o6KL + o6LL));
			a6OL[c6OL] = ((o6IL*2.0) - (o6GL + o6HL + o6JL + o6KL + o6LL));
			a6PL[c6PL] = ((o6JL*2.0) - (o6GL + o6HL + o6IL + o6KL + o6LL));
			a6QL[c6QL] = ((o6KL*2.0) - (o6GL + o6HL + o6IL + o6JL + o6LL));
			a6RL[c6RL] = ((o6LL*2.0) - (o6GL + o6HL + o6IL + o6JL + o6KL));
			
			c6ML++; if (c6ML < 0 || c6ML > d6M) c6ML = 0;
			c6NL++; if (c6NL < 0 || c6NL > d6N) c6NL = 0;
			c6OL++; if (c6OL < 0 || c6OL > d6O) c6OL = 0;
			c6PL++; if (c6PL < 0 || c6PL > d6P) c6PL = 0;
			c6QL++; if (c6QL < 0 || c6QL > d6Q) c6QL = 0;
			c6RL++; if (c6RL < 0 || c6RL > d6R) c6RL = 0;
			
			double o6ML = a6ML[c6ML-((c6ML > d6M)?d6M+1:0)];
			double o6NL = a6NL[c6NL-((c6NL > d6N)?d6N+1:0)];
			double o6OL = a6OL[c6OL-((c6OL > d6O)?d6O+1:0)];
			double o6PL = a6PL[c6PL-((c6PL > d6P)?d6P+1:0)];
			double o6QL = a6QL[c6QL-((c6QL > d6Q)?d6Q+1:0)];
			double o6RL = a6RL[c6RL-((c6RL > d6R)?d6R+1:0)];
			
			a6DR[c6DR] = ((o6ER*2.0) - (o6KR + o6QR + o6WR + o6ZDR + o6ZJR));
			a6JR[c6JR] = ((o6KR*2.0) - (o6ER + o6QR + o6WR + o6ZDR + o6ZJR));
			a6PR[c6PR] = ((o6QR*2.0) - (o6ER + o6KR + o6WR + o6ZDR + o6ZJR));
			a6VR[c6VR] = ((o6WR*2.0) - (o6ER + o6KR + o6QR + o6ZDR + o6ZJR));
			a6ZCR[c6ZCR] = ((o6ZDR*2.0) - (o6ER + o6KR + o6QR + o6WR + o6ZJR));
			a6ZIR[c6ZIR] = ((o6ZJR*2.0) - (o6ER + o6KR + o6QR + o6WR + o6ZDR));
			
			c6DR++; if (c6DR < 0 || c6DR > d6D) c6DR = 0;
			c6JR++; if (c6JR < 0 || c6JR > d6J) c6JR = 0;
			c6PR++; if (c6PR < 0 || c6PR > d6P) c6PR = 0;
			c6VR++; if (c6VR < 0 || c6VR > d6V) c6VR = 0;
			c6ZCR++; if (c6ZCR < 0 || c6ZCR > d6ZC) c6ZCR = 0;
			c6ZIR++; if (c6ZIR < 0 || c6ZIR > d6ZI) c6ZIR = 0;
			
			double o6DR = a6DR[c6DR-((c6DR > d6D)?d6D+1:0)];
			double o6JR = a6JR[c6JR-((c6JR > d6J)?d6J+1:0)];
			double o6PR = a6PR[c6PR-((c6PR > d6P)?d6P+1:0)];
			double o6VR = a6VR[c6VR-((c6VR > d6V)?d6V+1:0)];
			double o6ZCR = a6ZCR[c6ZCR-((c6ZCR > d6ZC)?d6ZC+1:0)];
			double o6ZIR = a6ZIR[c6ZIR-((c6ZIR > d6ZI)?d6ZI+1:0)];
			
			//-------- three
			
			a6SL[c6SL] = ((o6ML*2.0) - (o6NL + o6OL + o6PL + o6QL + o6RL));
			a6TL[c6TL] = ((o6NL*2.0) - (o6ML + o6OL + o6PL + o6QL + o6RL));
			a6UL[c6UL] = ((o6OL*2.0) - (o6ML + o6NL + o6PL + o6QL + o6RL));
			a6VL[c6VL] = ((o6PL*2.0) - (o6ML + o6NL + o6OL + o6QL + o6RL));
			a6WL[c6WL] = ((o6QL*2.0) - (o6ML + o6NL + o6OL + o6PL + o6RL));
			a6XL[c6XL] = ((o6RL*2.0) - (o6ML + o6NL + o6OL + o6PL + o6QL));
			
			c6SL++; if (c6SL < 0 || c6SL > d6S) c6SL = 0;
			c6TL++; if (c6TL < 0 || c6TL > d6T) c6TL = 0;
			c6UL++; if (c6UL < 0 || c6UL > d6U) c6UL = 0;
			c6VL++; if (c6VL < 0 || c6VL > d6V) c6VL = 0;
			c6WL++; if (c6WL < 0 || c6WL > d6W) c6WL = 0;
			c6XL++; if (c6XL < 0 || c6XL > d6X) c6XL = 0;
			
			double o6SL = a6SL[c6SL-((c6SL > d6S)?d6S+1:0)];
			double o6TL = a6TL[c6TL-((c6TL > d6T)?d6T+1:0)];
			double o6UL = a6UL[c6UL-((c6UL > d6U)?d6U+1:0)];
			double o6VL = a6VL[c6VL-((c6VL > d6V)?d6V+1:0)];
			double o6WL = a6WL[c6WL-((c6WL > d6W)?d6W+1:0)];
			double o6XL = a6XL[c6XL-((c6XL > d6X)?d6X+1:0)];
			
			a6CR[c6CR] = ((o6DR*2.0) - (o6JR + o6PR + o6VR + o6ZCR + o6ZIR));
			a6IR[c6IR] = ((o6JR*2.0) - (o6DR + o6PR + o6VR + o6ZCR + o6ZIR));
			a6OR[c6OR] = ((o6PR*2.0) - (o6DR + o6JR + o6VR + o6ZCR + o6ZIR));
			a6UR[c6UR] = ((o6VR*2.0) - (o6DR + o6JR + o6PR + o6ZCR + o6ZIR));
			a6ZBR[c6ZBR] = ((o6ZCR*2.0) - (o6DR + o6JR + o6PR + o6VR + o6ZIR));
			a6ZHR[c6ZHR] = ((o6ZIR*2.0) - (o6DR + o6JR + o6PR + o6VR + o6ZCR));
			
			c6CR++; if (c6CR < 0 || c6CR > d6C) c6CR = 0;
			c6IR++; if (c6IR < 0 || c6IR > d6I) c6IR = 0;
			c6OR++; if (c6OR < 0 || c6OR > d6O) c6OR = 0;
			c6UR++; if (c6UR < 0 || c6UR > d6U) c6UR = 0;
			c6ZBR++; if (c6ZBR < 0 || c6ZBR > d6ZB) c6ZBR = 0;
			c6ZHR++; if (c6ZHR < 0 || c6ZHR > d6ZH) c6ZHR = 0;
			
			double o6CR = a6CR[c6CR-((c6CR > d6C)?d6C+1:0)];
			double o6IR = a6IR[c6IR-((c6IR > d6I)?d6I+1:0)];
			double o6OR = a6OR[c6OR-((c6OR > d6O)?d6O+1:0)];
			double o6UR = a6UR[c6UR-((c6UR > d6U)?d6U+1:0)];
			double o6ZBR = a6ZBR[c6ZBR-((c6ZBR > d6ZB)?d6ZB+1:0)];
			double o6ZHR = a6ZHR[c6ZHR-((c6ZHR > d6ZH)?d6ZH+1:0)];
			
			//-------- four
			
			a6YL[c6YL] = ((o6SL*2.0) - (o6TL + o6UL + o6VL + o6WL + o6XL));
			a6ZAL[c6ZAL] = ((o6TL*2.0) - (o6SL + o6UL + o6VL + o6WL + o6XL));
			a6ZBL[c6ZBL] = ((o6UL*2.0) - (o6SL + o6TL + o6VL + o6WL + o6XL));
			a6ZCL[c6ZCL] = ((o6VL*2.0) - (o6SL + o6TL + o6UL + o6WL + o6XL));
			a6ZDL[c6ZDL] = ((o6WL*2.0) - (o6SL + o6TL + o6UL + o6VL + o6XL));
			a6ZEL[c6ZEL] = ((o6XL*2.0) - (o6SL + o6TL + o6UL + o6VL + o6WL));
			
			c6YL++; if (c6YL < 0 || c6YL > d6Y) c6YL = 0;
			c6ZAL++; if (c6ZAL < 0 || c6ZAL > d6ZA) c6ZAL = 0;
			c6ZBL++; if (c6ZBL < 0 || c6ZBL > d6ZB) c6ZBL = 0;
			c6ZCL++; if (c6ZCL < 0 || c6ZCL > d6ZC) c6ZCL = 0;
			c6ZDL++; if (c6ZDL < 0 || c6ZDL > d6ZD) c6ZDL = 0;
			c6ZEL++; if (c6ZEL < 0 || c6ZEL > d6ZE) c6ZEL = 0;
			
			double o6YL = a6YL[c6YL-((c6YL > d6Y)?d6Y+1:0)];
			double o6ZAL = a6ZAL[c6ZAL-((c6ZAL > d6ZA)?d6ZA+1:0)];
			double o6ZBL = a6ZBL[c6ZBL-((c6ZBL > d6ZB)?d6ZB+1:0)];
			double o6ZCL = a6ZCL[c6ZCL-((c6ZCL > d6ZC)?d6ZC+1:0)];
			double o6ZDL = a6ZDL[c6ZDL-((c6ZDL > d6ZD)?d6ZD+1:0)];
			double o6ZEL = a6ZEL[c6ZEL-((c6ZEL > d6ZE)?d6ZE+1:0)];
			
			a6BR[c6BR] = ((o6CR*2.0) - (o6IR + o6OR + o6UR + o6ZBR + o6ZHR));
			a6HR[c6HR] = ((o6IR*2.0) - (o6CR + o6OR + o6UR + o6ZBR + o6ZHR));
			a6NR[c6NR] = ((o6OR*2.0) - (o6CR + o6IR + o6UR + o6ZBR + o6ZHR));
			a6TR[c6TR] = ((o6UR*2.0) - (o6CR + o6IR + o6OR + o6ZBR + o6ZHR));
			a6ZAR[c6ZAR] = ((o6ZBR*2.0) - (o6CR + o6IR + o6OR + o6UR + o6ZHR));
			a6ZGR[c6ZGR] = ((o6ZHR*2.0) - (o6CR + o6IR + o6OR + o6UR + o6ZBR));
			
			c6BR++; if (c6BR < 0 || c6BR > d6B) c6BR = 0;
			c6HR++; if (c6HR < 0 || c6HR > d6H) c6HR = 0;
			c6NR++; if (c6NR < 0 || c6NR > d6N) c6NR = 0;
			c6TR++; if (c6TR < 0 || c6TR > d6T) c6TR = 0;
			c6ZBR++; if (c6ZBR < 0 || c6ZBR > d6ZB) c6ZBR = 0;
			c6ZGR++; if (c6ZGR < 0 || c6ZGR > d6ZG) c6ZGR = 0;
			
			double o6BR = a6BR[c6BR-((c6BR > d6B)?d6B+1:0)];
			double o6HR = a6HR[c6HR-((c6HR > d6H)?d6H+1:0)];
			double o6NR = a6NR[c6NR-((c6NR > d6N)?d6N+1:0)];
			double o6TR = a6TR[c6TR-((c6TR > d6T)?d6T+1:0)];
			double o6ZAR = a6ZAR[c6ZAR-((c6ZAR > d6ZA)?d6ZA+1:0)];
			double o6ZGR = a6ZGR[c6ZGR-((c6ZGR > d6ZG)?d6ZG+1:0)];
			
			//-------- five
			
			a6ZFL[c6ZFL] = ((o6YL*2.0) - (o6ZAL + o6ZBL + o6ZCL + o6ZDL + o6ZEL));
			a6ZGL[c6ZGL] = ((o6ZAL*2.0) - (o6YL + o6ZBL + o6ZCL + o6ZDL + o6ZEL));
			a6ZHL[c6ZHL] = ((o6ZBL*2.0) - (o6YL + o6ZAL + o6ZCL + o6ZDL + o6ZEL));
			a6ZIL[c6ZIL] = ((o6ZCL*2.0) - (o6YL + o6ZAL + o6ZBL + o6ZDL + o6ZEL));
			a6ZJL[c6ZJL] = ((o6ZDL*2.0) - (o6YL + o6ZAL + o6ZBL + o6ZCL + o6ZEL));
			a6ZKL[c6ZKL] = ((o6ZEL*2.0) - (o6YL + o6ZAL + o6ZBL + o6ZCL + o6ZDL));
			
			c6ZFL++; if (c6ZFL < 0 || c6ZFL > d6ZF) c6ZFL = 0;
			c6ZGL++; if (c6ZGL < 0 || c6ZGL > d6ZG) c6ZGL = 0;
			c6ZHL++; if (c6ZHL < 0 || c6ZHL > d6ZH) c6ZHL = 0;
			c6ZIL++; if (c6ZIL < 0 || c6ZIL > d6ZI) c6ZIL = 0;
			c6ZJL++; if (c6ZJL < 0 || c6ZJL > d6ZJ) c6ZJL = 0;
			c6ZKL++; if (c6ZKL < 0 || c6ZKL > d6ZK) c6ZKL = 0;
			
			double o6ZFL = a6ZFL[c6ZFL-((c6ZFL > d6ZF)?d6ZF+1:0)];
			double o6ZGL = a6ZGL[c6ZGL-((c6ZGL > d6ZG)?d6ZG+1:0)];
			double o6ZHL = a6ZHL[c6ZHL-((c6ZHL > d6ZH)?d6ZH+1:0)];
			double o6ZIL = a6ZIL[c6ZIL-((c6ZIL > d6ZI)?d6ZI+1:0)];
			double o6ZJL = a6ZJL[c6ZJL-((c6ZJL > d6ZJ)?d6ZJ+1:0)];
			double o6ZKL = a6ZKL[c6ZKL-((c6ZKL > d6ZK)?d6ZK+1:0)];
			
			a6AR[c6AR] = ((o6BR*2.0) - (o6HR + o6NR + o6TR + o6ZAR + o6ZGR));
			a6GR[c6GR] = ((o6HR*2.0) - (o6BR + o6NR + o6TR + o6ZAR + o6ZGR));
			a6MR[c6MR] = ((o6NR*2.0) - (o6BR + o6HR + o6TR + o6ZAR + o6ZGR));
			a6SR[c6SR] = ((o6TR*2.0) - (o6BR + o6HR + o6NR + o6ZAR + o6ZGR));
			a6YR[c6YR] = ((o6ZAR*2.0) - (o6BR + o6HR + o6NR + o6TR + o6ZGR));
			a6ZFR[c6ZFR] = ((o6ZGR*2.0) - (o6BR + o6HR + o6NR + o6TR + o6ZAR));
			
			c6AR++; if (c6AR < 0 || c6AR > d6A) c6AR = 0;
			c6GR++; if (c6GR < 0 || c6GR > d6G) c6GR = 0;
			c6MR++; if (c6MR < 0 || c6MR > d6M) c6MR = 0;
			c6SR++; if (c6SR < 0 || c6SR > d6S) c6SR = 0;
			c6YR++; if (c6YR < 0 || c6YR > d6Y) c6YR = 0;
			c6ZFR++; if (c6ZFR < 0 || c6ZFR > d6ZF) c6ZFR = 0;
			
			double o6AR = a6AR[c6AR-((c6AR > d6A)?d6A+1:0)];
			double o6GR = a6GR[c6GR-((c6GR > d6G)?d6G+1:0)];
			double o6MR = a6MR[c6MR-((c6MR > d6M)?d6M+1:0)];
			double o6SR = a6SR[c6SR-((c6SR > d6S)?d6S+1:0)];
			double o6YR = a6YR[c6YR-((c6YR > d6Y)?d6Y+1:0)];
			double o6ZFR = a6ZFR[c6ZFR-((c6ZFR > d6ZF)?d6ZF+1:0)];
			
			//-------- six
			
			f6AL = ((o6AR*2.0) - (o6GR + o6MR + o6SR + o6YR + o6ZFR));
			f6BL = ((o6GR*2.0) - (o6AR + o6MR + o6SR + o6YR + o6ZFR));
			f6CL = ((o6MR*2.0) - (o6AR + o6GR + o6SR + o6YR + o6ZFR));
			f6DL = ((o6SR*2.0) - (o6AR + o6GR + o6MR + o6YR + o6ZFR));
			f6EL = ((o6YR*2.0) - (o6AR + o6GR + o6MR + o6SR + o6ZFR));
			f6FL = ((o6ZFR*2.0) - (o6AR + o6GR + o6MR + o6SR + o6YR));
			
			f6FR = ((o6ZFL*2.0) - (o6ZGL + o6ZHL + o6ZIL + o6ZJL + o6ZKL));
			f6LR = ((o6ZGL*2.0) - (o6ZFL + o6ZHL + o6ZIL + o6ZJL + o6ZKL));
			f6RR = ((o6ZHL*2.0) - (o6ZFL + o6ZGL + o6ZIL + o6ZJL + o6ZKL));
			f6XR = ((o6ZIL*2.0) - (o6ZFL + o6ZGL + o6ZHL + o6ZJL + o6ZKL));
			f6ZER = ((o6ZJL*2.0) - (o6ZFL + o6ZGL + o6ZHL + o6ZIL + o6ZKL));
			f6ZKR = ((o6ZKL*2.0) - (o6ZFL + o6ZGL + o6ZHL + o6ZIL + o6ZJL));
			
			inputSampleL = (o6ZFL + o6ZGL + o6ZHL + o6ZIL + o6ZJL + o6ZKL)*0.001953125;
			inputSampleR = (o6AR + o6GR + o6MR + o6SR + o6YR + o6ZFR)*0.001953125;
			
			f6AL = (f6AL+f6AL+f6AL+fabs(avg6L))*0.25; avg6L = f6AL;
			f6FR = (f6FR+f6FR+f6FR+fabs(avg6R))*0.25; avg6R = f6FR;
			//manipulating deep reverb tail for realism
			
			inputSampleL += (earlyReflectionL * earlyLoudness);
			inputSampleR += (earlyReflectionR * earlyLoudness);
			
			bez[bez_CL] = bez[bez_BL];
			bez[bez_BL] = bez[bez_AL];
			bez[bez_AL] = inputSampleL;
			bez[bez_SampL] = 0.0;
			
			bez[bez_CR] = bez[bez_BR];
			bez[bez_BR] = bez[bez_AR];
			bez[bez_AR] = inputSampleR;
			bez[bez_SampR] = 0.0;
		}
		double X = bez[bez_cycle]*bezTrim;
		double CBL = (bez[bez_CL]*(1.0-X))+(bez[bez_BL]*X);
		double CBR = (bez[bez_CR]*(1.0-X))+(bez[bez_BR]*X);
		double BAL = (bez[bez_BL]*(1.0-X))+(bez[bez_AL]*X);
		double BAR = (bez[bez_BR]*(1.0-X))+(bez[bez_AR]*X);
		double CBAL = (bez[bez_BL]+(CBL*(1.0-X))+(BAL*X))*-0.0625;
		double CBAR = (bez[bez_BR]+(CBR*(1.0-X))+(BAR*X))*-0.0625;
		inputSampleL = CBAL+bez[bez_AvgOutSampL]; bez[bez_AvgOutSampL] = CBAL;
		inputSampleR = CBAR+bez[bez_AvgOutSampR]; bez[bez_AvgOutSampR] = CBAR;
		
		inputSampleL = (inputSampleL * wet)+(drySampleL * (1.0-wet));
		inputSampleR = (inputSampleR * wet)+(drySampleR * (1.0-wet));
		
		//begin 64 bit stereo floating point dither
		//int expon; frexp((double)inputSampleL, &expon);
		fpdL ^= fpdL << 13; fpdL ^= fpdL >> 17; fpdL ^= fpdL << 5;
		//inputSampleL += ((double(fpdL)-uint32_t(0x7fffffff)) * 1.1e-44l * pow(2,expon+62));
		//frexp((double)inputSampleR, &expon);
		fpdR ^= fpdR << 13; fpdR ^= fpdR >> 17; fpdR ^= fpdR << 5;
		//inputSampleR += ((double(fpdR)-uint32_t(0x7fffffff)) * 1.1e-44l * pow(2,expon+62));
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

    double f6AL = 0.0, f6BL = 0.0, f6CL = 0.0, f6DL = 0.0, f6EL = 0.0, f6FL = 0.0;
    double f6FR = 0.0, f6LR = 0.0, f6RR = 0.0, f6XR = 0.0, f6ZER = 0.0, f6ZKR = 0.0;
    double avg6L = 0.0, avg6R = 0.0;

    double bez[bez_total] {};
    double bezF[bez_total] {};

    double bypassMix = 0.0;

    float inPeakL = 0.0f, inPeakR = 0.0f;
    float outPeakL = 0.0f, outPeakR = 0.0f;
    bool inClip = false, outClip = false;
};

} // namespace forja::dsp::folego

namespace forja::dsp
{
using folego::FolegoDSP;
} // namespace forja::dsp