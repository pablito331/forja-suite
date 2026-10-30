#include "ForjaTopBar.h"

#include "ForjaTheme.h"

namespace forja::ui
{

namespace
{
float textWidth (const juce::Font& font, const juce::String& text)
{
    return juce::GlyphArrangement::getStringWidth (font, text);
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

ForjaTopBar::ForjaTopBar (bool compactLayout)
    : compact (compactLayout)
{
    addAndMakeVisible (fundamentosButton);
    fundamentosButton.setTooltip (utf8 ("Como este plugin funciona, por dentro."));
    fundamentosButton.onClick = [this]
    {
        if (onFundamentos != nullptr)
            onFundamentos();
    };

    if (kSocialUrl[0] != '\0')
    {
        addAndMakeVisible (redesButton);
        redesButton.setTooltip (utf8 ("Redes e link agregador da Forja."));
        redesButton.onClick = []
        {
            juce::URL (kSocialUrl).launchInDefaultBrowser();
        };
    }
}

void ForjaTopBar::setIdentity (juce::String productName,
                               juce::String familyName,
                               juce::String newSubtitle,
                               juce::String sourceChip)
{
    product = std::move (productName);
    family = std::move (familyName);
    subtitle = std::move (newSubtitle);
    source = std::move (sourceChip);
    repaint();
    resized();
}

void ForjaTopBar::setBypassed (bool shouldBeBypassed)
{
    if (bypassed == shouldBeBypassed)
        return;

    bypassed = shouldBeBypassed;
    repaint();
    resized();
}

juce::String ForjaTopBar::getStateText() const
{
    return bypassed ? juce::String ("BYPASS") : juce::String ("ATIVO");
}

ForjaTopBar::ChipLayout ForjaTopBar::getChipLayout() const
{
    const auto chipFont = tracked (uiFont (compact ? 6.5f : 7.5f, juce::Font::bold), 0.1f);
    const float chipY = compact ? 13.5f : 16.5f;
    const float chipHeight = compact ? 15.0f : 16.0f;
    float right = (float) getWidth() - 11.0f;

    const auto stateText = getStateText();
    const float stateWidth = 8.0f + 6.0f + 5.0f
                           + textWidth (chipFont, stateText) + 8.0f;
    const juce::Rectangle<float> state (right - stateWidth, chipY, stateWidth, chipHeight);
    right = state.getX() - 7.0f;

    const float mitWidth = textWidth (chipFont, "MIT") + 16.0f;
    const juce::Rectangle<float> mit (right - mitWidth, chipY, mitWidth, chipHeight);
    right -= mitWidth + 7.0f;

    juce::Rectangle<float> sourceChip;

    if (! compact)
    {
        const float sourceWidth = textWidth (chipFont, source) + 16.0f;
        sourceChip = { right - sourceWidth, chipY, sourceWidth, chipHeight };
        right -= sourceWidth + 7.0f;
    }

    juce::Rectangle<float> socialChip;

    if (kSocialUrl[0] != '\0')
    {
        const float socialWidth = textWidth (chipFont, "REDES") + 20.0f;
        socialChip = { right - socialWidth, chipY, socialWidth, chipHeight };
        right -= socialWidth + 7.0f;
    }

    const float infoWidth = compact ? 92.0f : 104.0f;
    const juce::Rectangle<float> info (right - infoWidth, compact ? 12.5f : 15.5f,
                                       infoWidth, compact ? 17.0f : 18.0f);
    return { state, mit, sourceChip, socialChip, info };
}

void ForjaTopBar::resized()
{
    const auto layout = getChipLayout();
    fundamentosButton.setBounds (layout.info.toNearestInt());

    if (kSocialUrl[0] != '\0')
        redesButton.setBounds (layout.social.toNearestInt());
}

void ForjaTopBar::paint (juce::Graphics& g)
{
    const auto bar = getLocalBounds().toFloat();

    juce::ColourGradient background (juce::Colour (0xff1d222a), 0.0f, 0.0f,
                                     juce::Colour (0xff171b22), 0.0f, bar.getBottom(), false);
    g.setGradientFill (background);
    g.fillRoundedRectangle (bar, 4.0f);
    g.setColour (juce::Colour (0xff262d36));
    g.drawRoundedRectangle (bar.reduced (0.5f), 4.0f, 1.0f);

    const juce::Rectangle<float> icon = compact ? juce::Rectangle<float> (10.0f, 8.0f, 26.0f, 26.0f)
                                                : juce::Rectangle<float> (11.0f, 9.5f, 30.0f, 30.0f);
    g.setColour (colour::ember.withAlpha (0.08f));
    g.fillRoundedRectangle (icon.expanded (2.0f), 7.0f);

    juce::ColourGradient iconFill (juce::Colour (0xff332519), icon.getTopLeft(),
                                   juce::Colour (0xff1c1a18), icon.getBottomRight(), false);
    g.setGradientFill (iconFill);
    g.fillRoundedRectangle (icon, 5.0f);
    g.setColour (juce::Colour (0xff70411e));
    g.drawRoundedRectangle (icon.reduced (0.5f), 5.0f, 1.0f);

    const float iconCx = icon.getCentreX();
    const float iconCy = icon.getCentreY();
    const float diamondRadius = compact ? 5.5f : 6.5f;
    juce::Path diamond;
    diamond.startNewSubPath (iconCx, iconCy - diamondRadius);
    diamond.lineTo (iconCx + diamondRadius, iconCy);
    diamond.lineTo (iconCx, iconCy + diamondRadius);
    diamond.lineTo (iconCx - diamondRadius, iconCy);
    diamond.closeSubPath();
    g.setColour (colour::ember);
    g.strokePath (diamond, juce::PathStrokeType (1.5f));
    g.fillEllipse (iconCx - 1.2f, iconCy - 1.2f, 2.4f, 2.4f);

    const float logoX = compact ? 44.0f : 57.0f;
    g.setFont (tracked (uiFont (compact ? 10.5f : 13.0f, juce::Font::bold), 0.16f));
    g.setColour (colour::ember);
    g.drawText ("FORJA", logoX, compact ? 5.0f : 9.0f, 140.0f, 18.0f,
                juce::Justification::centredLeft, false);

    g.setFont (tracked (uiFont (compact ? 7.0f : 9.0f, juce::Font::bold), 0.18f));
    g.setColour (juce::Colour (0xffa2a9b1));
    g.drawText (family, logoX, compact ? 21.0f : 28.0f, 150.0f, 12.0f,
                juce::Justification::centredLeft, false);

    g.setColour (juce::Colour (0xff2a313a));
    g.fillRect (compact ? 152.0f : 168.0f, compact ? 8.0f : 11.0f, 1.0f, 27.0f);

    const float productX = compact ? 164.0f : 182.0f;
    g.setFont (displayFont (compact ? 16.0f : 20.0f));
    g.setColour (colour::text);
    g.drawText (product, productX, compact ? 4.0f : 8.0f, compact ? 140.0f : 260.0f, 26.0f,
                juce::Justification::centredLeft, false);

    g.setFont (tracked (uiFont (compact ? 6.8f : 8.0f, juce::Font::bold), 0.16f));
    g.setColour (juce::Colour (0xff8e99a6));
    g.drawText (subtitle, productX + 1.0f, compact ? 25.0f : 32.0f,
                compact ? 130.0f : 260.0f, 10.0f,
                juce::Justification::centredLeft, false);

    const auto layout = getChipLayout();
    const auto chipFont = tracked (uiFont (compact ? 6.5f : 7.5f, juce::Font::bold), 0.1f);

    drawChip (g, layout.state, "", chipFont, colour::text,
              bypassed ? juce::Colour (0xff785126) : juce::Colour (0xff34523d),
              bypassed ? juce::Colour (0xff342617) : juce::Colour (0xff1e3027));

    g.setColour (bypassed ? colour::emberSoft : colour::success);
    g.fillEllipse (layout.state.getX() + 8.0f, layout.state.getCentreY() - 2.5f, 5.0f, 5.0f);

    g.setFont (chipFont);
    g.setColour (bypassed ? colour::emberSoft : juce::Colour (0xffa7d1ad));
    g.drawText (getStateText(), layout.state.withTrimmedLeft (19.0f).withTrimmedRight (8.0f),
                juce::Justification::centredLeft, false);

    drawChip (g, layout.mit, "MIT", chipFont,
              juce::Colour (0xffd29a60), juce::Colour (0xff5a432d), juce::Colour (0xff241d16));

    if (layout.source.getWidth() > 0.0f)
        drawChip (g, layout.source, source, chipFont,
                  juce::Colour (0xffa8b1bb), juce::Colour (0xff333b46),
                  juce::Colours::transparentBlack);
}

} // namespace forja::ui
