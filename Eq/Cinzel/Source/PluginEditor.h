#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <memory>

#include "forja/dsp/ForjaCinzel.h"
#include "forja/ui/ForjaCard.h"
#include "forja/ui/ForjaEditorBase.h"
#include "forja/ui/ForjaEqCurve.h"
#include "forja/ui/ForjaGainReductionMeter.h"
#include "forja/ui/ForjaKnob.h"
#include "forja/ui/ForjaMeter.h"

class CinzelAudioProcessor;

class CinzelAudioProcessorEditor final : public forja::ui::ForjaEditorBase
{
public:
    explicit CinzelAudioProcessorEditor (CinzelAudioProcessor&);
    ~CinzelAudioProcessorEditor() override = default;

private:
    void layoutContent (juce::Rectangle<int> content) override;
    void updateTelemetry() override;

    void refreshReadouts();
    void refreshCurve (bool force);
    void refreshBadge (bool clipped, bool bypassed, float inputDb, float reductionDb);
    void beginMarkerDrag (const juce::String& id);
    void applyMarkerDrag (const juce::String& id, float frequency, float dbDelta);

    CinzelAudioProcessor& processor;

    forja::ui::ForjaCard curveCard { 1, forja::ui::utf8 ("Curva"), true };
    forja::ui::ForjaCard bandCard { 2, forja::ui::utf8 ("Bandas") };
    forja::ui::ForjaCard dynamicsCard { 3, forja::ui::utf8 ("Din\u00e2mica") };

    forja::ui::ForjaEqCurve curve;

    forja::ui::ForjaKnob highFreqKnob { "ALTA", "freq", 48.0f, 0.0, 1.0, 0.01, 0.5,
                                        [] (double value) { return juce::String (value, 2); },
                                        { 80, true } };
    forja::ui::ForjaKnob highGainKnob { "ALTA", "ganho", 48.0f, 0.0, 1.0, 0.01, 0.5,
                                        [] (double value) { return juce::String (value, 2); },
                                        { 80, true } };
    forja::ui::ForjaKnob highResoKnob { "ALTA", "reso", 48.0f, 0.0, 1.0, 0.01, 0.5,
                                        [] (double value) { return juce::String (value, 2); },
                                        { 80, true } };
    forja::ui::ForjaKnob highMidFreqKnob { "M. ALTA", "freq", 48.0f, 0.0, 1.0, 0.01, 0.5,
                                           [] (double value) { return juce::String (value, 2); },
                                           { 80, true } };
    forja::ui::ForjaKnob highMidGainKnob { "M. ALTA", "ganho", 48.0f, 0.0, 1.0, 0.01, 0.5,
                                           [] (double value) { return juce::String (value, 2); },
                                           { 80, true } };
    forja::ui::ForjaKnob highMidResoKnob { "M. ALTA", "reso", 48.0f, 0.0, 1.0, 0.01, 0.5,
                                           [] (double value) { return juce::String (value, 2); },
                                           { 80, true } };
    forja::ui::ForjaKnob lowMidFreqKnob { "M. BAIXA", "freq", 48.0f, 0.0, 1.0, 0.01, 0.5,
                                          [] (double value) { return juce::String (value, 2); },
                                          { 80, true } };
    forja::ui::ForjaKnob lowMidGainKnob { "M. BAIXA", "ganho", 48.0f, 0.0, 1.0, 0.01, 0.5,
                                          [] (double value) { return juce::String (value, 2); },
                                          { 80, true } };
    forja::ui::ForjaKnob lowMidResoKnob { "M. BAIXA", "reso", 48.0f, 0.0, 1.0, 0.01, 0.5,
                                          [] (double value) { return juce::String (value, 2); },
                                          { 80, true } };

    forja::ui::ForjaKnob mixKnob { "MISTURA", "dry/wet", 92.0f, 0.0, 1.0, 0.01, 1.0,
                                   [] (double value) { return juce::String (value, 2); },
                                   { 130, true } };
    forja::ui::ForjaKnob dynKnob { forja::ui::utf8 ("DIN\u00c2MICA"), forja::ui::utf8 ("redu\u00e7\u00e3o"), 92.0f, 0.0, 1.0, 0.01, 0.0,
                                   [] (double value) { return juce::String (value, 2); },
                                   { 130, true } };

    forja::ui::ForjaGainReductionMeter gainReductionMeter;
    forja::ui::ForjaMeter outputMeter;

    juce::TextButton bypassButton { "Bypass" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> highFreqAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> highGainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> highResoAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> highMidFreqAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> highMidGainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> highMidResoAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lowMidFreqAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lowMidGainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lowMidResoAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> dynAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    forja::dsp::CinzelDSP curveAdapter;
    std::array<float, 9> lastCurveParams { };
    double lastCurveSampleRate = -1.0;
    bool curveInitialised = false;
    double dragStartGain = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CinzelAudioProcessorEditor)
};
