#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <memory>

#include "forja/ui/ForjaCard.h"
#include "forja/ui/ForjaChoice.h"
#include "forja/ui/ForjaEditorBase.h"
#include "forja/ui/ForjaGainReductionMeter.h"
#include "forja/ui/ForjaKnob.h"
#include "forja/ui/ForjaMeter.h"

class MuralhaAudioProcessor;

class MuralhaAudioProcessorEditor final : public forja::ui::ForjaEditorBase
{
public:
    explicit MuralhaAudioProcessorEditor (MuralhaAudioProcessor&);
    ~MuralhaAudioProcessorEditor() override = default;

private:
    void layoutContent (juce::Rectangle<int> content) override;
    void updateTelemetry() override;

    void syncMode();
    void refreshReadouts();
    void refreshBadge (bool clipped, bool bypassed, float inputDb, float grDb);

    MuralhaAudioProcessor& processor;

    forja::ui::ForjaCard inputCard { 1, "Entrada", true };
    forja::ui::ForjaCard clipCard { 2, "Clipe" };
    forja::ui::ForjaCard modeCard { 3, "Modo" };
    forja::ui::ForjaCard outCard { 4, forja::ui::utf8 ("Sa\u00edda") };

    forja::ui::ForjaKnob boostKnob { "BOOST", forja::ui::utf8 ("AT\u00c9 18 dB"), 108.0f,
                                     0.0, 1.0, 0.01, 0.0,
                                     [] (double value)
                                     { return juce::String (value * 18.0, 1) + " dB"; } };
    forja::ui::ForjaKnob matchKnob { "MATCH", forja::ui::utf8 ("AT\u00c9 18 dB"), 84.0f,
                                     0.0, 1.0, 0.01, 0.0,
                                     [] (double value)
                                     { return juce::String (value * 18.0, 1) + " dB"; } };
    forja::ui::ForjaKnob ceilingKnob { "CEILING", forja::ui::utf8 ("TETO"), 84.0f,
                                       0.0, 1.0, 0.01, 0.75,
                                       [] (double value)
                                       { return juce::String (value, 2); } };
    forja::ui::ForjaKnob noiseKnob { "NOISE", "DITHER", 84.0f,
                                     0.0, 1.0, 0.01, 0.7,
                                     [] (double value)
                                     { return juce::String (value, 2); } };

    forja::ui::ForjaChoice modeChoice { "MODO", { "BOOST", "MATCH", "CLIP ONLY" } };

    juce::Label inputNote;
    juce::Label clipNote;
    juce::Label modeNote;

    forja::ui::ForjaGainReductionMeter gainReductionMeter;
    forja::ui::ForjaMeter outputMeter;

    juce::TextButton bypassButton { "Bypass" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> boostAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> matchAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> ceilingAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> noiseAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    bool syncingMode = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MuralhaAudioProcessorEditor)
};
