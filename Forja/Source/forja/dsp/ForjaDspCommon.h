#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace forja::dsp
{

// Same double as M_PI on every platform Forja targets.
inline constexpr double kPi = 3.14159265358979323846;

inline float dbToGain (float db) noexcept
{
    return std::pow (10.0f, db * 0.05f);
}

inline float gainToDb (float gain) noexcept
{
    return gain > 0.0f ? 20.0f * std::log10 (gain) : -100.0f;
}

inline float clamp01 (float value) noexcept
{
    return std::clamp (value, 0.0f, 1.0f);
}

struct MeterFrame
{
    float inPeakL = 0.0f;
    float inPeakR = 0.0f;
    float outPeakL = 0.0f;
    float outPeakR = 0.0f;
    float gainReductionDb = 0.0f;
    bool inClip = false;
    bool outClip = false;
};

// Rampa linear de 10 ms por padrão, em double e sem alocação.
struct ForjaSmoother
{
    void prepare (double sampleRate, double rampSeconds = 0.010) noexcept
    {
        const double samples = std::max (1.0, sampleRate * rampSeconds);
        step = 1.0 / samples;
    }

    void reset (double value) noexcept
    {
        current = value;
        target = value;
    }

    void setTarget (double value) noexcept { target = value; }

    double getTarget() const noexcept { return target; }

    // Avança um sample e devolve o valor suavizado.
    double next() noexcept
    {
        if (current < target)
        {
            current += step;
            if (current > target)
                current = target;
        }
        else if (current > target)
        {
            current -= step;
            if (current < target)
                current = target;
        }

        return current;
    }

private:
    double current = 0.0;
    double target = 0.0;
    double step = 0.01;
};

// ClipOnly2: the same state machine and floating-point constants used by
// Pressure5/Pop2, with one extra delay slot so spacing == 16 is safe.
struct ClipOnly2Channel
{
    double lastSample = 0.0;
    double intermediate[17] {};
    bool wasPosClip = false;
    bool wasNegClip = false;

    void reset() noexcept
    {
        lastSample = 0.0;
        for (double& value : intermediate)
            value = 0.0;
        wasPosClip = false;
        wasNegClip = false;
    }

    double process (double sample, int spacing) noexcept
    {
        if (sample > 4.0) sample = 4.0;
        if (sample < -4.0) sample = -4.0;

        if (wasPosClip)
        {
            if (sample < lastSample)
                lastSample = 0.7058208 + (sample * 0.2609148);
            else
                lastSample = 0.2491717 + (lastSample * 0.7390851);
        }
        wasPosClip = false;
        if (sample > 0.9549925859)
        {
            wasPosClip = true;
            sample = 0.7058208 + (lastSample * 0.2609148);
        }

        if (wasNegClip)
        {
            if (sample > lastSample)
                lastSample = -0.7058208 + (sample * 0.2609148);
            else
                lastSample = -0.2491717 + (lastSample * 0.7390851);
        }
        wasNegClip = false;
        if (sample < -0.9549925859)
        {
            wasNegClip = true;
            sample = -0.7058208 + (lastSample * 0.2609148);
        }

        intermediate[spacing] = sample;
        const double output = lastSample;

        for (int x = spacing; x > 0; --x)
            intermediate[x - 1] = intermediate[x];

        lastSample = intermediate[0];
        return output;
    }
};

// ClipOnly3: the compressed chunk Airwindows drops into X2Buss/ToTape9.
// Unlike ClipOnly2 it has no ±4 pre-clamp; it relies on its own 0.9085097
// thresholds. `spacing` is the oversampling ratio; `noise` comes from the
// caller so each model keeps its own RNG. Buffers cover spacing up to 32.
struct ClipOnly3Channel
{
    static constexpr int maxSpacing = 32;

    double lastSample = 0.0;
    double intermediate[maxSpacing + 1] {};
    double slew[maxSpacing * 2 + 1] {};
    bool wasPosClip = false;
    bool wasNegClip = false;

    void reset() noexcept
    {
        lastSample = 0.0;
        for (double& value : intermediate)
            value = 0.0;
        for (double& value : slew)
            value = 0.0;
        wasPosClip = false;
        wasNegClip = false;
    }

    double process (double inputSample, int spacing, double noise) noexcept
    {
        if (wasPosClip)
        {
            if (inputSample < lastSample)
                lastSample = (0.9085097 * noise) + (inputSample * (1.0 - noise));
            else
                lastSample = 0.94;
        }
        wasPosClip = false;
        if (inputSample > 0.9085097)
        {
            wasPosClip = true;
            inputSample = (0.9085097 * noise) + (lastSample * (1.0 - noise));
        }

        if (wasNegClip)
        {
            if (inputSample > lastSample)
                lastSample = (-0.9085097 * noise) + (inputSample * (1.0 - noise));
            else
                lastSample = -0.94;
        }
        wasNegClip = false;
        if (inputSample < -0.9085097)
        {
            wasNegClip = true;
            inputSample = (-0.9085097 * noise) + (lastSample * (1.0 - noise));
        }

        slew[spacing * 2] = std::fabs (lastSample - inputSample);
        for (int x = spacing * 2; x > 0; --x)
            slew[x - 1] = slew[x];

        intermediate[spacing] = inputSample;
        inputSample = lastSample;
        for (int x = spacing; x > 0; --x)
            intermediate[x - 1] = intermediate[x];
        lastSample = intermediate[0];

        if (wasPosClip || wasNegClip)
        {
            for (int x = spacing; x > 0; --x)
                lastSample += intermediate[x];
            lastSample /= spacing;
        }

        double finalSlew = 0.0;
        for (int x = spacing * 2; x >= 0; --x)
            if (finalSlew < slew[x])
                finalSlew = slew[x];

        const double postclip = 0.94 / (1.0 + (finalSlew * 1.3986013));
        if (inputSample > postclip)
            inputSample = postclip;
        if (inputSample < -postclip)
            inputSample = -postclip;

        return inputSample;
    }
};

} // namespace forja::dsp
