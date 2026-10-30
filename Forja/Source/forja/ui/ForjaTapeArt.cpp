#include "ForjaTapeArt.h"

#include "ForjaTheme.h"

#include <cmath>

namespace forja::ui
{

namespace
{
juce::Colour artColour (float alpha)
{
    return colour::emberPale.withAlpha (alpha);
}

void drawReel (juce::Graphics& g, juce::Point<float> centre, float radius)
{
    g.setColour (artColour (0.40f));
    g.drawEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f, 1.3f);

    g.setColour (artColour (0.16f));
    g.fillEllipse (centre.x - radius * 0.30f, centre.y - radius * 0.30f,
                   radius * 0.60f, radius * 0.60f);

    for (int spoke = 0; spoke < 3; ++spoke)
    {
        const float angle = juce::MathConstants<float>::twoPi * (float) spoke / 3.0f
                          + juce::MathConstants<float>::pi / 6.0f;
        g.setColour (artColour (0.26f));
        g.drawLine (centre.x + std::cos (angle) * radius * 0.30f,
                    centre.y + std::sin (angle) * radius * 0.30f,
                    centre.x + std::cos (angle) * radius * 0.92f,
                    centre.y + std::sin (angle) * radius * 0.92f, 1.0f);
    }

    g.setColour (artColour (0.45f));
    g.drawEllipse (centre.x - 2.0f, centre.y - 2.0f, 4.0f, 4.0f, 1.0f);
}
} // namespace

ForjaTapeArt::ForjaTapeArt (Style styleToUse)
    : style (styleToUse)
{
    setInterceptsMouseClicks (false, false);
}

void ForjaTapeArt::paint (juce::Graphics& g)
{
    const auto area = getLocalBounds().toFloat().reduced (2.0f);

    if (area.getWidth() < 24.0f || area.getHeight() < 24.0f)
        return;

    if (style == Style::reels)
        paintReels (g, area);
    else
        paintCassette (g, area);
}

void ForjaTapeArt::paintReels (juce::Graphics& g, juce::Rectangle<float> area)
{
    const float radius = juce::jmin (area.getHeight() * 0.34f, area.getWidth() * 0.19f);
    const float centreY = area.getY() + radius + 4.0f;
    const juce::Point<float> left (area.getX() + area.getWidth() * 0.26f, centreY);
    const juce::Point<float> right (area.getX() + area.getWidth() * 0.74f, centreY);

    drawReel (g, left, radius);
    drawReel (g, right, radius);

    const float guideY = area.getBottom() - 10.0f;
    const float headY = area.getBottom() - 4.0f;
    const float centreX = area.getCentreX();

    juce::Path tape;
    tape.startNewSubPath (left.x, left.y + radius);
    tape.lineTo (left.x, guideY);
    tape.lineTo (centreX - 18.0f, guideY);
    tape.lineTo (centreX - 9.0f, headY);
    tape.lineTo (centreX + 9.0f, headY);
    tape.lineTo (centreX + 18.0f, guideY);
    tape.lineTo (right.x, guideY);
    tape.lineTo (right.x, right.y + radius);

    g.setColour (artColour (0.30f));
    g.strokePath (tape, juce::PathStrokeType (1.2f, juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));

    g.setColour (colour::ember.withAlpha (0.20f));
    g.fillRoundedRectangle (centreX - 13.0f, headY - 5.0f, 26.0f, 12.0f, 2.0f);
    g.setColour (artColour (0.38f));
    g.drawRoundedRectangle (centreX - 13.0f, headY - 5.0f, 26.0f, 12.0f, 2.0f, 1.0f);
}

void ForjaTapeArt::paintCassette (juce::Graphics& g, juce::Rectangle<float> area)
{
    const float shellWidth = juce::jmin (area.getWidth() * 0.78f, area.getHeight() * 1.62f);
    const float shellHeight = shellWidth / 1.62f;
    const juce::Rectangle<float> shell (area.getCentreX() - shellWidth * 0.5f,
                                        area.getCentreY() - shellHeight * 0.5f,
                                        shellWidth, shellHeight);

    g.setColour (colour::iron2.withAlpha (0.55f));
    g.fillRoundedRectangle (shell, 4.0f);
    g.setColour (artColour (0.30f));
    g.drawRoundedRectangle (shell, 4.0f, 1.2f);

    const auto label = shell.reduced (shellWidth * 0.06f, shellHeight * 0.08f)
                            .withHeight (shellHeight * 0.30f);
    g.setColour (artColour (0.10f));
    g.fillRoundedRectangle (label, 2.0f);
    g.setColour (artColour (0.22f));
    g.drawRoundedRectangle (label, 2.0f, 1.0f);

    g.setFont (monoFont (7.0f));
    g.setColour (artColour (0.40f));
    g.drawText ("FORJA C-60", label.withTrimmedLeft (4.0f), juce::Justification::centredLeft, false);

    const float hubY = shell.getY() + shellHeight * 0.60f;
    const float hubRadius = shellHeight * 0.15f;
    const juce::Point<float> leftHub (shell.getX() + shellWidth * 0.30f, hubY);
    const juce::Point<float> rightHub (shell.getX() + shellWidth * 0.70f, hubY);

    for (const auto& hub : { leftHub, rightHub })
    {
        g.setColour (artColour (0.26f));
        g.drawEllipse (hub.x - hubRadius, hub.y - hubRadius, hubRadius * 2.0f, hubRadius * 2.0f,
                       1.1f);
        g.setColour (artColour (0.12f));
        g.fillEllipse (hub.x - hubRadius * 0.45f, hub.y - hubRadius * 0.45f,
                       hubRadius * 0.9f, hubRadius * 0.9f);
    }

    g.setColour (artColour (0.22f));
    g.drawLine (leftHub.x + hubRadius * 0.9f, hubY, rightHub.x - hubRadius * 0.9f, hubY, 1.4f);

    const float mouthY = shell.getBottom() - shellHeight * 0.18f;
    juce::Path mouth;
    mouth.startNewSubPath (shell.getX() + shellWidth * 0.26f, mouthY);
    mouth.lineTo (shell.getX() + shellWidth * 0.40f, shell.getBottom() - 3.0f);
    mouth.lineTo (shell.getX() + shellWidth * 0.60f, shell.getBottom() - 3.0f);
    mouth.lineTo (shell.getX() + shellWidth * 0.74f, mouthY);
    mouth.closeSubPath();

    g.setColour (artColour (0.14f));
    g.fillPath (mouth);
    g.setColour (artColour (0.28f));
    g.strokePath (mouth, juce::PathStrokeType (1.0f));
}

} // namespace forja::ui
