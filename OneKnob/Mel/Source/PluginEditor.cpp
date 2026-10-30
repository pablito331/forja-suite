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

MelAudioProcessorEditor::MelAudioProcessorEditor (MelAudioProcessor& p)
    : ForjaEditorBase (
          p, p.apvts,
          { "MEL",
            "DE MIX",
            u ("BRILHO SUAVE"),
            "AIRWINDOWS SWEETEN",
            u ("Brilho \u00b7 DSP Airwindows Sweeten"),
            u ("DSP Airwindows Sweeten \u00b7 MIT \u00b7 lat\u00eancia 0 amostras"),
            forja::mel::makeForjaInfo() },
          true),
      processor (p)
{
    configureReadout (valueLabel, 27.0f, forja::ui::colour::emberSoft);
    configureHint (hintLabel);
    hintLabel.setText ("SWEETEN \u00b7 0 A 100 %", juce::dontSendNotification);
    configureNote (noteLabel, u ("Dobra o sinal ao quadrado e\nsubtrai uma m\u00e9dia curta:\nos picos arredondam e o\ntopo fica macio, sem chiado."));

    addAndMakeVisible (sweetCard);
    sweetCard.addAndMakeVisible (sweetenKnob);
    sweetCard.addAndMakeVisible (valueLabel);
    sweetCard.addAndMakeVisible (hintLabel);
    sweetCard.addAndMakeVisible (noteLabel);
    sweetCard.addAndMakeVisible (bypassButton);

    sweetenAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "sweeten", sweetenKnob.getSlider());

    bypassButton.setClickingTogglesState (true);
    bypassButton.setTooltip ("Bypass com rampa de 10 ms");
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, "bypass", bypassButton);

    sweetenKnob.onValueChange = [this] { refreshReadouts(); };

    refreshReadouts();

    setSize (windowWidth, windowHeight);
}

void MelAudioProcessorEditor::layoutContent (juce::Rectangle<int> content)
{
    sweetCard.setBounds (content);

    const auto area = sweetCard.getContentBounds();

    sweetenKnob.setBounds (area.getX() + 8, area.getY() - 8, sweetenKnob.getWidth(),
                           sweetenKnob.getHeight());

    const int textX = sweetenKnob.getRight() + 18;
    valueLabel.setBounds (textX, area.getY() - 9, 200, 34);
    hintLabel.setBounds (textX + 1, area.getY() + 25, 200, 14);
    noteLabel.setBounds (textX + 1, area.getY() + 42, 200, 48);

    bypassButton.setBounds (area.getRight() - 142, area.getBottom() - 26, 142, 26);
}

void MelAudioProcessorEditor::updateTelemetry()
{
    const auto snapshot = processor.readMeters();
    const bool bypassed = processor.isBypassed();
    const float inDb = juce::jmax (forja::ui::linearToDb (snapshot.inPeakL),
                                   forja::ui::linearToDb (snapshot.inPeakR));

    refreshBadge (snapshot.outClip, bypassed, inDb);
}

void MelAudioProcessorEditor::refreshReadouts()
{
    const double value = sweetenKnob.getSlider().getValue();
    valueLabel.setText (juce::String (value * 100.0, 0) + " %", juce::dontSendNotification);

    valueLabel.setColour (juce::Label::textColourId,
                          value > 0.005 ? forja::ui::colour::emberSoft
                                        : juce::Colour (0xff64707d));
}

void MelAudioProcessorEditor::refreshBadge (bool clipped, bool bypassed, float inputDb)
{
    auto kind = forja::ui::BadgeKind::ready;
    juce::String text ("PRONTO");
    juce::String detail (u ("Suaviza\u00e7\u00e3o por m\u00e9dia \u00b7 lat\u00eancia 0."));

    const float sweeten = processor.apvts.getRawParameterValue ("sweeten")->load();

    if (clipped)
    {
        kind = forja::ui::BadgeKind::clip;
        text = "CLIP";
        detail = u ("Sa\u00edda acima de 0 dBFS \u00b7 reduza o brilho.");
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
    else if (sweeten < 0.005f)
    {
        detail = u ("Brilho em 0 \u00b7 o sinal passa quase intacto.");
    }
    else
    {
        detail = u ("Brilho ativo \u00b7 arredondando os picos.");
    }

    setBadge (kind, text, detail);
}
