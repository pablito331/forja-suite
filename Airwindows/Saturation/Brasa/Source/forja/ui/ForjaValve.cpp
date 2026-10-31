#include "ForjaValve.h"

#include "forja/ui/ForjaTheme.h"

namespace forja::ui
{

void ForjaValve::setHeat (float newHeat)
{
    newHeat = juce::jlimit (0.0f, 1.0f, newHeat);
    if (std::abs (heat - newHeat) < 0.004f)
        return;

    heat = newHeat;
    repaint();
}

void ForjaValve::paint (juce::Graphics& g)
{
    const float scale = juce::jmin (getWidth() / 180.0f, getHeight() / 132.0f);
    if (scale <= 0.0f)
        return;

    const float offsetX = (getWidth() - 180.0f * scale) * 0.5f;
    const float offsetY = (getHeight() - 132.0f * scale) * 0.5f;

    g.saveState();
    g.addTransform (juce::AffineTransform::scale (scale).translated (offsetX, offsetY));

    const float haloAlpha = (0.12f + heat * 0.88f) * 0.7f;
    juce::ColourGradient halo (colour::ember.withAlpha (haloAlpha), 90.0f, 66.0f,
                               colour::ember.withAlpha (0.0f), 146.0f, 66.0f, true);
    g.setGradientFill (halo);
    g.fillEllipse (34.0f, 18.0f, 112.0f, 96.0f);

    juce::Path glass;
    glass.startNewSubPath (56.0f, 27.0f);
    glass.cubicTo (56.0f, 12.0f, 69.0f, 5.0f, 90.0f, 5.0f);
    glass.cubicTo (111.0f, 5.0f, 124.0f, 12.0f, 124.0f, 27.0f);
    glass.lineTo (124.0f, 78.0f);
    glass.cubicTo (124.0f, 92.0f, 111.0f, 99.0f, 90.0f, 99.0f);
    glass.cubicTo (69.0f, 99.0f, 56.0f, 92.0f, 56.0f, 78.0f);
    glass.closeSubPath();

    juce::ColourGradient glassFill (juce::Colour (0xff3a424d), 56.0f, 5.0f,
                                    juce::Colour (0xff0f1216), 124.0f, 99.0f, false);
    glassFill.addColour (0.45, juce::Colour (0xff1d2229));
    g.setGradientFill (glassFill);
    g.fillPath (glass);

    g.setColour (juce::Colour (0xff4a5360));
    g.strokePath (glass, juce::PathStrokeType (1.2f));

    juce::Path highlight;
    highlight.startNewSubPath (64.0f, 26.0f);
    highlight.cubicTo (64.0f, 15.0f, 73.0f, 11.0f, 86.0f, 10.0f);
    g.setColour (juce::Colour (0x20ffffff));
    g.strokePath (highlight, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));

    g.setColour (juce::Colour (0xff262c34));
    g.fillRoundedRectangle (61.0f, 95.0f, 58.0f, 17.0f, 3.0f);
    g.setColour (juce::Colour (0xff3b4451));
    g.drawRoundedRectangle (61.5f, 95.5f, 57.0f, 16.0f, 3.0f, 1.0f);

    g.setColour (juce::Colour (0xff68727f));
    g.drawLine (70.0f, 112.0f, 70.0f, 123.0f, 2.0f);
    g.drawLine (82.0f, 112.0f, 82.0f, 125.0f, 2.0f);
    g.drawLine (98.0f, 112.0f, 98.0f, 125.0f, 2.0f);
    g.drawLine (110.0f, 112.0f, 110.0f, 123.0f, 2.0f);

    g.setColour (juce::Colour (0xff171b21));
    g.fillRoundedRectangle (74.0f, 33.0f, 32.0f, 51.0f, 4.0f);
    g.setColour (juce::Colour (0xff414a56));
    g.drawRoundedRectangle (74.5f, 33.5f, 31.0f, 50.0f, 4.0f, 1.0f);

    g.setColour (juce::Colour (0xff303743));
    for (float y = 42.0f; y <= 66.0f; y += 8.0f)
        g.drawLine (78.0f, y, 102.0f, y, 1.0f);

    juce::Path filament;
    filament.startNewSubPath (84.0f, 43.0f);
    filament.quadraticTo (97.0f, 51.0f, 84.0f, 59.0f);
    filament.quadraticTo (71.0f, 67.0f, 84.0f, 75.0f);

    const float filamentAlpha = 0.28f + heat * 0.72f;
    g.setColour (colour::ember.withAlpha (filamentAlpha * 0.25f));
    g.strokePath (filament, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
    g.setColour (colour::ember.withAlpha (filamentAlpha));
    g.strokePath (filament, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));

    g.restoreState();
}

} // namespace forja::ui
