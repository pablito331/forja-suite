#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <memory>

#include "forja/ui/ForjaCanvas.h"
#include "forja/ui/ForjaCard.h"
#include "forja/ui/ForjaGRHistory.h"
#include "forja/ui/ForjaGainReductionMeter.h"
#include "forja/ui/ForjaInfoPanel.h"
#include "forja/ui/ForjaKnob.h"
#include "forja/ui/ForjaLookAndFeel.h"
#include "forja/ui/ForjaMeter.h"
#include "forja/ui/ForjaStatusBar.h"
#include "forja/ui/ForjaTopBar.h"

class TemperaAudioProcessor;

class TemperaAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                          private juce::Timer
{
public:
    explicit TemperaAudioProcessorEditor (TemperaAudioProcessor&);
    ~TemperaAudioProcessorEditor() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    void refreshBadge (bool clipped, bool bypassed, float inputDb, float grDb);
    void refreshMeta();

    TemperaAudioProcessor& processor;

    forja::ui::ForjaLookAndFeel lookAndFeel;
    juce::ComponentBoundsConstrainer constrainer;

    forja::ui::ForjaTopBar topBar;
    forja::ui::ForjaStatusBar statusBar;
    std::unique_ptr<forja::ui::ForjaInfoPanel> infoPanel;

    forja::ui::ForjaCard compressCard { 1, forja::ui::utf8 ("Compressão"), true };
    forja::ui::ForjaCard outputCard { 2, forja::ui::utf8 ("Saída") };
    forja::ui::ForjaCard motionCard { 3, "Movimento" };
    forja::ui::ForjaCard mixCard { 4, "Mistura" };

    forja::ui::ForjaKnob compressKnob { forja::ui::utf8 ("Compressão"), "compress", 108.0f,
                                        0.0, 1.0, 0.01, 0.0,
                                        [] (double value) { return juce::String (value, 2); } };
    forja::ui::ForjaKnob outputKnob { forja::ui::utf8 ("Saída"), "output", 92.0f,
                                      0.0, 1.0, 0.01, 0.5,
                                      [] (double value) { return juce::String (value, 2); } };
    forja::ui::ForjaKnob mixKnob { "Mistura", "mix", 92.0f,
                                   0.0, 1.0, 0.01, 1.0,
                                   [] (double value) { return juce::String (value, 2); } };

    forja::ui::ForjaGainReductionMeter gainReductionMeter;
    forja::ui::ForjaGRHistory grHistory;
    forja::ui::ForjaMeter outputMeter;

    juce::TextButton bypassButton { "Bypass" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> compressAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    bool lastBypass = false;
    double lastSampleRate = 0.0;
    int lastBlockSize = 0;

    static constexpr int windowWidth = 1000;
    static constexpr int windowHeight = 540;

    juce::Rectangle<int> topBarBounds { 7, 7, 986, 49 };
    juce::Rectangle<int> statusBounds { 7, 458, 986, 75 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TemperaAudioProcessorEditor)
};
