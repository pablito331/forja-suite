#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace forja::ui
{

class ForjaValve : public juce::Component
{
public:
    ForjaValve() = default;

    void setHeat (float newHeat);
    void paint (juce::Graphics& g) override;

private:
    float heat = 0.06f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ForjaValve)
};

} // namespace forja::ui
