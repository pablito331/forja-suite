#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <memory>

#include "forja/ui/ForjaCard.h"
#include "forja/ui/ForjaEditorBase.h"
#include "forja/ui/ForjaKnob.h"
#include "forja/ui/ForjaMeter.h"

class ChimeAudioProcessor;

class ChimeAudioProcessorEditor final : public forja::ui::ForjaEditorBase
{
public:
    explicit ChimeAudioProcessorEditor (ChimeAudioProcessor&);
    ~ChimeAudioProcessorEditor() override = default;

private:
    void layoutContent (juce::Rectangle<int> content) override;
    void updateTelemetry() override;

    void refreshReadouts();
    void refreshBadge (bool clipped, bool bypassed, float inputDb);

    ChimeAudioProcessor& processor;

    forja::ui::ForjaCard powerCard { 1, "Amp", true };
    forja::ui::ForjaCard toneCard { 2, "Tom" };
    forja::ui::ForjaCard speakerCard { 3, "Alto-falante" };
    forja::ui::ForjaCard signalCard { 4, "Sinal" };

    forja::ui::ForjaKnob compresKnob { forja::ui::utf8 ("COMPRESS\u00c3O"), "COMPRES", 64.0f, 0.0, 1.0, 0.01, 0.6,
                                       [] (double value) { return juce::String (value, 2); },
                                       { 140, true } };
    forja::ui::ForjaKnob speedKnob { "VELOCIDADE", "SPEED", 64.0f, 0.0, 1.0, 0.01, 0.8,
                                     [] (double value) { return juce::String (value, 2); },
                                     { 140, true } };
    forja::ui::ForjaKnob outputKnob { forja::ui::utf8 ("SA\u00cdDA"), "OUTPUT", 64.0f, 0.0, 1.0, 0.01, 0.8,
                                      [] (double value) { return juce::String (value, 2); },
                                      { 140, true } };

    forja::ui::ForjaKnob presenceKnob { forja::ui::utf8 ("PRESEN\u00c7A"), "PRESNCE", 72.0f, 0.0, 1.0, 0.01, 0.8,
                                        [] (double value) { return juce::String (value, 2); },
                                        { 180, true } };
    forja::ui::ForjaKnob highKnob { "AGUDOS", "HIGH", 72.0f, 0.0, 1.0, 0.01, 0.7,
                                    [] (double value) { return juce::String (value, 2); },
                                    { 180, true } };
    forja::ui::ForjaKnob midKnob { forja::ui::utf8 ("M\u00c9DIOS"), "MID", 72.0f, 0.0, 1.0, 0.01, 0.5,
                                   [] (double value) { return juce::String (value, 2); },
                                   { 180, true } };

    forja::ui::ForjaKnob lowKnob { "GRAVES", "LOW", 84.0f, 0.0, 1.0, 0.01, 0.7,
                                   [] (double value) { return juce::String (value, 2); },
                                   { 128, true } };
    forja::ui::ForjaKnob subKnob { "SUB", "SUB", 84.0f, 0.0, 1.0, 0.01, 0.4,
                                   [] (double value) { return juce::String (value, 2); },
                                   { 128, true } };
    forja::ui::ForjaKnob hspeakerKnob { "SPK AGUDO", "HSPEAKR", 84.0f, 0.0, 1.0, 0.01, 0.8,
                                        [] (double value) { return juce::String (value, 2); },
                                        { 128, true } };
    forja::ui::ForjaKnob lspeakerKnob { "SPK GRAVE", "LSPEAKR", 84.0f, 0.0, 1.0, 0.01, 0.4,
                                        [] (double value) { return juce::String (value, 2); },
                                        { 128, true } };

    juce::Label powerNote;
    juce::Label toneNote;
    juce::Label speakerNote;
    juce::Label signalNote;

    forja::ui::ForjaMeter outputMeter;

    juce::TextButton bypassButton { "Bypass" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> compresAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> presenceAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> highAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> midAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lowAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> subAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> hspeakerAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lspeakerAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> speedAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChimeAudioProcessorEditor)
};
