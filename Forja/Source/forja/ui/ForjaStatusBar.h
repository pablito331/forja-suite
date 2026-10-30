#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <utility>
#include <vector>

namespace forja::ui
{

enum class BadgeKind
{
    ready,
    bypass,
    clip,
    silent
};

class ForjaStatusBar : public juce::Component
{
public:
    explicit ForjaStatusBar (bool compactLayout = false);

    void setIdentity (juce::String productName, juce::String subtitleText);
    void setBadge (BadgeKind kind, juce::String text, juce::String detail);
    void setMeta (double sampleRate,
                  int blockSize,
                  std::vector<std::pair<juce::String, juce::String>> values);
    void setFooter (juce::String text);

    void paint (juce::Graphics& g) override;

private:
    bool compact = false;
    juce::String product = "Brasa";
    juce::String subtitle = "Saturador de valvula - DSP Airwindows Tube2";
    BadgeKind badgeKind = BadgeKind::silent;
    juce::String badgeText = "SEM SINAL";
    juce::String badgeDetail;
    juce::String footer;
    double sampleRate = 0.0;
    int bufferSize = 0;
    std::vector<std::pair<juce::String, juce::String>> meta;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ForjaStatusBar)
};

} // namespace forja::ui
