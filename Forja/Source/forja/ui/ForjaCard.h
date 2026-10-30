#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "ForjaTheme.h"

namespace forja::ui
{

class ForjaCard : public juce::Component
{
public:
    ForjaCard (int cardIndex, juce::String cardTitle, bool emberAccent = false)
        : index (cardIndex), title (std::move (cardTitle)), accent (emberAccent)
    {
        setInterceptsMouseClicks (true, true);
    }

    static constexpr int headerHeight = 34;

    juce::Rectangle<int> getContentBounds() const
    {
        return getLocalBounds().reduced (9, 0)
                                .withTrimmedTop (headerHeight + 10)
                                .withTrimmedBottom (11);
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();

        juce::ColourGradient background (colour::iron3, bounds.getTopLeft(),
                                         colour::iron2, bounds.getBottomRight(), false);
        background.addColour (0.8, colour::iron2);
        g.setGradientFill (background);
        g.fillRoundedRectangle (bounds, 5.0f);

        g.setColour (juce::Colour (0xff333b46));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 5.0f, 1.0f);

        g.setColour (juce::Colour (0x08ffffff));
        g.drawLine (6.0f, 1.0f, bounds.getRight() - 6.0f, 1.0f, 1.0f);

        if (accent)
        {
            juce::ColourGradient stripe (colour::ember, 0.0f, 0.0f,
                                         juce::Colour (0xff824419), 0.0f, bounds.getBottom(), false);
            g.setGradientFill (stripe);
        }
        else
        {
            g.setColour (juce::Colour (0xff3c4653));
        }

        g.fillRect (0.0f, 0.0f, 2.0f, bounds.getBottom());

        g.setColour (colour::line);
        g.drawLine (0.0f, (float) headerHeight, bounds.getRight(), (float) headerHeight, 1.0f);

        const auto indexFont = tracked (uiFont (7.0f, juce::Font::bold), 0.04f);
        const auto titleFont = tracked (uiFont (8.0f, juce::Font::bold), 0.16f);
        const auto indexText = juce::String (index).paddedLeft ('0', 2);
        const auto titleText = title.toUpperCase();

        const float indexWidth = juce::GlyphArrangement::getStringWidth (indexFont, indexText);
        const float titleWidth = juce::GlyphArrangement::getStringWidth (titleFont, titleText);
        const float gap = 7.0f;
        const float groupWidth = indexWidth + gap + titleWidth;
        float x = ((float) getWidth() - groupWidth) * 0.5f;

        g.setFont (indexFont);
        g.setColour (juce::Colour (0xff70523c));
        g.drawText (indexText, juce::Rectangle<float> (x, 0.0f, indexWidth, (float) headerHeight),
                    juce::Justification::centredLeft, false);

        x += indexWidth + gap;
        g.setFont (titleFont);
        g.setColour (juce::Colour (0xff8e99a6));
        g.drawText (titleText, juce::Rectangle<float> (x, 0.0f, titleWidth, (float) headerHeight),
                    juce::Justification::centredLeft, false);
    }

private:
    int index = 1;
    juce::String title;
    bool accent = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ForjaCard)
};

} // namespace forja::ui
