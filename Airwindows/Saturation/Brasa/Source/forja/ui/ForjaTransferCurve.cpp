#include "ForjaTransferCurve.h"

#include "forja/ui/ForjaTheme.h"
#include "forja/dsp/ForjaBrasa.h"

namespace forja::ui
{

void ForjaTransferCurve::setState (float inputPad, float tubeDrive, bool isBypassed)
{
    if (std::abs (input - inputPad) < 1.0e-4f
        && std::abs (tube - tubeDrive) < 1.0e-4f
        && bypassed == isBypassed)
        return;

    input = inputPad;
    tube = tubeDrive;
    bypassed = isBypassed;
    repaint();
}

void ForjaTransferCurve::paint (juce::Graphics& g)
{
    const auto legendHeight = 18;
    const auto frame = getLocalBounds().withHeight (juce::jmax (0, getHeight() - legendHeight)).toFloat();

    g.setColour (colour::iron0);
    g.fillRoundedRectangle (frame, 3.0f);
    g.setColour (colour::line);
    g.drawRoundedRectangle (frame.reduced (0.5f), 3.0f, 1.0f);

    const float left = frame.getX() + 28.0f;
    const float right = frame.getRight() - 16.0f;
    const float top = frame.getY() + 20.0f;
    const float bottom = frame.getBottom() - 22.0f;

    auto mapX = [left, right] (float value) { return left + (value + 1.0f) * 0.5f * (right - left); };
    auto mapY = [top, bottom] (float value) { return bottom - (value + 1.0f) * 0.5f * (bottom - top); };

    for (int i = 0; i < 5; ++i)
    {
        const float value = -1.0f + 0.5f * (float) i;
        const bool axis = (i == 2);
        g.setColour (axis ? colour::graphAxis : colour::graphGrid);
        g.fillRect (juce::Rectangle<float> (mapX (value) - 0.5f, top, 1.0f, bottom - top));
        g.fillRect (juce::Rectangle<float> (left, mapY (value) - 0.5f, right - left, 1.0f));
    }

    g.setColour (colour::emberSoft.withAlpha (0.45f));
    const float dashes[] = { 3.0f, 4.0f };
    g.drawDashedLine ({ mapX (-1.0f), mapY (-1.0f), mapX (1.0f), mapY (1.0f) },
                      dashes, 2, 1.0f);

    juce::Path curve;
    constexpr int steps = 220;
    for (int i = 0; i <= steps; ++i)
    {
        const float x = -1.0f + 2.0f * (float) i / (float) steps;
        const float y = forja::BrasaDSP::shapeStatic (x, input, tube);
        const float px = mapX (x);
        const float py = mapY (y);
        if (i == 0)
            curve.startNewSubPath (px, py);
        else
            curve.lineTo (px, py);
    }

    if (bypassed)
    {
        g.setColour (colour::ember.withAlpha (0.22f));
        g.strokePath (curve, juce::PathStrokeType (2.0f));
    }
    else
    {
        g.setColour (colour::ember.withAlpha (0.16f));
        g.strokePath (curve, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));

        g.setColour (colour::ember);
        g.strokePath (curve, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
    }

    g.setFont (uiFont (8.0f));
    g.setColour (colour::dim);
    g.drawText (utf8 ("−1"),
                (int) mapX (-1.0f) - 12, (int) bottom + 2, 24, 12, juce::Justification::centred, false);
    g.drawText ("0", (int) mapX (0.0f) - 12, (int) bottom + 2, 24, 12, juce::Justification::centred, false);
    g.drawText ("+1", (int) mapX (1.0f) - 12, (int) bottom + 2, 24, 12, juce::Justification::centred, false);

    g.setFont (tracked (uiFont (7.0f, juce::Font::bold), 0.12f));
    g.setColour (colour::dim.withAlpha (0.85f));
    g.drawText ("ENTRADA", (int) right - 84, (int) bottom + 2, 60, 12,
                juce::Justification::centredRight, false);
    g.drawText (utf8 ("SAÍDA"), (int) left, (int) top - 12, 80, 12,
                juce::Justification::centredLeft, false);

    g.setColour (juce::Colour (0xff70523c));
    g.drawText (bypassed ? juce::String ("BYPASS") : utf8 ("ESTÁTICA"), (int) right - 90, (int) top + 5, 90, 10,
                juce::Justification::centredRight, false);

    // legenda
    const int legendY = getHeight() - legendHeight + 3;
    int x = 0;

    g.setColour (colour::ember);
    g.fillRect (x, legendY + 4, 12, 2);
    x += 16;
    g.setFont (tracked (uiFont (7.0f, juce::Font::bold), 0.1f));
    g.setColour (colour::dim);
    g.drawText ("CURVA", x, legendY, 44, 10, juce::Justification::centredLeft, false);
    x += 50;

    g.setColour (colour::emberSoft.withAlpha (0.55f));
    g.fillRect (x, legendY + 4, 12, 2);
    x += 16;
    g.drawText (utf8 ("REFERÊNCIA"), x, legendY, 84, 10, juce::Justification::centredLeft, false);

    g.setColour (juce::Colour (0xff5b6470));
    g.drawText (utf8 ("ESTÁTICA · SEM HISTERESE"), 0, legendY, getWidth(), 10,
                juce::Justification::centredRight, false);
}

} // namespace forja::ui
