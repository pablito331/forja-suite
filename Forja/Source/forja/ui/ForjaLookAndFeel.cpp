#include "ForjaLookAndFeel.h"

#include "ForjaTheme.h"

namespace forja::ui
{

ForjaLookAndFeel::ForjaLookAndFeel()
{
    setDefaultSansSerifTypefaceName ("Segoe UI");
}

void ForjaLookAndFeel::drawRotarySlider (juce::Graphics& g,
                                         int x, int y, int width, int height,
                                         float sliderPos,
                                         float, float,
                                         juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
    const float size = juce::jmin (bounds.getWidth(), bounds.getHeight());
    const float cx = bounds.getCentreX();
    const float cy = bounds.getCentreY();

    const float trackRadius = size * 34.0f / 80.0f;
    const float faceRadius = size * 26.0f / 80.0f;
    const float strokeWidth = juce::jmax (2.0f, size * 3.7f / 80.0f);

    const float start = juce::MathConstants<float>::pi * 1.25f;
    const float end = juce::MathConstants<float>::pi * 2.75f;
    const float angle = start + juce::jlimit (0.0f, 1.0f, sliderPos) * (end - start);

    juce::Path track;
    track.addCentredArc (cx, cy, trackRadius, trackRadius, 0.0f, start, end, true);
    g.setColour (juce::Colour (0xff3b4552));
    g.strokePath (track, juce::PathStrokeType (strokeWidth, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    if (sliderPos > 0.001f)
    {
        juce::Path active;
        active.addCentredArc (cx, cy, trackRadius, trackRadius, 0.0f, start, angle, true);

        g.setColour (colour::ember.withAlpha (0.12f));
        g.strokePath (active, juce::PathStrokeType (strokeWidth * 3.2f, juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));

        g.setColour (colour::ember);
        g.strokePath (active, juce::PathStrokeType (strokeWidth, juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));
    }

    juce::ColourGradient face (juce::Colour (0xff343b45), cx - faceRadius, cy - faceRadius,
                               juce::Colour (0xff171b21), cx + faceRadius, cy + faceRadius, false);
    face.addColour (0.58, juce::Colour (0xff242a33));

    g.setGradientFill (face);
    g.fillEllipse (cx - faceRadius, cy - faceRadius, faceRadius * 2.0f, faceRadius * 2.0f);

    g.setColour (juce::Colour (0xff414a56));
    g.drawEllipse (cx - faceRadius, cy - faceRadius, faceRadius * 2.0f, faceRadius * 2.0f,
                   juce::jmax (1.0f, size / 80.0f));

    const float shineRadius = size * 23.5f / 80.0f;
    g.setColour (juce::Colour (0x12ffffff));
    g.drawEllipse (cx - shineRadius, cy - shineRadius, shineRadius * 2.0f, shineRadius * 2.0f,
                   juce::jmax (1.0f, size * 1.2f / 80.0f));

    const float needleRotation = angle - juce::MathConstants<float>::twoPi;
    const float needleInner = size * 12.0f / 80.0f;
    const float needleOuter = size * 21.0f / 80.0f;

    const auto needleTransform = juce::AffineTransform::rotation (needleRotation, cx, cy);
    const juce::Line<float> needle (cx, cy - needleOuter, cx, cy - needleInner);
    const juce::Line<float> needleRotated (needle.getStart().transformedBy (needleTransform),
                                           needle.getEnd().transformedBy (needleTransform));

    g.setColour (colour::ember.withAlpha (0.4f));
    g.drawLine (needleRotated, size * 3.6f / 80.0f);

    g.setColour (colour::emberPale);
    g.drawLine (needleRotated, juce::jmax (1.2f, size * 1.6f / 80.0f));

    g.setColour (juce::Colour (0xff57616d));
    const float centreRadius = size * 2.2f / 80.0f;
    g.fillEllipse (cx - centreRadius, cy - centreRadius, centreRadius * 2.0f, centreRadius * 2.0f);

    if ((bool) slider.getProperties().getWithDefault ("hideValueText", false))
        return;

    const bool modified = (bool) slider.getProperties().getWithDefault ("modified", false);
    g.setFont (uiFont (size * 0.115f, juce::Font::bold));
    g.setColour (modified ? colour::emberSoft : juce::Colour (0xffe6e9ed));
    g.drawText (slider.getTextFromValue (slider.getValue()),
                juce::Rectangle<float> (cx - faceRadius, cy + size * 0.105f,
                                        faceRadius * 2.0f, size * 0.16f),
                juce::Justification::centred, false);
}

void ForjaLookAndFeel::drawButtonBackground (juce::Graphics& g,
                                             juce::Button& button,
                                             const juce::Colour&,
                                             bool shouldDrawButtonAsHighlighted,
                                             bool shouldDrawButtonAsDown)
{
    const auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = button.getToggleState();

    juce::Colour top = juce::Colour (0xff282e37);
    juce::Colour bottom = juce::Colour (0xff191d24);
    juce::Colour border = colour::lineStrong;

    if (on)
    {
        top = juce::Colour (0xff342a1e);
        bottom = juce::Colour (0xff2a2219);
        border = juce::Colour (0xff785126);
    }
    else if (shouldDrawButtonAsHighlighted)
    {
        border = juce::Colour (0xff885021);
    }

    if (shouldDrawButtonAsDown)
        top = top.darker (0.18f);

    juce::ColourGradient background (top, bounds.getTopLeft(), bottom, bounds.getBottomLeft(), false);
    g.setGradientFill (background);
    g.fillRoundedRectangle (bounds, 3.0f);

    g.setColour (border);
    g.drawRoundedRectangle (bounds, 3.0f, 1.0f);

    if (on)
    {
        g.setColour (colour::ember.withAlpha (0.1f));
        g.fillRoundedRectangle (bounds.expanded (1.5f), 4.0f);
        g.setColour (border);
        g.drawRoundedRectangle (bounds, 3.0f, 1.0f);
    }
}

void ForjaLookAndFeel::drawButtonText (juce::Graphics& g,
                                       juce::TextButton& button,
                                       bool,
                                       bool)
{
    g.setFont (tracked (uiFont (8.0f, juce::Font::bold), 0.13f));
    g.setColour (button.getToggleState() ? colour::emberSoft : colour::muted);
    g.drawText (button.getButtonText().toUpperCase(),
                button.getLocalBounds().reduced (2, 0),
                juce::Justification::centred, false);
}

juce::Font ForjaLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return uiFont (9.5f, juce::Font::bold);
}

} // namespace forja::ui
