#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <memory>

#include "forja/ui/ForjaCard.h"
#include "forja/ui/ForjaEditorBase.h"
#include "forja/ui/ForjaKnob.h"

class CeraAudioProcessor;

class CeraAudioProcessorEditor final : public forja::ui::ForjaEditorBase
{
public:
    explicit CeraAudioProcessorEditor (CeraAudioProcessor&);
    ~CeraAudioProcessorEditor() override = default;

private:
    void layoutContent (juce::Rectangle<int> content) override;
    void updateTelemetry() override;

    void refreshReadouts();
    void refreshBadge (bool clipped, bool bypassed, float inputDb);

    CeraAudioProcessor& processor;

    forja::ui::ForjaCard warmCard { 1, "Calor de valvula", true };

    forja::ui::ForjaKnob warmthKnob { "CALOR", "ASYM", 108.0f, 0.0, 1.0, 0.01, 0.5,
                                      [] (double value)
                                      { return juce::String (value * 100.0, 0) + " %"; },
                                      { 148, false, false, false } };

    juce::Label valueLabel;
    juce::Label hintLabel;
    juce::Label noteLabel;

    juce::TextButton bypassButton { "Bypass" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> warmthAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CeraAudioProcessorEditor)
};
