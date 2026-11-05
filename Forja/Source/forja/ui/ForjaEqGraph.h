#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

namespace forja::ui
{

/* Grafico parametrico interativo no estilo Pro-Q3, generico da forja_ui.

   Ao contrario do ForjaEqCurve (que recebe pontos ja medidos e nos apenas de
   frequencia), o ForjaEqGraph e feito para o controle primario pelo grafico:
   cada no e uma banda real que se arrasta (freq no eixo x, ganho no eixo y),
   se molda com Shift-arrasto (largura / Q), se reseta com duplo-clique e troca
   de tipo pelo menu de contexto. O componente nao conhece nenhum plugin: o host
   empurra a lista de Nos e recebe callbacks por id, entao quem converte id em
   parametro e o editor.

   A curva composta vem de um sampler (o host devolve dB em cada frequencia),
   nunca de um vetor pre-computado: assim o grafico reflete o estado atual sem
   o editor reconstruir pontos a cada repaint. O RTA de fundo e opcional. */
class ForjaEqGraph : public juce::Component
{
public:
    enum Glyph : int
    {
        bell = 0,
        lowShelf = 1,
        highShelf = 2,
        highPass = 3,
        lowPass = 4,
        notch = 5,
        deEsser = 6
    };

    struct Node
    {
        juce::String id;
        float frequency = 1000.0f;   // Hz
        float gainDb = 0.0f;         // altura do no no eixo y (0 para so-frequencia)
        int glyph = bell;            // desenho do no
        bool enabled = true;         // off = muted, nao entra na curva
        bool selected = false;
        bool freqOnly = false;       // y travado (HP / LP / de-esser)
        bool allowWidth = true;      // Shift-arrasto muda a largura (Q)
        // Cor do no. Invalido = cai no ember padrao da casa; o host pinta cada
        // banda com uma cor propria para as bolinhas se distinguirem.
        juce::Colour accent;
        // Altura fixa dos nos freqOnly, em dB. 0 deixa na linha do centro;
        // negativo empurra para baixo (o de-esser mora numa faixa propria).
        float laneDb = 0.0f;

        float displayDb() const noexcept { return freqOnly ? laneDb : gainDb; }
    };

    ForjaEqGraph();

    void setNodes (std::vector<Node> newNodes);
    void setSpectrum (const float* bandDb, int count);
    void setBypassed (bool shouldBeBypassed);

    // Host devolve a resposta composta (dB) numa frequencia; o grafico amostra
    // ao longo do eixo x para desenhar a curva. Recebe nullptr para limpar.
    std::function<float (float frequencyHz)> responseSampler;

    // Interacao, sempre por id do no (o host faz o mapa id -> parametros).
    std::function<void (const juce::String& id)> onNodeSelected;
    // frequencyHz/gainDb guiam arrasto normal; quando widthDrag e true, o host
    // usa widthNorm (0 embaixo, 1 em cima) para a largura / Q do no.
    std::function<void (const juce::String& id, float frequencyHz, float gainDb,
                        float widthNorm, bool widthDrag)> onNodeDrag;
    std::function<void (const juce::String& id)> onNodeReset;      // duplo-clique
    std::function<void (const juce::String& id, int newGlyph)> onNodeGlyph; // menu
    std::function<void()> onBackgroundClicked;                     // clique fora

    void paint (juce::Graphics& g) override;
    void mouseMove (const juce::MouseEvent& event) override;
    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;
    void mouseUp (const juce::MouseEvent& event) override;
    void mouseDoubleClick (const juce::MouseEvent& event) override;

private:
    juce::Rectangle<float> getPlotBounds() const;
    juce::Point<float> toScreen (float frequency, float db, juce::Rectangle<float> plot) const;
    float frequencyForX (float x, juce::Rectangle<float> plot) const;
    float dbForY (float y, juce::Rectangle<float> plot) const;
    int findNodeAt (juce::Point<float> position, juce::Rectangle<float> plot) const;
    void drawGlyph (juce::Graphics& g, juce::Point<float> centre, int glyph, bool active,
                   bool enabled, juce::Colour accent) const;
    void showTypeMenu (int index, const juce::MouseEvent& event);

    std::vector<Node> nodes;
    std::vector<float> spectrum;
    bool bypassed = false;

    int dragIndex = -1;
    int hoverIndex = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ForjaEqGraph)
};

} // namespace forja::ui
