#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>

#include "engine/DrumEngine.h"
#include "forja/ui/ForjaEditorBase.h"

class DrumforgeAudioProcessor;

namespace drumforgeui
{
class KitPanel;
class MixerPanel;
class LoopsPanel;
} // namespace drumforgeui

class DrumforgeAudioProcessorEditor final : public forja::ui::ForjaEditorBase
{
public:
    explicit DrumforgeAudioProcessorEditor (DrumforgeAudioProcessor& processor);
    ~DrumforgeAudioProcessorEditor() override;

protected:
    void layoutContent (juce::Rectangle<int> content) override;
    void updateTelemetry() override;

private:
    void setActiveTab (int tabIndex);
    void refreshKit();

    DrumforgeAudioProcessor& processor;

    juce::TextButton kitsTab { "KITS" };
    juce::TextButton mixerTab { "MIXER" };
    juce::TextButton loopsTab { "LOOPS MIDI" };
    std::unique_ptr<drumforgeui::KitPanel> kitPanel;
    std::unique_ptr<drumforgeui::MixerPanel> mixerPanel;
    std::unique_ptr<drumforgeui::LoopsPanel> loopsPanel;

    int activeTab = 0;
    int lastKitRevision = -1;
    double clipHoldUntilMs = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumforgeAudioProcessorEditor)
};
