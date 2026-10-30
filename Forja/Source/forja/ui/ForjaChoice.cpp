#include "ForjaChoice.h"

#include "ForjaTheme.h"

namespace forja::ui
{

ForjaChoice::ForjaChoice (juce::String captionText, juce::StringArray items)
    : caption (std::move (captionText))
{
    combo.addItemList (items, 1);
    combo.setSelectedItemIndex (0, juce::dontSendNotification);
    combo.setJustificationType (juce::Justification::centredLeft);

    combo.setColour (juce::ComboBox::backgroundColourId, colour::iron1);
    combo.setColour (juce::ComboBox::textColourId, colour::text);
    combo.setColour (juce::ComboBox::outlineColourId, colour::lineStrong);
    combo.setColour (juce::ComboBox::arrowColourId, colour::muted);
    combo.setColour (juce::ComboBox::focusedOutlineColourId, colour::ember);
    combo.setColour (juce::PopupMenu::backgroundColourId, colour::iron2);
    combo.setColour (juce::PopupMenu::textColourId, colour::text);
    combo.setColour (juce::PopupMenu::highlightedBackgroundColourId, colour::ember.withAlpha (0.22f));
    combo.setColour (juce::PopupMenu::highlightedTextColourId, colour::emberPale);

    addAndMakeVisible (combo);
}

void ForjaChoice::paint (juce::Graphics& g)
{
    g.setFont (tracked (uiFont (7.0f, juce::Font::bold), 0.12f));
    g.setColour (colour::dim);
    g.drawText (caption.toUpperCase(), 0, 0, getWidth(), 11,
                juce::Justification::centredLeft, false);

    g.setColour (colour::ember.withAlpha (0.85f));
    g.fillRoundedRectangle (0.0f, 12.0f, 2.0f, (float) juce::jmax (0, getHeight() - 13), 1.0f);
}

void ForjaChoice::resized()
{
    combo.setBounds (8, 13, juce::jmax (40, getWidth() - 8), juce::jmax (18, getHeight() - 15));
}

} // namespace forja::ui
