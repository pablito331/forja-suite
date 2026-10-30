#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <memory>

#include "forja/ui/ForjaCard.h"
#include "forja/ui/ForjaEditorBase.h"
#include "forja/ui/ForjaKnob.h"

class @@PREFIX@@AudioProcessor;

class @@PREFIX@@AudioProcessorEditor final : public forja::ui::ForjaEditorBase
{
public:
    explicit @@PREFIX@@AudioProcessorEditor (@@PREFIX@@AudioProcessor&);
    ~@@PREFIX@@AudioProcessorEditor() override = default;

private:
    void layoutContent (juce::Rectangle<int> content) override;
    void updateTelemetry() override;

    void refreshReadouts();
    void refreshBadge (bool clipped, bool bypassed, float inputDb);

    @@PREFIX@@AudioProcessor& processor;

    forja::ui::ForjaCard mainCard { 1, "Principal", true };

    forja::ui::ForjaKnob amountKnob { "QUANTIDADE", "amount", 108.0f, 0.0, 1.0, 0.01, 0.5,
                                      [] (double value)
                                      { return juce::String (value * 100.0, 0) + " %"; },
                                      { 148, false, false, false } };

    juce::Label valueLabel;
    juce::Label hintLabel;
    juce::Label noteLabel;

    juce::TextButton bypassButton { "Bypass" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> amountAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (@@PREFIX@@AudioProcessorEditor)
};
