#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <memory>

#include "forja/ui/ForjaEditorBase.h"

class TracoAudioProcessor;

class TracoAudioProcessorEditor final : public forja::ui::ForjaEditorBase
{
public:
    explicit TracoAudioProcessorEditor (TracoAudioProcessor&);
    ~TracoAudioProcessorEditor() override;

private:
    void layoutContent (juce::Rectangle<int> content) override;
    void updateTelemetry() override;
    void refreshBadge (bool clipped, bool bypassed, float inPeak);

    TracoAudioProcessor& processor;

    juce::TextButton bypassButton { "Bypass" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TracoAudioProcessorEditor)
};
