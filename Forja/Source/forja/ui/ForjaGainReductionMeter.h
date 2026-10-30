#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace forja::ui
{

class ForjaGainReductionMeter : public juce::Component
{
public:
    ForjaGainReductionMeter() = default;

    // Valores em dB, normalmente entre 0 e -24.
    void setGainReductionDb (float leftDb, float rightDb);
    void setBypassed (bool shouldBeBypassed);

    void paint (juce::Graphics& g) override;

private:
    struct Channel
    {
        float db = 0.0f;
        float hold = 0.0f;
        double holdUntilMs = 0.0;
    };

    void updateChannel (Channel& channel, float db);
    float railNorm (float db) const;

    Channel left;
    Channel right;
    bool bypassed = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ForjaGainReductionMeter)
};

} // namespace forja::ui
