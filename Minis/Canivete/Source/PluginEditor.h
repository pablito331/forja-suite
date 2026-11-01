#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <memory>

#include "forja/ui/ForjaCard.h"
#include "forja/ui/ForjaChoice.h"
#include "forja/ui/ForjaEditorBase.h"
#include "forja/ui/ForjaKnob.h"
#include "forja/ui/ForjaMeter.h"

class CaniveteAudioProcessor;

class CaniveteAudioProcessorEditor final : public forja::ui::ForjaEditorBase
{
public:
    explicit CaniveteAudioProcessorEditor (CaniveteAudioProcessor&);
    ~CaniveteAudioProcessorEditor() override;

private:
    void layoutContent (juce::Rectangle<int> content) override;
    void updateTelemetry() override;

    void refreshBankViews();
    void refreshBadge (bool clipped, bool bypassed, bool hasBank);
    void rebuildBankChoice();

    CaniveteAudioProcessor& processor;

    forja::ui::ForjaCard bankCard;
    forja::ui::ForjaCard outCard;

    forja::ui::ForjaChoice bankChoice;
    juce::TextButton refreshButton;
    juce::Label statusLabel;
    juce::Label presetLabel;

    forja::ui::ForjaKnob volumeKnob;
    forja::ui::ForjaMeter outputMeter;
    juce::TextButton bypassButton { "Bypass" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> volumeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> bankAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    int lastBankRevision = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CaniveteAudioProcessorEditor)
};
