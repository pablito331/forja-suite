#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>

namespace forja::ui
{

class ForjaGRHistory : public juce::Component
{
public:
    ForjaGRHistory();

    void pushGainReductionDb (float leftDb, float rightDb);
    void setBypassed (bool shouldBeBypassed);

    void paint (juce::Graphics& g) override;

private:
    static constexpr int capacity = 180;

    std::array<float, capacity> left {};
    std::array<float, capacity> right {};
    bool bypassed = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ForjaGRHistory)
};

} // namespace forja::ui
