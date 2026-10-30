#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace forja::ui
{

// Arte decorativa de máquina de fita para os plugins de fita.
class ForjaTapeArt : public juce::Component
{
public:
    enum class Style { reels, cassette };

    explicit ForjaTapeArt (Style styleToUse);

    void paint (juce::Graphics& g) override;

private:
    void paintReels (juce::Graphics& g, juce::Rectangle<float> area);
    void paintCassette (juce::Graphics& g, juce::Rectangle<float> area);

    Style style;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ForjaTapeArt)
};

} // namespace forja::ui
