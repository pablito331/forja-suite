#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>
#include <utility>
#include <vector>

#include "ForjaCanvas.h"
#include "ForjaInfoContent.h"
#include "ForjaInfoPanel.h"
#include "ForjaLookAndFeel.h"
#include "ForjaStatusBar.h"
#include "ForjaTopBar.h"

namespace forja::ui
{

class ForjaEditorBase : public juce::AudioProcessorEditor,
                        private juce::Timer
{
public:
    struct Identity
    {
        juce::String product;
        juce::String family;
        juce::String subtitle;
        juce::String sourceChip;
        juce::String statusSubtitle;
        juce::String footer;
        ForjaInfo info;
    };

    ForjaEditorBase (juce::AudioProcessor& processor,
                     juce::AudioProcessorValueTreeState& state,
                     Identity identity,
                     bool compactLayout = false);
    ~ForjaEditorBase() override;

    ForjaTopBar& getTopBar() noexcept { return topBar; }
    ForjaStatusBar& getStatusBar() noexcept { return statusBar; }
    ForjaInfoPanel& getInfoPanel() noexcept { return *infoPanel; }
    juce::AudioProcessorValueTreeState& getState() noexcept { return apvts; }

    int getPreferredWidth() const noexcept { return compact ? 560 : 1000; }
    int getPreferredHeight() const noexcept { return compact ? 260 : 540; }
    bool isCompact() const noexcept { return compact; }

    void paint (juce::Graphics& g) override;
    void resized() override;

protected:
    virtual void layoutContent (juce::Rectangle<int> content) = 0;
    virtual void updateTelemetry() {}

    void setBadge (BadgeKind kind, juce::String text, juce::String detail);
    void setMetaValues (std::vector<std::pair<juce::String, juce::String>> values);
    void refreshBypassChip();

private:
    void timerCallback() override;

    juce::AudioProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;
    Identity identity;
    bool compact = false;
    bool lastBypass = false;
    double lastSampleRate = 0.0;
    int lastBlockSize = 0;

    ForjaLookAndFeel lookAndFeel;
    juce::ComponentBoundsConstrainer constrainer;
    ForjaTopBar topBar;
    ForjaStatusBar statusBar;
    std::unique_ptr<ForjaInfoPanel> infoPanel;
    std::vector<std::pair<juce::String, juce::String>> metaValues;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ForjaEditorBase)
};

} // namespace forja::ui
