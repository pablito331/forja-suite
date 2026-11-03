#pragma once

#include <array>
#include <memory>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "engine/Params.h"
#include "ui/PianoLidArt.h"
#include "forja/ui/ForjaCard.h"
#include "forja/ui/ForjaChoice.h"
#include "forja/ui/ForjaEditorBase.h"
#include "forja/ui/ForjaKnob.h"
#include "forja/ui/ForjaMeter.h"

class PianoforgeAudioProcessor;

// UI do PIANOFORGE em 1000x540. Tela PIANO: cinco cards de modelagem e molho
// (Dinamica, Corpo, Equalizacao em cima; Molho e Espaco embaixo) + banco e
// saida na coluna direita; a vaga que sobrava na linha de baixo agora abriga a
// arte da Tampa (um piano de cauda cuja tampa levanta com o parametro lid). Aba
// MOTOR: os discos de ajuste fino (sala, trim, largura, abafar, brilho, afins +
// tampa e curva de velocity realocadas para la, onde ha espaco de sobra) e a
// dose da camada de molho. O estado dos 22 parametros e todo via APVTS; os
// discos vivem no 0..1 do range e delegam a leitura legivel ao processor.paramText.
class PianoforgeAudioProcessorEditor final : public forja::ui::ForjaEditorBase,
                                             public juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit PianoforgeAudioProcessorEditor (PianoforgeAudioProcessor&);
    ~PianoforgeAudioProcessorEditor() override;

private:
    void layoutContent (juce::Rectangle<int> content) override;
    void updateTelemetry() override;

    void setActiveTab (int tabIndex);
    void rebuildBankChoice();
    void rebuildPresetChoice();
    void refreshBankViews();
    void refreshBadge (bool clipped, bool bypassed, bool hasBank);

    // Le um parametro pelo ParamId e devolve o valor ja normalizado (0..1) que o
    // APVTS guarda -- usado para dirigir a arte da tampa sem recalcular dominio.
    float engineParam01 (int paramId) const;

    // Cria um disco preso a um parametro do APVTS (range 0..1) e devolve o
    // attachment ja guardado no vetor do editor.
    std::unique_ptr<forja::ui::ForjaKnob> makeKnob (int paramId, const juce::String& hint);

    // Disco INVERTIDO para o Compressor: o gesto pedido pelo usuario e "girar para
    // a ESQUERDA achata mais". O APVTS continua em 0..1 com 0 = off (default honesto,
    // mapa bit-exato intocado); so a mao do knob e que espelha -- lado DIREITO = off,
    // puxando para a ESQUERDA o amount sobe. Sem SliderAttachment: o editor faz a
    // costura manual dos dois lados (slider <-> parametro), com trava de reentrancia
    // para automacao do host e carga de preset acompanharem o disco.
    std::unique_ptr<forja::ui::ForjaKnob> makeInvertedKnob (int paramId, const juce::String& hint);

    // juce::AudioProcessorValueTreeState::Listener (so escutado no "compressor").
    // Nesta versao do JUCE o Listener expoe um unico puro-virtual parameterChanged.
    void parameterChanged (const juce::String& parameterID, float newValue) override;

    PianoforgeAudioProcessor& processor;

    juce::TextButton pianoTab { "PIANO" };
    juce::TextButton motorTab { "MOTOR" };
    juce::Component pianoPanel;
    juce::Component motorPanel;

    // Tela PIANO: cinco cards (no maximo tres discos cada) + banco + saida.
    // "Corpo" e a camada fisica da Forja (curva do tampo + anel da madeira);
    // "Espaco" acolhe a ressonancia simpatica com chorus e reverb; "Molho" traz
    // os dois knobs de worship (compressor esmagado + molho escalonado).
    forja::ui::ForjaCard dynCard { 1, forja::ui::utf8 ("Din\u00e2mica") };
    forja::ui::ForjaCard corpoCard { 2, forja::ui::utf8 ("Corpo") };
    forja::ui::ForjaCard eqCard { 3, forja::ui::utf8 ("Equaliza\u00e7\u00e3o") };
    forja::ui::ForjaCard molhoCard { 4, forja::ui::utf8 ("Molho"), true };
    forja::ui::ForjaCard spaceCard { 5, forja::ui::utf8 ("Espa\u00e7o") };
    forja::ui::ForjaCard bankCard { 6, "Banco", true };
    forja::ui::ForjaCard outCard { 7, forja::ui::utf8 ("Sa\u00edda") };

    // Espelho grafico da tampa: ocupa a vaga que sobrava na linha de baixo da
    // tela PIANO e reage a lid / body / ring do APVTS (arte nativa, nao-editavel).
    forja::pianoforge::PianoLidArt pianoArt;

    // Aba MOTOR: um card com os dez discos de ajuste fino (2 fileiras x 5).
    forja::ui::ForjaCard motorCard { 1, forja::ui::utf8 ("Motor") };

    std::array<std::unique_ptr<forja::ui::ForjaKnob>, forja::pianoforge::Param_Count> knobs;

    forja::ui::ForjaChoice bankChoice { forja::ui::utf8 ("BANCO"), {} };
    forja::ui::ForjaChoice instrumentChoice { forja::ui::utf8 ("Instrumento"), {} };
    juce::TextButton refreshButton { forja::ui::utf8 ("ATUALIZAR") };
    juce::Label statusLabel;
    forja::ui::ForjaMeter outputMeter;
    juce::TextButton bypassButton { "Bypass" };

    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> attachments;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> bankAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    int activeTab = 0;
    int lastBankRevision = -1;
    int lastPresetCount = -1;

    // Costura do disco invertido do Compressor (ver makeInvertedKnob). O editor e o
    // unico ouvinte do parametro "compressor"; a trava evita o loop slider<->param.
    juce::Slider* compressorSlider = nullptr;
    bool syncingCompressor = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PianoforgeAudioProcessorEditor)
};
