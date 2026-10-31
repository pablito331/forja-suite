#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <memory>

#include "forja/ui/ForjaCard.h"
#include "forja/ui/ForjaEditorBase.h"
#include "forja/ui/ForjaMeter.h"

class DiqueAudioProcessor;

class DiqueAudioProcessorEditor final : public forja::ui::ForjaEditorBase
{
public:
    explicit DiqueAudioProcessorEditor (DiqueAudioProcessor&);
    ~DiqueAudioProcessorEditor() override = default;

private:
    void layoutContent (juce::Rectangle<int> content) override;
    void updateTelemetry() override;

    void refreshBadge (bool clipped, bool bypassed, float inputDb, float grDb);

    DiqueAudioProcessor& processor;

    forja::ui::ForjaCard clipCard { 1, "Clipe de pico", true };

    juce::Label noteLabel;

    forja::ui::ForjaMeter outputMeter;

    juce::TextButton bypassButton { "Bypass" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DiqueAudioProcessorEditor)
};
