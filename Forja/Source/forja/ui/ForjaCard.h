#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

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

    // O painel contextual do TRACO reutiliza o mesmo card trocando titulo e
    // indice conforme a secao selecionada, entao expoe os dois.
    void setTitle (juce::String newTitle)
    {
        title = std::move (newTitle);
        repaint();
    }

    void setIndex (int newIndex)
    {
        index = newIndex;
        repaint();
    }

    juce::Rectangle<int> getContentBounds() const
    {
        return getLocalBounds().reduced (9, 0)
                                .withTrimmedTop (headerHeight + 10)
                                .withTrimmedBottom (11);
    }

    // Painel flutuante opcional (usado pelo TRACO): arrastar pelo cabecalho
    // move o card, arrastar o canto inferior direito redimensiona. Desligado
    // por padrao, entao os cards fixos das outras bases seguem exatamente como
    // eram - nenhum mouse handler extra age enquanto floatable e false.
    void setFloatable (bool shouldBeFloatable)
    {
        floatable = shouldBeFloatable;
        repaint();
    }

    std::function<void()> onGeometryChanged;

    static constexpr int minFloatWidth = 250;
    static constexpr int minFloatHeight = 116;

    void mouseDown (const juce::MouseEvent& event) override
    {
        if (! floatable)
        {
            juce::Component::mouseDown (event);
            return;
        }

        const juce::Point<int> local (event.x, event.y);
        const auto bounds = getLocalBounds();
        const juce::Rectangle<int> grip (bounds.getRight() - 16, bounds.getBottom() - 16, 16, 16);

        if (grip.contains (local))
            dragMode = resizeDrag;
        else if (local.y <= headerHeight)
            dragMode = moveDrag;
        else
            dragMode = noDrag;

        if (dragMode != noDrag)
        {
            dragStart = event.getScreenPosition();
            startBounds = getBounds();
        }
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        if (! floatable || dragMode == noDrag)
        {
            juce::Component::mouseDrag (event);
            return;
        }

        const auto delta = event.getScreenPosition() - dragStart;
        auto next = startBounds;

        if (dragMode == moveDrag)
        {
            next.setPosition (startBounds.getPosition() + delta);
        }
        else
        {
            next.setWidth (juce::jmax (minFloatWidth, startBounds.getWidth() + delta.x));
            next.setHeight (juce::jmax (minFloatHeight, startBounds.getHeight() + delta.y));
        }

        if (auto* parent = getParentComponent())
        {
            const auto pb = parent->getLocalBounds();
            next.setWidth (juce::jmin (next.getWidth(), pb.getWidth()));
            next.setHeight (juce::jmin (next.getHeight(), pb.getHeight()));
            next.setX (juce::jlimit (pb.getX(),
                                     juce::jmax (pb.getX(), pb.getRight() - next.getWidth()),
                                     next.getX()));
            next.setY (juce::jlimit (pb.getY(),
                                     juce::jmax (pb.getY(), pb.getBottom() - next.getHeight()),
                                     next.getY()));
        }

        setBounds (next);

        if (onGeometryChanged != nullptr)
            onGeometryChanged();
    }

    void mouseUp (const juce::MouseEvent& event) override
    {
        dragMode = noDrag;
        juce::Component::mouseUp (event);
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

        if (floatable)
        {
            // Puxador de resize no canto inferior direito: tres hachuras.
            g.setColour (colour::dim.withAlpha (0.75f));
            const auto b = getLocalBounds().toFloat();

            for (int i = 0; i < 3; ++i)
            {
                const float off = 4.0f + static_cast<float> (i) * 3.5f;
                g.drawLine (b.getRight() - off, b.getBottom() - 4.0f,
                            b.getRight() - 4.0f, b.getBottom() - off, 1.0f);
            }
        }
    }

private:
    int index = 1;
    juce::String title;
    bool accent = false;

    enum DragMode { noDrag = 0, moveDrag = 1, resizeDrag = 2 };
    bool floatable = false;
    int dragMode = noDrag;
    juce::Point<int> dragStart {};
    juce::Rectangle<int> startBounds {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ForjaCard)
};

} // namespace forja::ui
