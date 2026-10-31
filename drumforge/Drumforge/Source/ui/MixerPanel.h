#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <memory>

#include "engine/DrumEngine.h"
#include "forja/ui/ForjaCard.h"

class DrumforgeAudioProcessor;

namespace drumforgeui
{

class MixerStrip;
class MasterStrip;

// Mesa do Drumforge: 16 canais rolando na horizontal mais o master fixo.
class MixerPanel : public juce::Component
{
public:
    explicit MixerPanel (DrumforgeAudioProcessor& processor);
    ~MixerPanel() override;

    void refreshNames();

    void setMeters (const std::array<forja::drumforge::DrumEngine::PadMeter,
                                     forja::drumforge::kNumPads>& padMeters,
                    const forja::drumforge::DrumEngine::MasterMeter& masterMeter);

    void resized() override;

private:
    DrumforgeAudioProcessor& processor;

    forja::ui::ForjaCard card { 1, "MIXER", true };
    juce::Viewport viewport;
    juce::Component stripHolder;
    std::array<std::unique_ptr<MixerStrip>, forja::drumforge::kNumPads> strips;
    std::unique_ptr<MasterStrip> master;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixerPanel)
};

} // namespace drumforgeui
