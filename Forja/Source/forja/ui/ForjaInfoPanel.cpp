#include "ForjaInfoPanel.h"

#include "ForjaTheme.h"

#include <cmath>

namespace forja::ui
{

class ForjaInfoPanel::Content final : public juce::Component
{
public:
    explicit Content (const ForjaInfo& source)
        : info (source)
    {
        setInterceptsMouseClicks (false, false);
        buildText();
    }

    int getRequiredHeight (int width) const
    {
        juce::TextLayout layout;
        layout.createLayout (text, (float) juce::jmax (80, width - 8));
        return (int) std::ceil (layout.getHeight()) + 18;
    }

    void paint (juce::Graphics& g) override
    {
        juce::TextLayout layout;
        layout.createLayout (text, (float) juce::jmax (80, getWidth() - 8));
        layout.draw (g, juce::Rectangle<float> (0.0f, 8.0f,
                                                (float) getWidth(),
                                                (float) juce::jmax (1, getHeight() - 8)));
    }

private:
    void addHeading (const juce::String& heading)
    {
        text.append ("\n", uiFont (8.0f), colour::muted);
        text.append (heading.toUpperCase(), tracked (uiFont (9.0f, juce::Font::bold), 0.13f),
                     colour::emberSoft);
        text.append ("\n\n", uiFont (8.0f), colour::muted);
    }

    void addParagraph (const juce::String& paragraph)
    {
        text.append (paragraph, uiFont (11.0f), colour::muted);
        text.append ("\n\n", uiFont (8.0f), colour::muted);
    }

    void buildText()
    {
        text.setWordWrap (juce::AttributedString::byWord);

        if (info.tagline.isNotEmpty())
        {
            text.append (info.tagline, uiFont (11.0f), colour::text);
            text.append ("\n\n", uiFont (8.0f), colour::muted);
        }

        for (const auto& section : info.sections)
        {
            addHeading (section.heading);
            for (const auto& paragraph : section.paragraphs)
                addParagraph (paragraph);
        }

        if (! info.controls.empty())
        {
            addHeading (utf8 ("Controles"));

            for (const auto& control : info.controls)
            {
                text.append (control.name, uiFont (10.0f, juce::Font::bold), colour::text);
                if (control.range.isNotEmpty())
                {
                    text.append ("   ", uiFont (8.0f), colour::dim);
                    text.append (control.range, monoFont (8.5f), colour::emberSoft);
                }
                text.append ("\n", uiFont (8.0f), colour::muted);
                text.append (control.description, uiFont (10.5f), colour::muted);
                text.append ("\n\n", uiFont (8.0f), colour::muted);
            }
        }

        addHeading (utf8 ("Origem e licen\u00e7a"));

        const auto& prov = info.provenance;
        if (prov.source.isNotEmpty())
        {
            text.append (prov.source, uiFont (10.5f, juce::Font::bold), colour::text);
            text.append ("\n", uiFont (8.0f), colour::muted);
        }
        if (prov.author.isNotEmpty())
        {
            text.append (prov.author, uiFont (10.5f), colour::muted);
            text.append ("\n", uiFont (8.0f), colour::muted);
        }
        if (prov.license.isNotEmpty())
        {
            text.append (prov.license, uiFont (10.5f), colour::muted);
            text.append ("\n", uiFont (8.0f), colour::muted);
        }
        if (prov.commit.isNotEmpty())
        {
            text.append (prov.commit, monoFont (8.5f), colour::dim);
            text.append ("\n", uiFont (8.0f), colour::muted);
        }
        if (prov.notes.isNotEmpty())
        {
            text.append ("\n", uiFont (8.0f), colour::muted);
            text.append (prov.notes, uiFont (10.5f), colour::muted);
        }
    }

    ForjaInfo info;
    juce::AttributedString text;
};

ForjaInfoPanel::ForjaInfoPanel (ForjaInfo infoToShow)
    : info (std::move (infoToShow))
{
    setInterceptsMouseClicks (true, true);
    setWantsKeyboardFocus (true);
    setVisible (false);

    content = std::make_unique<Content> (info);
    viewport.setViewedComponent (content.get(), false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (10);
    viewport.getVerticalScrollBar().setColour (juce::ScrollBar::thumbColourId,
                                               colour::ember.withAlpha (0.55f));
    viewport.getVerticalScrollBar().setColour (juce::ScrollBar::trackColourId,
                                               juce::Colours::transparentBlack);
    addAndMakeVisible (viewport);

    addAndMakeVisible (closeButton);
    closeButton.onClick = [this] { closeFromUser(); };
    closeButton.setTooltip (utf8 ("Fechar fundamentos"));
}

ForjaInfoPanel::~ForjaInfoPanel()
{
    viewport.setViewedComponent (nullptr, false);
}

void ForjaInfoPanel::showPanel()
{
    setVisible (true);
    toFront (true);
    grabKeyboardFocus();
    closeButton.toFront (false);
}

void ForjaInfoPanel::hidePanel()
{
    setVisible (false);
}

void ForjaInfoPanel::closeFromUser()
{
    hidePanel();
    if (onClosed != nullptr)
        onClosed();
}

void ForjaInfoPanel::paint (juce::Graphics& g)
{
    g.setColour (juce::Colours::black.withAlpha (0.66f));
    g.fillAll();

    const auto sheet = sheetBounds.toFloat();

    juce::ColourGradient background (colour::iron3, sheet.getTopLeft(),
                                     colour::iron0, sheet.getBottomRight(), false);
    background.addColour (0.75, colour::iron1);
    g.setGradientFill (background);
    g.fillRoundedRectangle (sheet, 7.0f);

    g.setColour (colour::lineStrong);
    g.drawRoundedRectangle (sheet.reduced (0.5f), 7.0f, 1.0f);

    g.setColour (colour::ember.withAlpha (0.9f));
    g.fillRoundedRectangle (sheet.getX() + 18.0f, sheet.getY() + 1.0f,
                            sheet.getWidth() - 36.0f, 2.0f, 1.0f);

    const float x = sheet.getX() + 28.0f;
    const float y = sheet.getY() + 14.0f;

    g.setFont (tracked (uiFont (7.0f, juce::Font::bold), 0.18f));
    g.setColour (colour::emberSoft);
    g.drawText ("FUNDAMENTOS", x, y, 180.0f, 10.0f,
                juce::Justification::centredLeft, false);

    g.setFont (displayFont (23.0f));
    g.setColour (colour::text);
    g.drawText (info.title, x, y + 12.0f, sheet.getWidth() - 140.0f, 28.0f,
                juce::Justification::centredLeft, false);

    g.setFont (tracked (uiFont (8.0f, juce::Font::bold), 0.13f));
    g.setColour (juce::Colour (0xff8e99a6));
    g.drawText (info.subtitle, x + 1.0f, y + 39.0f, sheet.getWidth() - 140.0f, 12.0f,
                juce::Justification::centredLeft, false);

    g.setColour (juce::Colour (0xff343c47));
    g.fillRect (x, sheet.getY() + 70.0f, sheet.getWidth() - 56.0f, 1.0f);
}

void ForjaInfoPanel::resized()
{
    const int sheetWidth = juce::jmin (900, juce::jmax (320, getWidth() - 32));
    const int sheetHeight = juce::jmin (480, juce::jmax (200, getHeight() - 24));
    sheetBounds = juce::Rectangle<int> ((getWidth() - sheetWidth) / 2,
                                        (getHeight() - sheetHeight) / 2,
                                        sheetWidth, sheetHeight);

    closeButton.setBounds (sheetBounds.getRight() - 100, sheetBounds.getY() + 18, 68, 24);

    const auto viewBounds = juce::Rectangle<int> (sheetBounds.getX() + 28,
                                                  sheetBounds.getY() + 80,
                                                  sheetBounds.getWidth() - 56,
                                                  sheetBounds.getHeight() - 100);
    viewport.setBounds (viewBounds);

    const int contentWidth = juce::jmax (80, viewport.getWidth() - 12);
    content->setSize (contentWidth, content->getRequiredHeight (contentWidth));
}

void ForjaInfoPanel::mouseDown (const juce::MouseEvent& event)
{
    if (! sheetBounds.contains (event.getPosition()))
    {
        closeFromUser();
        return;
    }

    grabKeyboardFocus();
}

bool ForjaInfoPanel::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        closeFromUser();
        return true;
    }

    return false;
}

} // namespace forja::ui
