#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <memory>

#include "engine/SampleStore.h"
#include "forja/ui/ForjaCard.h"
#include "forja/ui/ForjaEditorBase.h"
#include "forja/ui/ForjaKnob.h"
#include "forja/ui/ForjaMeter.h"

class MinidrumAudioProcessor;

// Foto do kit: preview decodificado no thread de carga, ou placeholder
// vectorial (aro + pele) quando o kit nao traz imagem.
class KitArt final : public juce::Component
{
public:
    void setKit (std::shared_ptr<const forja::drumforge::LoadedKit> kit);

    void paint (juce::Graphics& g) override;

private:
    std::shared_ptr<const forja::drumforge::LoadedKit> shown;
};

class MinidrumAudioProcessorEditor final : public forja::ui::ForjaEditorBase
{
public:
    explicit MinidrumAudioProcessorEditor (MinidrumAudioProcessor&);
    ~MinidrumAudioProcessorEditor() override;

private:
    void layoutContent (juce::Rectangle<int> content) override;
    void updateTelemetry() override;

    void chooseKitFile();
    void refreshKitViews();
    void refreshBadge (bool clipped, bool bypassed, bool hasKit);

    MinidrumAudioProcessor& processor;

    forja::ui::ForjaCard kitCard;
    forja::ui::ForjaCard outCard;

    KitArt kitArt;
    juce::TextButton loadButton;
    juce::Label statusLabel;

    forja::ui::ForjaKnob volumeKnob;
    forja::ui::ForjaMeter outputMeter;
    juce::TextButton bypassButton { "Bypass" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> volumeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
    std::unique_ptr<juce::FileChooser> chooser;

    int lastKitRevision = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MinidrumAudioProcessorEditor)
};
