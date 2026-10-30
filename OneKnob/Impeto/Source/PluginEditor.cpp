#include "PluginEditor.h"

#include <cmath>

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

ImpetoAudioProcessorEditor::ImpetoAudioProcessorEditor (ImpetoAudioProcessor& p)
    : ForjaEditorBase (
          p, p.apvts,
          { u ("\u00cdMPETO"),
            "DE MIX",
            u ("COMPRESS\u00c3O R\u00c1PIDA"),
            "AIRWINDOWS DYNO",
            u ("Din\u00e2mica r\u00e1pida \u00b7 DSP Airwindows Dyno"),
            u ("DSP Airwindows Dyno \u00b7 MIT \u00b7 lat\u00eancia 0 amostras"),
            forja::impeto::makeForjaInfo() },
          true),
      processor (p)
{
    configureReadout (valueLabel, 27.0f, forja::ui::colour::emberSoft);
    configureHint (hintLabel);
    hintLabel.setText (u ("INPUT \u00b7 \u221212 A +12 dB"), juce::dontSendNotification);
    configureNote (noteLabel, u ("A quarta pot\u00eancia do sinal\nvira a curva: picos\nseguram, o corpo fica.\n\u00c9 compress\u00e3o de forma,\nn\u00e3o de envelope."));

    addAndMakeVisible (punchCard);
    punchCard.addAndMakeVisible (inputKnob);
    punchCard.addAndMakeVisible (valueLabel);
    punchCard.addAndMakeVisible (hintLabel);
    punchCard.addAndMakeVisible (noteLabel);
    punchCard.addAndMakeVisible (bypassButton);

    inputAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "input", inputKnob.getSlider());

    bypassButton.setClickingTogglesState (true);
    bypassButton.setTooltip ("Bypass com rampa de 10 ms");
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, "bypass", bypassButton);

    inputKnob.onValueChange = [this] { refreshReadouts(); };

    refreshReadouts();

    setSize (windowWidth, windowHeight);
}

void ImpetoAudioProcessorEditor::layoutContent (juce::Rectangle<int> content)
{
    punchCard.setBounds (content);

    const auto area = punchCard.getContentBounds();

    inputKnob.setBounds (area.getX() + 8, area.getY() - 8, inputKnob.getWidth(),
                         inputKnob.getHeight());

    const int textX = inputKnob.getRight() + 18;
    valueLabel.setBounds (textX, area.getY() - 9, 210, 34);
    hintLabel.setBounds (textX + 1, area.getY() + 25, 210, 14);
    noteLabel.setBounds (textX + 1, area.getY() + 42, 200, 48);

    bypassButton.setBounds (area.getRight() - 142, area.getBottom() - 26, 142, 26);
}

void ImpetoAudioProcessorEditor::updateTelemetry()
{
    const auto snapshot = processor.readMeters();
    const bool bypassed = processor.isBypassed();
    const float inDb = juce::jmax (forja::ui::linearToDb (snapshot.inPeakL),
                                   forja::ui::linearToDb (snapshot.inPeakR));

    refreshBadge (snapshot.outClip, bypassed, inDb);
}

void ImpetoAudioProcessorEditor::refreshReadouts()
{
    const double value = inputKnob.getSlider().getValue();
    const double db = value * 24.0 - 12.0;
    valueLabel.setText (juce::String (db, 1) + " dB", juce::dontSendNotification);

    valueLabel.setColour (juce::Label::textColourId,
                          std::abs (db) > 0.05 ? forja::ui::colour::emberSoft
                                               : juce::Colour (0xff64707d));
}

void ImpetoAudioProcessorEditor::refreshBadge (bool clipped, bool bypassed, float inputDb)
{
    auto kind = forja::ui::BadgeKind::ready;
    juce::String text ("PRONTO");
    juce::String detail (u ("Compress\u00e3o de forma \u00b7 lat\u00eancia 0."));

    const float input = processor.apvts.getRawParameterValue ("input")->load();

    if (clipped)
    {
        kind = forja::ui::BadgeKind::clip;
        text = "CLIP";
        detail = u ("Sa\u00edda acima de 0 dBFS \u00b7 reduza o ganho de entrada.");
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
    else if (std::abs (input - 0.5f) < 0.005f)
    {
        detail = u ("Entrada em 0,0 dB \u00b7 a curva ainda segura os picos.");
    }
    else
    {
        detail = u ("\u00cdmpeto ativo \u00b7 picos seguram, o corpo fica.");
    }

    setBadge (kind, text, detail);
}
