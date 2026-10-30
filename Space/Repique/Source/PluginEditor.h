#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <memory>

#include "forja/ui/ForjaCard.h"
#include "forja/ui/ForjaChoice.h"
#include "forja/ui/ForjaEditorBase.h"
#include "forja/ui/ForjaKnob.h"
#include "forja/ui/ForjaMeter.h"
#include "forja/ui/ForjaTapeArt.h"

class RepiqueAudioProcessor;

class RepiqueAudioProcessorEditor final : public forja::ui::ForjaEditorBase
{
public:
    explicit RepiqueAudioProcessorEditor (RepiqueAudioProcessor&);
    ~RepiqueAudioProcessorEditor() override = default;

private:
    void layoutContent (juce::Rectangle<int> content) override;
    void updateTelemetry() override;

    void refreshReadouts();
    void refreshBadge (bool clipped, bool bypassed, float inputDb);
    void syncDivision();
    void updateSyncUi();

    RepiqueAudioProcessor& processor;

    forja::ui::ForjaCard timeCard { 1, "Tempo", true };
    forja::ui::ForjaCard tapeCard { 2, "Fita" };
    forja::ui::ForjaCard filterCard { 3, "Filtro" };
    forja::ui::ForjaCard mixCard { 4, "Mistura" };

    forja::ui::ForjaKnob timeKnob { "TEMPO", "TIME", 92.0f, 0.0, 1.0, 0.01, 1.0,
                                    [this] (double value) { return formatTime (value); },
                                    { 130, true } };
    forja::ui::ForjaKnob regenKnob { forja::ui::utf8 ("REGENERA\u00c7\u00c3O"), "REGEN", 64.0f,
                                     0.0, 1.0, 0.01, 0.0,
                                     [] (double value)
                                     { return juce::String (value * value * 100.0, 0) + " %"; },
                                     { 104, true } };
    forja::ui::ForjaKnob flutterKnob { "FLUTTER", "FLUT", 64.0f, 0.0, 1.0, 0.01, 0.0,
                                       [] (double value)
                                       { return juce::String (value * value * 100.0, 0) + " %"; },
                                       { 104, true } };
    forja::ui::ForjaKnob freqKnob { forja::ui::utf8 ("FREQU\u00caNCIA"), "FREQ", 92.0f, 0.0, 1.0,
                                    0.01, 0.5,
                                    [this] (double value) { return formatFrequency (value); },
                                    { 130, true } };
    forja::ui::ForjaKnob resoKnob { forja::ui::utf8 ("RESSON\u00c2NCIA"), "RESO", 92.0f, 0.0, 1.0,
                                    0.01, 0.0,
                                    [] (double value)
                                    { return juce::String ((value * value) + 0.01, 2); },
                                    { 130, true } };
    forja::ui::ForjaKnob mixKnob { "MISTURA", "MIX", 92.0f, 0.0, 1.0, 0.01, 1.0,
                                   [] (double value)
                                   { return juce::String (value * 100.0, 0) + " %"; },
                                   { 130, true } };

    juce::Label timeNote;
    juce::Label tapeNote;
    juce::Label filterNote;
    juce::Label mixNote;

    juce::TextButton syncButton { "SYNC" };
    forja::ui::ForjaChoice divisionChoice { forja::ui::utf8 ("DIVIS\u00c3O"),
                                            { "1/16", "1/8", "1/4", "1/2", "1/1", "2/1" } };

    forja::ui::ForjaTapeArt reelArt { forja::ui::ForjaTapeArt::Style::reels };

    forja::ui::ForjaMeter outputMeter;

    juce::TextButton bypassButton { "Bypass" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> timeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> regenAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> flutterAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> freqAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> resoAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncAttachment;

    juce::String formatTime (double value) const;
    juce::String formatFrequency (double value) const;
    juce::String formatSyncTime() const;

    bool syncingDivision = false;
    bool syncUiSync = false;
    bool syncUiLocked = false;
    bool syncUiInitialized = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RepiqueAudioProcessorEditor)
};
