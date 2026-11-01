#pragma once

#include <array>
#include <atomic>
#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "forja/dsp/ForjaDspCommon.h"
#include "rack/RackEngine.h"
#include "rack/RackState.h"
#include "rack/HostedLibrary.h"
#include "ui/HostedSlot.h"

using namespace forjarack;

class ForjarackAudioProcessor final : public juce::AudioProcessor,
                                      public forjarackui::IHostedSlot,
                                      private juce::Timer
{
public:
    ForjarackAudioProcessor();
    ~ForjarackAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "ForjaRack"; }
    bool acceptsMidi() const override { return true; }   // slots instrument (sf2/sf3, mini drum)
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Rack"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorParameter* getBypassParameter() const override;

    juce::AudioProcessorValueTreeState apvts;

    RackEngine& getEngine() noexcept { return engine; }
    double getSampleRateOrCurrent() const noexcept { return getSampleRate(); }

    // forjarackui::IHostedSlot ------------------------------------------------
    // A gaveta HOSPEDADA expoe seus controles pelo plugin vivo (nao pelo
    // ModuleSpec fixo) e as gavetas de instrumento com biblioteca (MINIDRUM,
    // CANIVETE) expem a lista do modulo + ARQUIVO. A UI so pergunta; o
    // processor faz (thread de mensagens).
    bool isHosted (int slot) const override;
    bool isDrawer (int slot) const override;
    juce::StringArray fileList() const override;
    juce::StringArray drawerFileNames (int slot) const override;
    juce::String fileName (int slot) const override;
    void chooseFile (int slot, const juce::String& name) override;
    int rescan() override;
    void browseFile (int slot) override;
    int paramCount (int slot) const override;
    juce::String paramLabel (int slot, int index) const override;
    bool canOpenEditor (int slot) const override;
    void openEditor (int slot) override;

    // Reescaneia Documents/Forja/Rack (botao PROCURAR). Devolve quantos achou.
    int rescanFiles();

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    static juce::String slotId (int slot, const juce::String& suffix);
    static juce::String knobId (int slot, int knob);

    void cacheParameterPointers();
    void timerCallback() override;
    void syncGraphFromParams (bool applyDefaults);
    RackSnapshot buildSnapshot() const;
    MidiEventSpan collectMidi (juce::MidiBuffer& midiMessages,
                               std::array<MidiEvent, kMaxRackMidiEvents>& storage) const;
    bool isBypassed() const;

    RackEngine engine;

    // Gaveta hospedada: biblioteca de scan + estado por slot. O plugin vivo de
    // um slot e um HostedModule adotado no engine (moduleAt); aqui guardamos
    // apenas o nome do arquivo escolhido e a janela do editor nativo aberta.
    HostedLibrary hostedLibrary;
    std::array<juce::String, kNumSlots> hostedFile {};
    std::array<std::unique_ptr<juce::Component>, kNumSlots> hostedWindows {};
    int hostedIndex = -1;   // indice do catalogo para a gaveta HOSPEDADO

    HostedModule* hostedModuleAt (int slot) const noexcept;
    void adoptHosted (int slot);
    void seedHostedKnobs (int slot, HostedModule& module);
    static juce::String hostedStateKey (int slot);

    // Gaveta de arquivos (instrumentos com biblioteca propria): o modulo vivo
    // e a fonte da lista; a escolha externa (FileChooser) vive em propriedade
    // do estado, igual ao arquivo hospedado, porque a lista e dinamica.
    ModuleBase* drawerModuleAt (int slot) const noexcept;
    void applyDrawerChoice (int slot, const juce::String& path);
    void syncDrawerKnob (int slot, int index, int count);
    void startDrawerBrowse (int slot, const juce::File& startDir, const juce::String& filter,
                            bool foldersAllowed, bool directoriesToo);
    static juce::String drawerStateKey (int slot);

    std::array<juce::String, kNumSlots> drawerFile {};   // escolha persistida ("" = knob manda)
    std::array<bool, kNumSlots> drawerApplied {};        // escolha ja entregue ao modulo vivo
    std::shared_ptr<juce::FileChooser> drawerChooser;    // mantem viva a janela async
    int browseSlot = -1;   // slot dono do FileChooser aberto (callback async)

    std::array<std::atomic<float>*, kNumSlots> typePtr {};
    std::array<std::atomic<float>*, kNumSlots> bypassPtr {};
    std::array<std::atomic<float>*, kNumSlots> soloPtr {};
    std::array<std::array<std::atomic<float>*, kMaxModuleParams>, kNumSlots> knobPtr {};

    std::atomic<float>* inputTrimPtr = nullptr;
    std::atomic<float>* outputTrimPtr = nullptr;
    std::atomic<float>* phasePtr = nullptr;

    std::array<int, kNumSlots> cachedTypes {};

    juce::AudioBuffer<float> dryBuffer;
    forja::dsp::ForjaSmoother bypassSmoother;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ForjarackAudioProcessor)
};
