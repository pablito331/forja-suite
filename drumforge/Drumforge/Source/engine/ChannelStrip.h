#pragma once

#include <algorithm>
#include <array>
#include <cmath>

#include "Compressor.h"
#include "Filters.h"
#include "Mapping.h"

namespace forja::drumforge
{

// Canal de um pad: ganho de entrada (TRIM), compressor, EQ de quatro estagios
// (HPF + graves + medios + agudos), pan e fader com mute/solo. A saida ja esta
// pronta para o barramento; os sends sao lidos como ganhos pos-fader.
class ChannelStrip
{
public:
    void prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
        gainSmoothing = mapping::smoothingCoefficient (12.0, sampleRate);
        panSmoothing = mapping::smoothingCoefficient (18.0, sampleRate);
        compressor.prepare (sampleRate);
        reset();
    }

    void reset() noexcept
    {
        for (auto& filter : filters)
            filter.reset();

        compressor.reset();
        inputGain = 1.0;
        outputGain = 1.0;
        panLeft = panRight = 0.7071067811865476;
        send1Gain = send2Gain = 0.0;
        peak = 0.0f;
        clip = false;
        cachedParameters = {};
        cachedParametersValid = false;
    }

    void setParameters (const PadParameters& newParameters, bool silenced) noexcept
    {
        parameters = newParameters;
        muteTarget = silenced ? 0.0 : 1.0;

        compressor.setSettings (mapping::thresholdDb (parameters.compThreshold),
                                mapping::ratio (parameters.compRatio));

        updateFilters();
    }

    void process (float* left, float* right, int numSamples) noexcept
    {
        if (numSamples <= 0)
            return;

        const double inputTarget = (double) mapping::dbToGain (mapping::trimDb (parameters.trim));
        const double outputTarget = (double) mapping::dbToGain (mapping::faderDb (parameters.fader))
                                  * muteTarget;
        const float pan = mapping::panUnit (parameters.pan);
        const double panTargetLeft = (double) mapping::panGainLeft (pan);
        const double panTargetRight = (double) mapping::panGainRight (pan);
        const double sendTarget1 = (double) mapping::unit (parameters.send1);
        const double sendTarget2 = (double) mapping::unit (parameters.send2);
        const bool invert = parameters.phase;

        for (int i = 0; i < numSamples; ++i)
        {
            inputGain += (inputTarget - inputGain) * gainSmoothing;
            const float sign = invert ? -1.0f : 1.0f;
            float x = left[i] * (float) inputGain * sign;
            float y = right[i] * (float) inputGain * sign;

            x = filters[0].process (x);
            y = filters[1].process (y);
            left[i] = x;
            right[i] = y;
        }

        compressor.process (left, right, numSamples);

        for (int i = 0; i < numSamples; ++i)
        {
            float x = left[i];
            float y = right[i];

            x = filters[2].process (x);
            y = filters[3].process (y);
            x = filters[4].process (x);
            y = filters[5].process (y);
            x = filters[6].process (x);
            y = filters[7].process (y);

            outputGain += (outputTarget - outputGain) * gainSmoothing;
            panLeft += (panTargetLeft - panLeft) * panSmoothing;
            panRight += (panTargetRight - panRight) * panSmoothing;
            send1Gain += (sendTarget1 - send1Gain) * gainSmoothing;
            send2Gain += (sendTarget2 - send2Gain) * gainSmoothing;

            x = (float) (x * outputGain * panLeft);
            y = (float) (y * outputGain * panRight);

            const float magnitude = std::max (std::abs (x), std::abs (y));
            peak = std::max (peak, magnitude);
            if (magnitude >= 1.0f)
                clip = true;

            left[i] = x;
            right[i] = y;
        }
    }

    int outputBus() const noexcept { return parameters.output; }
    double getSend1() const noexcept { return send1Gain; }
    double getSend2() const noexcept { return send2Gain; }

    float readPeak() noexcept
    {
        const float value = peak;
        peak = 0.0f;
        return value;
    }

    float readReductionDb() noexcept { return compressor.readMinimumReductionDb(); }

    bool readClip() noexcept
    {
        const bool value = clip;
        clip = false;
        return value;
    }

private:
    void updateFilters() noexcept
    {
        if (cachedParametersValid
            && cachedParameters.hpf == parameters.hpf
            && cachedParameters.eqLow == parameters.eqLow
            && cachedParameters.eqMid == parameters.eqMid
            && cachedParameters.eqHigh == parameters.eqHigh)
        {
            return;
        }

        cachedParameters = parameters;
        cachedParametersValid = true;

        const auto highPass = detail::coefficients::highPass (sampleRate,
                                                              mapping::hpfHz (parameters.hpf));
        const auto low = detail::coefficients::lowShelf (sampleRate, 80.0,
                                                         mapping::eqDb (parameters.eqLow));
        const auto mid = detail::coefficients::peaking (sampleRate, 1200.0,
                                                        mapping::eqDb (parameters.eqMid));
        const auto high = detail::coefficients::highShelf (sampleRate, 8000.0,
                                                           mapping::eqDb (parameters.eqHigh));

        // Dois canais por estagio: 0/1 HPF, 2/3 graves, 4/5 medios, 6/7 agudos.
        for (int channel = 0; channel < 2; ++channel)
        {
            filters[(size_t) channel].setCoefficients (highPass);
            filters[(size_t) (2 + channel)].setCoefficients (low);
            filters[(size_t) (4 + channel)].setCoefficients (mid);
            filters[(size_t) (6 + channel)].setCoefficients (high);
        }
    }

    double sampleRate = 48000.0;
    PadParameters parameters {};
    PadParameters cachedParameters {};
    bool cachedParametersValid = false;

    std::array<detail::Biquad, 8> filters {};
    detail::Compressor compressor;

    double inputGain = 1.0;
    double outputGain = 1.0;
    double panLeft = 0.7071067811865476;
    double panRight = 0.7071067811865476;
    double send1Gain = 0.0;
    double send2Gain = 0.0;
    double muteTarget = 1.0;
    double gainSmoothing = 0.01;
    double panSmoothing = 0.01;

    float peak = 0.0f;
    bool clip = false;
};

} // namespace forja::drumforge
