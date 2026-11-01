#pragma once

#include <atomic>
#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>

#include "minidrum/MiniDrum.h"

class MinidrumAudioProcessor final : public juce::AudioProcessor,
                                     private juce::Timer
{
public:
    MinidrumAudioProcessor();
    ~MinidrumAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Minidrum"; }
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

    // ---- kit (thread de mensagens) ----
    // O mini e um tocador: o som do kit manda. A mix gravada no arquivo dirige
    // os pads; o knob VOLUME e a unica vontade do usuario sobre o nivel.
    void loadKitFile (const juce::File& file);

    std::shared_ptr<const forja::drumforge::LoadedKit> getLoadedKit() const
    {
        return drum.getLoadedKit();
    }

    juce::String getKitName() const { return drum.getKitName(); }
    juce::String getKitStatus() const { return drum.getStatus(); }
    bool isLoadingKit() const { return drum.isLoading(); }
    int getLoadProgress() const { return drum.getProgress(); }

    // Sobe a cada carga concluida (sucesso ou falha); a UI compara com o ultimo
    // valor visto para atualizar foto e status sem polling pesado.
    int getKitRevision() const noexcept { return kitRevision.load (std::memory_order_relaxed); }

    bool isBypassed() const;

    forja::minidrum::MiniDrum& getDrum() noexcept { return drum; }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void timerCallback() override;

    forja::minidrum::MiniDrum drum;
    std::atomic<float>* volumeParam = nullptr;
    std::atomic<int> kitRevision { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MinidrumAudioProcessor)
};
