#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace forja::ui
{

class ForjaTopBar : public juce::Component
{
public:
    explicit ForjaTopBar (bool compactLayout = false);

    void setIdentity (juce::String productName,
                      juce::String familyName,
                      juce::String subtitle,
                      juce::String sourceChip);
    void setBypassed (bool shouldBeBypassed);

    void paint (juce::Graphics& g) override;
    void resized() override;

    std::function<void()> onFundamentos;

private:
    struct ChipLayout
    {
        juce::Rectangle<float> state;
        juce::Rectangle<float> mit;
        juce::Rectangle<float> source;
        juce::Rectangle<float> social;
        juce::Rectangle<float> info;
    };

    ChipLayout getChipLayout() const;
    juce::String getStateText() const;

    bool compact = false;
    juce::String product = "Brasa";
    juce::String family = "DE VALVULAS";
    juce::String subtitle = "SATURADOR DE VALVULA";
    juce::String source = "AIRWINDOWS TUBE2";
    bool bypassed = false;

    juce::TextButton fundamentosButton { "Fundamentos" };
    juce::TextButton redesButton { "Redes" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ForjaTopBar)
};

} // namespace forja::ui
