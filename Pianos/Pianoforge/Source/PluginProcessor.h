#pragma once

#include <array>
#include <atomic>
#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>

#include "engine/Params.h"
#include "engine/PianoEngine.h"

class PianoforgeAudioProcessor final : public juce::AudioProcessor,
                                       private juce::Timer,
                                       private juce::AudioProcessorValueTreeState::Listener
{
public:
    PianoforgeAudioProcessor();
    ~PianoforgeAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Pianoforge"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Piano"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorParameter* getBypassParameter() const override;

    juce::AudioProcessorValueTreeState apvts;

    // ---- banco (thread de mensagens) ----
    // O piano toca o que estiver em Documents/Forja/Pianoforge (.sf2/.sf3/.pforge).
    // O choice BANCO escolhe pelo indice na lista; o estado guarda tambem o caminho
    // absoluto para reabrir um banco que saiu da pasta.
    void refreshLibrary();
    void loadBankFile (const juce::File& file);
    void loadBankByIndex (int index);
    void selectBankByName (const juce::String& name);

    // ---- instrumento (preset unico do banco) ----
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

    int getBankRevision() const noexcept { return bankRevision.load (std::memory_order_relaxed); }

    bool isBypassed() const;

    forja::pianoforge::PianoEngine& getEngine() noexcept { return engine; }

    // Texto legivel de um parametro no dominio do engine (0..1 -> dominio real),
    // compartilhado pelo APVTS e pelos discos do editor.
    static juce::String paramText (int paramId, double value01);

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void timerCallback() override;
    void parameterChanged (const juce::String& parameterID, float newValue) override;

    forja::pianoforge::PianoEngine engine;

    std::array<std::atomic<float>*, forja::pianoforge::Param_Count> rawParams {};
    juce::AudioParameterChoice* bankParam = nullptr;
    std::atomic<int> bankRevision { 0 };

    bool applyingState = false;   // replaceState nao deve deflagrar carga

    juce::Array<juce::File> bankFiles;   // so thread de mensagens toca aqui
    juce::String lastLoadedPath;
    juce::String pendingPresetName;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PianoforgeAudioProcessor)
};
