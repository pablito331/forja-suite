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

LixaAudioProcessorEditor::LixaAudioProcessorEditor (LixaAudioProcessor& p)
    : ForjaEditorBase (
          p, p.apvts,
          { "LIXA",
            "DE MIX",
            "DRIVE LIMPO",
            "AIRWINDOWS PURESTDRIVE",
            u ("Drive limpo \u00b7 DSP Airwindows PurestDrive"),
            u ("DSP Airwindows PurestDrive \u00b7 MIT \u00b7 lat\u00eancia 0 amostras"),
            forja::lixa::makeForjaInfo() },
          true),
      processor (p)
{
    configureReadout (valueLabel, 27.0f, forja::ui::colour::emberSoft);
    configureHint (hintLabel);
    hintLabel.setText ("DRIVE \u00b7 PURESTDRIVE", juce::dontSendNotification);
    configureNote (noteLabel, u ("Uma seta de seno suaviza a\nonda. Em 0,00 o sinal passa\nintacto; subindo, o drive\nlimpa os picos sem chiado."));

    addAndMakeVisible (driveCard);
    driveCard.addAndMakeVisible (driveKnob);
    driveCard.addAndMakeVisible (valueLabel);
    driveCard.addAndMakeVisible (hintLabel);
    driveCard.addAndMakeVisible (noteLabel);
    driveCard.addAndMakeVisible (bypassButton);

    driveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "drive", driveKnob.getSlider());

    bypassButton.setClickingTogglesState (true);
    bypassButton.setTooltip ("Bypass com rampa de 10 ms");
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, "bypass", bypassButton);

    driveKnob.onValueChange = [this] { refreshReadouts(); };

    refreshReadouts();

    setSize (windowWidth, windowHeight);
}

void LixaAudioProcessorEditor::layoutContent (juce::Rectangle<int> content)
{
    driveCard.setBounds (content);

    const auto area = driveCard.getContentBounds();

    driveKnob.setBounds (area.getX() + 8, area.getY() - 8, driveKnob.getWidth(),
                         driveKnob.getHeight());

    const int textX = driveKnob.getRight() + 18;
    valueLabel.setBounds (textX, area.getY() - 9, 200, 34);
    hintLabel.setBounds (textX + 1, area.getY() + 25, 200, 14);
    noteLabel.setBounds (textX + 1, area.getY() + 42, 200, 48);

    bypassButton.setBounds (area.getRight() - 142, area.getBottom() - 26, 142, 26);
}

void LixaAudioProcessorEditor::updateTelemetry()
{
    const auto snapshot = processor.readMeters();
    const bool bypassed = processor.isBypassed();
    const float inDb = juce::jmax (forja::ui::linearToDb (snapshot.inPeakL),
                                   forja::ui::linearToDb (snapshot.inPeakR));

    refreshBadge (snapshot.outClip, bypassed, inDb);
}

void LixaAudioProcessorEditor::refreshReadouts()
{
    const double value = driveKnob.getSlider().getValue();
    valueLabel.setText (juce::String (value * 100.0, 0) + " %", juce::dontSendNotification);

    const auto& colour = forja::ui::colour::emberSoft;
    valueLabel.setColour (juce::Label::textColourId,
                          value > 0.005 ? colour : juce::Colour (0xff64707d));
}

void LixaAudioProcessorEditor::refreshBadge (bool clipped, bool bypassed, float inputDb)
{
    auto kind = forja::ui::BadgeKind::ready;
    juce::String text ("PRONTO");
    juce::String detail (u ("Drive de seno \u00b7 lat\u00eancia 0."));

    const float drive = processor.apvts.getRawParameterValue ("drive")->load();

    if (clipped)
    {
        kind = forja::ui::BadgeKind::clip;
        text = "CLIP";
        detail = u ("Sa\u00edda acima de 0 dBFS \u00b7 reduza o drive.");
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
    else if (drive < 0.005f)
    {
        detail = u ("Drive em 0 \u00b7 o sinal passa intacto.");
    }
    else
    {
        detail = u ("Drive ativo \u00b7 suavizando os picos.");
    }

    setBadge (kind, text, detail);
}
