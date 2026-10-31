#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <memory>

#include "forja/ui/ForjaCard.h"
#include "forja/ui/ForjaChoice.h"
#include "forja/ui/ForjaEditorBase.h"
#include "forja/ui/ForjaKnob.h"
#include "forja/ui/ForjaMeter.h"

class EcoAudioProcessor;

class EcoAudioProcessorEditor final : public forja::ui::ForjaEditorBase
{
public:
    explicit EcoAudioProcessorEditor (EcoAudioProcessor&);
    ~EcoAudioProcessorEditor() override = default;

private:
    void layoutContent (juce::Rectangle<int> content) override;
    void updateTelemetry() override;

    void refreshReadouts();
    void refreshBadge (bool clipped, bool bypassed, float inputDb);
    void syncSpace();
    void syncDivision();
    void updateSyncUi();
    juce::String formatSyncTime() const;

    EcoAudioProcessor& processor;

    forja::ui::ForjaCard spaceCard { 1, "Espaco", true };
    forja::ui::ForjaCard echoCard { 2, "Eco" };
    forja::ui::ForjaCard mixCard { 3, "Mistura" };
    forja::ui::ForjaCard outCard { 4, "Saida" };

    forja::ui::ForjaChoice spaceChoice { "ESPACO",
                                         { "01 | 1-419 ms | 5.286 lugares",
                                           "02 | 2-423 ms | 5.392 lugares",
                                           "03 | 1-436 ms | 5.711 lugares",
                                           "04 | 1-436 ms | 5.721 lugares",
                                           "05 | 2-463 ms | 6.453 lugares",
                                           "06 | 3-472 ms | 6.729 lugares",
                                           "07 | 3-444 ms | 5.975 lugares",
                                           "08 | 2-457 ms | 6.291 lugares",
                                           "09 | 2-461 ms | 6.401 lugares",
                                           "10 | 2-473 ms | 6.729 lugares",
                                           "11 | 3-483 ms | 7.027 lugares",
                                           "12 | 3-489 ms | 7.203 lugares",
                                           "13 | 2-489 ms | 7.206 lugares",
                                           "14 | 2-492 ms | 7.281 lugares",
                                           "15 | 2-519 ms | 8.101 lugares",
                                           "16 | 2-423 ms | 5.399 lugares",
                                           "17 | 2-473 ms | 6.725 lugares" } };

    forja::ui::ForjaKnob regenKnob { forja::ui::utf8 ("REGENERA\u00c7\u00c3O"), "REGEN", 64.0f,
                                     0.0, 1.0, 0.01, 0.5,
                                     [] (double value)
                                     { return juce::String (value * 100.0, 0) + " %"; },
                                     { 104, true } };
    forja::ui::ForjaKnob speedKnob { "VELOCIDADE", "SPEED", 64.0f, 0.0, 1.0, 0.01, 0.5,
                                     [] (double value)
                                     { return juce::String (value * 100.0, 0) + " %"; },
                                     { 104, true } };
    forja::ui::ForjaKnob mixKnob { "MISTURA", "MIX", 92.0f, 0.0, 1.0, 0.01, 0.5,
                                   [] (double value)
                                   { return juce::String (value * 100.0, 0) + " %"; },
                                   { 130, true } };

    juce::Label spaceNote;
    juce::Label echoNote;
    juce::Label mixNote;

    juce::TextButton syncButton { "SYNC" };
    forja::ui::ForjaChoice divisionChoice { forja::ui::utf8 ("DIVIS\u00c3O"),
                                            { "1/16", "1/8", "1/4", "1/2", "1/1", "2/1" } };

    forja::ui::ForjaMeter outputMeter;

    juce::TextButton bypassButton { "Bypass" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> regenAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> speedAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncAttachment;

    bool syncingSpace = false;
    bool syncingDivision = false;
    bool syncUiSync = false;
    bool syncUiLocked = false;
    bool syncUiInitialized = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EcoAudioProcessorEditor)
};
