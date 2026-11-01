#pragma once

#include <atomic>
#include <string>
#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

#include "MiniDrum.h"
#include "rack/Module.h"

namespace forja::minidrum
{

// Gaveta de instrumento do ForjaRack: envolve o MESMO tocador MiniDrum do
// plugin standalone pelo contrato ModuleBase. O modulo recebe MIDI como span
// POD, materializa num MidiBuffer e soma o par estereo no scratch que o engine
// da serie joga na cadeia. Os quatro auxiliares do Drumforge nao existem aqui:
// so a saida master (bus 0) e entregue.
//
// Dois knobs: 0 VOLUME (0..1) e 1 KIT (indice na biblioteca compartilhada com
// o Drumforge). O KIT e lido no audio como indice desejado (atomic POD) e a
// carga em si acontece em idle(), na thread de mensagens: o caminho de audio
// nunca toca no disco.
class MiniDrumModule final : public forjarack::ModuleBase
{
public:
    MiniDrumModule();

    void prepare (double sampleRate) override;
    void setParam (int index, float value) override;
    void setBypass (bool bypassed) override { bypassedFlag = bypassed; }

    void process (float* left, float* right, int numSamples) override;
    float readOutputPeak() override { return lastPeak; }
    int latencySamples() const override { return 0; }

    bool isInstrument() const noexcept override { return true; }
    void processMidi (const forjarack::MidiEventSpan& span) noexcept override;

    // Roda na thread de mensagens (RackEngine::idleModules): libera kits
    // substituidos, colhe a carga externa pendente e atende um pedido de
    // troca de KIT feito pelo audio ou pela lista da gaveta.
    void idle() override;

    // Gaveta de arquivos: a biblioteca vira seletor e o FileChooser do rack
    // pode trazer um kit de fora. Thread de mensagens (IHostedSlot no processor).
    bool supportsDrawerFiles() const noexcept override { return true; }
    std::vector<std::string> drawerFileNames() const override;
    void drawerRefreshLibrary() override { refreshLibrary(); reselectAfterRefresh(); }
    std::string drawerCurrentName() const override;
    bool drawerSelectFile (const std::string& name) override;
    void drawerLoadExternalFile (const std::string& path) override;

    // Unica janela de teste para o estado interno do wrapper (o rack nunca usa).
    float getVolumeForTest() const noexcept { return drum.getVolume(); }

private:
    // Capacidade interna do engine. Blocos maiores que isto sao pulados (o
    // scratch ja vem zerado: pass-through silencioso), igual ao rack.
    static constexpr int engineCapacity = 4096;

    MiniDrum drum;
    juce::MidiBuffer midi;

    // Lista da biblioteca, montada na thread de mensagens (prepare/idle). O
    // audio so ve o tamanho por kitCount e o indice desejado por desiredKit.
    std::vector<juce::File> kits;

    bool bypassedFlag = false;
    float lastPeak = 0.0f;

    std::atomic<int> kitCount { 0 };
    std::atomic<int> desiredKit { -1 };
    int loadedKit = -1;

    // Ultima escolha da gaveta (thread de mensagens). Persiste atraves de um
    // re-prepare: o kit escolhido e re-adotado contra a lista reescaneada.
    std::string selectedPath;                 // caminho da escolha ativa (lista ou externo)
    std::atomic<bool> externalActive { false };
    std::string externalPath;                 // carga externa pendente (esvazia ao colher)
    std::string loadedExternalName;           // nome exibido do kit externo carregado

    void refreshLibrary();
    // Resolve selectedPath contra kits[] depois de um escaneamento: indice da
    // lista (knob manda) ou externo (knob fica mudo ate a proxima escolha).
    void reselectAfterRefresh();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MiniDrumModule)
};

// Publica a gaveta no registro do rack (thread de mensagens, idempotente). O
// ForjaRack a chama antes de montar a lista de escolhas dos slots, para o
// MINIDRUM entrar ACRESCENTADO ao fim sem reordenar os modulos ja gravados.
void registerMinidrumModule();

} // namespace forja::minidrum
