#include "ForjaStatusBar.h"

#include "ForjaTheme.h"

#include <cmath>

namespace forja::ui
{

namespace
{
float textWidth (const juce::Font& font, const juce::String& text)
{
    return juce::GlyphArrangement::getStringWidth (font, text);
}

juce::String sampleRateText (double sr)
{
    if (sr <= 0.0)
        return "\u2014";

    if (sr >= 1000.0)
        return juce::String (juce::roundToInt (sr / 1000.0)) + " kHz";

    return juce::String (juce::roundToInt (sr)) + " Hz";
}

void drawChip (juce::Graphics& g,
               juce::Rectangle<float> area,
               const juce::String& text,
               const juce::Font& font,
               juce::Colour textColour,
               juce::Colour borderColour,
               juce::Colour fillColour)
{
    g.setColour (fillColour);
    g.fillRoundedRectangle (area, 3.0f);
    g.setColour (borderColour);
    g.drawRoundedRectangle (area.reduced (0.5f), 3.0f, 1.0f);
    g.setFont (font);
    g.setColour (textColour);
    g.drawText (text, area, juce::Justification::centred, false);
}
} // namespace

ForjaStatusBar::ForjaStatusBar (bool compactLayout)
    : compact (compactLayout)
{
}

void ForjaStatusBar::setIdentity (juce::String productName, juce::String subtitleText)
{
    product = std::move (productName);
    subtitle = std::move (subtitleText);
    repaint();
}

void ForjaStatusBar::setBadge (BadgeKind kind, juce::String text, juce::String detail)
{
    if (badgeKind == kind && badgeText == text && badgeDetail == detail)
        return;

    badgeKind = kind;
    badgeText = std::move (text);
    badgeDetail = std::move (detail);
    repaint();
}

void ForjaStatusBar::setMeta (double sr,
                              int blockSize,
                              std::vector<std::pair<juce::String, juce::String>> values)
{
    sampleRate = sr;
    bufferSize = blockSize;
    meta = std::move (values);
    repaint();
}

void ForjaStatusBar::setFooter (juce::String text)
{
    if (footer == text)
        return;

    footer = std::move (text);
    repaint();
}

void ForjaStatusBar::paint (juce::Graphics& g)
{
    const auto bar = getLocalBounds().toFloat();

    if (compact)
    {
        juce::ColourGradient compactBackground (juce::Colour (0xff20252d), 0.0f, 0.0f,
                                                juce::Colour (0xff191d24), 0.0f, bar.getBottom(), false);
        g.setGradientFill (compactBackground);
        g.fillRoundedRectangle (bar, 4.0f);
        g.setColour (juce::Colour (0xff343d49));
        g.drawRoundedRectangle (bar.reduced (0.5f), 4.0f, 1.0f);

        g.setColour (colour::ember.withAlpha (0.88f));
        g.fillRoundedRectangle (10.0f, 10.0f, 3.0f, bar.getHeight() - 20.0f, 2.0f);

        g.setFont (displayFont (12.0f));
        g.setColour (juce::Colour (0xffeef0f2));
        g.drawText (product, 21.0f, 5.0f, 146.0f, 16.0f, juce::Justification::centredLeft, false);

        g.setFont (uiFont (7.0f));
        g.setColour (juce::Colour (0xff929ca7));
        g.drawText (subtitle, 22.0f, 21.0f, 146.0f, 10.0f, juce::Justification::centredLeft, true);

        g.setColour (juce::Colour (0xff343c47));
        g.fillRect (172.0f, 9.0f, 1.0f, bar.getHeight() - 18.0f);

        const auto badgeFont = tracked (uiFont (6.5f, juce::Font::bold), 0.1f);
        const float badgeWidth = textWidth (badgeFont, badgeText) + 14.0f;
        const juce::Rectangle<float> badgeBounds (182.0f, 11.0f, badgeWidth, 14.0f);

        juce::Colour badgeTextColour = juce::Colour (0xff8b95a0);
        juce::Colour badgeBorder = juce::Colour (0xff303743);
        juce::Colour badgeFill = juce::Colour (0xff191d24);

        if (badgeKind == BadgeKind::ready)
        {
            badgeTextColour = juce::Colour (0xffa7d1ad);
            badgeBorder = juce::Colour (0xff34523d);
            badgeFill = juce::Colour (0xff1e3027);
        }
        else if (badgeKind == BadgeKind::bypass)
        {
            badgeTextColour = colour::emberSoft;
            badgeBorder = juce::Colour (0xff785126);
            badgeFill = juce::Colour (0xff342617);
        }
        else if (badgeKind == BadgeKind::clip)
        {
            badgeTextColour = juce::Colour (0xffff9a8e);
            badgeBorder = juce::Colour (0xff71403b);
            badgeFill = juce::Colour (0xff321d1b);
        }

        drawChip (g, badgeBounds, badgeText, badgeFont, badgeTextColour, badgeBorder, badgeFill);

        const float detailX = badgeBounds.getRight() + 7.0f;
        g.setFont (uiFont (8.5f));
        g.setColour (juce::Colour (0xffc5ccd4));
        g.drawText (badgeDetail, detailX, 10.0f,
                    juce::jmax (40.0f, bar.getRight() - 12.0f - detailX), 16.0f,
                    juce::Justification::centredLeft, true);
        return;
    }

    juce::ColourGradient background (juce::Colour (0xff20252d), 0.0f, 0.0f,
                                     juce::Colour (0xff191d24), 0.0f, bar.getBottom(), false);
    g.setGradientFill (background);
    g.fillRoundedRectangle (bar, 4.0f);
    g.setColour (juce::Colour (0xff343d49));
    g.drawRoundedRectangle (bar.reduced (0.5f), 4.0f, 1.0f);

    g.setColour (colour::ember.withAlpha (0.88f));
    g.fillRoundedRectangle (12.0f, 13.0f, 3.0f, bar.getHeight() - 26.0f, 2.0f);

    g.setFont (displayFont (15.0f));
    g.setColour (juce::Colour (0xffeef0f2));
    g.drawText (product, 25.0f, 14.0f, 190.0f, 18.0f, juce::Justification::centredLeft, false);

    g.setFont (uiFont (8.5f));
    g.setColour (juce::Colour (0xff929ca7));
    g.drawText (subtitle, 25.0f, 35.0f, 190.0f, 12.0f, juce::Justification::centredLeft, false);

    g.setColour (juce::Colour (0xff343c47));
    g.fillRect (226.0f, 13.0f, 1.0f, bar.getHeight() - 26.0f);

    const auto badgeFont = tracked (uiFont (7.0f, juce::Font::bold), 0.1f);
    const float badgeWidth = textWidth (badgeFont, badgeText) + 14.0f;
    const juce::Rectangle<float> badgeBounds (245.0f, 23.0f, badgeWidth, 16.0f);

    juce::Colour badgeTextColour = juce::Colour (0xff8b95a0);
    juce::Colour badgeBorder = juce::Colour (0xff303743);
    juce::Colour badgeFill = juce::Colour (0xff191d24);

    if (badgeKind == BadgeKind::ready)
    {
        badgeTextColour = juce::Colour (0xffa7d1ad);
        badgeBorder = juce::Colour (0xff34523d);
        badgeFill = juce::Colour (0xff1e3027);
    }
    else if (badgeKind == BadgeKind::bypass)
    {
        badgeTextColour = colour::emberSoft;
        badgeBorder = juce::Colour (0xff785126);
        badgeFill = juce::Colour (0xff342617);
    }
    else if (badgeKind == BadgeKind::clip)
    {
        badgeTextColour = juce::Colour (0xffff9a8e);
        badgeBorder = juce::Colour (0xff71403b);
        badgeFill = juce::Colour (0xff321d1b);
    }

    drawChip (g, badgeBounds, badgeText, badgeFont, badgeTextColour, badgeBorder, badgeFill);

    g.setFont (uiFont (9.5f));
    g.setColour (juce::Colour (0xffc5ccd4));
    const float detailX = badgeBounds.getRight() + 7.0f;
    const float detailWidth = juce::jmax (80.0f, 530.0f - detailX);
    g.drawText (badgeDetail, detailX, 22.0f, detailWidth, 16.0f,
                juce::Justification::centredLeft, true);

    g.setFont (uiFont (8.5f));
    g.setColour (juce::Colour (0xff77828e));
    g.drawText (utf8 ("Duplo clique = padr\u00e3o \u00b7 setas = ajuste fino \u00b7 clique no CLIP para limpar"),
                245.0f, 44.0f, 282.0f, 12.0f, juce::Justification::centredLeft, false);

    g.setColour (juce::Colour (0xff343c47));
    g.fillRect (534.0f, 13.0f, 1.0f, bar.getHeight() - 26.0f);

    const auto mono = monoFont (8.0f);
    float x = 545.0f;
    const float metaY = 20.0f;

    auto drawMeta = [&] (const juce::String& label, const juce::String& value)
    {
        if (x > bar.getRight() - 24.0f)
            return;

        g.setFont (mono);
        g.setColour (juce::Colour (0xff84909c));
        g.drawText (label, x, metaY, 90.0f, 11.0f, juce::Justification::centredLeft, false);
        x += textWidth (mono, label) + 3.0f;

        g.setColour (juce::Colour (0xffb3bdc7));
        g.drawText (value, x, metaY, 90.0f, 11.0f, juce::Justification::centredLeft, false);
        x += textWidth (mono, value) + 13.0f;
    };

    drawMeta ("sr", sampleRateText (sampleRate));
    drawMeta ("buffer", juce::String (bufferSize));

    for (const auto& item : meta)
        drawMeta (item.first, item.second);

    g.setFont (mono);
    g.setColour (juce::Colour (0xff84909c));
    g.drawText (footer, 545.0f, 44.0f, 420.0f, 11.0f,
                juce::Justification::centredLeft, false);
}

} // namespace forja::ui
