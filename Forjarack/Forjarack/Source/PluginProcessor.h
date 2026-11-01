#pragma once

#include <array>
#include <atomic>

#include <juce_audio_processors/juce_audio_processors.h>

#include "forja/dsp/ForjaDspCommon.h"
#include "rack/RackEngine.h"
#include "rack/RackState.h"

using namespace forjarack;

class ForjarackAudioProcessor final : public juce::AudioProcessor,
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
