#pragma once

#include <atomic>
#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>

#include "canivete/SoundFontEngine.h"

class CaniveteAudioProcessor final : public juce::AudioProcessor,
                                     private juce::Timer,
                                     private juce::AudioProcessorValueTreeState::Listener
{
public:
    CaniveteAudioProcessor();
    ~CaniveteAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Canivete"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Bank"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorParameter* getBypassParameter() const override;

    juce::AudioProcessorValueTreeState apvts;

    // ---- bank (thread de mensagens) ----
    // O canivete toca o que estiver em Documents/Forja/Canivete. O choice BANK
    // escolhe pelo indice na lista; o estado guarda tambem o caminho absoluto
    // para reabrir um bank que saiu da pasta.
    void refreshLibrary();
    void loadBankFile (const juce::File& file);
    void loadBankByIndex (int index);

    // Pedido do editor quando o usuario escolhe um nome que so aparece numa
    // re-varredura (a lista do choice e congelada na abertura do plugin).
    void selectBankByName (const juce::String& name);

    // ---- instrumento (preset do canal melodic 0) ----
    // Escolhido pelo INDICE na lista do bank. Manda para o engine (atomic) e o
    // audio aplica no proximo bloco. Program change do host ainda vence.
    void setPresetIndex (int index);

    const juce::Array<juce::File>& getBankFiles() const { return bankFiles; }

    juce::String getBankName() const { return engine.getBankName(); }
    juce::String getBankStatus() const { return engine.getStatus(); }
    bool isLoadingBank() const { return engine.isLoading(); }
    bool hasBank() const { return engine.hasBank(); }
    int getNumPresets() const { return engine.getNumPresets(); }
    juce::String getPresetName (int index) const { return engine.getPresetName (index); }
    int getSelectedPresetIndex() const { return engine.getSelectedPresetIndex(); }
    juce::String getSelectedPresetName() const { return engine.getSelectedPresetName(); }

    // Sobe a cada carga concluida (sucesso ou falha); a UI compara com o ultimo
    // valor visto para atualizar status sem polling pesado.
    int getBankRevision() const noexcept { return bankRevision.load (std::memory_order_relaxed); }

    bool isBypassed() const;

    forja::canivete::SoundFontEngine& getEngine() noexcept { return engine; }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void timerCallback() override;
    void parameterChanged (const juce::String& parameterID, float newValue) override;

    forja::canivete::SoundFontEngine engine;
    std::atomic<float>* volumeParam = nullptr;
    juce::AudioParameterChoice* bankParam = nullptr;
    std::atomic<int> bankRevision { 0 };

    bool applyingState = false;   // replaceState nao deve deflagrar carga

    juce::Array<juce::File> bankFiles;   // so thread de mensagens toca aqui
    juce::String lastLoadedPath;         // caminho do bank em uso (para o estado)
    juce::String pendingPresetName;      // instrumento a restaurar apos a carga do bank

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CaniveteAudioProcessor)
};
