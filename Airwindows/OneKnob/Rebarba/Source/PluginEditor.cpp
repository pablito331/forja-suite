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

RebarbaAudioProcessorEditor::RebarbaAudioProcessorEditor (RebarbaAudioProcessor& p)
    : ForjaEditorBase (
          p, p.apvts,
          { "REBARBA",
            "DE MIX",
            u ("SUAVIZA\u00c7\u00c3O DE SLEW"),
            "AIRWINDOWS SLEW3",
            u ("Slew \u00b7 DSP Airwindows Slew3"),
            u ("DSP Airwindows Slew3 \u00b7 MIT \u00b7 lat\u00eancia 0 amostras"),
            forja::rebarba::makeForjaInfo() },
          true),
      processor (p)
{
    configureReadout (valueLabel, 27.0f, forja::ui::colour::emberSoft);
    configureHint (hintLabel);
    hintLabel.setText ("CLAMPING \u00b7 0 A 100 %", juce::dontSendNotification);
    configureNote (noteLabel, u ("Limita o passo entre\nsamples: a onda perde\narestas e o topo fica\nmais liso. Em 0,00 passa\npraticamente intacto."));

    addAndMakeVisible (smoothCard);
    smoothCard.addAndMakeVisible (slewKnob);
    smoothCard.addAndMakeVisible (valueLabel);
    smoothCard.addAndMakeVisible (hintLabel);
    smoothCard.addAndMakeVisible (noteLabel);
    smoothCard.addAndMakeVisible (bypassButton);

    slewAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "slew", slewKnob.getSlider());

    bypassButton.setClickingTogglesState (true);
    bypassButton.setTooltip ("Bypass com rampa de 10 ms");
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, "bypass", bypassButton);

    slewKnob.onValueChange = [this] { refreshReadouts(); };

    refreshReadouts();

    setSize (windowWidth, windowHeight);
}

void RebarbaAudioProcessorEditor::layoutContent (juce::Rectangle<int> content)
{
    smoothCard.setBounds (content);

    const auto area = smoothCard.getContentBounds();

    slewKnob.setBounds (area.getX() + 8, area.getY() - 8, slewKnob.getWidth(),
                        slewKnob.getHeight());

    const int textX = slewKnob.getRight() + 18;
    valueLabel.setBounds (textX, area.getY() - 9, 200, 34);
    hintLabel.setBounds (textX + 1, area.getY() + 25, 200, 14);
    noteLabel.setBounds (textX + 1, area.getY() + 42, 200, 48);

    bypassButton.setBounds (area.getRight() - 142, area.getBottom() - 26, 142, 26);
}

void RebarbaAudioProcessorEditor::updateTelemetry()
{
    const auto snapshot = processor.readMeters();
    const bool bypassed = processor.isBypassed();
    const float inDb = juce::jmax (forja::ui::linearToDb (snapshot.inPeakL),
                                   forja::ui::linearToDb (snapshot.inPeakR));

    refreshBadge (snapshot.outClip, bypassed, inDb);
}

void RebarbaAudioProcessorEditor::refreshReadouts()
{
    const double value = slewKnob.getSlider().getValue();
    valueLabel.setText (juce::String (value * 100.0, 0) + " %", juce::dontSendNotification);

    valueLabel.setColour (juce::Label::textColourId,
                          value > 0.005 ? forja::ui::colour::emberSoft
                                        : juce::Colour (0xff64707d));
}

void RebarbaAudioProcessorEditor::refreshBadge (bool clipped, bool bypassed, float inputDb)
{
    auto kind = forja::ui::BadgeKind::ready;
    juce::String text ("PRONTO");
    juce::String detail (u ("Limite de passo \u00b7 lat\u00eancia 0."));

    const float slew = processor.apvts.getRawParameterValue ("slew")->load();

    if (clipped)
    {
        kind = forja::ui::BadgeKind::clip;
        text = "CLIP";
        detail = u ("Sa\u00edda acima de 0 dBFS \u00b7 reduza a suavidade.");
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
    else if (slew < 0.005f)
    {
        detail = u ("Suavidade em 0 \u00b7 o sinal passa praticamente intacto.");
    }
    else
    {
        detail = u ("Suavidade ativa \u00b7 arredondando as arestas da onda.");
    }

    setBadge (kind, text, detail);
}
