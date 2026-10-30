#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace forja::ui
{

class ForjaChoice : public juce::Component
{
public:
    ForjaChoice (juce::String captionText, juce::StringArray items);

    juce::ComboBox& getComboBox() noexcept { return combo; }

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    juce::String caption;
    juce::ComboBox combo;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ForjaChoice)
};

} // namespace forja::ui
