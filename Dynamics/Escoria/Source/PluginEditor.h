#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <memory>

#include "forja/dsp/ForjaEscoria.h"
#include "forja/ui/ForjaCard.h"
#include "forja/ui/ForjaEditorBase.h"
#include "forja/ui/ForjaKnob.h"
#include "forja/ui/ForjaMeter.h"

class EscoriaAudioProcessor;

class EscoriaAudioProcessorEditor final : public forja::ui::ForjaEditorBase
{
public:
    explicit EscoriaAudioProcessorEditor (EscoriaAudioProcessor&);
    ~EscoriaAudioProcessorEditor() override = default;

private:
    void layoutContent (juce::Rectangle<int> content) override;
    void updateTelemetry() override;

    void refreshReadouts();
    void refreshBadge (bool clipped, bool bypassed, float inputDb);

    EscoriaAudioProcessor& processor;

    forja::ui::ForjaCard detectorCard { 1, "Intensidade", true };
    forja::ui::ForjaCard timeCard { 2, "Tempo" };
    forja::ui::ForjaCard focusCard { 3, "Foco" };

    forja::ui::ForjaKnob intenseKnob { "INTENSIDADE", "DE-ESSER", 92.0f, 0.0, 1.0, 0.01, 0.0,
                                       [] (double value) { return juce::String (value, 2); },
                                       { 130, true } };
    forja::ui::ForjaKnob sharpKnob { "VELOCIDADE", forja::ui::utf8 ("DETEC\u00c7\u00c3O"), 92.0f, 0.0, 1.0, 0.01, 0.5,
                                     [] (double value) { return juce::String (value, 2); },
                                     { 130, true } };
    forja::ui::ForjaKnob depthKnob { "PROFUNDIDADE", forja::ui::utf8 ("REDU\u00c7\u00c3O"), 92.0f, 0.0, 1.0, 0.01, 0.5,
                                     [] (double value) { return juce::String (value, 2); },
                                     { 130, true } };
    forja::ui::ForjaKnob filterKnob { "FILTRO", "AGUDOS", 92.0f, 0.0, 1.0, 0.01, 0.5,
                                      [] (double value) { return juce::String (value, 2); },
                                      { 130, true } };
    forja::ui::ForjaKnob senseKnob { "MONITOR", forja::ui::utf8 ("S\u00d3 O REMOVIDO"), 92.0f, 0.0, 1.0, 0.01, 0.0,
                                     [] (double value) { return juce::String (value, 2); },
                                     { 130, true } };

    juce::Label sharpNote;
    juce::Label depthNote;
    juce::Label filterNote;
    juce::Label senseNote;

    forja::ui::ForjaMeter outputMeter;

    juce::TextButton bypassButton { "Bypass" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> intenseAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sharpAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> depthAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> filterAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> senseAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EscoriaAudioProcessorEditor)
};
