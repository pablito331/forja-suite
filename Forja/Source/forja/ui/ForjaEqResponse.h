#pragma once

#include <juce_dsp/juce_dsp.h>

#include "ForjaEqCurve.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace forja::ui
{

// Mede a resposta do próprio adapter por FFT de um impulso em nível baixo.
// Evita duplicar a matemática do filtro na UI: a curva é o que o DSP faz.
template <typename Adapter, typename ApplyParameters>
std::vector<ForjaEqCurve::Point> measureEqResponse (Adapter& adapter,
                                                    double sampleRate,
                                                    ApplyParameters&& applyParameters,
                                                    int pointCount = 220)
{
    constexpr int fftOrder = 16;
    constexpr int fftSize = 1 << fftOrder;
    constexpr float impulseLevel = 1.0e-3f; // -60 dBFS mantém modelos não lineares quase lineares

    adapter.prepare (sampleRate);
    applyParameters (adapter);

    std::vector<double> left ((std::size_t) fftSize, 0.0);
    std::vector<double> right ((std::size_t) fftSize, 0.0);
    left[0] = (double) impulseLevel;
    right[0] = (double) impulseLevel;

    adapter.processDouble (left.data(), right.data(), fftSize);

    juce::dsp::FFT fft (fftOrder);
    std::vector<float> spectrum ((std::size_t) fftSize * 2, 0.0f);

    for (int i = 0; i < fftSize; ++i)
        spectrum[(std::size_t) i] = (float) left[(std::size_t) i];

    fft.performFrequencyOnlyForwardTransform (spectrum.data());

    const auto magnitudeAt = [&] (double frequency)
    {
        const double bin = frequency * (double) fftSize / sampleRate;
        const int lower = juce::jlimit (0, fftSize / 2 - 1, (int) std::floor (bin));
        const int upper = juce::jmin (fftSize / 2 - 1, lower + 1);
        const float t = (float) (bin - (double) lower);

        const float a = spectrum[(std::size_t) lower];
        const float b = spectrum[(std::size_t) upper];
        return a + (b - a) * t;
    };

    std::vector<ForjaEqCurve::Point> points;
    points.reserve ((std::size_t) pointCount);

    for (int i = 0; i < pointCount; ++i)
    {
        const float norm = (float) i / (float) (pointCount - 1);
        const float frequency = 20.0f * std::pow (1000.0f, norm);
        const float magnitude = magnitudeAt ((double) frequency) / impulseLevel;
        const float db = magnitude > 0.0f ? 20.0f * std::log10 (magnitude) : -60.0f;

        points.push_back ({ frequency, juce::jlimit (-60.0f, 24.0f, db) });
    }

    return points;
}

} // namespace forja::ui
