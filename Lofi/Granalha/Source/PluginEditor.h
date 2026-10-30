#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <memory>

#include "forja/ui/ForjaCard.h"
#include "forja/ui/ForjaEditorBase.h"
#include "forja/ui/ForjaKnob.h"
#include "forja/ui/ForjaMeter.h"

class GranalhaAudioProcessor;

class GranalhaAudioProcessorEditor final : public forja::ui::ForjaEditorBase
{
public:
    explicit GranalhaAudioProcessorEditor (GranalhaAudioProcessor&);
    ~GranalhaAudioProcessorEditor() override = default;

private:
    void layoutContent (juce::Rectangle<int> content) override;
    void updateTelemetry() override;

    void refreshReadouts();
    void refreshBadge (bool clipped, bool bypassed, float inputDb);

    GranalhaAudioProcessor& processor;

    forja::ui::ForjaCard grainCard { 1, "Granalha", true };
    forja::ui::ForjaCard bitCard { 2, "Bits" };
    forja::ui::ForjaCard shapeCard { 3, "Forma de Onda" };
    forja::ui::ForjaCard outCard { 4, "Saida" };

    forja::ui::ForjaKnob derezKnob { "DEREZ", "BITS", 92.0f, 0.0, 1.0, 0.01, 1.0,
                                     [] (double value)
                                     { return juce::String (value * 100.0, 0) + " %"; } };
    forja::ui::ForjaKnob trimKnob { "TRIM", "TRIM", 92.0f, 0.0, 1.0, 0.01, 1.0,
                                    [] (double value)
                                    { return juce::String (value * 100.0, 0) + " %"; } };
    forja::ui::ForjaKnob squareKnob { forja::ui::utf8 ("QUADRADO"), "SQUARE", 92.0f, 0.0, 1.0,
                                      0.01, 0.0,
                                      [] (double value)
                                      { return juce::String (value * 100.0, 0) + " %"; } };
    forja::ui::ForjaKnob brightKnob { "BRILHO", "BRIGHT", 92.0f, 0.0, 1.0, 0.01, 1.0,
                                      [] (double value)
                                      { return juce::String (value * 100.0, 0) + " %"; } };
    forja::ui::ForjaKnob bassisKnob { "GRAVES", "BASSIS", 92.0f, 0.0, 1.0, 0.01, 0.0,
                                      [] (double value)
                                      { return juce::String (value * 100.0, 0) + " %"; } };
    forja::ui::ForjaKnob outputKnob { forja::ui::utf8 ("SA\u00cdDA"), "OUTPUT", 92.0f, 0.0, 1.0,
                                      0.01, 1.0,
                                      [] (double value)
                                      { return juce::String (value * 100.0, 0) + " %"; } };

    juce::Label grainNote;
    juce::Label bitNote;
    juce::Label shapeNote;
    juce::Label outputNote;

    forja::ui::ForjaMeter outputMeter;

    juce::TextButton bypassButton { "Bypass" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> derezAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> trimAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> squareAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> brightAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bassisAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GranalhaAudioProcessorEditor)
};
