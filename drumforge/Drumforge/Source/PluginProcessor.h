#pragma once

#include <array>
#include <atomic>
#include <memory>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "engine/DrumEngine.h"
#include "engine/Kit.h"
#include "engine/SampleStore.h"
#include "loops/LoopLibrary.h"
#include "loops/LoopPlayer.h"

class DrumforgeAudioProcessor final : public juce::AudioProcessor,
                                      private juce::Timer
{
public:
    DrumforgeAudioProcessor();
    ~DrumforgeAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Drumforge"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Kit"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorParameter* getBypassParameter() const override;

    juce::AudioProcessorValueTreeState apvts;

    // Kits: sempre manipulados na thread de mensagens. applyMix == true marca
    // uma carga manual (botao ou arrastar e soltar) e mexe nos knobs; a
    // restauracao de estado da DAW passa false, para nao pisar o preset do host.
    void loadKitFileAsync (const juce::File& kitFile, bool applyMix = true);
    void importKitFolderAsync (const juce::File& folder, bool applyMix = true);
    void importDroppedFile (const juce::File& dropped, bool applyMix = true);

    std::shared_ptr<const forja::drumforge::LoadedKit> getLoadedKit() const;
    forja::drumforge::Kit getKitCopy() const;
    // Copia o kit ativo com a mesa atual capturada (hasMix = true), pronto para
    // saveToFile gravar um .dfkit premixado de qualquer kit de origem.
    forja::drumforge::Kit getKitForSaving() const;
    void markKitSaved (const juce::File& file);
    juce::String getPadName (int padIndex) const;
    juce::String getKitStatus() const;
    int getLoadProgress() const;
    bool isLoadingKit() const;
    int getKitRevision() const noexcept { return kitRevision; }

    juce::File getLibraryDirectory() const;
    juce::Array<juce::File> findLibraryKits() const;

    // Loops MIDI: leitura e biblioteca na thread de mensagens.
    juce::File getLoopDirectory() const;
    std::vector<forja::drumforge::loops::Entry> findLoops() const;
    bool loadLoopFile (const juce::File& loopFile);
    juce::File importLoopFile (const juce::File& loopFile);
    int importLoopFolder (const juce::File& folder);
    juce::String getLoopStatus() const { return loopStatus; }
    juce::String getSelectedLoopPath() const;
    juce::String getSelectedLoopName() const;
    std::shared_ptr<const forja::drumforge::loops::Sequence> getPreviewLoop() const;

    void startLoopPreview();
    void stopLoopPreview();
    bool isLoopPreviewing() const;
    double getLoopPositionBeats() const;
    double getEffectiveBpm() const;
    double getHostBpm() const { return hostBpm.load (std::memory_order_relaxed); }

    // Mapa nota -> pad do kit adotado, lido na thread de mensagens (a grade da
    // aba Loops nao pode tocar no mapa do engine enquanto o audio escreve).
    std::array<int, 128> noteToPadMap() const;

    forja::drumforge::DrumEngine& getEngine() noexcept { return engine; }
    void previewPad (int padIndex, float velocity);

private:
    struct PadPointers
    {
        std::atomic<float>* trim = nullptr;
        std::atomic<float>* fader = nullptr;
        std::atomic<float>* pan = nullptr;
        std::atomic<float>* hpf = nullptr;
        std::atomic<float>* eqLow = nullptr;
        std::atomic<float>* eqMid = nullptr;
        std::atomic<float>* eqHigh = nullptr;
        std::atomic<float>* compThreshold = nullptr;
        std::atomic<float>* compRatio = nullptr;
        std::atomic<float>* send1 = nullptr;
        std::atomic<float>* send2 = nullptr;
        std::atomic<float>* mute = nullptr;
        std::atomic<float>* solo = nullptr;
        std::atomic<float>* phase = nullptr;
        std::atomic<float>* output = nullptr;
    };

    struct MasterPointers
    {
        std::atomic<float>* trim = nullptr;
        std::atomic<float>* comp = nullptr;
        std::atomic<float>* limiter = nullptr;
        std::atomic<float>* fx1Regen = nullptr;
        std::atomic<float>* fx1Position = nullptr;
        std::atomic<float>* fx2Drive = nullptr;
    };

    struct RetiredKit
    {
        std::shared_ptr<const forja::drumforge::LoadedKit> kit;
        int token = 0;
    };

    struct RetiredLoop
    {
        std::shared_ptr<const forja::drumforge::loops::Sequence> sequence;
        int token = 0;
    };

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    static juce::String padId (int padIndex, const juce::String& suffix);

    void cacheParameterPointers();
    void timerCallback() override;

    void startLoad (forja::drumforge::Kit kit, bool applyMix);
    void applyMixerFromKit (const forja::drumforge::Kit& kit);
    void adoptLoadedKit (std::unique_ptr<const forja::drumforge::LoadedKit> loaded);
    forja::drumforge::EngineParameters readEngineParameters() const;
    bool isBypassed() const;
    void reapRetiredKits();
    void reapRetiredLoops();
    void updateHostTempo();

    forja::drumforge::DrumEngine engine;
    forja::drumforge::SampleStore store;
    forja::drumforge::Kit fallbackKit;

    std::array<PadPointers, forja::drumforge::kNumPads> padPointers;
    MasterPointers masterPointers;

    std::shared_ptr<const forja::drumforge::LoadedKit> activeKit;
    std::vector<RetiredKit> retiredKits;
    std::shared_ptr<std::atomic<bool>> aliveFlag;
    int kitToken = 0;
    int kitRevision = 0;
    juce::String kitStatus { "Nenhum kit carregado" };
    juce::File lastKitFile;

    forja::drumforge::loops::LoopPlayer loopPlayer;
    std::shared_ptr<const forja::drumforge::loops::Sequence> previewLoop;
    std::vector<RetiredLoop> retiredLoops;
    juce::String loopStatus { "Nenhum loop selecionado" };
    int loopToken = 0;
    std::atomic<double> hostBpm { 120.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumforgeAudioProcessor)
};
