#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <memory>

#include "forja/ui/ForjaCard.h"
#include "forja/ui/ForjaEditorBase.h"
#include "forja/ui/ForjaKnob.h"
#include "forja/ui/ForjaMeter.h"

class DeluxeAudioProcessor;

class DeluxeAudioProcessorEditor final : public forja::ui::ForjaEditorBase
{
public:
    explicit DeluxeAudioProcessorEditor (DeluxeAudioProcessor&);
    ~DeluxeAudioProcessorEditor() override = default;

private:
    void layoutContent (juce::Rectangle<int> content) override;
    void updateTelemetry() override;

    void refreshReadouts();
    void refreshBadge (bool clipped, bool bypassed, float inputDb);

    DeluxeAudioProcessor& processor;

    forja::ui::ForjaCard ampCard { 1, "Amp", true };
    forja::ui::ForjaCard toneCard { 2, "Tom" };
    forja::ui::ForjaCard bodyCard { 3, "Corpo" };
    forja::ui::ForjaCard signalCard { 4, "Sinal" };

    forja::ui::ForjaKnob hellKnob { "HELL", "AGUDO EXT", 64.0f, 0.0, 1.0, 0.01, 0.5,
                                    [] (double value) { return juce::String (value, 2); },
                                    { 140, true } };
    forja::ui::ForjaKnob fizzKnob { "FIZZ", "AGUDO", 64.0f, 0.0, 1.0, 0.01, 0.5,
                                    [] (double value) { return juce::String (value, 2); },
                                    { 140, true } };
    forja::ui::ForjaKnob pickKnob { "PICK", "ATAQUE", 64.0f, 0.0, 1.0, 0.01, 0.5,
                                    [] (double value) { return juce::String (value, 2); },
                                    { 140, true } };

    forja::ui::ForjaKnob satanKnob { "SATAN", forja::ui::utf8 ("M\u00c9DIO AG"), 72.0f, 0.0, 1.0, 0.01, 0.5,
                                     [] (double value) { return juce::String (value, 2); },
                                     { 180, true } };
    forja::ui::ForjaKnob dangerKnob { "DANGER", forja::ui::utf8 ("M\u00c9DIO"), 72.0f, 0.0, 1.0, 0.01, 0.5,
                                      [] (double value) { return juce::String (value, 2); },
                                      { 180, true } };
    forja::ui::ForjaKnob criticalKnob { "CRITICAL", forja::ui::utf8 ("M\u00c9DIO GR"), 72.0f, 0.0, 1.0, 0.01, 0.5,
                                        [] (double value) { return juce::String (value, 2); },
                                        { 180, true } };

    forja::ui::ForjaKnob hMeatKnob { "H MEAT", "CORPO ALTO", 84.0f, 0.0, 1.0, 0.01, 0.5,
                                     [] (double value) { return juce::String (value, 2); },
                                     { 128, true } };
    forja::ui::ForjaKnob lMeatKnob { "L MEAT", "CORPO BAIXO", 84.0f, 0.0, 1.0, 0.01, 0.5,
                                     [] (double value) { return juce::String (value, 2); },
                                     { 128, true } };
    forja::ui::ForjaKnob swingKnob { "SWING", "GRAVE", 84.0f, 0.0, 1.0, 0.01, 0.5,
                                     [] (double value) { return juce::String (value, 2); },
                                     { 128, true } };
    forja::ui::ForjaKnob rarelyKnob { "RARELY", "SUB", 84.0f, 0.0, 1.0, 0.01, 0.5,
                                      [] (double value) { return juce::String (value, 2); },
                                      { 128, true } };

    juce::Label ampNote;
    juce::Label toneNote;
    juce::Label bodyNote;
    juce::Label signalNote;

    forja::ui::ForjaMeter outputMeter;

    juce::TextButton bypassButton { "Bypass" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> hellAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> fizzAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> pickAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> satanAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> dangerAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> criticalAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> hMeatAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lMeatAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> swingAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> rarelyAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DeluxeAudioProcessorEditor)
};
