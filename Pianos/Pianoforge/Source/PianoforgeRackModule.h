#pragma once

#include <atomic>
#include <string>
#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

#include "engine/PianoEngine.h"
#include "rack/Module.h"

namespace forja::pianoforge
{

// Gaveta de instrumento do ForjaRack: envolve o MESMO PianoEngine do plugin
// standalone pelo contrato ModuleBase. O modulo recebe MIDI como span POD (so
// notas on/off; o nucleo do rack nao expoe CC/program change), o materializa
// num MidiBuffer e escreve o par estereo no scratch que a serie joga na cadeia.
// O pedal de sustain e a cadeia DSP modelada (martelo, ressonancia, tampa) valem
// tambem aqui: o engine e o mesmo, so a porta de MIDI e mais estreita.
//
// Dois knobs: 0 VOLUME (0..1, dominio do engine) e 1 BANK (indice na biblioteca
// Documents/Forja/Pianoforge: .sf2, .sf3 quando o build traz o decodificador e
// .pforge do projeto-irmao). O BANK e lido como indice desejado (atomic POD) e a
// carga em si acontece em idle(), na thread de mensagens: o audio nunca toca no
// disco.
class PianoforgeModule final : public forjarack::ModuleBase
{
public:
    PianoforgeModule();

    void prepare (double sampleRate) override;
    void setParam (int index, float value) override;
    void setBypass (bool bypassed) override { bypassedFlag = bypassed; }

    void process (float* left, float* right, int numSamples) override;
    float readOutputPeak() override { return lastPeak; }
    int latencySamples() const override { return 0; }

    bool isInstrument() const noexcept override { return true; }
    void processMidi (const forjarack::MidiEventSpan& span) noexcept override;

    // Roda na thread de mensagens (RackEngine::idleModules): colhe o resultado
    // da carga em background (lista ou FileChooser da gaveta), libera banks
    // substituidos e atende um pedido de troca de BANK ou de preset.
    void idle() override;

    // Gaveta de arquivos: a biblioteca vira seletor e o FileChooser do rack pode
    // trazer um piano de fora (.sf2/.sf3/.pforge). Thread de mensagens.
    bool supportsDrawerFiles() const noexcept override { return true; }
    std::vector<std::string> drawerFileNames() const override;
    void drawerRefreshLibrary() override { refreshLibrary(); reselectAfterRefresh(); }
    std::string drawerCurrentName() const override;
    bool drawerSelectFile (const std::string& name) override;
    void drawerLoadExternalFile (const std::string& path) override;

    // Instrumento (preset unico do banco) dentro do bank carregado. A lista so
    // aparece depois que a carga assenta; a escolha e resolvida no idle().
    bool supportsDrawerInstruments() const noexcept override { return true; }
    std::vector<std::string> drawerInstrumentNames() const override;
    std::string drawerCurrentInstrument() const override;
    void drawerSelectInstrument (const std::string& name) override;

    // Janela de teste para o estado interno do wrapper (o rack nunca usa).
    float getVolumeForTest() const noexcept { return engine.getParameter (Param_Volume); }

private:
    static constexpr int engineCapacity = 4096;

    PianoEngine engine;
    juce::MidiBuffer midi;

    std::vector<juce::File> banks;

    bool bypassedFlag = false;
    float lastPeak = 0.0f;

    std::atomic<int> bankCount { 0 };
    std::atomic<int> desiredBank { -1 };
    int loadedBank = -1;

    std::string selectedPath;
    std::atomic<bool> externalActive { false };
    std::string externalPath;
    std::string loadedExternalName;

    std::string selectedInstrument;

    void refreshLibrary();
    void reselectAfterRefresh();
    void applyPendingInstrument();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PianoforgeModule)
};

// Publica a gaveta no registro do rack (thread de mensagens, idempotente). O
// ForjaRack a chama antes de montar a lista de escolhas dos slots, para o
// PIANOFORGE entrar ACRESCENTADO ao fim sem reordenar os modulos ja gravados.
void registerPianoforgeModule();

} // namespace forja::pianoforge
