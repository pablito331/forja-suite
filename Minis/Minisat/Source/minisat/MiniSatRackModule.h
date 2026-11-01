#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "MiniSat.h"
#include "rack/Module.h"

namespace forja::minisat
{

// Gaveta de drive do ForjaRack: envolve a MESMA valve do MiniSat do plugin
// standalone pelo contrato ModuleBase. O MiniSat roda in place no scratch
// esterio do engine, identico ao caminho do plugin: um knob DRIVE 0..1 com o
// mapeamento documentado sobre o pad de entrada do Tube2.
//
// Nao usa AdapterModule porque o MeterFrame do BrasaDSP nao traz reducao de
// ganho (a valvula nao comprime); o pico de saida alimenta o LED do slot.
class MiniSatModule final : public forjarack::ModuleBase
{
public:
    MiniSatModule() = default;

    void prepare (double sampleRate) override;
    void setParam (int index, float value) override;
    void setBypass (bool bypassed) override { bypassedFlag = bypassed; }

    void process (float* left, float* right, int numSamples) override;
    float readOutputPeak() override { return lastPeak; }
    int latencySamples() const override { return 0; }

private:
    // Capacidade do caminho mono interno. Em esterio o modulo nao usa
    // scratch e blocos maiores seguem processados pelo adaptador.
    MiniSat sat;

    bool bypassedFlag = false;
    float lastPeak = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MiniSatModule)
};

// Publica a gaveta no registro do rack (thread de mensagens, idempotente). O
// ForjaRack a chama antes de montar a lista de escolhas dos slots, para o
// MINISAT entrar ACRESCENTADO ao fim sem reordenar os modulos ja gravados.
void registerMinisatModule();

} // namespace forja::minisat
