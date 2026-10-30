#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <memory>

#include "forja/dsp/ForjaLima.h"
#include "forja/ui/ForjaCard.h"
#include "forja/ui/ForjaEditorBase.h"
#include "forja/ui/ForjaEqCurve.h"
#include "forja/ui/ForjaKnob.h"
#include "forja/ui/ForjaMeter.h"

class LimaAudioProcessor;

class LimaAudioProcessorEditor final : public forja::ui::ForjaEditorBase
{
public:
    explicit LimaAudioProcessorEditor (LimaAudioProcessor&);
    ~LimaAudioProcessorEditor() override = default;

private:
    void layoutContent (juce::Rectangle<int> content) override;
    void updateTelemetry() override;

    void refreshReadouts();
    void refreshCurve (bool force);
    void refreshBadge (bool clipped, bool bypassed, float inputDb);
    void applyMarkerDrag (const juce::String& id, float frequency);

    LimaAudioProcessor& processor;

    forja::ui::ForjaCard curveCard { 1, forja::ui::utf8 ("Curva"), true };
    forja::ui::ForjaCard gainCard { 2, forja::ui::utf8 ("Ganhos") };
    forja::ui::ForjaCard freqCard { 3, forja::ui::utf8 ("Frequ\u00eancias") };

    forja::ui::ForjaEqCurve curve;

    forja::ui::ForjaKnob highGainKnob { "ALTA", "ganho", 92.0f, 0.0, 1.0, 0.01, 0.5,
                                        [] (double value) { return juce::String (value, 2); },
                                        { 130, true } };
    forja::ui::ForjaKnob highMidGainKnob { "M. ALTA", "ganho", 92.0f, 0.0, 1.0, 0.01, 0.5,
                                           [] (double value) { return juce::String (value, 2); },
                                           { 130, true } };
    forja::ui::ForjaKnob lowMidGainKnob { "M. BAIXA", "ganho", 92.0f, 0.0, 1.0, 0.01, 0.5,
                                          [] (double value) { return juce::String (value, 2); },
                                          { 130, true } };
    forja::ui::ForjaKnob bassGainKnob { "GRAVES", "ganho", 92.0f, 0.0, 1.0, 0.01, 0.5,
                                        [] (double value) { return juce::String (value, 2); },
                                        { 130, true } };

    forja::ui::ForjaKnob highFreqKnob { "ALTA", "freq", 92.0f, 0.0, 1.0, 0.01, 0.5,
                                        [] (double value) { return juce::String (value, 2); },
                                        { 130, true } };
    forja::ui::ForjaKnob highMidFreqKnob { "M. ALTA", "freq", 92.0f, 0.0, 1.0, 0.01, 0.5,
                                           [] (double value) { return juce::String (value, 2); },
                                           { 130, true } };
    forja::ui::ForjaKnob lowMidFreqKnob { "M. BAIXA", "freq", 92.0f, 0.0, 1.0, 0.01, 0.5,
                                          [] (double value) { return juce::String (value, 2); },
                                          { 130, true } };
    forja::ui::ForjaKnob bassFreqKnob { "GRAVES", "freq", 92.0f, 0.0, 1.0, 0.01, 0.5,
                                        [] (double value) { return juce::String (value, 2); },
                                        { 130, true } };

    forja::ui::ForjaMeter outputMeter;

    juce::TextButton bypassButton { "Bypass" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> highGainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> highMidGainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lowMidGainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bassGainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> highFreqAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> highMidFreqAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lowMidFreqAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bassFreqAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    forja::dsp::LimaDSP curveAdapter;
    std::array<float, 8> lastCurveParams { };
    double lastCurveSampleRate = -1.0;
    bool curveInitialised = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LimaAudioProcessorEditor)
};
