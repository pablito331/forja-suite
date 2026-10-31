#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <memory>

#include "forja/dsp/ForjaCadinho.h"
#include "forja/ui/ForjaCard.h"
#include "forja/ui/ForjaEditorBase.h"
#include "forja/ui/ForjaGainReductionMeter.h"
#include "forja/ui/ForjaKnob.h"
#include "forja/ui/ForjaMeter.h"

class CadinhoAudioProcessor;

class CadinhoAudioProcessorEditor final : public forja::ui::ForjaEditorBase
{
public:
    explicit CadinhoAudioProcessorEditor (CadinhoAudioProcessor&);
    ~CadinhoAudioProcessorEditor() override = default;

private:
    void layoutContent (juce::Rectangle<int> content) override;
    void updateTelemetry() override;

    void refreshReadouts();
    void refreshBadge (bool clipped, bool bypassed, float inputDb, float reductionDb);

    CadinhoAudioProcessor& processor;

    forja::ui::ForjaCard highCard { 1, "Agudos", true };
    forja::ui::ForjaCard midCard { 2, forja::ui::utf8 ("M\u00e9dios") };
    forja::ui::ForjaCard bassCard { 3, "Graves" };
    forja::ui::ForjaCard dynCard { 4, forja::ui::utf8 ("Din\u00e2mica e sa\u00edda") };

    forja::ui::ForjaKnob highKnob { "AGUDOS", "HIGH", 92.0f, 0.0, 1.0, 0.01, 0.5,
                                    [] (double value) { return juce::String (value, 2); },
                                    { 130, true } };
    forja::ui::ForjaKnob highFreqKnob { "FREQ. ALTA", "HIGHF", 92.0f, 0.0, 1.0, 0.01, 0.5,
                                        [] (double value) { return juce::String (value, 2); },
                                        { 130, true } };
    forja::ui::ForjaKnob highMidKnob { "M. ALTA", "HMID", 64.0f, 0.0, 1.0, 0.01, 0.5,
                                       [] (double value) { return juce::String (value, 2); },
                                       { 100, true } };
    forja::ui::ForjaKnob highMidFreqKnob { "FREQ. M. ALTA", "HMIDF", 64.0f, 0.0, 1.0, 0.01, 0.5,
                                           [] (double value) { return juce::String (value, 2); },
                                           { 100, true } };
    forja::ui::ForjaKnob lowMidKnob { "M. BAIXA", "LMID", 64.0f, 0.0, 1.0, 0.01, 0.5,
                                      [] (double value) { return juce::String (value, 2); },
                                      { 100, true } };
    forja::ui::ForjaKnob lowMidFreqKnob { "FREQ. M. BAIXA", "LMIDF", 64.0f, 0.0, 1.0, 0.01, 0.5,
                                          [] (double value) { return juce::String (value, 2); },
                                          { 100, true } };
    forja::ui::ForjaKnob bassKnob { "GRAVES", "BASS", 92.0f, 0.0, 1.0, 0.01, 0.5,
                                    [] (double value) { return juce::String (value, 2); },
                                    { 138, true } };
    forja::ui::ForjaKnob bassFreqKnob { "FREQ. GRAVE", "BASSF", 92.0f, 0.0, 1.0, 0.01, 0.5,
                                        [] (double value) { return juce::String (value, 2); },
                                        { 138, true } };
    forja::ui::ForjaKnob threshKnob { forja::ui::utf8 ("DIN\u00c2MICA"), "THRESH", 64.0f, 0.0, 1.0, 0.01, 1.0,
                                      [] (double value) { return juce::String (value, 2); },
                                      { 104, true } };
    forja::ui::ForjaKnob faderKnob { "FADER", forja::ui::utf8 ("SA\u00cdDA"), 64.0f, 0.0, 1.0, 0.01, 0.5,
                                     [] (double value) { return juce::String (value, 2); },
                                     { 104, true } };

    juce::Label highNote;
    juce::Label midNoteA;
    juce::Label midNoteB;
    juce::Label bassNote;
    juce::Label bassFreqNote;

    forja::ui::ForjaMeter outputMeter;
    forja::ui::ForjaGainReductionMeter gainReductionMeter;

    juce::TextButton bypassButton { "Bypass" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> highAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> highFreqAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> highMidAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> highMidFreqAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lowMidAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lowMidFreqAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bassAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bassFreqAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> threshAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> faderAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CadinhoAudioProcessorEditor)
};
