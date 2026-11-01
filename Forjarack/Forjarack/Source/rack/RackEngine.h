#pragma once

#include <array>
#include <atomic>
#include <memory>
#include <vector>

#include "forja/dsp/ForjaDspCommon.h"

#include "Module.h"
#include "RackState.h"

namespace forjarack
{

// Cadeia Serie 500: 8 slots em serie, trim de entrada, inversao de fase, trim
// de saida. O grafo (quais modulos existem) e montado na thread de mensagens
// sob suspendProcessing; o controle (bypass/solo/params/master)
// chega por um RackSnapshot POD lido a cada bloco, sem alocar no audio.
class RackEngine
{
public:
    struct MasterMeters
    {
        float inPeakL = 0.0f;
        float inPeakR = 0.0f;
        float outPeakL = 0.0f;
        float outPeakR = 0.0f;
        bool clip = false;
    };

    void prepare (double sampleRate, int maxBlockSamples);
    void release();

    // Thread de mensagens, sob suspendProcessing: troca o modulo de um slot e
    // o prepara com a taxa atual. Indice 0 (vazio) ou fora do registro remove
    // o modulo (pass-through).
    void setSlotModule (int slot, int moduleIndex);
    int slotModuleIndex (int slot) const noexcept { return slotIndices[(size_t) slot]; }

    // Consome o snapshot e escreve a saida 'wet' in place (ja com trims e fase).
    // Instrumentos geram em scratch pre-alocado e somam na cadeia; MIDI e
    // entregue aos slots instrument antes do process do mesmo bloco.
    void process (const RackSnapshot& snapshot, float* left, float* right, int numSamples,
                  const MidiEventSpan& midi = {});

    float readSlotPeak (int slot);           // pico de saida da gaveta
    // GR do ultimo bloco do slot em dB (negativa). Leitura por load (nao por
    // exchange): o valor e o estado do ultimo bloco, nao um evento a consumir.
    float readSlotGainReductionDb (int slot) const;
    MasterMeters readMasterMeters();
    void resetClip() noexcept { clipResetRequested.store (true, std::memory_order_release); }

    // Thread de mensagens. Da a cada modulo vivo a chance de liberar material
    // em desuso (kits substituidos do mini drum). Nao toca no caminho de audio.
    void idleModules();

    int latencySamples() const noexcept { return 0; }   // ambiente/reverb: PDC 0

private:
    static float trimGain (float normalized) noexcept;

    double sampleRate = 44100.0;
    std::array<std::unique_ptr<ModuleBase>, kNumSlots> modules {};
    std::array<int, kNumSlots> slotIndices {};

    // Scratch dos instrumentos: alocado em prepare, nunca no caminho de audio.
    // Bloco maior que o scratch salta o instrumento (pass-through documentado).
    std::vector<float> scratchL;
    std::vector<float> scratchR;

    std::array<std::atomic<float>, kNumSlots> slotPeaks {};
    std::array<std::atomic<float>, kNumSlots> slotGrDb {};
    std::atomic<float> inPeakL { 0.0f };
    std::atomic<float> inPeakR { 0.0f };
    std::atomic<float> outPeakL { 0.0f };
    std::atomic<float> outPeakR { 0.0f };
    std::atomic<bool> clipFlag { false };
    std::atomic<bool> clipResetRequested { false };
};

} // namespace forjarack
