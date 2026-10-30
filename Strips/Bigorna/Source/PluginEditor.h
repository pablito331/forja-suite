#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <memory>

#include "forja/dsp/ForjaBigorna.h"
#include "forja/ui/ForjaCard.h"
#include "forja/ui/ForjaEditorBase.h"
#include "forja/ui/ForjaGainReductionMeter.h"
#include "forja/ui/ForjaKnob.h"
#include "forja/ui/ForjaMeter.h"

class BigornaAudioProcessor;

class BigornaAudioProcessorEditor final : public forja::ui::ForjaEditorBase
{
public:
    explicit BigornaAudioProcessorEditor (BigornaAudioProcessor&);
    ~BigornaAudioProcessorEditor() override = default;

private:
    void layoutContent (juce::Rectangle<int> content) override;
    void updateTelemetry() override;

    void refreshReadouts();
    void refreshBadge (bool clipped, bool bypassed, float inputDb, float reductionDb);

    BigornaAudioProcessor& processor;

    forja::ui::ForjaCard tomCard { 1, "Tom", true };
    forja::ui::ForjaCard freqCard { 2, forja::ui::utf8 ("Frequ\u00eancias") };
    forja::ui::ForjaCard filterCard { 3, forja::ui::utf8 ("Filtros e sa\u00edda") };
    forja::ui::ForjaCard compCard { 4, "Compressor" };

    forja::ui::ForjaKnob trebleKnob { "AGUDOS", forja::ui::utf8 ("\u00b16 dB"), 68.0f, 0.0, 1.0, 0.01, 0.5,
                                      [] (double value) { return juce::String (value, 2); },
                                      { 130, true } };
    forja::ui::ForjaKnob midKnob { forja::ui::utf8 ("M\u00c9DIOS"), forja::ui::utf8 ("\u00b16 dB"), 68.0f, 0.0, 1.0, 0.01, 0.5,
                                   [] (double value) { return juce::String (value, 2); },
                                   { 130, true } };
    forja::ui::ForjaKnob bassKnob { "GRAVES", forja::ui::utf8 ("\u00b16 dB"), 68.0f, 0.0, 1.0, 0.01, 0.5,
                                    [] (double value) { return juce::String (value, 2); },
                                    { 130, true } };
    forja::ui::ForjaKnob trebFreqKnob { "FREQ. ALTA", "TREBFRQ", 92.0f, 0.0, 1.0, 0.01, 0.5,
                                        [] (double value) { return juce::String (value, 2); },
                                        { 138, true } };
    forja::ui::ForjaKnob bassFreqKnob { "FREQ. GRAVE", "BASSFRQ", 92.0f, 0.0, 1.0, 0.01, 0.5,
                                        [] (double value) { return juce::String (value, 2); },
                                        { 138, true } };
    forja::ui::ForjaKnob lowCapKnob { "PASSA-BAIXOS", "LOWCAP", 92.0f, 0.0, 1.0, 0.01, 1.0,
                                      [] (double value) { return juce::String (value, 2); },
                                      { 138, true } };
    forja::ui::ForjaKnob hiCapKnob { "PASSA-ALTOS", "HICAP", 92.0f, 0.0, 1.0, 0.01, 0.0,
                                     [] (double value) { return juce::String (value, 2); },
                                     { 138, true } };
    forja::ui::ForjaKnob compresKnob { forja::ui::utf8 ("COMPRESS\u00c3O"), "COMPRES", 64.0f, 0.0, 1.0, 0.01, 0.0,
                                       [] (double value) { return juce::String (value, 2); },
                                       { 104, true } };
    forja::ui::ForjaKnob compSpdKnob { "VELOCIDADE", "COMPSPD", 64.0f, 0.0, 1.0, 0.01, 0.0,
                                       [] (double value) { return juce::String (value, 2); },
                                       { 104, true } };
    forja::ui::ForjaKnob outputKnob { forja::ui::utf8 ("SA\u00cdDA"), "OUTPUT", 92.0f, 0.0, 1.0, 0.01, 0.33,
                                      [] (double value) { return juce::String (value, 2); },
                                      { 130, true } };

    juce::Label neutralNote;
    juce::Label trebFreqNote;
    juce::Label bassFreqNote;
    juce::Label lowCapNote;
    juce::Label hiCapNote;

    forja::ui::ForjaGainReductionMeter gainReductionMeter;
    forja::ui::ForjaMeter outputMeter;

    juce::TextButton bypassButton { "Bypass" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> trebleAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> midAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bassAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> trebFreqAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bassFreqAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lowCapAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> hiCapAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> compresAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> compSpdAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BigornaAudioProcessorEditor)
};
