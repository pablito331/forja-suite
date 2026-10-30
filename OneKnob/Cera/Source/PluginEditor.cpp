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

CeraAudioProcessorEditor::CeraAudioProcessorEditor (CeraAudioProcessor& p)
    : ForjaEditorBase (
          p, p.apvts,
          { "CERA",
            "DE MIX",
            u ("CALOR ASSIM\u00c9TRICO"),
            "AIRWINDOWS PURESTWARM3",
            u ("Calor \u00b7 DSP Airwindows PurestWarm3"),
            u ("DSP Airwindows PurestWarm3 \u00b7 MIT \u00b7 lat\u00eancia 0 amostras"),
            forja::cera::makeForjaInfo() },
          true),
      processor (p)
{
    configureReadout (valueLabel, 27.0f, forja::ui::colour::emberSoft);
    configureHint (hintLabel);
    hintLabel.setText ("ASYM \u00b7 0 A 100 %", juce::dontSendNotification);
    configureNote (noteLabel, u ("Curva assim\u00e9trica somada por\nderivadas. Em 0,50 \u00e9\nneutro; abaixo e acima o\ncalor muda de lado."));

    addAndMakeVisible (warmCard);
    warmCard.addAndMakeVisible (warmthKnob);
    warmCard.addAndMakeVisible (valueLabel);
    warmCard.addAndMakeVisible (hintLabel);
    warmCard.addAndMakeVisible (noteLabel);
    warmCard.addAndMakeVisible (bypassButton);

    warmthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "warmth", warmthKnob.getSlider());

    bypassButton.setClickingTogglesState (true);
    bypassButton.setTooltip ("Bypass com rampa de 10 ms");
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, "bypass", bypassButton);

    warmthKnob.onValueChange = [this] { refreshReadouts(); };

    refreshReadouts();

    setSize (windowWidth, windowHeight);
}

void CeraAudioProcessorEditor::layoutContent (juce::Rectangle<int> content)
{
    warmCard.setBounds (content);

    const auto area = warmCard.getContentBounds();

    warmthKnob.setBounds (area.getX() + 8, area.getY() - 8, warmthKnob.getWidth(),
                          warmthKnob.getHeight());

    const int textX = warmthKnob.getRight() + 18;
    valueLabel.setBounds (textX, area.getY() - 9, 200, 34);
    hintLabel.setBounds (textX + 1, area.getY() + 25, 200, 14);
    noteLabel.setBounds (textX + 1, area.getY() + 42, 200, 48);

    bypassButton.setBounds (area.getRight() - 142, area.getBottom() - 26, 142, 26);
}

void CeraAudioProcessorEditor::updateTelemetry()
{
    const auto snapshot = processor.readMeters();
    const bool bypassed = processor.isBypassed();
    const float inDb = juce::jmax (forja::ui::linearToDb (snapshot.inPeakL),
                                   forja::ui::linearToDb (snapshot.inPeakR));

    refreshBadge (snapshot.outClip, bypassed, inDb);
}

void CeraAudioProcessorEditor::refreshReadouts()
{
    const double value = warmthKnob.getSlider().getValue();
    valueLabel.setText (juce::String (value * 100.0, 0) + " %", juce::dontSendNotification);

    valueLabel.setColour (juce::Label::textColourId,
                          std::abs (value - 0.5) > 0.005 ? forja::ui::colour::emberSoft
                                                         : juce::Colour (0xff64707d));
}

void CeraAudioProcessorEditor::refreshBadge (bool clipped, bool bypassed, float inputDb)
{
    auto kind = forja::ui::BadgeKind::ready;
    juce::String text ("PRONTO");
    juce::String detail (u ("Curva assim\u00e9trica \u00b7 lat\u00eancia 0."));

    const float warmth = processor.apvts.getRawParameterValue ("warmth")->load();

    if (clipped)
    {
        kind = forja::ui::BadgeKind::clip;
        text = "CLIP";
        detail = u ("Sa\u00edda acima de 0 dBFS \u00b7 reduza o calor.");
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
    else if (std::abs (warmth - 0.5f) < 0.005f)
    {
        detail = u ("Calor em 0,50 \u00b7 ponto neutro do original.");
    }
    else
    {
        detail = u ("Calor ativo \u00b7 somando derivadas assim\u00e9tricas.");
    }

    setBadge (kind, text, detail);
}
