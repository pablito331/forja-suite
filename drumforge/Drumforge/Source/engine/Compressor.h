#pragma once

#include <algorithm>
#include <cmath>

#include "Mapping.h"

namespace forja::drumforge::detail
{

// Compressor de canal: detector de pico estereo-linkado, joelho suave de 6 dB
// e reducao suavizada em dois tempos. A reducao e publicada em dB (negativo).
class Compressor
{
public:
    void prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
        attackCoefficient = mapping::smoothingCoefficient (3.0, sampleRate);
        releaseCoefficient = mapping::smoothingCoefficient (120.0, sampleRate);
        detectorRelease = mapping::smoothingCoefficient (60.0, sampleRate);
        reset();
    }

    void reset() noexcept
    {
        detector = 0.0;
        reductionDb = 0.0;
        minimumReductionDb = 0.0;
    }

    void setSettings (float newThresholdDb, float newRatio) noexcept
    {
        thresholdDb = std::clamp (newThresholdDb, -60.0f, 0.0f);
        ratio = std::max (1.0f, newRatio);
    }

    void process (float* left, float* right, int numSamples) noexcept
    {
        const float slope = 1.0f - 1.0f / ratio;

        for (int i = 0; i < numSamples; ++i)
        {
            const float peak = std::max (std::abs (left[i]), std::abs (right[i]));
            const double target = (double) peak;

            if (target > detector)
                detector = target;
            else
                detector += (target - detector) * detectorRelease;

            const float levelDb = detector > 1.0e-6 ? 20.0f * std::log10 ((float) detector)
                                                   : -120.0f;
            const float over = levelDb - thresholdDb;

            float targetReduction = 0.0f;
            if (over > kneeDb * 0.5f)
                targetReduction = over * slope;
            else if (over > -kneeDb * 0.5f)
            {
                const float knee = (over + kneeDb * 0.5f) / kneeDb;
                targetReduction = slope * knee * knee * kneeDb * 0.5f;
            }

            targetReduction = std::min (targetReduction, 48.0f);

            const double coefficient = targetReduction > reductionDb ? attackCoefficient
                                                                    : releaseCoefficient;
            reductionDb += ((double) targetReduction - reductionDb) * coefficient;
            minimumReductionDb = std::min (minimumReductionDb, (float) -reductionDb);

            const float gain = mapping::dbToGain ((float) -reductionDb);
            left[i] *= gain;
            right[i] *= gain;
        }
    }

    float currentReductionDb() const noexcept { return (float) -reductionDb; }

    float readMinimumReductionDb() noexcept
    {
        const float value = minimumReductionDb;
        minimumReductionDb = 0.0f;
        return value;
    }

private:
    static constexpr float kneeDb = 6.0f;

    double sampleRate = 48000.0;
    double detector = 0.0;
    double reductionDb = 0.0;
    float minimumReductionDb = 0.0f;
    float thresholdDb = -12.0f;
    float ratio = 2.0f;
    double attackCoefficient = 0.01;
    double releaseCoefficient = 0.001;
    double detectorRelease = 0.001;
};

} // namespace forja::drumforge::detail
