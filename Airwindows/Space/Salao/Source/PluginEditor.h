#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <memory>

#include "forja/ui/ForjaCard.h"
#include "forja/ui/ForjaEditorBase.h"
#include "forja/ui/ForjaKnob.h"
#include "forja/ui/ForjaMeter.h"

class SalaoAudioProcessor;

class SalaoAudioProcessorEditor final : public forja::ui::ForjaEditorBase
{
public:
    explicit SalaoAudioProcessorEditor (SalaoAudioProcessor&);
    ~SalaoAudioProcessorEditor() override = default;

private:
    void layoutContent (juce::Rectangle<int> content) override;
    void updateTelemetry() override;

    void refreshReadouts();
    void refreshBadge (bool clipped, bool bypassed, float inputDb);

    SalaoAudioProcessor& processor;

    forja::ui::ForjaCard hallCard { 1, "Sala", true };
    forja::ui::ForjaCard timeCard { 2, "Tempo" };
    forja::ui::ForjaCard spaceCard { 3, "Distancia" };
    forja::ui::ForjaCard outCard { 4, "Saida" };

    forja::ui::ForjaKnob regenKnob { forja::ui::utf8 ("REGENERA\u00c7\u00c3O"), "REGEN", 104.0f,
                                     0.0, 1.0, 0.01, 0.5,
                                     [] (double value)
                                     { return juce::String (value * 100.0, 0) + " %"; },
                                     { 138, true } };
    forja::ui::ForjaKnob positionKnob { forja::ui::utf8 ("POSI\u00c7\u00c3O"), "POSITIN", 104.0f,
                                        0.0, 1.0, 0.01, 0.75,
                                        [] (double value)
                                        { return juce::String (value * 100.0, 0) + " %"; },
                                        { 138, true } };
    forja::ui::ForjaKnob derezKnob { "DOWNREZ", "DOWNREZ", 104.0f, 0.0, 1.0, 0.01, 1.0,
                                     [] (double value)
                                     { return juce::String (value * 100.0, 0) + " %"; },
                                     { 138, true } };
    forja::ui::ForjaKnob distanceKnob { forja::ui::utf8 ("DIST\u00c2NCIA"), "DISTNCE", 104.0f,
                                        0.0, 1.0, 0.01, 0.25,
                                        [] (double value)
                                        { return juce::String (value * 100.0, 0) + " %"; },
                                        { 138, true } };
    forja::ui::ForjaKnob mixKnob { "MISTURA", "MIX", 92.0f, 0.0, 1.0, 0.01, 0.5,
                                   [] (double value)
                                   { return juce::String (value * 100.0, 0) + " %"; },
                                   { 130, true } };

    juce::Label hallNote;
    juce::Label spaceNote;
    juce::Label mixNote;

    forja::ui::ForjaMeter outputMeter;

    juce::TextButton bypassButton { "Bypass" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> regenAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> positionAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> derezAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> distanceAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SalaoAudioProcessorEditor)
};
