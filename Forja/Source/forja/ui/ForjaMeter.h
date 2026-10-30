#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace forja::ui
{

class ForjaMeter : public juce::Component
{
public:
    ForjaMeter();

    // Recebe peaks lineares acumulados desde o último tick da UI.
    void setLevels (float peakLeft, float peakRight);
    void setClip (bool isClipped);

    void paint (juce::Graphics& g) override;
    void mouseUp (const juce::MouseEvent& event) override;
    void mouseMove (const juce::MouseEvent& event) override;

    std::function<void()> onClipReset;

private:
    struct Channel
    {
        float db = -100.0f;
        float hold = -100.0f;
        double holdUntilMs = 0.0;
    };

    void updateChannel (Channel& channel, float linearPeak);
    juce::Rectangle<int> getClipBounds() const;
    float railNorm (float db) const;

    Channel left;
    Channel right;
    bool clipped = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ForjaMeter)
};

} // namespace forja::ui
