#pragma once

#include <algorithm>
#include <array>
#include <atomic>

#include <juce_audio_basics/juce_audio_basics.h>

#include "ChannelStrip.h"
#include "Mapping.h"
#include "MasterBus.h"
#include "SampleStore.h"
#include "VoicePool.h"

namespace forja::drumforge
{

// Motor do Drumforge: 16 pads fixos, 64 vozes, 5 barramentos de saida
// (master + 4 auxiliares) e FX compartilhados no master. O kit entra como
// ponteiro somente leitura trocado por token, para o audio nunca alocar.
class DrumEngine
{
public:
    struct PadMeter
    {
        float peak = 0.0f;
        float reductionDb = 0.0f;
        bool clip = false;
    };

    struct MasterMeter
    {
        float peakLeft = 0.0f;
        float peakRight = 0.0f;
        float reductionDb = 0.0f;
        float compReductionDb = 0.0f;
        float limReductionDb = 0.0f;
        bool clip = false;
    };

    DrumEngine();

    void prepare (double newSampleRate, int maximumBlockSize);
    void reset();
    void setParameters (const EngineParameters& newParameters) noexcept { parameters = newParameters; }

    // Chamado na thread de mensagens. O motor adota o kit no proximo bloco e
    // publica acceptedKitToken() quando os samples antigos ja nao sao usados.
    void requestKit (const LoadedKit* kit, int token) noexcept;
    int acceptedKitToken() const noexcept { return acceptedToken.load (std::memory_order_acquire); }

    // Previa pela UI e pelo leitor de loops: SPSC sem alocacao. O offset e
    // relativo ao proximo bloco e casa com os eventos MIDI no renderSegment.
    void pushTrigger (int padIndex, float velocity, int offsetSamples = 0) noexcept;

    void process (const juce::MidiBuffer& midi,
                  const std::array<juce::AudioBuffer<float>*, kNumOutputBuses>& outputs,
                  int numSamples,
                  bool bypassed) noexcept;

    PadMeter readPadMeter (int padIndex) noexcept;
    MasterMeter readMasterMeter() noexcept;
    int activeVoices() const noexcept { return voices.activeCount(); }

    int lastTriggeredLayer (int padIndex) const noexcept;
    int lastTriggeredRoundRobin (int padIndex) const noexcept;
    int triggerCount() const noexcept { return totalTriggers; }
    bool isPadPlayable (int padIndex) const noexcept;
    int getCapacity() const noexcept { return capacity; }

    // Mapa nota -> pad do kit adotado (-1 quando a nota nao toca nada).
    int padForNote (int note) const noexcept
    {
        return juce::isPositiveAndBelow (note, 128) ? noteToPad[(size_t) note] : -1;
    }

private:
    struct TriggerEvent
    {
        int pad = -1;
        float velocity = 0.0f;
        int offset = 0;
    };

    static constexpr int triggerCapacity = 128;

    void adoptPendingKit() noexcept;
    void handleMessage (const juce::MidiMessage& message) noexcept;
    void triggerPad (int padIndex, float velocity) noexcept;
    void renderSegment (int offset, int numSamples) noexcept;
    void publishMeters() noexcept;
    bool popTrigger (TriggerEvent& event) noexcept;

    double sampleRate = 48000.0;
    int capacity = 512;
    EngineParameters parameters {};
    bool anySolo = false;

    std::array<ChannelStrip, kNumPads> strips;
    std::array<juce::AudioBuffer<float>, kNumPads> padBuffers;
    std::array<juce::AudioBuffer<float>, kNumOutputBuses> busBuffers;
    juce::AudioBuffer<float> fx1Buffer;
    juce::AudioBuffer<float> fx2Buffer;
    juce::AudioBuffer<float> bypassEnvelope;

    VoicePool voices;
    MasterBus master;

    std::atomic<const LoadedKit*> pendingKit { nullptr };
    std::atomic<int> pendingToken { 0 };
    std::atomic<int> acceptedToken { 0 };

    const LoadedKit* activeKit = nullptr;
    std::array<bool, kNumPads> padPlayable {};
    std::array<int, 128> noteToPad {};
    std::array<std::array<int, kMaxLayers>, kNumPads> roundRobinCursor {};

    std::array<TriggerEvent, triggerCapacity> triggerQueue {};
    std::atomic<int> triggerWrite { 0 };
    std::atomic<int> triggerRead { 0 };

    // Fila drenada no bloco, ordenada por offset (so a thread de audio usa).
    std::array<TriggerEvent, triggerCapacity> drainedTriggers {};
    int drainedCount = 0;

    std::array<std::atomic<float>, kNumPads> meterPeak;
    std::array<std::atomic<float>, kNumPads> meterReduction;
    std::array<std::atomic<bool>, kNumPads> meterClip;

    std::atomic<float> masterPeakLeft { 0.0f };
    std::atomic<float> masterPeakRight { 0.0f };
    std::atomic<float> masterReduction { 0.0f };
    std::atomic<float> masterCompReduction { 0.0f };
    std::atomic<float> masterLimReduction { 0.0f };
    std::atomic<bool> masterClip { false };

    std::array<int, kNumPads> lastLayer {};
    std::array<int, kNumPads> lastRoundRobin {};
    int totalTriggers = 0;

    double bypassSmoothing = 0.01;
    double bypassGain = 1.0;
};

} // namespace forja::drumforge
