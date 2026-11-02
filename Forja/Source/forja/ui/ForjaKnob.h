#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "ForjaTheme.h"

namespace forja::ui
{

// Opcoes de layout do knob. Moram em namespace e nao dentro de ForjaKnob porque o
// GCC do runner de Linux (C++20) nao aceita um aggregate aninhado com default member
// initializers sendo value-inicializado num default argument da propria classe
// envolvente: "default member initializer for ForjaKnob::Options::minWidth required
// before the end of its enclosing class" derrubava PluginProcessor.cpp de todo plugin
// que incluia o header. MSVC e clang aceitavam, entao so a CI de release pegava.
// Continua aggregate SEM construtor declarado: os editores passam listas parciais
// ({ 130, true }) e um construtor de quatro argumentos reprovava todas elas com
// C2665 no MSVC.
struct ForjaKnobOptions
{
    int minWidth = 160;
    bool showHint = true;
    bool showLabel = true;
    bool showValue = true;
};

class ForjaKnob : public juce::Component
{
public:
    using Options = ForjaKnobOptions;

    ForjaKnob (juce::String knobLabel,
               juce::String knobHint,
               float knobDiameter,
               double minimum,
               double maximum,
               double interval,
               double defaultValue,
               std::function<juce::String (double)> valueFormatter = {},
               Options options = Options())
        : label (std::move (knobLabel)),
          hint (std::move (knobHint)),
          diameter (knobDiameter),
          defaultVal (defaultValue),
          formatter (std::move (valueFormatter)),
          layoutOptions (options)
    {
        slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setRange (minimum, maximum, interval);
        slider.setDoubleClickReturnValue (true, defaultValue);
        slider.setMouseDragSensitivity (180);
        slider.setWantsKeyboardFocus (true);
        slider.setName (label);
        slider.setTooltip (hint);

        if (! layoutOptions.showValue)
            slider.getProperties().set ("hideValueText", true);

        if (formatter != nullptr)
            slider.textFromValueFunction = [this] (double value) { return formatter (value); };

        slider.onValueChange = [this]
        {
            updateModifiedState();
            if (onValueChange != nullptr)
                onValueChange();
        };

        addAndMakeVisible (slider);
        updateModifiedState();
        const bool showsText = layoutOptions.showLabel || layoutOptions.showHint;
        setSize (juce::jmax (layoutOptions.minWidth, (int) std::ceil (knobDiameter) + 40),
                 (int) std::ceil (knobDiameter) + (showsText ? 27 : 4));
    }

    juce::Slider& getSlider() noexcept { return slider; }

    std::function<void()> onValueChange;

    void paint (juce::Graphics& g) override
    {
        const int top = (int) std::ceil (diameter);
        const int centreX = getWidth() / 2;

        if (layoutOptions.showLabel)
        {
            g.setFont (uiFont (10.0f, juce::Font::bold));
            g.setColour (juce::Colour (0xffc3cad2));
            g.drawText (label, 0, top + 3, getWidth(), 12, juce::Justification::centred, false);
        }

        if (layoutOptions.showHint)
        {
            g.setFont (tracked (uiFont (7.0f, juce::Font::bold), 0.12f));
            g.setColour (juce::Colour (0xff64707d));
            g.drawText (hint.toUpperCase(), 0, top + 16, getWidth(), 9, juce::Justification::centred, false);
        }

        if (isModified())
        {
            g.setColour (colour::ember.withAlpha (0.25f));
            g.fillEllipse ((float) centreX + diameter * 0.37f - 4.0f, 0.0f, 8.0f, 8.0f);
            g.setColour (colour::ember);
            g.fillEllipse ((float) centreX + diameter * 0.37f - 2.0f, 2.0f, 4.0f, 4.0f);
        }
    }

    void resized() override
    {
        slider.setBounds ((getWidth() - (int) diameter) / 2, 0, (int) diameter, (int) diameter);
    }

private:
    bool isModified() const
    {
        return std::abs (slider.getValue() - defaultVal) > slider.getInterval() * 0.5;
    }

    void updateModifiedState()
    {
        slider.getProperties().set ("modified", isModified());
        repaint();
    }

    juce::String label;
    juce::String hint;
    float diameter = 92.0f;
    double defaultVal = 0.0;
    std::function<juce::String (double)> formatter;
    Options layoutOptions;

    juce::Slider slider;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ForjaKnob)
};

} // namespace forja::ui
