#pragma once

#include <array>
#include <memory>

#include <juce_gui_basics/juce_gui_basics.h>

#include "forja/ui/ForjaEditorBase.h"
#include "rack/RackState.h"
#include "ui/SlotComponent.h"

class ForjarackAudioProcessor;

namespace forjarackui
{
class MasterSection;
} // namespace forjarackui

class ForjarackAudioProcessorEditor final : public forja::ui::ForjaEditorBase
{
public:
    explicit ForjarackAudioProcessorEditor (ForjarackAudioProcessor& processor);
    ~ForjarackAudioProcessorEditor() override;

protected:
    void layoutContent (juce::Rectangle<int> content) override;
    void updateTelemetry() override;

private:
    ForjarackAudioProcessor& processor;

    std::array<std::unique_ptr<forjarackui::SlotComponent>, forjarack::kNumSlots> slots {};
    std::unique_ptr<forjarackui::MasterSection> master;
    std::array<int, forjarack::kNumSlots> lastType {};
    // A carga de um plugin hospedado termina no timer do processor; quando o
    // numero de params aparece, reloidamos a gaveta para desenhar os knobs.
    std::array<int, forjarack::kNumSlots> lastHostedParams {};
    double clipHoldUntilMs = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ForjarackAudioProcessorEditor)
};
