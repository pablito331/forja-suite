#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <memory>
#include <vector>

#include "forja/ui/ForjaCard.h"
#include "forja/ui/ForjaEditorBase.h"
#include "forja/ui/ForjaEqGraph.h"
#include "forja/ui/ForjaMeter.h"

class TracoAudioProcessor;

namespace forja::ui
{
class ForjaKnob;
class ForjaChoice;
} // namespace forja::ui

class TracoAudioProcessorEditor final : public forja::ui::ForjaEditorBase
{
public:
    explicit TracoAudioProcessorEditor (TracoAudioProcessor&);
    ~TracoAudioProcessorEditor() override;

private:
    void layoutContent (juce::Rectangle<int> content) override;
    void updateTelemetry() override;
    void refreshBadge (bool clipped, bool bypassed, float inPeak);

    // Selecao de secao no grafico: -1 nao seleciona nada, 0 = HP, 1 = LP,
    // 2..6 = bandas 0..4, 7 = de-esser. Mapeia o modelo do grafico ao APVTS.
    void selectSection (int section);
    void rebuildPanel();
    void layoutPanelContents();
    void refreshGraph();

    void handleNodeDrag (const juce::String& id, float frequencyHz, float gainDb,
                         float widthNorm, bool widthDrag);
    void handleNodeReset (const juce::String& id);
    void handleNodeGlyph (const juce::String& id, int glyph);

    static int sectionForId (const juce::String& id);

    void setParam (const juce::String& id, float norm);

    TracoAudioProcessor& processor;

    forja::ui::ForjaEqGraph graph;
    forja::ui::ForjaMeter inputMeter;
    forja::ui::ForjaMeter outputMeter;

    // Painel contextual (Pro-Q3): card flutuante centrado abaixo do grafico,
    // so visivel com uma secao selecionada. Os controles dentro dele mudam de
    // tipo conforme a secao, entao sao reconstruidos a cada selecao.
    forja::ui::ForjaCard panelCard { 1, "SECAO" };
    int selectedSection = -1;
    bool panelPlacedByUser = false; // painel movido/redimensionado: para de centralizar
    std::vector<std::unique_ptr<juce::Component>> panelWidgets;
    // No JUCE 9 os attachments nao herdam de uma base comum: sliders e botoes
    // moram em vetores propios, destruidos juntos na troca de selecao.
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> panelSliderAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> panelButtonAttachments;

    // Ponteiros nao-proprietarios dos controles do painel, para reposicionar em
    // cada resized sem reconstruir (e sem religar attachments) a cada tick.
    forja::ui::ForjaKnob* panelKnobA = nullptr;
    forja::ui::ForjaKnob* panelKnobB = nullptr;
    forja::ui::ForjaKnob* panelKnobC = nullptr;
    forja::ui::ForjaChoice* panelChoice = nullptr;
    juce::ToggleButton* panelOnToggle = nullptr;
    juce::ToggleButton* panelListenToggle = nullptr;

    juce::TextButton bypassButton { "Bypass" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TracoAudioProcessorEditor)
};
