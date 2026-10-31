#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace forja::ui
{

class ForjaTransferCurve : public juce::Component
{
public:
    ForjaTransferCurve() = default;

    void setState (float inputPad, float tubeDrive, bool isBypassed);
    void paint (juce::Graphics& g) override;

private:
    float input = 0.5f;
    float tube = 0.5f;
    bool bypassed = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ForjaTransferCurve)
};

} // namespace forja::ui
