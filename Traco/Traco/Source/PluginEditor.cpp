#include "PluginEditor.h"

#include "ForjaInfoText.h"
#include "PluginProcessor.h"

namespace
{
juce::String u (const char* text)
{
    return forja::ui::utf8 (text);
}

constexpr int windowWidth = 1000;
constexpr int windowHeight = 540;
} // namespace

TracoAudioProcessorEditor::TracoAudioProcessorEditor (TracoAudioProcessor& p)
    : ForjaEditorBase (
          p, p.apvts,
          { "TRACO",
            u ("EQ GR\u00c1FICO"),
            u ("PARAM\u00c9TRICO DE BANCADA"),
            "DSP ORIGINAL FORJA",
            u ("5 bandas + de-esser \u00b7 lat\u00eancia 0"),
            u ("DSP Forja sobre o Audio EQ Cookbook \u00b7 RTA de entrada e sa\u00edda \u00b7 lat\u00eancia 0 amostras"),
            forja::traco::makeForjaInfo() }),
      processor (p)
{
    addAndMakeVisible (bypassButton);

    bypassButton.setClickingTogglesState (true);
    bypassButton.setTooltip (u ("Bypass com rampa de 10 ms"));
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, "bypass", bypassButton);

    setSize (windowWidth, windowHeight);
}

TracoAudioProcessorEditor::~TracoAudioProcessorEditor()
{
}

void TracoAudioProcessorEditor::layoutContent (juce::Rectangle<int> content)
{
    // Fatia 3 desenha aqui o grafico e o painel contextual da banda.
    bypassButton.setBounds (content.getRight() - 120, content.getBottom() - 30, 110, 26);
}

void TracoAudioProcessorEditor::updateTelemetry()
{
    const auto snapshot = processor.readMeters();

    setMetaValues ({ { u ("ENTRADA"), juce::String (snapshot.inPeakL, 3) },
                     { u ("SA\u00cdDA"), juce::String (snapshot.outPeakL, 3) },
                     { u ("DE-ESSER"), juce::String (snapshot.essGainReductionDb, 1) + " dB" } });

    refreshBadge (snapshot.inClip || snapshot.outClip, processor.isBypassed(), snapshot.inPeakL);
}

void TracoAudioProcessorEditor::refreshBadge (bool clipped, bool bypassed, float inPeak)
{
    auto kind = forja::ui::BadgeKind::ready;
    juce::String text ("PRONTO");
    juce::String detail;

    if (clipped)
    {
        kind = forja::ui::BadgeKind::clip;
        text = "CLIP";
        detail = u ("Saida acima de 0 dBFS \u00b7 reduza o ganho das bandas.");
    }
    else if (bypassed)
    {
        kind = forja::ui::BadgeKind::bypass;
        text = "BYPASS";
        detail = u ("Bypass ativo \u00b7 sinal seco com rampa de 10 ms.");
    }
    else if (inPeak <= 0.001f)
    {
        kind = forja::ui::BadgeKind::silent;
        text = "SEM SINAL";
        detail = u ("Sem sinal na entrada \u00b7 aguardando \u00e1udio da DAW.");
    }
    else
    {
        detail = u ("Clique numa banda para abrir os controles dela.");
    }

    setBadge (kind, text, detail);
}
