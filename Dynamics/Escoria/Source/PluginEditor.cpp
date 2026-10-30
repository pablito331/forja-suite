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
constexpr int wideCardWidth = 386;
constexpr int narrowCardWidth = 292;
constexpr int cardGap = 8;

void configureNote (juce::Label& label, const juce::String& text)
{
    label.setText (text, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (forja::ui::uiFont (8.0f));
    label.setColour (juce::Label::textColourId, juce::Colour (0xff64707d));
    label.setInterceptsMouseClicks (false, false);
}
} // namespace

EscoriaAudioProcessorEditor::EscoriaAudioProcessorEditor (EscoriaAudioProcessor& p)
    : ForjaEditorBase (
          p, p.apvts,
          { u ("ESC\u00d3RIA"),
            u ("DE DIN\u00c2MICA"),
            u ("DE-ESSER DIN\u00c2MICO"),
            "AIRWINDOWS DEBESS",
            u ("De-esser \u00b7 DSP Airwindows DeBess"),
            u ("DSP Airwindows DeBess \u00b7 MIT \u00b7 lat\u00eancia 0 amostras"),
            forja::escoria::makeForjaInfo() }),
      processor (p)
{
    configureNote (sharpNote, u ("Janela do detector\nde 2 a 40 amostras."));
    configureNote (depthNote, u ("Fundo da redu\u00e7\u00e3o\n0 = m\u00e1ximo, 1 = nenhum."));
    configureNote (filterNote, u ("Ponto de separa\u00e7\u00e3o\ndos agudos processados."));
    configureNote (senseNote, u ("Acima de 0,50 a sa\u00edda\ntoca s\u00f3 o removido."));

    addAndMakeVisible (detectorCard);
    addAndMakeVisible (timeCard);
    addAndMakeVisible (focusCard);

    detectorCard.addAndMakeVisible (intenseKnob);
    detectorCard.addAndMakeVisible (outputMeter);
    detectorCard.addAndMakeVisible (bypassButton);

    timeCard.addAndMakeVisible (sharpKnob);
    timeCard.addAndMakeVisible (depthKnob);
    timeCard.addAndMakeVisible (sharpNote);
    timeCard.addAndMakeVisible (depthNote);

    focusCard.addAndMakeVisible (filterKnob);
    focusCard.addAndMakeVisible (senseKnob);
    focusCard.addAndMakeVisible (filterNote);
    focusCard.addAndMakeVisible (senseNote);

    intenseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "intense", intenseKnob.getSlider());
    sharpAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "sharp", sharpKnob.getSlider());
    depthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "depth", depthKnob.getSlider());
    filterAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "filter", filterKnob.getSlider());
    senseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "sense", senseKnob.getSlider());

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

    intenseKnob.onValueChange = onKnobMoved;
    sharpKnob.onValueChange = onKnobMoved;
    depthKnob.onValueChange = onKnobMoved;
    filterKnob.onValueChange = onKnobMoved;
    senseKnob.onValueChange = onKnobMoved;

    refreshReadouts();

    setSize (windowWidth, windowHeight);
}

void EscoriaAudioProcessorEditor::layoutContent (juce::Rectangle<int> content)
{
    const int y = content.getY();

    detectorCard.setBounds (content.getX(), y, wideCardWidth, cardHeight);
    timeCard.setBounds (content.getX() + wideCardWidth + cardGap, y, narrowCardWidth, cardHeight);
    focusCard.setBounds (content.getX() + wideCardWidth + narrowCardWidth + cardGap * 2, y,
                         narrowCardWidth, cardHeight);

    const auto detectorArea = detectorCard.getContentBounds();
    const int knobWidth = intenseKnob.getWidth();
    const int knobHeight = intenseKnob.getHeight();

    intenseKnob.setBounds (detectorArea.getX() + (detectorArea.getWidth() - knobWidth) / 2,
                           detectorArea.getY() + 10, knobWidth, knobHeight);

    const int meterTop = detectorArea.getY() + knobHeight + 26;
    outputMeter.setBounds (detectorArea.getX(), meterTop, detectorArea.getWidth(),
                           detectorArea.getBottom() - 34 - meterTop);
    bypassButton.setBounds (detectorArea.getX(), detectorArea.getBottom() - 26,
                            detectorArea.getWidth(), 26);

    const auto placePair = [] (juce::Rectangle<int> area,
                               forja::ui::ForjaKnob& first,
                               forja::ui::ForjaKnob& second)
    {
        const int half = area.getWidth() / 2;
        const int width = first.getWidth();
        const int height = first.getHeight();

        first.setBounds (area.getX() + (half - width) / 2, area.getY() + 14, width, height);
        second.setBounds (area.getX() + half + (half - width) / 2, area.getY() + 14,
                          width, height);
    };

    const auto timeArea = timeCard.getContentBounds();
    placePair (timeArea, sharpKnob, depthKnob);

    const auto focusArea = focusCard.getContentBounds();
    placePair (focusArea, filterKnob, senseKnob);

    const int noteTop = timeArea.getY() + sharpKnob.getHeight() + 40;
    const int halfWidth = timeArea.getWidth() / 2;

    sharpNote.setBounds (timeArea.getX(), noteTop, halfWidth, 52);
    depthNote.setBounds (timeArea.getX() + halfWidth, noteTop, halfWidth, 52);
    filterNote.setBounds (focusArea.getX(), noteTop, halfWidth, 52);
    senseNote.setBounds (focusArea.getX() + halfWidth, noteTop, halfWidth, 52);
}

void EscoriaAudioProcessorEditor::updateTelemetry()
{
    const auto snapshot = processor.readMeters();
    const bool bypassed = processor.isBypassed();

    outputMeter.setLevels (snapshot.outPeakL, snapshot.outPeakR);
    outputMeter.setClip (snapshot.outClip);

    const float inDb = juce::jmax (forja::ui::linearToDb (snapshot.inPeakL),
                                   forja::ui::linearToDb (snapshot.inPeakR));

    refreshBadge (snapshot.outClip, bypassed, inDb);
}

void EscoriaAudioProcessorEditor::refreshReadouts()
{
    setMetaValues ({
        { "intensidade", juce::String (intenseKnob.getSlider().getValue(), 2) },
        { "velocidade", juce::String (sharpKnob.getSlider().getValue(), 2) },
        { u ("profundidade"), juce::String (depthKnob.getSlider().getValue(), 2) }
    });
}

void EscoriaAudioProcessorEditor::refreshBadge (bool clipped, bool bypassed, float inputDb)
{
    auto kind = forja::ui::BadgeKind::ready;
    juce::String text ("PRONTO");
    juce::String detail (u ("Detector de transientes agudos \u00b7 lat\u00eancia 0."));

    const float intense = processor.apvts.getRawParameterValue ("intense")->load();
    const float sense = processor.apvts.getRawParameterValue ("sense")->load();

    if (clipped)
    {
        kind = forja::ui::BadgeKind::clip;
        text = "CLIP";
        detail = u ("Sa\u00edda acima de 0 dBFS \u00b7 reduza a intensidade.");
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
    else if (intense < 0.005f)
    {
        detail = u ("Intensidade em 0 \u00b7 o sinal passa intacto.");
    }
    else if (sense > 0.5f)
    {
        detail = u ("Monitor ligado \u00b7 ouvindo s\u00f3 o que foi removido.");
    }
    else
    {
        detail = u ("De-esser ativo \u00b7 reduzindo picos de agudo.");
    }

    setBadge (kind, text, detail);
}
