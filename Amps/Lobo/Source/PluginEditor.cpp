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

void configureNote (juce::Label& label, const juce::String& text)
{
    label.setText (text, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::topLeft);
    label.setFont (forja::ui::uiFont (8.0f));
    label.setColour (juce::Label::textColourId, juce::Colour (0xff64707d));
    label.setInterceptsMouseClicks (false, false);
}
} // namespace

LoboAudioProcessorEditor::LoboAudioProcessorEditor (LoboAudioProcessor& p)
    : ForjaEditorBase (
          p, p.apvts,
          { "LOBO",
            u ("DE AMP"),
            "TRANSFORMADOR DI",
            "AIRWINDOWS WOLFBOT",
            u ("DI de transformador \u00b7 DSP Airwindows Wolfbot"),
            u ("DSP Airwindows Wolfbot \u00b7 MIT \u00b7 lat\u00eancia 0 amostras"),
            forja::lobo::makeForjaInfo() },
          true),
      processor (p)
{
    configureNote (noteLabel,
                   u ("Wolfbot n\u00e3o tem controles: ele condiciona a entrada com dois "
                      "preditores Kalman em s\u00e9rie, um passa-altos e um passa-baixos, "
                      "como um DI de transformador.\n\n"
                      "Coloque na entrada de guitarra ou baixo e siga com o amp. O "
                      "BYPASS compara o sinal seco."));

    addAndMakeVisible (diCard);
    diCard.addAndMakeVisible (outputMeter);
    diCard.addAndMakeVisible (noteLabel);
    diCard.addAndMakeVisible (bypassButton);

    bypassButton.setClickingTogglesState (true);
    bypassButton.setTooltip ("Bypass com rampa de 10 ms");
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, "bypass", bypassButton);

    outputMeter.onClipReset = [this]
    {
        processor.resetClip();
        outputMeter.setClip (false);
    };

    setSize (windowWidth, windowHeight);
}

void LoboAudioProcessorEditor::layoutContent (juce::Rectangle<int> content)
{
    diCard.setBounds (content);

    const auto area = diCard.getContentBounds();

    outputMeter.setBounds (area.getX() + 2, area.getY() - 6, 136, 94);
    noteLabel.setBounds (area.getX() + 152, area.getY() - 6, area.getWidth() - 300, 92);
    bypassButton.setBounds (area.getRight() - 142, area.getBottom() - 26, 142, 26);
}

void LoboAudioProcessorEditor::updateTelemetry()
{
    const auto snapshot = processor.readMeters();
    const bool bypassed = processor.isBypassed();

    outputMeter.setLevels (snapshot.outPeakL, snapshot.outPeakR);
    outputMeter.setClip (snapshot.outClip);

    const float inDb = juce::jmax (forja::ui::linearToDb (snapshot.inPeakL),
                                   forja::ui::linearToDb (snapshot.inPeakR));

    refreshBadge (snapshot.outClip, bypassed, inDb);
}

void LoboAudioProcessorEditor::refreshBadge (bool clipped, bool bypassed, float inputDb)
{
    auto kind = forja::ui::BadgeKind::ready;
    juce::String text ("PRONTO");
    juce::String detail (u ("DI de transformador \u00b7 lat\u00eancia 0."));

    if (clipped)
    {
        kind = forja::ui::BadgeKind::clip;
        text = "CLIP";
        detail = u ("Sa\u00edda acima de 0 dBFS \u00b7 reduza o n\u00edvel na entrada.");
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
    else
    {
        detail = u ("DI ativo \u00b7 preditores Kalman condicionando a entrada.");
    }

    setBadge (kind, text, detail);
}
