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

DiqueAudioProcessorEditor::DiqueAudioProcessorEditor (DiqueAudioProcessor& p)
    : ForjaEditorBase (
          p, p.apvts,
          { "DIQUE",
            u ("PROTE\u00c7\u00c3O FINAL"),
            u ("CLIPE DE PICO"),
            "AIRWINDOWS FINALCLIP",
            u ("Clipe de pico \u00b7 DSP Airwindows FinalClip"),
            u ("DSP Airwindows FinalClip \u00b7 MIT \u00b7 lat\u00eancia 1 amostra"),
            forja::dique::makeForjaInfo() },
          true),
      processor (p)
{
    configureNote (noteLabel,
                   u ("FinalClip n\u00e3o tem controles e s\u00f3 atua acima de +4,18 dBFS "
                      "(pico 1,618). Abaixo disso o sinal passa intacto, com 1 amostra de "
                      "atraso.\n\n"
                      "Use no fim da cadeia, depois de est\u00e1gios que possam gerar overs. "
                      "N\u00e3o substitui um limitador de 0 dBFS."));

    addAndMakeVisible (clipCard);
    clipCard.addAndMakeVisible (outputMeter);
    clipCard.addAndMakeVisible (noteLabel);
    clipCard.addAndMakeVisible (bypassButton);

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

void DiqueAudioProcessorEditor::layoutContent (juce::Rectangle<int> content)
{
    clipCard.setBounds (content);

    const auto area = clipCard.getContentBounds();

    outputMeter.setBounds (area.getX() + 2, area.getY() - 6, 136, 94);
    noteLabel.setBounds (area.getX() + 152, area.getY() - 6, area.getWidth() - 300, 92);
    bypassButton.setBounds (area.getRight() - 142, area.getBottom() - 26, 142, 26);
}

void DiqueAudioProcessorEditor::updateTelemetry()
{
    const auto snapshot = processor.readMeters();
    const bool bypassed = processor.isBypassed();

    outputMeter.setLevels (snapshot.outPeakL, snapshot.outPeakR);
    outputMeter.setClip (snapshot.outClip);

    const float inDb = juce::jmax (forja::ui::linearToDb (snapshot.inPeakL),
                                   forja::ui::linearToDb (snapshot.inPeakR));

    refreshBadge (snapshot.outClip, bypassed, inDb, snapshot.gainReductionDb);
}

void DiqueAudioProcessorEditor::refreshBadge (bool clipped, bool bypassed, float inputDb,
                                              float grDb)
{
    auto kind = forja::ui::BadgeKind::ready;
    juce::String text ("PRONTO");
    juce::String detail (u ("Prote\u00e7\u00e3o de picos extremos \u00b7 atua acima de +4,18 dBFS."));

    if (clipped)
    {
        kind = forja::ui::BadgeKind::clip;
        text = "CLIP";
        detail = grDb < -0.1f
                     ? u ("Aparando picos acima de +4,18 dBFS \u00b7 ") + juce::String (grDb, 1)
                           + u (" dB de redu\u00e7\u00e3o.")
                     : u ("Pico de sa\u00edda acima de 0 dBFS, mas abaixo do gatilho de +4,18 dBFS.");
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
    else if (grDb < -0.1f)
    {
        detail = u ("Aparando picos acima de +4,18 dBFS \u00b7 ") + juce::String (grDb, 1)
               + u (" dB de redu\u00e7\u00e3o.");
    }
    else
    {
        detail = u ("Transparente at\u00e9 +4,18 dBFS \u00b7 sem picos extremos.");
    }

    setBadge (kind, text, detail);
}
