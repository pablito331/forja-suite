#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <memory>

#include "forja/ui/ForjaCard.h"
#include "forja/ui/ForjaEditorBase.h"
#include "forja/ui/ForjaKnob.h"
#include "forja/ui/ForjaMeter.h"

class CrickAudioProcessor;

class CrickAudioProcessorEditor final : public forja::ui::ForjaEditorBase
{
public:
    explicit CrickAudioProcessorEditor (CrickAudioProcessor&);
    ~CrickAudioProcessorEditor() override = default;

private:
    void layoutContent (juce::Rectangle<int> content) override;
    void updateTelemetry() override;

    void refreshReadouts();
    void refreshBadge (bool clipped, bool bypassed, float inputDb);

    CrickAudioProcessor& processor;

    forja::ui::ForjaCard driveCard { 1, "Drive", true };
    forja::ui::ForjaCard toneCard { 2, "Tom" };
    forja::ui::ForjaCard signalCard { 3, "Sinal" };

    forja::ui::ForjaKnob driveKnob { "DRIVE", "GANHO", 108.0f, 0.0, 1.0, 0.01, 0.5,
                                     [] (double value) { return juce::String (value, 2); },
                                     { 160, true } };
    forja::ui::ForjaKnob toneKnob { "TOM", "TONE", 108.0f, 0.0, 1.0, 0.01, 0.5,
                                    [] (double value) { return juce::String (value, 2); },
                                    { 160, true } };

    juce::Label driveNote;
    juce::Label toneNote;
    juce::Label signalNote;

    forja::ui::ForjaMeter outputMeter;

    juce::TextButton bypassButton { "Bypass" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> driveAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> toneAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CrickAudioProcessorEditor)
};
