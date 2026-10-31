#pragma once

#include <algorithm>
#include <array>
#include <cstdint>

#include <juce_audio_basics/juce_audio_basics.h>

#include "Mapping.h"
#include "SampleStore.h"

namespace forja::drumforge
{

// Uma voz toca um sample com interpolacao linear e um envelope de corte curto
// usado pelo choke. Nada aqui aloca: os buffers pertencem ao kit carregado.
struct Voice
{
    const SampleData* sample = nullptr;
    int padIndex = -1;
    int chokeGroup = 0;
    double position = 0.0;
    double increment = 1.0;
    float gain = 0.0f;
    float fade = 1.0f;
    float fadeStep = 0.0f;
    std::uint64_t age = 0;
    bool active = false;
};

class VoicePool
{
public:
    void prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
        reset();
    }

    void reset() noexcept
    {
        for (auto& voice : voices)
            voice = Voice {};

        counter = 0;
    }

    void start (const SampleData* sample, int padIndex, int chokeGroup, float gain,
                double increment) noexcept
    {
        Voice* target = nullptr;

        for (auto& voice : voices)
        {
            if (! voice.active)
            {
                target = &voice;
                break;
            }
        }

        // Roubo: a voz mais antiga cede o lugar, mesmo que ainda esteja tocando.
        if (target == nullptr)
        {
            target = &voices[0];
            for (auto& voice : voices)
                if (voice.age < target->age)
                    target = &voice;
        }

        target->sample = sample;
        target->padIndex = padIndex;
        target->chokeGroup = chokeGroup;
        target->position = 0.0;
        target->increment = increment > 0.0 ? increment : 1.0;
        target->gain = gain;
        target->fade = 1.0f;
        target->fadeStep = 0.0f;
        target->age = ++counter;
        target->active = true;
    }

    void choke (int group, double fadeSeconds) noexcept
    {
        if (group <= 0)
            return;

        const auto step = (float) (1.0 / std::max (0.001, fadeSeconds * sampleRate));

        for (auto& voice : voices)
            if (voice.active && voice.chokeGroup == group && voice.fadeStep <= 0.0f)
                voice.fadeStep = step;
    }

    int activeCount() const noexcept
    {
        int total = 0;
        for (const auto& voice : voices)
            if (voice.active)
                ++total;

        return total;
    }

    int activeCountForPad (int padIndex) const noexcept
    {
        int total = 0;
        for (const auto& voice : voices)
            if (voice.active && voice.padIndex == padIndex)
                ++total;

        return total;
    }

    // Cada buffer de pad recebe apenas as vozes daquele pad: os strips sao
    // independentes e um sample nunca vaza para o canal vizinho.
    void render (juce::AudioBuffer<float>& padBuffer, int padIndex, int startSample,
                 int numSamples) noexcept
    {
        if (numSamples <= 0 || padBuffer.getNumChannels() < 2)
            return;

        float* outLeft = padBuffer.getWritePointer (0, startSample);
        float* outRight = padBuffer.getWritePointer (1, startSample);

        for (auto& voice : voices)
        {
            if (! voice.active || voice.padIndex != padIndex)
                continue;

            const auto* data = voice.sample;

            if (data == nullptr || data->length < 2)
            {
                voice.active = false;
                continue;
            }

            const int length = data->length;
            const float* sourceLeft = data->audio.getReadPointer (0);
            const float* sourceRight = data->channels > 1 ? data->audio.getReadPointer (1) : nullptr;

            double position = voice.position;
            float fade = voice.fade;
            const float fadeStep = voice.fadeStep;

            int index = 0;
            while (index < numSamples)
            {
                if (position >= (double) (length - 1))
                {
                    voice.active = false;
                    break;
                }

                const int sampleIndex = (int) position;
                const float fraction = (float) (position - (double) sampleIndex);
                const float nextLeft = sourceLeft[sampleIndex + 1];
                const float left = sourceLeft[sampleIndex] + (nextLeft - sourceLeft[sampleIndex]) * fraction;

                float right = left;

                if (sourceRight != nullptr)
                {
                    const float nextRight = sourceRight[sampleIndex + 1];
                    right = sourceRight[sampleIndex] + (nextRight - sourceRight[sampleIndex]) * fraction;
                }

                const float amplitude = voice.gain * fade;
                outLeft[index] += left * amplitude;
                outRight[index] += right * amplitude;

                position += voice.increment;

                if (fadeStep > 0.0f)
                {
                    fade -= fadeStep;
                    if (fade <= 0.0f)
                    {
                        fade = 0.0f;
                        voice.active = false;
                        ++index;
                        break;
                    }
                }

                ++index;
            }

            voice.position = position;
            voice.fade = fade;
        }
    }

private:
    std::array<Voice, kMaxVoices> voices {};
    std::uint64_t counter = 0;
    double sampleRate = 48000.0;
};

} // namespace forja::drumforge
