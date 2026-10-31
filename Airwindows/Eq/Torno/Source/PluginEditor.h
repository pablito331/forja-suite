#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <memory>

#include "forja/ui/ForjaCard.h"
#include "forja/ui/ForjaEditorBase.h"
#include "forja/ui/ForjaKnob.h"
#include "forja/ui/ForjaMeter.h"

class TornoAudioProcessor;

class TornoAudioProcessorEditor final : public forja::ui::ForjaEditorBase
{
public:
    explicit TornoAudioProcessorEditor (TornoAudioProcessor&);
    ~TornoAudioProcessorEditor() override = default;

private:
    void layoutContent (juce::Rectangle<int> content) override;
    void updateTelemetry() override;

    void refreshReadouts();
    void refreshBadge (bool clipped, bool bypassed, float inputDb);

    TornoAudioProcessor& processor;

    forja::ui::ForjaCard inputCard { 1, forja::ui::utf8 ("Entrada"), true };
    forja::ui::ForjaCard trebleCard { 2, forja::ui::utf8 ("Agudos") };
    forja::ui::ForjaCard bassCard { 3, forja::ui::utf8 ("Graves") };
    forja::ui::ForjaCard outputCard { 4, forja::ui::utf8 ("Sa\u00edda") };

    forja::ui::ForjaKnob inputKnob { "ENTRADA", "drive", 92.0f, 0.0, 1.0, 0.01, 0.5,
                                     [] (double value) { return juce::String (value, 2); } };
    forja::ui::ForjaKnob trebleKnob { "AGUDOS", "shelf", 108.0f, 0.0, 1.0, 0.01, 0.5,
                                      [] (double value) { return juce::String (value, 2); } };
    forja::ui::ForjaKnob bassKnob { "GRAVES", "shelf", 108.0f, 0.0, 1.0, 0.01, 0.5,
                                    [] (double value) { return juce::String (value, 2); } };

    juce::TextButton planoButton { "Plano" };
    juce::TextButton bypassButton { "Bypass" };

    juce::Label trebleNote;
    juce::Label bassNote;

    forja::ui::ForjaMeter outputMeter;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> inputAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> trebleAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bassAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TornoAudioProcessorEditor)
};
