#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>

#include "ForjaInfoContent.h"

namespace forja::ui
{

class ForjaInfoPanel : public juce::Component
{
public:
    explicit ForjaInfoPanel (ForjaInfo infoToShow);
    ~ForjaInfoPanel() override;

    void showPanel();
    void hidePanel();

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& event) override;
    bool keyPressed (const juce::KeyPress& key) override;

    std::function<void()> onClosed;

private:
    class Content;

    ForjaInfo info;
    juce::Viewport viewport;
    std::unique_ptr<Content> content;
    juce::TextButton closeButton { "Fechar" };
    juce::Rectangle<int> sheetBounds;

    void closeFromUser();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ForjaInfoPanel)
};

} // namespace forja::ui
