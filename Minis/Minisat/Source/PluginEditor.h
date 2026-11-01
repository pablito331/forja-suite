#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <memory>

#include "ForjaTransferCurve.h"
#include "forja/ui/ForjaCard.h"
#include "forja/ui/ForjaEditorBase.h"
#include "forja/ui/ForjaKnob.h"
#include "forja/ui/ForjaMeter.h"

class MinisatAudioProcessor;

class MinisatAudioProcessorEditor final : public forja::ui::ForjaEditorBase
{
public:
    explicit MinisatAudioProcessorEditor (MinisatAudioProcessor&);
    ~MinisatAudioProcessorEditor() override;

private:
    void layoutContent (juce::Rectangle<int> content) override;
    void updateTelemetry() override;
    void refreshCurve();
    void refreshBadge (bool clipped, bool bypassed, float inPeak);

    MinisatAudioProcessor& processor;

    forja::ui::ForjaCard curveCard;
    forja::ui::ForjaCard outCard;

    forja::ui::ForjaTransferCurve curve;
    forja::ui::ForjaKnob driveKnob;
    forja::ui::ForjaMeter outputMeter;
    juce::TextButton bypassButton { "Bypass" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> driveAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MinisatAudioProcessorEditor)
};
