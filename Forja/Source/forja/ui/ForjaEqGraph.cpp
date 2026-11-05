#include "ForjaEqGraph.h"

#include "ForjaSpectrum.h"
#include "ForjaTheme.h"

#include <algorithm>
#include <cmath>

namespace forja::ui
{

namespace
{
constexpr float minFrequency = 20.0f;
constexpr float maxFrequency = 20000.0f;
constexpr float maxDb = 18.0f;
constexpr float logSpan = 3.0f; // log10 (20000 / 20)
constexpr float nodeHitRadius = 13.0f;

struct GridLine
{
    float frequency;
    const char* label;
};

const GridLine verticalLines[] =
{
    { 30.0f, "30" }, { 50.0f, "50" }, { 100.0f, "100" }, { 200.0f, "200" },
    { 300.0f, nullptr }, { 500.0f, "500" }, { 1000.0f, "1k" }, { 2000.0f, "2k" },
    { 3000.0f, nullptr }, { 5000.0f, "5k" }, { 10000.0f, "10k" }, { 20000.0f, "20k" }
};

const float horizontalDb[] = { -12.0f, -6.0f, 0.0f, 6.0f, 12.0f };

juce::String formatFrequency (float hz)
{
    if (hz >= 1000.0f)
        return juce::String (hz / 1000.0f, hz < 10000.0f ? 1 : 0) + "k";

    return juce::String (juce::roundToInt (hz));
}
} // namespace

ForjaEqGraph::ForjaEqGraph()
{
    setInterceptsMouseClicks (true, false);
}

void ForjaEqGraph::setNodes (std::vector<Node> newNodes)
{
    nodes = std::move (newNodes);
    repaint();
}

void ForjaEqGraph::setSpectrum (const float* bandDb, int count)
{
    spectrum.resize (static_cast<std::size_t> (std::max (0, count)));

    for (int i = 0; i < count && i < ForjaSpectrum::bandCount; ++i)
        spectrum[static_cast<std::size_t> (i)] = bandDb[i];

    repaint();
}

void ForjaEqGraph::setBypassed (bool shouldBeBypassed)
{
    if (bypassed == shouldBeBypassed)
        return;

    bypassed = shouldBeBypassed;
    repaint();
}

juce::Rectangle<float> ForjaEqGraph::getPlotBounds() const
{
    return getLocalBounds().toFloat().withTrimmedLeft (26.0f)
                                           .withTrimmedRight (8.0f)
                                           .withTrimmedTop (8.0f)
                                           .withTrimmedBottom (16.0f);
}

juce::Point<float> ForjaEqGraph::toScreen (float frequency, float db,
                                           juce::Rectangle<float> plot) const
{
    const float clamped = std::clamp (frequency, minFrequency, maxFrequency);
    const float normX = std::log10 (clamped / minFrequency) / logSpan;
    const float normY = std::clamp (db / maxDb, -1.0f, 1.0f);

    return { plot.getX() + normX * plot.getWidth(),
             plot.getCentreY() - normY * plot.getHeight() * 0.5f };
}

float ForjaEqGraph::frequencyForX (float x, juce::Rectangle<float> plot) const
{
    const float norm = juce::jlimit (0.0f, 1.0f, (x - plot.getX()) / plot.getWidth());
    return minFrequency * std::pow (1000.0f, norm);
}

float ForjaEqGraph::dbForY (float y, juce::Rectangle<float> plot) const
{
    const float norm = (plot.getCentreY() - y) / (plot.getHeight() * 0.5f);
    return juce::jlimit (-maxDb, maxDb, norm * maxDb);
}

int ForjaEqGraph::findNodeAt (juce::Point<float> position, juce::Rectangle<float> plot) const
{
    for (int i = (int) nodes.size() - 1; i >= 0; --i)
    {
        const auto& node = nodes[(std::size_t) i];
        const auto screen = toScreen (node.frequency, node.displayDb(), plot);

        if (position.getDistanceFrom (screen) <= nodeHitRadius)
            return i;
    }

    return -1;
}

void ForjaEqGraph::drawGlyph (juce::Graphics& g, juce::Point<float> centre, int glyph,
                              bool active, bool enabled, juce::Colour accent) const
{
    const float radius = active ? 8.5f : 6.5f;
    const bool hasAccent = accent.getAlpha() > 0;
    const juce::Colour base = hasAccent ? accent : colour::ember;
    const juce::Colour line = ! enabled ? colour::dim
                                        : (active ? base : base.withAlpha (0.78f));

    // Halo de selecao: no ativo arde na sua cor, os demais ficam apagados.
    g.setColour (line.withAlpha (active ? 0.32f : 0.16f));
    g.fillEllipse (centre.x - radius - 3.0f, centre.y - radius - 3.0f,
                   (radius + 3.0f) * 2.0f, (radius + 3.0f) * 2.0f);

    const float thickness = active ? 2.0f : 1.5f;
    const juce::PathStrokeType stroke (thickness, juce::PathStrokeType::curved,
                                       juce::PathStrokeType::rounded);
    g.setColour (line);
    const float box = radius * 2.0f;

    switch (glyph)
    {
        case notch:
        {
            // Diamante aberto: silhueta de rejeicao pontual.
            juce::Path diamond;
            diamond.startNewSubPath (centre.x, centre.y - radius);
            diamond.lineTo (centre.x + radius, centre.y);
            diamond.lineTo (centre.x, centre.y + radius);
            diamond.lineTo (centre.x - radius, centre.y);
            diamond.closeSubPath();
            g.strokePath (diamond, stroke);
            break;
        }

        case deEsser:
        {
            // Anel com ponto central: marca do split sibilante dedicado.
            g.drawEllipse (centre.x - radius, centre.y - radius, box, box, thickness);
            g.fillEllipse (centre.x - 2.4f, centre.y - 2.4f, 4.8f, 4.8f);
            break;
        }

        case highPass:
        case lowPass:
        {
            g.drawEllipse (centre.x - radius, centre.y - radius, box, box, thickness);
            // Seta diagonal: sobe a direita no HP, desce no LP.
            const float dy = glyph == highPass ? -radius : radius;
            juce::Path arrow;
            arrow.startNewSubPath (centre.x - radius * 0.6f, centre.y - dy * 0.4f);
            arrow.lineTo (centre.x + radius * 0.7f, centre.y + dy * 0.6f);
            g.strokePath (arrow, stroke);
            break;
        }

        case lowShelf:
        case highShelf:
        {
            g.fillEllipse (centre.x - 3.2f, centre.y - 3.2f, 6.4f, 6.4f);
            const float dir = glyph == lowShelf ? -1.0f : 1.0f;
            juce::Path shelf;
            shelf.startNewSubPath (centre.x + dir * radius, centre.y - radius * 0.5f);
            shelf.lineTo (centre.x + dir * radius, centre.y + radius * 0.5f);
            g.strokePath (shelf, stroke);
            break;
        }

        case bell:
        default:
            g.fillEllipse (centre.x - radius * 0.55f, centre.y - radius * 0.55f,
                           radius * 1.1f, radius * 1.1f);
            g.drawEllipse (centre.x - radius, centre.y - radius, box, box, thickness);
            break;
    }
}

void ForjaEqGraph::paint (juce::Graphics& g)
{
    const auto plot = getPlotBounds();

    g.setColour (colour::meterTrack);
    g.fillRoundedRectangle (plot, 4.0f);

    for (const auto& line : verticalLines)
    {
        const auto point = toScreen (line.frequency, 0.0f, plot);
        g.setColour (line.frequency == 1000.0f ? colour::graphAxis : colour::graphGrid);
        g.drawVerticalLine (juce::roundToInt (point.x), plot.getY(), plot.getBottom());

        if (line.label != nullptr)
        {
            g.setFont (monoFont (7.0f));
            g.setColour (colour::dim);
            g.drawText (line.label, juce::roundToInt (point.x) - 12, (int) plot.getBottom() + 2,
                        24, 11, juce::Justification::centred, false);
        }
    }

    for (float db : horizontalDb)
    {
        const auto point = toScreen (1000.0f, db, plot);
        g.setColour (db == 0.0f ? colour::graphAxis : colour::graphGrid);
        g.drawHorizontalLine (juce::roundToInt (point.y), plot.getX(), plot.getRight());

        g.setFont (monoFont (7.0f));
        g.setColour (db == 0.0f ? colour::muted : colour::dim);
        g.drawText (db > 0.0f ? "+" + juce::String ((int) db) : juce::String ((int) db),
                    0, juce::roundToInt (point.y) - 6, 22, 12,
                    juce::Justification::centredRight, false);
    }

    // RTA de entrada atras de tudo: medicao, area discreta em muted para nao
    // competir com a curva em ember.
    if (! spectrum.empty())
    {
        const int bands = static_cast<int> (spectrum.size());
        const float floor = ForjaSpectrum::floorDb;

        juce::Path area;
        const auto bandX = [bands, &plot] (int i)
        {
            const float norm = (static_cast<float> (i) + 0.5f) / static_cast<float> (bands);
            return plot.getX() + norm * plot.getWidth();
        };

        area.startNewSubPath (bandX (0), plot.getBottom());

        for (int i = 0; i < bands; ++i)
        {
            const float norm = std::clamp ((spectrum[static_cast<std::size_t> (i)] - floor)
                                           / (0.0f - floor), 0.0f, 1.0f);
            area.lineTo (bandX (i), plot.getBottom() - norm * plot.getHeight());
        }

        area.lineTo (bandX (bands - 1), plot.getBottom());
        area.closeSubPath();

        g.setGradientFill (juce::ColourGradient (colour::muted.withAlpha (0.22f),
                                                 plot.getCentreX(), plot.getY(),
                                                 colour::muted.withAlpha (0.05f),
                                                 plot.getCentreX(), plot.getBottom(), false));
        g.fillPath (area);
    }

    // Curva composta: o sampler devolve dB em cada frequencia amostrada.
    if (responseSampler != nullptr)
    {
        const int steps = 256;
        juce::Path curve;
        juce::Path fill;
        const float centreY = plot.getCentreY();

        for (int i = 0; i <= steps; ++i)
        {
            const float norm = static_cast<float> (i) / static_cast<float> (steps);
            const float frequency = minFrequency * std::pow (1000.0f, norm);
            const float db = responseSampler (frequency);
            const auto point = toScreen (frequency, db, plot);

            if (i == 0)
            {
                curve.startNewSubPath (point);
                fill.startNewSubPath (point.x, centreY);
            }
            else
            {
                curve.lineTo (point);
            }

            fill.lineTo (point);
        }

        fill.lineTo (plot.getRight(), centreY);
        fill.closeSubPath();

        const auto curveColour = bypassed ? colour::dim : colour::ember;

        g.setGradientFill (juce::ColourGradient (curveColour.withAlpha (0.18f), plot.getCentreX(),
                                                 centreY, curveColour.withAlpha (0.02f),
                                                 plot.getCentreX(), plot.getBottom(), false));
        g.fillPath (fill);

        g.setColour (curveColour);
        g.strokePath (curve, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
    }

    // Nos por cima da curva.
    for (int i = 0; i < (int) nodes.size(); ++i)
    {
        const auto& node = nodes[(std::size_t) i];
        const auto centre = toScreen (node.frequency, node.displayDb(), plot);
        const bool active = node.selected || i == dragIndex || i == hoverIndex;

        drawGlyph (g, centre, node.glyph, active, node.enabled && ! bypassed, node.accent);

        if (node.selected)
        {
            const auto text = node.id.isNotEmpty()
                                ? formatFrequency (node.frequency)
                                      + (node.freqOnly ? juce::String()
                                                       : "  " + juce::String (node.gainDb, 1) + " dB")
                                : juce::String();

            if (text.isNotEmpty())
            {
                g.setFont (monoFont (8.0f));
                g.setColour (node.accent.getAlpha() > 0 ? node.accent.brighter (0.25f)
                                                         : colour::emberPale);
                const int textWidth = juce::roundToInt (
                    juce::GlyphArrangement::getStringWidth (monoFont (8.0f), text));
                const bool placeLeft = centre.x + 11.0f + textWidth > plot.getRight();
                g.drawText (text,
                            placeLeft ? juce::roundToInt (centre.x) - textWidth - 11
                                      : juce::roundToInt (centre.x) + 11,
                            juce::roundToInt (centre.y) - 16, textWidth, 12,
                            juce::Justification::centredLeft, false);
            }
        }
    }
}

void ForjaEqGraph::mouseMove (const juce::MouseEvent& event)
{
    const int index = findNodeAt (event.position, getPlotBounds());

    if (index != hoverIndex)
    {
        hoverIndex = index;
        setMouseCursor (index >= 0 ? juce::MouseCursor::PointingHandCursor
                                   : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void ForjaEqGraph::mouseDown (const juce::MouseEvent& event)
{
    const auto plot = getPlotBounds();
    const int index = findNodeAt (event.position, plot);

    if (event.mods.isPopupMenu())
    {
        if (index >= 0)
            showTypeMenu (index, event);
        return;
    }

    if (index < 0)
    {
        dragIndex = -1;

        if (onBackgroundClicked != nullptr)
            onBackgroundClicked();

        return;
    }

    dragIndex = index;

    for (auto& node : nodes)
        node.selected = false;
    nodes[(std::size_t) index].selected = true;

    if (onNodeSelected != nullptr)
        onNodeSelected (nodes[(std::size_t) index].id);

    repaint();
}

void ForjaEqGraph::mouseDrag (const juce::MouseEvent& event)
{
    if (dragIndex < 0 || dragIndex >= (int) nodes.size())
        return;

    const auto plot = getPlotBounds();
    auto& node = nodes[(std::size_t) dragIndex];
    const float frequency = frequencyForX (event.position.x, plot);
    const float gainDb = node.freqOnly ? 0.0f : dbForY (event.position.y, plot);
    const float widthNorm = juce::jlimit (0.0f, 1.0f,
                                          (plot.getBottom() - event.position.y) / plot.getHeight());
    const bool widthDrag = event.mods.isShiftDown() && node.allowWidth;

    node.frequency = frequency;
    node.gainDb = gainDb;

    if (onNodeDrag != nullptr)
        onNodeDrag (node.id, frequency, gainDb, widthNorm, widthDrag);

    repaint();
}

void ForjaEqGraph::mouseUp (const juce::MouseEvent&)
{
    dragIndex = -1;
}

void ForjaEqGraph::mouseDoubleClick (const juce::MouseEvent& event)
{
    const int index = findNodeAt (event.position, getPlotBounds());

    if (index < 0)
        return;

    if (onNodeReset != nullptr)
        onNodeReset (nodes[(std::size_t) index].id);
}

void ForjaEqGraph::showTypeMenu (int index, const juce::MouseEvent& event)
{
    juce::ignoreUnused (event);

    if (index < 0 || index >= (int) nodes.size())
        return;

    const auto& node = nodes[(std::size_t) index];

    if (node.freqOnly || onNodeGlyph == nullptr)
        return;

    juce::PopupMenu menu;
    menu.addItem (bell + 1, "Campainha", true, node.glyph == bell);
    menu.addItem (lowShelf + 1, "Prateleira baixa", true, node.glyph == lowShelf);
    menu.addItem (highShelf + 1, "Prateleira alta", true, node.glyph == highShelf);
    menu.addItem (highPass + 1, "Passa-alta", true, node.glyph == highPass);
    menu.addItem (lowPass + 1, "Passa-baixa", true, node.glyph == lowPass);
    menu.addItem (notch + 1, "Notch", true, node.glyph == notch);

    // showMenuAsync em vez do síncrono: a forja_ui nao liga JUCE_MODAL_LOOPS_PERMITTED
    // (nem deve, para nao aninhar loops modais em todo plugin que a usa). O callback
    // captura o id por valor, entao survive mesmo se o no for reposicionado.
    menu.showMenuAsync (juce::PopupMenu::Options()
                            .withTargetComponent (this)
                            .withMousePosition(),
                        [this, id = node.id] (int result)
                        {
                            if (result != 0 && onNodeGlyph != nullptr)
                                onNodeGlyph (id, result - 1);
                        });
}

} // namespace forja::ui
