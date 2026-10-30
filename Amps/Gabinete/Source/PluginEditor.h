#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <memory>

#include "forja/ui/ForjaCard.h"
#include "forja/ui/ForjaEditorBase.h"
#include "forja/ui/ForjaKnob.h"
#include "forja/ui/ForjaMeter.h"

class GabineteAudioProcessor;

class GabineteAudioProcessorEditor final : public forja::ui::ForjaEditorBase
{
public:
    explicit GabineteAudioProcessorEditor (GabineteAudioProcessor&);
    ~GabineteAudioProcessorEditor() override = default;

private:
    void layoutContent (juce::Rectangle<int> content) override;
    void updateTelemetry() override;

    void refreshReadouts();
    void refreshBadge (bool clipped, bool bypassed, float inputDb);

    GabineteAudioProcessor& processor;

    forja::ui::ForjaCard lowCard { 1, "Passa-baixos", true };
    forja::ui::ForjaCard highCard { 2, "Passa-altos" };
    forja::ui::ForjaCard signalCard { 3, "Sinal" };

    forja::ui::ForjaKnob lowpassKnob { "PASSA-BAIXOS", "LOWPASS", 108.0f, 0.0, 1.0, 0.01, 1.0,
                                       [] (double value) { return juce::String (value, 2); },
                                       { 160, true } };
    forja::ui::ForjaKnob hipassKnob { "PASSA-ALTOS", "HIPASS", 108.0f, 0.0, 1.0, 0.01, 0.0,
                                      [] (double value) { return juce::String (value, 2); },
                                      { 160, true } };

    juce::Label lowNote;
    juce::Label highNote;
    juce::Label signalNote;

    forja::ui::ForjaMeter outputMeter;

    juce::TextButton bypassButton { "Bypass" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lowpassAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> hipassAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GabineteAudioProcessorEditor)
};
