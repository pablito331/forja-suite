#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace forja::ui
{

class ForjaLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ForjaLookAndFeel();

    void drawRotarySlider (juce::Graphics& g,
                           int x, int y, int width, int height,
                           float sliderPos,
                           float rotaryStartAngle,
                           float rotaryEndAngle,
                           juce::Slider& slider) override;

    void drawButtonBackground (juce::Graphics& g,
                               juce::Button& button,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override;

    void drawButtonText (juce::Graphics& g,
                         juce::TextButton& button,
                         bool shouldDrawButtonAsHighlighted,
                         bool shouldDrawButtonAsDown) override;

    juce::Font getComboBoxFont (juce::ComboBox&) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ForjaLookAndFeel)
};

} // namespace forja::ui
