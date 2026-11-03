#pragma once

#include <algorithm>
#include <cmath>

#include <juce_gui_basics/juce_gui_basics.h>

#include "forja/ui/ForjaTheme.h"

namespace forja::pianoforge
{

// Vista FRONTAL estilizada de um piano de cauda, do ponto de vista de quem toca
// (nao mais de lado). Desenhada em JUCE nativo (sem WebView, sem assets -- regra
// da suite). Da base para o topo, como o pianista ve:
//
//   - o teclado (teclas brancas + pretas) na frente, ocupando a largura;
//   - o atril/espel com a PARTITURA no meio, onde aparece o NOME do soundfont
//     carregado (o banco em uso, vindo do processor);
//   - atras do atril, a Tampa levantando sobre dobrica: o angulo segue o
//     parametro TAMPA (Param_Lid, 0..1) do APVTS;
//   - pela fresta da tampa escapa a brasa do Corpo (Param_Body), e por tras do
//     atril insinua-se o brilho das cordas com a Madeira (Param_Ring).
//
// Nao e interativo -- e um espelho do estado na vaga que sobrava na tela PIANO.
// O editor o atualiza no tick de 30 Hz e passa o nome do banco quando muda; o
// componente so repinta quando algo sai do lugar. Todo o desenho trabalha em
// coordenadas fracionarias da propria caixa, entao escala com o ForjaCanvas sem
// depender de getWidth() absoluto.
class PianoLidArt : public juce::Component
{
public:
    PianoLidArt()
    {
        setInterceptsMouseClicks (false, false);
        setRepaintsOnMouseActivity (false);
    }

    // Recebe os tres valores normalizados (0..1) do APVTS. Guarda e so pede
    // repaint quando algum saiu do lugar, para nao pintar 30x/s a toa.
    void update (float lid01, float body01, float ring01) noexcept
    {
        const auto cl = std::clamp (lid01, 0.0f, 1.0f);
        const auto cb = std::clamp (body01, 0.0f, 1.0f);
        const auto cr = std::clamp (ring01, 0.0f, 1.0f);

        if (cl != lid || cb != body || cr != ring)
        {
            lid = cl;
            body = cb;
            ring = cr;
            repaint();
        }
    }

    // Nome do banco/soundfont em uso, escrito na partitura do atril. Vazio
    // (nenhum banco) deixa a partitura em branco, como folha sem titulo.
    void setName (const juce::String& bankName)
    {
        if (bankName != name)
        {
            name = bankName;
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        paintFrame (g, bounds);

        auto inner = bounds.reduced (10.0f, 8.0f);
        inner.removeFromTop (4.0f);                        // folga do topo (sem caption)
        inner.removeFromBottom (8.0f);                      // rodape

        const float W = inner.getWidth();
        const float H = inner.getHeight();
        if (W < 60.0f || H < 40.0f)
            return;

        const float x0 = inner.getX();
        const float bottom = inner.getBottom();
        const float top = inner.getY();

        // Faixas verticais (de baixo para cima): teclado, atril+partitura, tampa.
        const float keyH = H * 0.24f;
        const float keyTop = bottom - keyH;
        const float deskTop = keyTop - H * 0.05f;          // borda do atril
        const float musicTop = deskTop - H * 0.30f;        // altura da partitura

        drawLid (g, x0, W, top, keyTop);
        drawMusicStand (g, x0, W, musicTop, deskTop);
        drawKeyboard (g, x0, W, keyTop, bottom);
    }

private:
    void paintFrame (juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        juce::ColourGradient background (forja::ui::colour::iron3, bounds.getTopLeft(),
                                         forja::ui::colour::iron2, bounds.getBottomRight(), false);
        background.addColour (0.8, forja::ui::colour::iron2);
        g.setGradientFill (background);
        g.fillRoundedRectangle (bounds, 5.0f);

        g.setColour (juce::Colour (0xff333b46));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 5.0f, 1.0f);

        // Filete de brasa a esquerda, como os cards da suite.
        g.setColour (forja::ui::colour::ember.withAlpha (0.55f));
        g.fillRect (0.0f, 0.0f, 2.0f, bounds.getBottom());

        // Sem caption: o desenho do piano e o unico conteudo (o nome do banco ja
        // aparece na partitura do atril).
    }

    // Tampa vista de frente: uma REGUA RIGIDA de comprimento constante, dobrada
    // numa ponta fixa (direita) que VARRE para cima ao girar -- como uma asa de
    // gaivota / porta de carro, nao um painel que cresce. O ANGO (nao o tamanho)
    // segue o parametro lid. Fechada (lid=0) fica quase deitada atras do atril;
    // aberta, levanta, revela o interior e pela fresta escapa a brasa (body); as
    // cordas (ring) acendem por dentro.
    void drawLid (juce::Graphics& g, float x0, float W, float top, float keyTop)
    {
        const float backY = keyTop - (keyTop - top) * 0.12f;    // linha da dobra (atras)
        const float span = W * 0.86f;                           // alcance horizontal alvo

        // Dobrica fixa no canto direito; a regua gira em torno dela.
        const float hingeX = x0 + W * 0.94f;
        const float hingeY = backY;

        // Room acima da dobra: a regua nao pode passar dele quando erguida, senao
        // sai do topo (com caixa larga e baixa o angulo jogaria a ponta para fora).
        const float vertRoom = juce::jmax (24.0f, backY - top);
        const float lidLen = juce::jmin (span, vertRoom / 0.92f);

        // Angulo de abertura: deitada (7 graus) quase fechada ate erguida (64).
        const float ang = juce::degreesToRadians (7.0f + lid * 57.0f);
        const float tipX = hingeX - lidLen * std::cos (ang);
        const float tipY = hingeY - lidLen * std::sin (ang);

        const float thick = juce::jmax (3.0f, (keyTop - top) * 0.08f);

        // Interior revelado: triangulo entre a regua e a linha da dobra. Quanto
        // mais aberta, mais a brasa (body) acende por dentro.
        if (lid > 0.03f)
        {
            const float alpha = lid * (0.25f + 0.55f * body);
            juce::Path interior;
            interior.startNewSubPath (hingeX, hingeY);
            interior.lineTo (tipX, tipY);
            interior.lineTo (tipX, hingeY);
            interior.closeSubPath();

            juce::ColourGradient heat (forja::ui::colour::ember.withAlpha (alpha),
                                       tipX, tipY, forja::ui::colour::ember.withAlpha (0.0f),
                                       hingeX, hingeY, false);
            heat.addColour (0.6, forja::ui::colour::emberSoft.withAlpha (alpha * 0.45f));
            g.setGradientFill (heat);
            g.fillPath (interior);
        }

        // Regua rigida da tampa: paralelogramo de espessura constante ao longo do
        // angulo (por isso GIRA em vez de crescer). Perpendicular ao eixo da regua.
        const float nx = std::sin (ang);
        const float ny = -std::cos (ang);
        juce::Path plank;
        plank.startNewSubPath (hingeX, hingeY);
        plank.lineTo (tipX, tipY);
        plank.lineTo (tipX + nx * thick, tipY + ny * thick);
        plank.lineTo (hingeX + nx * thick, hingeY + ny * thick);
        plank.closeSubPath();

        juce::ColourGradient lacquer (juce::Colour (0xff39424e), tipX, tipY,
                                      forja::ui::colour::iron1, hingeX, hingeY, false);
        g.setGradientFill (lacquer);
        g.fillPath (plank);

        g.setColour (forja::ui::colour::lineStrong);
        g.strokePath (plank, juce::PathStrokeType (1.0f));

        // Suporte (tambor/lid prop) curto segurando a regua quando aberta.
        if (lid > 0.20f)
        {
            const float px = tipX + (hingeX - tipX) * 0.28f;
            const float pyTop = tipY + (hingeY - tipY) * 0.28f + thick;
            g.setColour (forja::ui::colour::lineStrong.withAlpha (0.8f));
            g.drawLine (px, pyTop, px, hingeY, 1.2f);
        }

        // Cordas: verticais curtas pendendo da regua ate a linha da dobra, dentro
        // do vao aberto; acendem com a Madeira (ring) e ganham altura com a tampa.
        if (ring > 0.03f && lid > 0.12f)
        {
            g.setColour (forja::ui::colour::emberPale.withAlpha (0.12f + 0.5f * ring));
            const int n = 9;
            for (int k = 0; k < n; ++k)
            {
                const float fx = tipX + (hingeX - tipX) * (static_cast<float> (k) + 0.5f)
                                 / static_cast<float> (n);
                const float frac = (hingeX - fx) / (hingeX - tipX);   // 0 no pivo, 1 na ponta
                const float yTop = hingeY - (hingeY - tipY) * frac + thick;
                g.drawLine (fx, yTop, fx, hingeY, 0.6f);
            }
        }
    }

    // Atril + partitura no centro, onde mora o nome do soundfont carregado.
    void drawMusicStand (juce::Graphics& g, float x0, float W, float musicTop, float deskTop)
    {
        const float sheetW = W * 0.62f;
        const float sheetH = deskTop - musicTop;
        const float sx = x0 + (W - sheetW) * 0.5f;

        // Espelho da partitura (uma folha clara levemente tombada).
        juce::Path sheet;
        sheet.startNewSubPath (sx, deskTop);
        sheet.lineTo (sx + sheetW * 0.04f, musicTop);
        sheet.lineTo (sx + sheetW * 0.96f, musicTop);
        sheet.lineTo (sx + sheetW, deskTop);
        sheet.closeSubPath();

        g.setColour (juce::Colour (0xffd7dce2).withAlpha (0.92f));
        g.fillPath (sheet);
        g.setColour (forja::ui::colour::line);
        g.strokePath (sheet, juce::PathStrokeType (0.8f));

        // Pautas decorativas nas bordas da folha.
        g.setColour (forja::ui::colour::dim.withAlpha (0.35f));
        for (int line = 0; line < 3; ++line)
        {
            const float fy = musicTop + sheetH * (0.22f + 0.28f * static_cast<float> (line));
            g.drawHorizontalLine (static_cast<int> (fy), sx + sheetW * 0.08f,
                                  sx + sheetW * 0.92f);
        }

        // Nome do banco ao centro da partitura, elidido para caber.
        if (name.isNotEmpty())
        {
            const auto area = juce::Rectangle<float> (sx, musicTop, sheetW, sheetH);
            g.setColour (juce::Colour (0xff232a33));
            g.setFont (forja::ui::tracked (forja::ui::uiFont (11.0f, juce::Font::bold), 0.05f));
            g.drawText (name, area.toNearestInt(), juce::Justification::centred, true);
        }
    }

    // Teclado visto de frente: teclas brancas + o padrao de 5 pretas por oitava.
    void drawKeyboard (juce::Graphics& g, float x0, float W, float keyTop, float bottom)
    {
        const int octaves = 4;
        const int whiteKeys = octaves * 7;
        const float kbW = W * 0.9f;
        const float kx0 = x0 + (W - kbW) * 0.5f;
        const float kw = kbW / static_cast<float> (whiteKeys);

        g.setColour (juce::Colour (0xffe7ebf0));
        g.fillRect (kx0, keyTop, kbW, bottom - keyTop);

        g.setColour (forja::ui::colour::line);
        for (int k = 0; k <= whiteKeys; ++k)
            g.drawVerticalLine (static_cast<int> (kx0 + static_cast<float> (k) * kw),
                                 keyTop, bottom);

        // Pretas: depois das brancas 0,1,3,4,5 de cada oitava (padrao C D _ F G A).
        const float blackW = kw * 0.56f;
        const float blackH = (bottom - keyTop) * 0.62f;
        g.setColour (juce::Colour (0xff12161c));
        for (int o = 0; o < octaves; ++o)
        {
            for (const int pos : { 0, 1, 3, 4, 5 })
            {
                const int whiteIndex = o * 7 + pos + 1;
                const float bx = kx0 + static_cast<float> (whiteIndex) * kw - blackW * 0.5f;
                g.fillRoundedRectangle (bx, keyTop, blackW, blackH, 1.2f);
            }
        }
    }

    float lid = 0.0f;
    float body = 0.0f;
    float ring = 0.0f;
    juce::String name;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PianoLidArt)
};

} // namespace forja::pianoforge
