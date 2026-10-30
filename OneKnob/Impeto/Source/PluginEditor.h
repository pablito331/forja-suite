#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <memory>

#include "forja/ui/ForjaCard.h"
#include "forja/ui/ForjaEditorBase.h"
#include "forja/ui/ForjaKnob.h"

class ImpetoAudioProcessor;

class ImpetoAudioProcessorEditor final : public forja::ui::ForjaEditorBase
{
public:
    explicit ImpetoAudioProcessorEditor (ImpetoAudioProcessor&);
    ~ImpetoAudioProcessorEditor() override = default;

private:
    void layoutContent (juce::Rectangle<int> content) override;
    void updateTelemetry() override;

    void refreshReadouts();
    void refreshBadge (bool clipped, bool bypassed, float inputDb);

    ImpetoAudioProcessor& processor;

    forja::ui::ForjaCard punchCard { 1, forja::ui::utf8 ("\u00cdMPETO"), true };

    forja::ui::ForjaKnob inputKnob { "IMPETO", "INPUT", 108.0f, 0.0, 1.0, 0.01, 0.5,
                                     [] (double value)
                                     { return juce::String (value * 24.0 - 12.0, 1) + " dB"; },
                                     { 148, false, false, false } };

    juce::Label valueLabel;
    juce::Label hintLabel;
    juce::Label noteLabel;

    juce::TextButton bypassButton { "Bypass" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> inputAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ImpetoAudioProcessorEditor)
};
