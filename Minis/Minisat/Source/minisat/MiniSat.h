#pragma once

#include <atomic>

#include <juce_audio_basics/juce_audio_basics.h>

#include "forja/dsp/ForjaBrasa.h"

namespace forja::minisat
{

// Fronteira reutilizavel do MiniSat: a valve do BRASA (Airwindows Tube2, MIT)
// em um saturador de um unico knob. O DSP cru do adaptador
// forja::BrasaDSP nao e alterado; o knob DRIVE e uma camada Forja que mapeia
// o drive 0..1 sobre o pad de entrada documentado do Tube2:
//
//   input  = 0.5 + 0.5 * drive   (drive 0 = default documentado do BRASA;
//                                 drive 1 = sinal sem pad, clamp na valve)
//   tube   = 0.5                 (caracter fixo no default do BRASA)
//   output = 0 dB                (sem makeup gain: a camada nao compensa)
//
// Vive separado do AudioProcessor para que o plugin standalone e a gaveta
// do ForjaRack acionem exatamente o mesmo caminho de audio.
class MiniSat
{
public:
    static constexpr float kTubeFixed = 0.5f;
    static constexpr float kInputAtZeroDrive = 0.5f;
    static constexpr float kInputAtFullDrive = 1.0f;

    // Capacidade do scratch mono e do view do rack: blocos maiores sao
    // pass-through (o caminho integral nunca corta o sinal).
    static constexpr int kCapacity = 4096;

    // Mapeamento documentado drive -> Parameters do adaptador.
    static forja::BrasaDSP::Parameters parametersForDrive (float drive, bool bypassed) noexcept;

    MiniSat() = default;

    void prepare (double sampleRate, int maximumBlockSize);

    // Thread de audio, in place. Em mono (right nulo ou igual a left) o
    // wrapper copia o sinal para o scratch e o usa como canal direito, igual
    // ao BRASA: a valvula espera um par e o dither tem estado propio por canal.
    void process (float* left, float* right, int numSamples, bool bypassed) noexcept;

    // Escrita na thread de mensagens; o audio le no inicio de cada bloco.
    // default 0 = estado documentado do BRASA.
    void setDrive (float drive01) noexcept { drive.store (drive01, std::memory_order_relaxed); }
    float getDrive() const noexcept { return drive.load (std::memory_order_relaxed); }

    forja::BrasaDSP::MeterFrame readMeters (bool resetAfterRead = true) noexcept
    {
        return dsp.readMeters (resetAfterRead);
    }

    void resetClip() noexcept { dsp.resetClip(); }

    forja::BrasaDSP& getDsp() noexcept { return dsp; }

private:
    forja::BrasaDSP dsp;
    juce::AudioBuffer<float> monoScratch;
    std::atomic<float> drive { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MiniSat)
};

} // namespace forja::minisat
