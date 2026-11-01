#pragma once

#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>

#include "Module.h"

namespace forjarack
{

// Gaveta HOSPEDADA: um ModuleBase que envolve um plugin carregado em runtime
// (juce::AudioPluginInstance). E o mesmo contrato que os adaptadores Airwindows
// compilados, mas o DSP vem de fora: a pessoa solta um .vst3/.component/.dll em
// Documents/Forja/Rack e o ForjaRack o roda como gaveta, sem recompilar a suite.
//
// O core do rack (RackEngine/ModuleCatalog) NAO conhece esta classe nem JUCE de
// processors: ela so e instanciada pelo processador (thread de mensagens) e
// adotada via RackEngine::adoptSlotModule. Por isso este header puxa
// juce_audio_processors e nunca entra no teste headless forja_forjarack_engine.
//
// Efeito processia estereo in place (2 in / 2 out). Instrumento (bus de entrada
// vazio) consome o MIDI nativo do bloco e gera audio proprio no scratch do
// engine. A latencia reportada pelo plugin sobe para o host (PDC).
class HostedModule final : public ModuleBase
{
public:
    HostedModule() = default;
    ~HostedModule() override;

    HostedModule (const HostedModule&) = delete;
    HostedModule& operator= (const HostedModule&) = delete;

    // Adota uma instancia ja criada (vem do HostedLibrary). Detecta instrumento
    // pelo numero de buses de entrada. Devolve false + motivo se a instancia
    // nao serve ao rack (sem saida, por exemplo). Chamado na thread de mensagens,
    // antes de prepare.
    bool adoptInstance (std::unique_ptr<juce::AudioPluginInstance> instance,
                        juce::String& error);

    // ModuleBase -------------------------------------------------------------
    void prepare (double sampleRate) override;
    void setBlockSize (int maxSamples) noexcept override;
    void setParam (int index, float value) override;
    void setBypass (bool bypassed) override;
    void process (float* left, float* right, int numSamples) override;
    void processMidi (const MidiEventSpan& midi) noexcept override;
    float readOutputPeak() override;
    int latencySamples() const override;
    bool isInstrument() const noexcept override { return instrument; }

    // Consulta para a UI (thread de mensagens). Numero de knobs e rotulos do
    // plugin vivo, nao do ModuleSpec fixo: e por aqui que o SlotComponent do
    // tipo HOSPEDADO desenha seus controles dinamicos.
    int numHostedParams() const noexcept;
    juce::String hostedParamLabel (int index) const;
    // Valor normalizado (0..1) atual do param do plugin, para semear os knobs
    // do rack na adocao sem chutar o estado do plugin para 0.5.
    float hostedParamValue (int index) const;
    // Trava o setParam do rack quando a UI nativa do plugin esta aberta: assim
    // o editor proprio comanda os params sem ser sobrescrito a cada bloco.
    void setParamsLockedFromHost (bool locked) noexcept { paramsLocked = locked; }
    // Devolve o component do editor vivo. No JUCE 9 quem posse do editor do
    // plugin e o proprio AudioProcessor (createEditorAndMakeActive / getActiveEditor),
    // entao aqui NUNCA se devolve unique_ptr: a janela hospeda por referencia
    // (setContentNonOwned). Sem editor proprio, cai na lista generica de params.
    juce::Component* createNativeEditor();

    const juce::String& pluginName() const noexcept { return name; }

private:
    std::unique_ptr<juce::AudioPluginInstance> instance;
    std::unique_ptr<juce::AudioProcessorEditor> genericEditor;  // fallback sem UI propria
    juce::AudioBuffer<float> buffer;   // ponte float* -> AudioBuffer do plugin
    juce::MidiBuffer emptyMidi;        // efeito: bloco MIDI vazio reusado
    const juce::MidiBuffer* nativeMidi = nullptr;   // instrumento: aponta o MidiBuffer do host

    juce::String name;
    double sr = 44100.0;
    int block = 512;
    float lastPeak = 0.0f;
    bool bypassed = false;
    bool instrument = false;
    bool paramsLocked = false;   // UI nativa aberta: rack nao escreve nos params
};

} // namespace forjarack
