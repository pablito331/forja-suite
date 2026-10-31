#pragma once

#include <atomic>
#include <cmath>
#include <cstddef>

#include "engine/DrumEngine.h"
#include "loops/LoopLibrary.h"

namespace forja::drumforge::loops
{

// Preview do loop MIDI: dispara as pecas no engine no BPM pedido. O ponteiro da
// sequencia chega por token, como o kit, para nao alocar nem travar no audio.
class LoopPlayer
{
public:
    LoopPlayer() noexcept
    {
        pending.store (nullptr, std::memory_order_relaxed);
    }

    void prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
        stop();
    }

    // Thread de mensagens. O chamador mantem a sequencia viva enquanto o token
    // estiver pendente ou ativo.
    void request (const Sequence* sequence, int token) noexcept
    {
        pending.store (sequence, std::memory_order_release);
        pendingToken.store (token, std::memory_order_release);
    }

    void start() noexcept
    {
        restartPending.store (true, std::memory_order_release);
        playing.store (true, std::memory_order_release);
    }

    void stop() noexcept
    {
        playing.store (false, std::memory_order_release);
    }

    bool isPlaying() const noexcept
    {
        return playing.load (std::memory_order_acquire);
    }

    int activeToken() const noexcept
    {
        return activeTokenValue.load (std::memory_order_acquire);
    }

    double positionBeats() const noexcept
    {
        return uiBeat.load (std::memory_order_relaxed);
    }

    // Thread de audio, antes de DrumEngine::process: os golpes entram na fila
    // de disparos com offset em amostras, entao o preview cai no ponto exato.
    void process (int numSamples, double bpm, DrumEngine& engine) noexcept
    {
        const int token = pendingToken.load (std::memory_order_acquire);

        if (token != activeTokenValue.load (std::memory_order_relaxed))
        {
            active = pending.load (std::memory_order_acquire);
            activeTokenValue.store (token, std::memory_order_release);
            restart();
            uiBeat.store (0.0, std::memory_order_relaxed);
        }

        if (active == nullptr || numSamples <= 0 || bpm <= 0.0 || sampleRate <= 0.0)
            return;

        if (restartPending.exchange (false, std::memory_order_acq_rel))
        {
            restart();
            uiBeat.store (0.0, std::memory_order_relaxed);
        }

        if (! playing.load (std::memory_order_acquire))
            return;

        const double loopBeats = active->beats > 0.0 ? active->beats : 4.0;
        const double beatsPerSample = bpm / (60.0 * sampleRate);
        double remaining = (double) numSamples;
        double consumed = 0.0;

        while (remaining > 0.0)
        {
            // O bloco pode atravessar o ponto de loop: o corte acontece no
            // compasso inteiro e o cursor volta para o inicio do arquivo.
            const double step = juce::jmin (remaining, (loopBeats - beatCursor) / beatsPerSample);
            const double windowEnd = beatCursor + step * beatsPerSample;

            while (nextHit < active->hits.size() && active->hits[nextHit].beat < windowEnd)
            {
                const auto& hit = active->hits[nextHit++];

                if (hit.beat < beatCursor - 1.0e-9)
                    continue;

                const int pad = engine.padForNote (hit.note);

                if (pad < 0)
                    continue;

                const double offset = consumed + (hit.beat - beatCursor) / beatsPerSample;
                engine.pushTrigger (pad, hit.velocity,
                                    juce::jlimit (0, numSamples - 1, (int) offset));
            }

            consumed += step;
            remaining -= step;
            beatCursor = windowEnd;

            if (beatCursor >= loopBeats - 1.0e-9)
            {
                beatCursor = 0.0;
                nextHit = 0;
            }
            else if (step <= 0.0)
            {
                break;
            }
        }

        uiBeat.store (beatCursor, std::memory_order_relaxed);
    }

private:
    void restart() noexcept
    {
        nextHit = 0;
        beatCursor = 0.0;
    }

    double sampleRate = 48000.0;

    std::atomic<bool> playing { false };
    std::atomic<bool> restartPending { false };
    std::atomic<const Sequence*> pending { nullptr };
    std::atomic<int> pendingToken { 0 };
    std::atomic<int> activeTokenValue { 0 };

    const Sequence* active = nullptr;
    std::size_t nextHit = 0;
    double beatCursor = 0.0;

    std::atomic<double> uiBeat { 0.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoopPlayer)
};

} // namespace forja::drumforge::loops
