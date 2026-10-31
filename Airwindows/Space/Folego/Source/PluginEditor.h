#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <memory>

#include "forja/ui/ForjaCard.h"
#include "forja/ui/ForjaEditorBase.h"
#include "forja/ui/ForjaKnob.h"
#include "forja/ui/ForjaMeter.h"

class FolegoAudioProcessor;

class FolegoAudioProcessorEditor final : public forja::ui::ForjaEditorBase
{
public:
    explicit FolegoAudioProcessorEditor (FolegoAudioProcessor&);
    ~FolegoAudioProcessorEditor() override = default;

private:
    void layoutContent (juce::Rectangle<int> content) override;
    void updateTelemetry() override;

    void refreshReadouts();
    void refreshBadge (bool clipped, bool bypassed, float inputDb);

    FolegoAudioProcessor& processor;

    forja::ui::ForjaCard clubCard { 1, "Clube", true };
    forja::ui::ForjaCard bodyCard { 2, "Corpo" };
    forja::ui::ForjaCard filterCard { 3, "Filtro e Reflexoes" };
    forja::ui::ForjaCard outCard { 4, "Saida" };

    forja::ui::ForjaKnob regenKnob { forja::ui::utf8 ("REGENERA\u00c7\u00c3O"), "REGEN", 92.0f,
                                     0.0, 1.0, 0.01, 0.5,
                                     [] (double value)
                                     { return juce::String (value * 100.0, 0) + " %"; },
                                     { 134, true } };
    forja::ui::ForjaKnob derezKnob { "DEREZ", "DEREZ", 92.0f, 0.0, 1.0, 0.01, 0.5,
                                     [] (double value)
                                     { return juce::String (value * 100.0, 0) + " %"; },
                                     { 134, true } };
    forja::ui::ForjaKnob filterKnob { "FILTRO", "FILTER", 92.0f, 0.0, 1.0, 0.01, 0.5,
                                      [] (double value)
                                      { return juce::String (value * 100.0, 0) + " %"; },
                                      { 134, true } };
    forja::ui::ForjaKnob earlyKnob { forja::ui::utf8 ("REFLEX\u00d5ES"), "EARLYRF", 92.0f,
                                     0.0, 1.0, 0.01, 0.5,
                                     [] (double value)
                                     { return juce::String (value * 100.0, 0) + " %"; },
                                     { 134, true } };
    forja::ui::ForjaKnob positionKnob { forja::ui::utf8 ("POSI\u00c7\u00c3O"), "POSITIN", 92.0f,
                                        0.0, 1.0, 0.01, 0.9,
                                        [] (double value)
                                        { return juce::String (value * 100.0, 0) + " %"; },
                                        { 134, true } };
    forja::ui::ForjaKnob mixKnob { "MISTURA", "MIX", 92.0f, 0.0, 1.0, 0.01, 0.5,
                                   [] (double value)
                                   { return juce::String (value * 100.0, 0) + " %"; },
                                   { 130, true } };

    juce::Label clubNote;
    juce::Label bodyNote;
    juce::Label filterNote;
    juce::Label mixNote;

    forja::ui::ForjaMeter outputMeter;

    juce::TextButton bypassButton { "Bypass" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> regenAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> derezAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> filterAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> earlyAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> positionAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FolegoAudioProcessorEditor)
};
