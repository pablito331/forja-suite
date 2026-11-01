#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <cmath>
#include <functional>
#include <utility>

#include "forja/ui/ForjaTheme.h"

namespace forjarackui
{

// Knob de gaveta: mostrador compacto com rotulo fixo embaixo e valor por cima
// enquanto arrasta. O acento segue a cor da familia do modulo.
class RackKnob : public juce::Component
{
public:
    explicit RackKnob (juce::String labelText, float knobSize = 30.0f)
        : label (std::move (labelText)), diameter (knobSize)
    {
        slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setRange (0.0, 1.0, 0.01);
        slider.setMouseDragSensitivity (140);
        slider.setDoubleClickReturnValue (true, 0.0);
        slider.getProperties().set ("hideValueText", true);
        slider.setName (label);
        slider.setTooltip (label);
        addAndMakeVisible (slider);
    }

    juce::Slider& getSlider() noexcept { return slider; }

    void setLabelText (const juce::String& text)
    {
        label = text;
        slider.setName (text);
        slider.setTooltip (text);
        repaint();
    }

    void setAccent (juce::Colour accent)
    {
        accentColour = accent;
        slider.getProperties().set ("accent", static_cast<double> (accent.getARGB()));
        repaint();
    }

    void setFormatter (std::function<juce::String (double)> formatter)
    {
        slider.textFromValueFunction = [formatter = std::move (formatter)] (double value)
        {
            return formatter != nullptr ? formatter (value) : juce::String (value, 2);
        };
    }

    int getPreferredHeight() const noexcept { return static_cast<int> (std::ceil (diameter)) + 12; }

    // A gaveta redimensiona o mostrador conforme o espaco da coluna de knobs.
    void setDiameter (float newSize)
    {
        if (std::abs (newSize - diameter) < 0.5f)
            return;

        diameter = newSize;
        resized();
        repaint();
    }

    // Altura da faixa de rotulo sob o mostrador. A gaveta aperta esse espaco
    // quando o modulo tem muitas fileiras; o knob nunca invade a fileira vizinha.
    void setLabelHeight (int height)
    {
        if (height == labelH)
            return;

        labelH = height;
        resized();
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const bool active = slider.isMouseOverOrDragging (true);

        g.setFont (forja::ui::tracked (forja::ui::uiFont (6.6f, juce::Font::bold), 0.06f));
        g.setColour (active ? accentColour.brighter() : forja::ui::colour::muted);

        const auto text = active ? slider.getTextFromValue (slider.getValue())
                                 : label.toUpperCase();

        const int size = static_cast<int> (std::ceil (diameter));

        g.drawText (text, 0, size, getWidth(), labelH,
                    juce::Justification::centred, true);
    }

    void resized() override
    {
        const int size = static_cast<int> (std::ceil (diameter));
        slider.setBounds ((getWidth() - size) / 2, 0, size, size);
    }

private:
    juce::String label;
    juce::Colour accentColour { forja::ui::colour::ember };
    float diameter = 30.0f;
    int labelH = 11;
    juce::Slider slider;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RackKnob)
};

} // namespace forjarackui
