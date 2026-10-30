#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <memory>

#include "forja/ui/ForjaCanvas.h"
#include "forja/ui/ForjaCard.h"
#include "forja/ui/ForjaInfoPanel.h"
#include "forja/ui/ForjaKnob.h"
#include "forja/ui/ForjaLookAndFeel.h"
#include "forja/ui/ForjaMeter.h"
#include "forja/ui/ForjaStatusBar.h"
#include "forja/ui/ForjaTopBar.h"
#include "forja/ui/ForjaTransferCurve.h"
#include "forja/ui/ForjaValve.h"

class BrasaAudioProcessor;

class BrasaAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                        private juce::Timer
{
public:
    explicit BrasaAudioProcessorEditor (BrasaAudioProcessor&);
    ~BrasaAudioProcessorEditor() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    void refreshBadge (bool clipped, bool bypassed, float inputDb);
    void refreshMeta();

    BrasaAudioProcessor& processor;

    forja::ui::ForjaLookAndFeel lookAndFeel;
    juce::ComponentBoundsConstrainer constrainer;

    forja::ui::ForjaTopBar topBar;
    forja::ui::ForjaStatusBar statusBar;
    std::unique_ptr<forja::ui::ForjaInfoPanel> infoPanel;

    forja::ui::ForjaCard inputCard { 1, "Entrada" };
    forja::ui::ForjaCard valveCard { 2, forja::ui::utf8 ("Válvula"), true };
    forja::ui::ForjaCard curveCard { 3, forja::ui::utf8 ("Curva de transferência") };
    forja::ui::ForjaCard outputCard { 4, forja::ui::utf8 ("Saída") };

    forja::ui::ForjaKnob inputKnob { "Entrada", "input", 92.0f, 0.0, 1.0, 0.01, 0.5,
                                     [] (double value) { return juce::String (value, 2); } };
    forja::ui::ForjaKnob tubeKnob { forja::ui::utf8 ("Válvula"), "drive", 108.0f, 0.0, 1.0, 0.01, 0.5,
                                    [] (double value) { return juce::String (value, 2); } };
    forja::ui::ForjaKnob outputKnob { forja::ui::utf8 ("Saída"), "trim", 92.0f, -24.0, 24.0, 0.1, 0.0,
                                      [] (double value)
                                      {
                                          return juce::String (value > 0.0 ? "+" : "")
                                               + juce::String (value, 1) + " dB";
                                      } };

    forja::ui::ForjaMeter inputMeter;
    forja::ui::ForjaMeter outputMeter;
    forja::ui::ForjaTransferCurve curve;
    forja::ui::ForjaValve valve;
    juce::Label valveCaption;

    juce::TextButton bypassButton { "Bypass" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> inputAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> tubeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    bool lastBypass = false;
    double lastSampleRate = 0.0;
    int lastBlockSize = 0;

    static constexpr int windowWidth = 1000;
    static constexpr int windowHeight = 540;

    juce::Rectangle<int> topBarBounds { 7, 7, 986, 49 };
    juce::Rectangle<int> statusBounds { 7, 458, 986, 75 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BrasaAudioProcessorEditor)
};
