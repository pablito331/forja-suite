#pragma once

#include <array>
#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "rack/ModuleCatalog.h"
#include "rack/RackState.h"
#include "ui/HostedSlot.h"
#include "ui/RackKnob.h"

namespace forjarackui
{

// Gaveta de um slot da Serie 500: cabecalho colorido pela familia, seletor de
// categoria + seletor de modulo (a categoria e filtro visual da UI; o parametro
// 'type' continua a unica fonte de verdade), chaves Bypass/Solo, coluna de
// knobs do modulo escolhido e led de saida.
class SlotComponent : public juce::Component
{
public:
    // hosted fornece os controles dinamicos da gaveta HOSPEDADO (nada de knobs
    // do ModuleSpec); e null para um SlotComponent sem hosting (testes, futuro).
    SlotComponent (juce::AudioProcessorValueTreeState& state, int slotIndex,
                   IHostedSlot* hosted);
    ~SlotComponent() override;

    // Reloa fachada/labels/visibilidade a partir dos params atuais.
    void refresh();

    // Pico de saida (0..1) lido do engine na telemetria; move o led.
    void setOutputPeak (float peak);

    // Reducao de ganho do ultimo bloco em dB (negativa). So desenha quando o
    // modulo atual e um compressor/limitador (spec.showsGainReduction).
    void setGainReductionDb (float db);

    // Indice do modulo escolhido (para o editor detectar troca e reloidar).
    int currentTypeIndex() const;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    static juce::String slotId (int slot, const juce::String& suffix);
    static juce::String knobId (int slot, int knob);

    juce::AudioParameterChoice* typeParameter() const;

    // Altura reservada no rodape: led de saida + barra de GR (quando o modulo
    // comprime). Usada por resized() e paint() para nao sobrepor os knobs.
    int bottomReserved() const;

    // Preenche o seletor de modulos com a categoria filtrada (item id =
    // indice do catalogo + 1) e seleciona o modulo atual.
    void populateModuleChoices();
    // Ajusta o seletor de categoria para a do modulo atual, sem re-filtrar.
    void syncCategoryChoice();
    // Escreve o indice do catalogo no parametro 'type' (fonte de verdade).
    void selectCatalogIndex (int index);
    // True quando a fileira de INSTRUMENTO ocupa espaco (seletor visivel com
    // bank pronto); resized() usa isto para empurrar botoes e knobs.
    bool instrumentRowVisible() const noexcept;

    juce::AudioProcessorValueTreeState& apvts;
    int slot = 0;
    juce::String slotLabel;
    juce::Colour accentColour { forja::ui::colour::ember };
    float outputPeak = 0.0f;
    bool showsGr = false;
    float grDb = 0.0f;
    float grHold = 0.0f;
    double grHoldUntilMs = 0.0;

    juce::ComboBox categorySelector;
    juce::ComboBox selector;
    juce::TextButton bypassButton { "Bypass" };
    juce::TextButton soloButton { "Solo" };

    // Gaveta hospedada: seletor de arquivo + botao PROCURAR (revarre a pasta)
    // + botao EDITAR (abre a UI nativa/gerica do plugin em janela propria).
    IHostedSlot* hosted = nullptr;
    juce::ComboBox fileSelector;
    juce::TextButton procurarButton;
    juce::TextButton editarButton;

    // Segunda fileira da gaveta de instrumento (CANIVETE): presets do bank
    // carregado. So aparece quando o modulo vivo expoe lista nao vazia (a
    // carga em background deixa a lista vazia; a telemetria re-idrata depois).
    juce::ComboBox instrumentSelector;

    std::array<std::unique_ptr<RackKnob>, forjarack::kMaxModuleParams> knobs {};
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>,
               forjarack::kMaxModuleParams> knobAttachments {};
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>, 2>
        toggleAttachments {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SlotComponent)
};

} // namespace forjarackui
