#include "PluginEditor.h"

#include "ForjaInfoText.h"
#include "PluginProcessor.h"
#include "minisat/MiniSat.h"

namespace
{
juce::String u (const char* text)
{
    return forja::ui::utf8 (text);
}

constexpr int windowWidth = 560;
constexpr int windowHeight = 260;
} // namespace

MinisatAudioProcessorEditor::MinisatAudioProcessorEditor (MinisatAudioProcessor& p)
    : ForjaEditorBase (
          p, p.apvts,
          { "MINISAT",
            u ("DE V\u00c1LVULAS"),
            "SATURADOR DE 1 KNOB",
            "AIRWINDOWS TUBE2",
            u ("V\u00e1lvula do BRASA \u00b7 um knob"),
            u ("DSP Airwindows Tube2 \u00b7 camada Forja no drive \u00b7 lat\u00eancia 0 amostras"),
            forja::minisat::makeForjaInfo() },
          true),
      processor (p),
      curveCard (1, u ("Curva de transfer\u00eancia"), true),
      outCard (2, u ("Sa\u00edda"), true),
      driveKnob ("DRIVE",
                 u ("pad de entrada do Tube2"),
                 64.0f,
                 0.0, 1.0, 0.01, 0.0,
                 [] (double value) { return juce::String (value, 2); },
                 forja::ui::ForjaKnob::Options { 120, true, true, true })
{
    addAndMakeVisible (curveCard);
    curveCard.addAndMakeVisible (curve);

    addAndMakeVisible (outCard);
    outCard.addAndMakeVisible (driveKnob);
    outCard.addAndMakeVisible (outputMeter);
    outCard.addAndMakeVisible (bypassButton);

    driveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "drive", driveKnob.getSlider());

    bypassButton.setClickingTogglesState (true);
    bypassButton.setTooltip (u ("Bypass com rampa de 10 ms"));
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, "bypass", bypassButton);

    driveKnob.onValueChange = [this] { refreshCurve(); };

    const auto resetClips = [this]
    {
        processor.resetClip();
        outputMeter.setClip (false);
    };

    outputMeter.onClipReset = resetClips;

    refreshCurve();

    setSize (windowWidth, windowHeight);
}

MinisatAudioProcessorEditor::~MinisatAudioProcessorEditor()
{
}

void MinisatAudioProcessorEditor::layoutContent (juce::Rectangle<int> content)
{
    const auto curva = content.withWidth (content.getWidth() * 58 / 100);
    const auto saida = content.withTrimmedLeft (curva.getWidth() + 6);

    curveCard.setBounds (curva);
    outCard.setBounds (saida);

    // A curva ocupa o cartao quase inteiro: em compacto o header consome 34 px
    // e o ForjaTransferCurve ainda reserva 18 px de legenda, entao o plotting
    // comeca abaixo do header e vai ate o rodape do cartao.
    const auto cardArea = curveCard.getLocalBounds();
    curve.setBounds (9, 40, cardArea.getWidth() - 18, cardArea.getHeight() - 49);

    const auto outArea = outCard.getContentBounds();

    driveKnob.setBounds (outArea.getX() - 4, outArea.getY() - 10, 120, 96);
    outputMeter.setBounds (driveKnob.getRight() + 4, outArea.getY() - 6,
                           outArea.getRight() - driveKnob.getRight() - 148, 94);
    bypassButton.setBounds (outArea.getRight() - 142, outArea.getBottom() - 26, 142, 26);
}

void MinisatAudioProcessorEditor::refreshCurve()
{
    // O cartao mostra a curva real do caminho: os valores mapeados pelo
    // wrapper, nao o knob cru.
    const auto parameters = forja::minisat::MiniSat::parametersForDrive (
        (float) driveKnob.getSlider().getValue(), processor.isBypassed());

    curve.setState (parameters.input, parameters.tube, processor.isBypassed());
}

void MinisatAudioProcessorEditor::updateTelemetry()
{
    const auto snapshot = processor.readMeters();

    outputMeter.setLevels (snapshot.outPeakL, snapshot.outPeakR);
    outputMeter.setClip (snapshot.outClip);

    refreshCurve();

    const auto drive = processor.apvts.getRawParameterValue ("drive")->load();

    setMetaValues ({ { "DRIVE", juce::String (drive, 2) },
                     { u ("ENTRADA"), juce::String (forja::minisat::MiniSat::parametersForDrive (drive, false).input, 2) } });

    refreshBadge (snapshot.inClip || snapshot.outClip, processor.isBypassed(), snapshot.inPeakL);
}

void MinisatAudioProcessorEditor::refreshBadge (bool clipped, bool bypassed, float inPeak)
{
    auto kind = forja::ui::BadgeKind::ready;
    juce::String text ("PRONTO");
    juce::String detail;

    if (clipped)
    {
        kind = forja::ui::BadgeKind::clip;
        text = "CLIP";
        detail = u ("Sa\u00edda acima de 0 dBFS \u00b7 baixe o DRIVE.");
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
        detail = u ("V\u00e1lvula do BRASA \u00b7 o DRIVE mapeia o pad de entrada.");
    }

    setBadge (kind, text, detail);
}
