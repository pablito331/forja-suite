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

class PrensaAudioProcessor;

class PrensaAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                         private juce::Timer
{
public:
    explicit PrensaAudioProcessorEditor (PrensaAudioProcessor&);
    ~PrensaAudioProcessorEditor() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    void refreshBadge (bool clipped, bool bypassed, float inputDb, float grDb);
    void refreshMeta();

    PrensaAudioProcessor& processor;

    forja::ui::ForjaLookAndFeel lookAndFeel;
    juce::ComponentBoundsConstrainer constrainer;

    forja::ui::ForjaTopBar topBar;
    forja::ui::ForjaStatusBar statusBar;
    std::unique_ptr<forja::ui::ForjaInfoPanel> infoPanel;

    forja::ui::ForjaCard pressureCard { 1, forja::ui::utf8 ("Pressão"), true };
    forja::ui::ForjaCard timeCard { 2, "Tempo" };
    forja::ui::ForjaCard clawCard { 3, "Garra" };
    forja::ui::ForjaCard outputCard { 4, forja::ui::utf8 ("Saída") };

    forja::ui::ForjaKnob pressureKnob { forja::ui::utf8 ("Pressão"), "pressure", 108.0f,
                                        0.0, 1.0, 0.01, 0.0,
                                        [] (double value) { return juce::String (value, 2); } };
    forja::ui::ForjaKnob speedKnob { "Velocidade", "speed", 84.0f,
                                     0.0, 1.0, 0.01, 0.25,
                                     [] (double value) { return juce::String (value, 2); } };
    forja::ui::ForjaKnob mewinessKnob { "Miado", "mewiness", 84.0f,
                                        0.0, 1.0, 0.01, 1.0,
                                        [] (double value) { return juce::String (value, 2); } };
    forja::ui::ForjaKnob pawclawKnob { "Garra", "pawclaw", 84.0f,
                                       0.0, 1.0, 0.01, 0.5,
                                       [] (double value) { return juce::String (value, 2); } };
    forja::ui::ForjaKnob outputKnob { forja::ui::utf8 ("Saída"), "output", 76.0f,
                                      0.0, 1.0, 0.01, 0.5,
                                      [] (double value) { return juce::String (value, 2); } };
    forja::ui::ForjaKnob mixKnob { "Mistura", "mix", 76.0f,
                                   0.0, 1.0, 0.01, 1.0,
                                   [] (double value) { return juce::String (value, 2); } };

    forja::ui::ForjaGainReductionMeter gainReductionMeter;
    forja::ui::ForjaGRHistory grHistory;
    forja::ui::ForjaMeter outputMeter;

    juce::TextButton bypassButton { "Bypass" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> pressureAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> speedAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mewinessAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> pawclawAttachment;
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PrensaAudioProcessorEditor)
};
