#pragma once

#include <atomic>
#include <string>
#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

#include "SoundFontEngine.h"
#include "rack/Module.h"

namespace forja::canivete
{

// Gaveta de instrumento do ForjaRack: envolve o MESMO player SoundFontEngine do
// plugin standalone pelo contrato ModuleBase. O modulo recebe MIDI como span
// POD (so notas on/off, o nucleo do rack nao expoe CC/program change), o
// materializa num MidiBuffer e soma o par estereo no scratch que a serie joga
// na cadeia. Multi-timbral completo (program change, CC, pitch bend) so existe
// no caminho do plugin, onde o host entrega um MidiBuffer inteiro.
//
// Dois knobs: 0 VOLUME (0..1) e 1 BANK (indice na biblioteca
// Documents/Forja/Canivete). O BANK e lido como indice desejado (atomic POD) e
// a carga em si acontece em idle(), na thread de mensagens: o caminho de audio
// nunca toca no disco.
class CaniveteModule final : public forjarack::ModuleBase
{
public:
    CaniveteModule();

    void prepare (double sampleRate) override;
    void setParam (int index, float value) override;
    void setBypass (bool bypassed) override { bypassedFlag = bypassed; }

    void process (float* left, float* right, int numSamples) override;
    float readOutputPeak() override { return lastPeak; }
    int latencySamples() const override { return 0; }

    bool isInstrument() const noexcept override { return true; }
    void processMidi (const forjarack::MidiEventSpan& span) noexcept override;

    // Roda na thread de mensagens (RackEngine::idleModules): colhe o resultado
    // da carga em background (da lista ou do FileChooser da gaveta), libera
    // banks substituidos e atende um pedido de troca de BANK feito pelo audio
    // ou pela lista da gaveta.
    void idle() override;

    // Gaveta de arquivos: a biblioteca vira seletor e o FileChooser do rack
    // pode trazer um soundfont de fora. Thread de mensagens (IHostedSlot).
    bool supportsDrawerFiles() const noexcept override { return true; }
    std::vector<std::string> drawerFileNames() const override;
    void drawerRefreshLibrary() override { refreshLibrary(); reselectAfterRefresh(); }
    std::string drawerCurrentName() const override;
    bool drawerSelectFile (const std::string& name) override;
    void drawerLoadExternalFile (const std::string& path) override;

    // Unica janela de teste para o estado interno do wrapper (o rack nunca usa).
    float getVolumeForTest() const noexcept { return engine.getVolume(); }

private:
    // Capacidade interna do engine. Blocos maiores que isto sao pulados (o
    // scratch ja vem zerado: pass-through silencioso), igual ao rack.
    static constexpr int engineCapacity = 4096;

    SoundFontEngine engine;
    juce::MidiBuffer midi;

    // Lista da biblioteca, montada na thread de mensagens (prepare/idle). O
    // audio so ve o tamanho por bankCount e o indice desejado por desiredBank.
    std::vector<juce::File> banks;

    bool bypassedFlag = false;
    float lastPeak = 0.0f;

    std::atomic<int> bankCount { 0 };
    std::atomic<int> desiredBank { -1 };
    int loadedBank = -1;

    // Ultima escolha da gaveta (thread de mensagens). Persiste atraves de um
    // re-prepare: o bank escolhido e re-adotado contra a lista reescaneada.
    std::string selectedPath;                 // caminho da escolha ativa (lista ou externo)
    std::atomic<bool> externalActive { false };
    std::string externalPath;                 // carga externa pendente (esvazia ao colher)
    std::string loadedExternalName;           // nome exibido do bank externo carregado

    void refreshLibrary();
    // Resolve selectedPath contra banks[] depois de um escaneamento: indice da
    // lista (knob manda) ou externo (knob fica mudo ate a proxima escolha).
    void reselectAfterRefresh();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CaniveteModule)
};

// Publica a gaveta no registro do rack (thread de mensagens, idempotente). O
// ForjaRack a chama antes de montar a lista de escolhas dos slots, para o
// CANIVETE entrar ACRESCENTADO ao fim sem reordenar os modulos ja gravados.
void registerCaniveteModule();

} // namespace forja::canivete
