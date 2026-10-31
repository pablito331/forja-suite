#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <cmath>
#include <functional>
#include <utility>

#include "forja/ui/ForjaTheme.h"

namespace drumforgeui
{

// Knob compacto da mesa: o mostrador da Forja em tamanho de console, com o
// rotulo embaixo e o valor no lugar dele enquanto o mouse esta em cima.
class MiniKnob : public juce::Component
{
public:
    explicit MiniKnob (juce::String labelText, float knobSize = 21.0f)
        : label (std::move (labelText)), diameter (knobSize)
    {
        slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setRange (0.0, 1.0, 0.01);
        slider.setMouseDragSensitivity (160);
        slider.setDoubleClickReturnValue (true, 0.0);
        slider.getProperties().set ("hideValueText", true);
        slider.setName (label);
        slider.setTooltip (label);
        addAndMakeVisible (slider);

        setSize (juce::jmax (26, (int) std::ceil (diameter)), (int) std::ceil (diameter) + 12);
    }

    juce::Slider& getSlider() noexcept { return slider; }

    void setFormatter (std::function<juce::String (double)> formatter)
    {
        slider.textFromValueFunction = [formatter = std::move (formatter)] (double value)
        {
            return formatter != nullptr ? formatter (value) : juce::String (value, 2);
        };
    }

    void paint (juce::Graphics& g) override
    {
        const bool active = slider.isMouseOverOrDragging (true);

        g.setFont (active ? forja::ui::uiFont (7.2f, juce::Font::bold)
                          : forja::ui::tracked (forja::ui::uiFont (6.6f, juce::Font::bold), 0.08f));
        g.setColour (active ? forja::ui::colour::emberSoft : forja::ui::colour::muted);

        const auto text = active ? slider.getTextFromValue (slider.getValue())
                                 : label.toUpperCase();

        g.drawText (text, 0, (int) std::ceil (diameter), getWidth(), 11,
                    juce::Justification::centred, false);
    }

    void resized() override
    {
        const int size = (int) std::ceil (diameter);
        slider.setBounds ((getWidth() - size) / 2, 0, size, size);
    }

private:
    juce::String label;
    float diameter = 21.0f;
    juce::Slider slider;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MiniKnob)
};

} // namespace drumforgeui
