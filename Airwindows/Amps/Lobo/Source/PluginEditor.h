#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <memory>

#include "forja/ui/ForjaCard.h"
#include "forja/ui/ForjaEditorBase.h"
#include "forja/ui/ForjaMeter.h"

class LoboAudioProcessor;

class LoboAudioProcessorEditor final : public forja::ui::ForjaEditorBase
{
public:
    explicit LoboAudioProcessorEditor (LoboAudioProcessor&);
    ~LoboAudioProcessorEditor() override = default;

private:
    void layoutContent (juce::Rectangle<int> content) override;
    void updateTelemetry() override;

    void refreshBadge (bool clipped, bool bypassed, float inputDb);

    LoboAudioProcessor& processor;

    forja::ui::ForjaCard diCard { 1, "DI de transformador", true };

    juce::Label noteLabel;

    forja::ui::ForjaMeter outputMeter;

    juce::TextButton bypassButton { "Bypass" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoboAudioProcessorEditor)
};
