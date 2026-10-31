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

constexpr int windowWidth = 1000;
constexpr int windowHeight = 540;
constexpr int cardHeight = 388;

juce::String modeName (float value)
{
    switch (juce::jlimit (0, 2, (int) (value * 2.999f)))
    {
        case 0: return "BOOST";
        case 1: return "MATCH";
        default: return "CLIP ONLY";
    }
}

void configureNote (juce::Label& label, const juce::String& text)
{
    label.setText (text, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (forja::ui::uiFont (8.0f));
    label.setColour (juce::Label::textColourId, juce::Colour (0xff64707d));
    label.setInterceptsMouseClicks (false, false);
}
} // namespace

MuralhaAudioProcessorEditor::MuralhaAudioProcessorEditor (MuralhaAudioProcessor& p)
    : ForjaEditorBase (
          p, p.apvts,
          { "MURALHA",
            u ("DE LIMITADORES"),
            u ("LIMITADOR ADAPTATIVO"),
            "AIRWINDOWS ADCLIP9",
            u ("Limitador adaptativo \u00b7 DSP Airwindows ADClip9"),
            u ("DSP Airwindows ADClip9 \u00b7 MIT \u00b7 lat\u00eancia 1 amostra"),
            forja::muralha::makeForjaInfo() }),
      processor (p)
{
    configureNote (inputNote,
                   u ("Boost empurra a entrada em at\u00e9 18 dB.\n"
                      "Match devolve o ganho no modo MATCH."));
    configureNote (clipNote,
                   u ("Ceiling define o teto do clipe.\n"
                      "Noise adiciona o dither do original."));
    configureNote (modeNote,
                   u ("Boost: clipe empurrado.\n"
                      "Match: clipe com ganho casado.\n"
                      "Clip Only: s\u00f3 a prote\u00e7\u00e3o de pico."));

    addAndMakeVisible (inputCard);
    addAndMakeVisible (clipCard);
    addAndMakeVisible (modeCard);
    addAndMakeVisible (outCard);

    inputCard.addAndMakeVisible (boostKnob);
    inputCard.addAndMakeVisible (matchKnob);
    inputCard.addAndMakeVisible (inputNote);

    clipCard.addAndMakeVisible (ceilingKnob);
    clipCard.addAndMakeVisible (noiseKnob);
    clipCard.addAndMakeVisible (clipNote);

    modeCard.addAndMakeVisible (modeChoice);
    modeCard.addAndMakeVisible (modeNote);
    modeCard.addAndMakeVisible (gainReductionMeter);

    outCard.addAndMakeVisible (outputMeter);
    outCard.addAndMakeVisible (bypassButton);

    boostAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "boost", boostKnob.getSlider());
    matchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "match", matchKnob.getSlider());
    ceilingAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "ceiling", ceilingKnob.getSlider());
    noiseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "noise", noiseKnob.getSlider());

    modeChoice.getComboBox().onChange = [this]
    {
        if (syncingMode)
            return;

        const int index = modeChoice.getComboBox().getSelectedItemIndex();
        if (index >= 0)
        {
            processor.apvts.getParameter ("mode")->setValueNotifyingHost ((float) index * 0.5f);
            refreshReadouts();
        }
    };

    bypassButton.setClickingTogglesState (true);
    bypassButton.setTooltip ("Bypass com rampa de 10 ms");
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, "bypass", bypassButton);

    outputMeter.onClipReset = [this]
    {
        processor.resetClip();
        outputMeter.setClip (false);
    };

    const auto onKnobMoved = [this] { refreshReadouts(); };
    boostKnob.onValueChange = onKnobMoved;
    matchKnob.onValueChange = onKnobMoved;
    ceilingKnob.onValueChange = onKnobMoved;
    noiseKnob.onValueChange = onKnobMoved;

    syncMode();
    refreshReadouts();

    setSize (windowWidth, windowHeight);
}

void MuralhaAudioProcessorEditor::layoutContent (juce::Rectangle<int> content)
{
    const int y = content.getY();

    inputCard.setBounds (content.getX(), y, 190, cardHeight);
    clipCard.setBounds (content.getX() + 198, y, 236, cardHeight);
    modeCard.setBounds (content.getX() + 442, y, 294, cardHeight);
    outCard.setBounds (content.getX() + 744, y, 236, cardHeight);

    const auto inputArea = inputCard.getContentBounds();

    boostKnob.setBounds (inputArea.getX() + (inputArea.getWidth() - boostKnob.getWidth()) / 2,
                         inputArea.getY() + 6, boostKnob.getWidth(), boostKnob.getHeight());
    matchKnob.setBounds (inputArea.getX() + (inputArea.getWidth() - matchKnob.getWidth()) / 2,
                         boostKnob.getBottom() + 18, matchKnob.getWidth(), matchKnob.getHeight());
    inputNote.setBounds (inputArea.getX(), matchKnob.getBottom() + 10,
                         inputArea.getWidth(), 40);

    const auto clipArea = clipCard.getContentBounds();

    ceilingKnob.setBounds (clipArea.getX() + (clipArea.getWidth() - ceilingKnob.getWidth()) / 2,
                           clipArea.getY() + 10, ceilingKnob.getWidth(), ceilingKnob.getHeight());
    noiseKnob.setBounds (clipArea.getX() + (clipArea.getWidth() - noiseKnob.getWidth()) / 2,
                         ceilingKnob.getBottom() + 16, noiseKnob.getWidth(), noiseKnob.getHeight());
    clipNote.setBounds (clipArea.getX(), noiseKnob.getBottom() + 10,
                        clipArea.getWidth(), 52);

    const auto modeArea = modeCard.getContentBounds();

    modeChoice.setBounds (modeArea.getX(), modeArea.getY() + 2, modeArea.getWidth(), 46);
    modeNote.setBounds (modeArea.getX(), modeChoice.getBottom() + 6, modeArea.getWidth(), 54);
    gainReductionMeter.setBounds (modeArea.getX(), modeNote.getBottom() + 8,
                                  modeArea.getWidth(),
                                  modeArea.getBottom() - modeNote.getBottom() - 8);

    const auto outArea = outCard.getContentBounds();

    bypassButton.setBounds (outArea.getX(), outArea.getBottom() - 26, outArea.getWidth(), 26);
    outputMeter.setBounds (outArea.getX(), outArea.getY() + 8,
                           outArea.getWidth(), bypassButton.getY() - 8 - (outArea.getY() + 8));
}

void MuralhaAudioProcessorEditor::updateTelemetry()
{
    const auto snapshot = processor.readMeters();
    const bool bypassed = processor.isBypassed();

    outputMeter.setLevels (snapshot.outPeakL, snapshot.outPeakR);
    outputMeter.setClip (snapshot.outClip);
    gainReductionMeter.setGainReductionDb (snapshot.gainReductionDb, snapshot.gainReductionDb);
    gainReductionMeter.setBypassed (bypassed);

    const float inDb = juce::jmax (forja::ui::linearToDb (snapshot.inPeakL),
                                   forja::ui::linearToDb (snapshot.inPeakR));

    syncMode();
    refreshBadge (snapshot.outClip, bypassed, inDb, snapshot.gainReductionDb);
}

void MuralhaAudioProcessorEditor::syncMode()
{
    const float mode = processor.apvts.getRawParameterValue ("mode")->load();
    const int index = juce::jlimit (0, 2, (int) (mode * 2.999f));
    auto& combo = modeChoice.getComboBox();

    if (combo.getSelectedItemIndex() == index || combo.isPopupActive())
        return;

    syncingMode = true;
    combo.setSelectedItemIndex (index, juce::dontSendNotification);
    syncingMode = false;
    refreshReadouts();
}

void MuralhaAudioProcessorEditor::refreshReadouts()
{
    const float mode = processor.apvts.getRawParameterValue ("mode")->load();

    setMetaValues ({
        { "boost", juce::String (boostKnob.getSlider().getValue() * 18.0, 1) + " dB" },
        { "match", juce::String (matchKnob.getSlider().getValue() * 18.0, 1) + " dB" },
        { "noise", juce::String (noiseKnob.getSlider().getValue(), 2) },
        { "ceiling", juce::String (ceilingKnob.getSlider().getValue(), 2) },
        { "modo", modeName (mode) }
    });
}

void MuralhaAudioProcessorEditor::refreshBadge (bool clipped, bool bypassed, float inputDb,
                                                float grDb)
{
    auto kind = forja::ui::BadgeKind::ready;
    juce::String text ("PRONTO");
    juce::String detail (u ("Limitador adaptativo \u00b7 lat\u00eancia 1 amostra."));

    if (clipped)
    {
        kind = forja::ui::BadgeKind::clip;
        text = "CLIP";
        detail = u ("Sa\u00edda acima de 0 dBFS \u00b7 reduza o boost ou o ceiling.");
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
        detail = u ("Limitando \u00b7 ") + juce::String (grDb, 1)
               + u (" dB de redu\u00e7\u00e3o de pico.");
    }
    else
    {
        const float mode = processor.apvts.getRawParameterValue ("mode")->load();
        detail = u ("Modo ") + modeName (mode) + u (" \u00b7 sinal abaixo do ceiling.");
    }

    setBadge (kind, text, detail);
}
