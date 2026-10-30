#include "ForjaGRHistory.h"

#include "ForjaTheme.h"

namespace forja::ui
{

ForjaGRHistory::ForjaGRHistory()
{
    setInterceptsMouseClicks (false, false);
}

void ForjaGRHistory::pushGainReductionDb (float leftDb, float rightDb)
{
    for (int i = 0; i < capacity - 1; ++i)
    {
        left[(size_t) i] = left[(size_t) i + 1];
        right[(size_t) i] = right[(size_t) i + 1];
    }

    left[(size_t) capacity - 1] = juce::jlimit (-24.0f, 0.0f, leftDb);
    right[(size_t) capacity - 1] = juce::jlimit (-24.0f, 0.0f, rightDb);
    repaint();
}

void ForjaGRHistory::setBypassed (bool shouldBeBypassed)
{
    if (bypassed == shouldBeBypassed)
        return;

    bypassed = shouldBeBypassed;
    repaint();
}

void ForjaGRHistory::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    const float top = 18.0f;
    const float bottom = bounds.getBottom() - 16.0f;
    const float graphHeight = juce::jmax (1.0f, bottom - top);

    juce::ColourGradient background (colour::iron0, bounds.getTopLeft(),
                                     colour::iron2, bounds.getBottomRight(), false);
    g.setGradientFill (background);
    g.fillRoundedRectangle (bounds, 4.0f);

    g.setColour (colour::line.withAlpha (0.8f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 4.0f, 1.0f);

    g.setFont (tracked (uiFont (7.0f, juce::Font::bold), 0.13f));
    g.setColour (bypassed ? colour::dim : colour::muted);
    g.drawText ("GR", bounds.getX() + 6.0f, 4.0f, 40.0f, 10.0f,
                juce::Justification::centredLeft, false);

    g.setFont (monoFont (7.0f));
    g.setColour (juce::Colour (0xff59636f));
    g.drawText ("6 s", bounds.getRight() - 40.0f, 4.0f, 34.0f, 10.0f,
                juce::Justification::centredRight, false);

    g.setFont (monoFont (6.5f));
    const float labelWidth = 22.0f;
    for (int line = 0; line < 3; ++line)
    {
        const float norm = (float) line / 2.0f;
        const float y = top + graphHeight * norm;
        g.setColour (line == 0 ? colour::graphAxis : colour::graphGrid);
        g.drawHorizontalLine ((int) y, bounds.getX() + labelWidth + 4.0f, bounds.getRight() - 6.0f);
        g.setColour (juce::Colour (0xff59636f));
        g.drawText (line == 0 ? "0" : (line == 1 ? "-12" : "-24"),
                    bounds.getX() + 4.0f, (int) y - 4.0f, labelWidth, 8.0f,
                    juce::Justification::centredRight, false);
    }

    const float graphLeft = bounds.getX() + labelWidth + 4.0f;
    const float graphRight = bounds.getRight() - 6.0f;
    const float graphWidth = juce::jmax (1.0f, graphRight - graphLeft);

    auto drawLine = [&] (const std::array<float, capacity>& data, juce::Colour colour)
    {
        juce::Path path;
        bool started = false;

        for (int i = 0; i < capacity; ++i)
        {
            const float x = graphLeft + graphWidth * ((float) i / (float) (capacity - 1));
            const float y = top + graphHeight * (-data[(size_t) i] / 24.0f);

            if (! started)
            {
                path.startNewSubPath (x, y);
                started = true;
            }
            else
            {
                path.lineTo (x, y);
            }
        }

        g.setColour (colour);
        g.strokePath (path, juce::PathStrokeType (1.25f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
    };

    if (bypassed)
    {
        g.setColour (colour::dim.withAlpha (0.35f));
        g.drawHorizontalLine ((int) top, graphLeft, graphRight);
        g.setFont (tracked (uiFont (7.0f, juce::Font::bold), 0.18f));
        g.drawText ("BYPASS", bounds, juce::Justification::centred, false);
        return;
    }

    drawLine (right, colour::emberSoft.withAlpha (0.55f));
    drawLine (left, colour::ember);
}

} // namespace forja::ui
