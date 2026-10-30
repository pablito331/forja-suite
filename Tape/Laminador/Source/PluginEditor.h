#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <memory>

#include "forja/dsp/ForjaLaminador.h"
#include "forja/ui/ForjaCard.h"
#include "forja/ui/ForjaEditorBase.h"
#include "forja/ui/ForjaKnob.h"
#include "forja/ui/ForjaMeter.h"
#include "forja/ui/ForjaTapeArt.h"

class LaminadorAudioProcessor;

class LaminadorAudioProcessorEditor final : public forja::ui::ForjaEditorBase
{
public:
    explicit LaminadorAudioProcessorEditor (LaminadorAudioProcessor&);
    ~LaminadorAudioProcessorEditor() override = default;

private:
    void layoutContent (juce::Rectangle<int> content) override;
    void updateTelemetry() override;

    void refreshReadouts();
    void refreshBadge (bool clipped, bool bypassed, float inputDb);

    LaminadorAudioProcessor& processor;

    forja::ui::ForjaCard inputCard { 1, "Entrada", true };
    forja::ui::ForjaCard colorCard { 2, forja::ui::utf8 ("Cor e cabe\u00e7ote") };
    forja::ui::ForjaCard flutterCard { 3, "Flutter" };
    forja::ui::ForjaCard outputCard { 4, forja::ui::utf8 ("Sa\u00edda") };

    forja::ui::ForjaKnob inputKnob { "ENTRADA", "INPUT", 92.0f, 0.0, 1.0, 0.01, 0.5,
                                     [] (double value) { return juce::String (value, 2); },
                                     { 130, true } };
    forja::ui::ForjaKnob tiltKnob { forja::ui::utf8 ("INCLINA\u00c7\u00c3O"), "TILT", 92.0f, 0.0, 1.0, 0.01, 0.5,
                                    [] (double value) { return juce::String (value, 2); },
                                    { 130, true } };
    forja::ui::ForjaKnob shapeKnob { "FORMA", "SHAPE", 64.0f, 0.0, 1.0, 0.01, 0.5,
                                     [] (double value) { return juce::String (value, 2); },
                                     { 104, true } };
    forja::ui::ForjaKnob biasKnob { "BIAS", "BIAS", 64.0f, 0.0, 1.0, 0.01, 0.5,
                                    [] (double value) { return juce::String (value, 2); },
                                    { 104, true } };
    forja::ui::ForjaKnob headBmpKnob { forja::ui::utf8 ("CABE\u00c7OTE"), "HEADBMP", 64.0f, 0.0, 1.0, 0.01, 0.5,
                                       [] (double value) { return juce::String (value, 2); },
                                       { 104, true } };
    forja::ui::ForjaKnob headFrqKnob { forja::ui::utf8 ("FREQ. CABE\u00c7OTE"), "HEADFRQ", 64.0f, 0.0, 1.0, 0.01, 0.5,
                                       [] (double value)
                                       { return juce::String (25.0 + (value * value * 175.0), 0) + " Hz"; },
                                       { 104, true } };
    forja::ui::ForjaKnob flutterKnob { "FLUTTER", "FLUT", 92.0f, 0.0, 1.0, 0.01, 0.5,
                                       [] (double value) { return juce::String (value, 2); },
                                       { 130, true } };
    forja::ui::ForjaKnob flutSpdKnob { "VEL. FLUTTER", "FLUTSPD", 92.0f, 0.0, 1.0, 0.01, 0.5,
                                       [] (double value) { return juce::String (value, 2); },
                                       { 130, true } };
    forja::ui::ForjaKnob outputKnob { forja::ui::utf8 ("SA\u00cdDA"), "OUTPUT", 92.0f, 0.0, 1.0, 0.01, 0.5,
                                      [] (double value) { return juce::String (value, 2); },
                                      { 130, true } };

    juce::Label inputNote;
    juce::Label colorNote;
    juce::Label flutterNote;

    forja::ui::ForjaTapeArt tapeArt { forja::ui::ForjaTapeArt::Style::reels };

    forja::ui::ForjaMeter outputMeter;

    juce::TextButton bypassButton { "Bypass" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> inputAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> tiltAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> shapeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> biasAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> headBmpAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> headFrqAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> flutterAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> flutSpdAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LaminadorAudioProcessorEditor)
};
