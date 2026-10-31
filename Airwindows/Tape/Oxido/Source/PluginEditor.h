#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <memory>

#include "forja/dsp/ForjaOxido.h"
#include "forja/ui/ForjaCard.h"
#include "forja/ui/ForjaEditorBase.h"
#include "forja/ui/ForjaKnob.h"
#include "forja/ui/ForjaMeter.h"
#include "forja/ui/ForjaTapeArt.h"

class OxidoAudioProcessor;

class OxidoAudioProcessorEditor final : public forja::ui::ForjaEditorBase
{
public:
    explicit OxidoAudioProcessorEditor (OxidoAudioProcessor&);
    ~OxidoAudioProcessorEditor() override = default;

private:
    void layoutContent (juce::Rectangle<int> content) override;
    void updateTelemetry() override;

    void refreshReadouts();
    void refreshBadge (bool clipped, bool bypassed, float inputDb);

    OxidoAudioProcessor& processor;

    forja::ui::ForjaCard ioCard { 1, forja::ui::utf8 ("Entrada e sa\u00edda"), true };
    forja::ui::ForjaCard tapeCard { 2, "Fita" };
    forja::ui::ForjaCard textureCard { 3, "Textura" };
    forja::ui::ForjaCard mixCard { 4, "Mistura" };

    forja::ui::ForjaKnob inputKnob { "ENTRADA", "INPUT", 92.0f, 0.0, 1.0, 0.01, 0.5,
                                     [] (double value)
                                     { return juce::String ((value * 36.0) - 18.0, 1) + " dB"; },
                                     { 130, true } };
    forja::ui::ForjaKnob outputKnob { forja::ui::utf8 ("SA\u00cdDA"), "OUTPUT", 92.0f, 0.0, 1.0, 0.01, 0.5,
                                      [] (double value)
                                      { return juce::String ((value * 36.0) - 18.0, 1) + " dB"; },
                                      { 130, true } };
    forja::ui::ForjaKnob tapeHighKnob { "FITA AGUDO", "TAPEHI", 64.0f, 0.0, 1.0, 0.01,
                                        0.562341325190349,
                                        [] (double value)
                                        { return juce::String ((((value * value) * (value * value))
                                                                * 148.5) + 1.5, 1) + " ips"; },
                                        { 104, true } };
    forja::ui::ForjaKnob tapeLowKnob { "FITA GRAVE", "TAPELO", 64.0f, 0.0, 1.0, 0.01,
                                       0.562341325190349,
                                       [] (double value)
                                       { return juce::String ((((value * value) * (value * value))
                                                               * 148.5) + 1.5, 1) + " ips"; },
                                       { 104, true } };
    forja::ui::ForjaKnob flutterKnob { "FLUTTER", "FLUT", 92.0f, 0.0, 1.0, 0.01, 0.5,
                                       [] (double value) { return juce::String (value, 2); },
                                       { 130, true } };
    forja::ui::ForjaKnob noiseKnob { forja::ui::utf8 ("RU\u00cdDO"), "NOISE", 92.0f, 0.0, 1.0, 0.01, 0.5,
                                     [] (double value) { return juce::String (value, 2); },
                                     { 130, true } };
    forja::ui::ForjaKnob mixKnob { "MISTURA", "MIX", 92.0f, 0.0, 1.0, 0.01, 1.0,
                                   [] (double value)
                                   { return juce::String ((value * 2.0) - 1.0, 2); },
                                   { 130, true } };

    juce::Label inputNote;
    juce::Label tapeNote;
    juce::Label textureNote;
    juce::Label mixNote;

    forja::ui::ForjaTapeArt cassetteArt { forja::ui::ForjaTapeArt::Style::cassette };

    forja::ui::ForjaMeter outputMeter;

    juce::TextButton bypassButton { "Bypass" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> inputAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> tapeHighAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> tapeLowAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> flutterAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> noiseAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OxidoAudioProcessorEditor)
};
