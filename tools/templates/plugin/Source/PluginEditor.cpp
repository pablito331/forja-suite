#include "PluginEditor.h"

#include "ForjaInfoText.h"
#include "PluginProcessor.h"
#include "forja/ui/ForjaTheme.h"

namespace
{
juce::String u (const char* text)
{
    return forja::ui::utf8 (text);
}

constexpr int windowWidth = 560;
constexpr int windowHeight = 260;

void configureReadout (juce::Label& label, float height, juce::Colour colour)
{
    label.setJustificationType (juce::Justification::centredLeft);
    label.setFont (forja::ui::displayFont (height));
    label.setColour (juce::Label::textColourId, colour);
    label.setInterceptsMouseClicks (false, false);
}

void configureHint (juce::Label& label)
{
    label.setJustificationType (juce::Justification::centredLeft);
    label.setFont (forja::ui::uiFont (8.5f));
    label.setColour (juce::Label::textColourId, juce::Colour (0xff8e99a6));
    label.setInterceptsMouseClicks (false, false);
}

void configureNote (juce::Label& label, const juce::String& text)
{
    label.setText (text, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::topLeft);
    label.setFont (forja::ui::uiFont (8.0f));
    label.setColour (juce::Label::textColourId, juce::Colour (0xff64707d));
    label.setInterceptsMouseClicks (false, false);
}
} // namespace

@@PREFIX@@AudioProcessorEditor::@@PREFIX@@AudioProcessorEditor (@@PREFIX@@AudioProcessor& p)
    : ForjaEditorBase (
          p, p.apvts,
          { "@@PRODUCT@@",
            "@@FAMILY_LABEL@@",
            "@@SUBTITLE@@",
            "@@AIRWINDOWS@@",
            u ("@@STATUS_SUBTITLE@@"),
            u ("@@FOOTER@@"),
            forja::@@SLUG@@::makeForjaInfo() },
          true),
      processor (p)
{
    configureReadout (valueLabel, 27.0f, forja::ui::colour::emberSoft);
    configureHint (hintLabel);
    hintLabel.setText ("@@AIRWINDOWS@@", juce::dontSendNotification);
    configureNote (noteLabel, u ("Substitua esta nota pelo papel\nmusical do plugin. O painel\nFUNDAMENTOS traz o texto\ncompleto."));

    addAndMakeVisible (mainCard);
    mainCard.addAndMakeVisible (amountKnob);
    mainCard.addAndMakeVisible (valueLabel);
    mainCard.addAndMakeVisible (hintLabel);
    mainCard.addAndMakeVisible (noteLabel);
    mainCard.addAndMakeVisible (bypassButton);

    amountAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "amount", amountKnob.getSlider());

    bypassButton.setClickingTogglesState (true);
    bypassButton.setTooltip ("Bypass com rampa de 10 ms");
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, "bypass", bypassButton);

    amountKnob.onValueChange = [this] { refreshReadouts(); };

    refreshReadouts();

    setSize (windowWidth, windowHeight);
}

void @@PREFIX@@AudioProcessorEditor::layoutContent (juce::Rectangle<int> content)
{
    mainCard.setBounds (content);

    const auto area = mainCard.getContentBounds();

    amountKnob.setBounds (area.getX() + 8, area.getY() - 8, amountKnob.getWidth(),
                          amountKnob.getHeight());

    const int textX = amountKnob.getRight() + 18;
    valueLabel.setBounds (textX, area.getY() - 9, 200, 34);
    hintLabel.setBounds (textX + 1, area.getY() + 25, 200, 14);
    noteLabel.setBounds (textX + 1, area.getY() + 42, 200, 48);

    bypassButton.setBounds (area.getRight() - 142, area.getBottom() - 26, 142, 26);
}

void @@PREFIX@@AudioProcessorEditor::updateTelemetry()
{
    const auto snapshot = processor.readMeters();
    const bool bypassed = processor.isBypassed();
    const float inDb = juce::jmax (forja::ui::linearToDb (snapshot.inPeakL),
                                   forja::ui::linearToDb (snapshot.inPeakR));

    refreshBadge (snapshot.outClip, bypassed, inDb);
}

void @@PREFIX@@AudioProcessorEditor::refreshReadouts()
{
    const double value = amountKnob.getSlider().getValue();
    valueLabel.setText (juce::String (value * 100.0, 0) + " %", juce::dontSendNotification);

    valueLabel.setColour (juce::Label::textColourId,
                          value > 0.005 ? forja::ui::colour::emberSoft
                                        : juce::Colour (0xff64707d));
}

void @@PREFIX@@AudioProcessorEditor::refreshBadge (bool clipped, bool bypassed, float inputDb)
{
    auto kind = forja::ui::BadgeKind::ready;
    juce::String text ("PRONTO");
    juce::String detail (u ("Lat\u00eancia 0 amostras."));

    if (clipped)
    {
        kind = forja::ui::BadgeKind::clip;
        text = "CLIP";
        detail = u ("Sa\u00edda acima de 0 dBFS.");
    }
    else if (bypassed)
    {
        kind = forja::ui::BadgeKind::bypass;
        text = "BYPASS";
        detail = u ("Bypass ativo \u00b7 sinal seco com rampa de 10 ms.");
    }
    else if (inputDb <= -60.0f)
    {
        kind = forja::ui::BadgeKind::silent;
        text = "SEM SINAL";
        detail = u ("Sem sinal na entrada \u00b7 aguardando \u00e1udio da DAW.");
    }

    setBadge (kind, text, detail);
}
