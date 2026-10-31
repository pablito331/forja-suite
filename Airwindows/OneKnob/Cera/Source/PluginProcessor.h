#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>

#include "forja/dsp/ForjaCera.h"

class CeraAudioProcessor final : public juce::AudioProcessor
{
public:
    CeraAudioProcessor();
    ~CeraAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Cera"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorParameter* getBypassParameter() const override;

    struct MeterSnapshot
    {
        float inPeakL = 0.0f;
        float inPeakR = 0.0f;
        float outPeakL = 0.0f;
        float outPeakR = 0.0f;
        bool inClip = false;
        bool outClip = false;
    };

    MeterSnapshot readMeters();
    void resetClip();
    bool isBypassed() const;

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    static void publishMax (std::atomic<float>& target, float value) noexcept;

    forja::dsp::CeraDSP dsp;
    juce::AudioBuffer<float> monoScratch;

    std::atomic<float> meterInL { 0.0f };
    std::atomic<float> meterInR { 0.0f };
    std::atomic<float> meterOutL { 0.0f };
    std::atomic<float> meterOutR { 0.0f };
    std::atomic<bool> meterInClip { false };
    std::atomic<bool> meterOutClip { false };
    std::atomic<bool> clipResetRequested { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CeraAudioProcessor)
};
