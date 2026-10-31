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

void configureNote (juce::Label& label, const juce::String& text)
{
    label.setText (text, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (forja::ui::uiFont (8.0f));
    label.setColour (juce::Label::textColourId, juce::Colour (0xff64707d));
    label.setInterceptsMouseClicks (false, false);
}
} // namespace

BigornaAudioProcessorEditor::BigornaAudioProcessorEditor (BigornaAudioProcessor& p)
    : ForjaEditorBase (
          p, p.apvts,
          { "BIGORNA",
            u ("DE CHANNEL STRIPS"),
            "STRIP COMPLETO",
            "AIRWINDOWS CSTRIP2",
            u ("Channel strip \u00b7 DSP Airwindows CStrip2"),
            u ("DSP Airwindows CStrip2 \u00b7 MIT \u00b7 lat\u00eancia 0 amostras"),
            forja::bigorna::makeForjaInfo() }),
      processor (p)
{
    configureNote (neutralNote, u ("0,50 nos tr\u00eas = EQ neutro."));
    configureNote (trebFreqNote, u ("Split de agudos\ndo EQ de tr\u00eas bandas."));
    configureNote (bassFreqNote, u ("Split de graves\ndo EQ de tr\u00eas bandas."));
    configureNote (lowCapNote, u ("Corta agudos.\n1,00 = aberto."));
    configureNote (hiCapNote, u ("Corta graves.\n0,00 = aberto."));

    addAndMakeVisible (tomCard);
    addAndMakeVisible (freqCard);
    addAndMakeVisible (filterCard);
    addAndMakeVisible (compCard);

    tomCard.addAndMakeVisible (trebleKnob);
    tomCard.addAndMakeVisible (midKnob);
    tomCard.addAndMakeVisible (bassKnob);
    tomCard.addAndMakeVisible (neutralNote);

    freqCard.addAndMakeVisible (trebFreqKnob);
    freqCard.addAndMakeVisible (bassFreqKnob);
    freqCard.addAndMakeVisible (trebFreqNote);
    freqCard.addAndMakeVisible (bassFreqNote);

    filterCard.addAndMakeVisible (lowCapKnob);
    filterCard.addAndMakeVisible (hiCapKnob);
    filterCard.addAndMakeVisible (lowCapNote);
    filterCard.addAndMakeVisible (hiCapNote);
    filterCard.addAndMakeVisible (outputKnob);
    filterCard.addAndMakeVisible (outputMeter);

    compCard.addAndMakeVisible (compresKnob);
    compCard.addAndMakeVisible (compSpdKnob);
    compCard.addAndMakeVisible (gainReductionMeter);
    compCard.addAndMakeVisible (bypassButton);

    trebleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "treble", trebleKnob.getSlider());
    midAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "mid", midKnob.getSlider());
    bassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "bass", bassKnob.getSlider());
    trebFreqAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "trebFreq", trebFreqKnob.getSlider());
    bassFreqAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "bassFreq", bassFreqKnob.getSlider());
    lowCapAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "lowCap", lowCapKnob.getSlider());
    hiCapAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "hiCap", hiCapKnob.getSlider());
    compresAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "compres", compresKnob.getSlider());
    compSpdAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "compSpd", compSpdKnob.getSlider());
    outputAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "output", outputKnob.getSlider());

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

    trebleKnob.onValueChange = onKnobMoved;
    midKnob.onValueChange = onKnobMoved;
    bassKnob.onValueChange = onKnobMoved;
    trebFreqKnob.onValueChange = onKnobMoved;
    bassFreqKnob.onValueChange = onKnobMoved;
    lowCapKnob.onValueChange = onKnobMoved;
    hiCapKnob.onValueChange = onKnobMoved;
    compresKnob.onValueChange = onKnobMoved;
    compSpdKnob.onValueChange = onKnobMoved;
    outputKnob.onValueChange = onKnobMoved;

    refreshReadouts();

    setSize (windowWidth, windowHeight);
}

void BigornaAudioProcessorEditor::layoutContent (juce::Rectangle<int> content)
{
    const int y = content.getY();

    tomCard.setBounds (content.getX(), y, 190, cardHeight);
    freqCard.setBounds (content.getX() + 198, y, 236, cardHeight);
    filterCard.setBounds (content.getX() + 442, y, 294, cardHeight);
    compCard.setBounds (content.getX() + 744, y, 236, cardHeight);

    const auto tomArea = tomCard.getContentBounds();
    const int tomKnobWidth = trebleKnob.getWidth();
    const int tomKnobHeight = trebleKnob.getHeight();
    const int tomX = tomArea.getX() + (tomArea.getWidth() - tomKnobWidth) / 2;

    trebleKnob.setBounds (tomX, tomArea.getY() + 4, tomKnobWidth, tomKnobHeight);
    midKnob.setBounds (tomX, tomArea.getY() + 103, tomKnobWidth, tomKnobHeight);
    bassKnob.setBounds (tomX, tomArea.getY() + 202, tomKnobWidth, tomKnobHeight);
    neutralNote.setBounds (tomArea.getX(), tomArea.getBottom() - 30, tomArea.getWidth(), 28);

    const auto freqArea = freqCard.getContentBounds();
    const int freqKnobWidth = trebFreqKnob.getWidth();
    const int freqKnobHeight = trebFreqKnob.getHeight();
    const int freqX = freqArea.getX() + (freqArea.getWidth() - freqKnobWidth) / 2;

    trebFreqKnob.setBounds (freqX, freqArea.getY() + 10, freqKnobWidth, freqKnobHeight);
    bassFreqKnob.setBounds (freqX, freqArea.getY() + 139, freqKnobWidth, freqKnobHeight);
    trebFreqNote.setBounds (freqArea.getX(), freqArea.getY() + 264, freqArea.getWidth(), 28);
    bassFreqNote.setBounds (freqArea.getX(), freqArea.getY() + 294, freqArea.getWidth(), 28);

    const auto filterArea = filterCard.getContentBounds();
    const int capWidth = lowCapKnob.getWidth();

    lowCapKnob.setBounds (filterArea.getX(), filterArea.getY() + 8, capWidth,
                          lowCapKnob.getHeight());
    hiCapKnob.setBounds (filterArea.getX() + capWidth, filterArea.getY() + 8, capWidth,
                         hiCapKnob.getHeight());
    lowCapNote.setBounds (filterArea.getX(), filterArea.getY() + 131, capWidth, 26);
    hiCapNote.setBounds (filterArea.getX() + capWidth, filterArea.getY() + 131, capWidth, 26);

    outputKnob.setBounds (filterArea.getX() + 8, filterArea.getY() + 172,
                          outputKnob.getWidth(), outputKnob.getHeight());
    outputMeter.setBounds (filterArea.getX() + 146, filterArea.getY() + 172, 130,
                           outputKnob.getHeight());

    const auto compArea = compCard.getContentBounds();
    const int compKnobWidth = compresKnob.getWidth();
    constexpr int compGap = 10;
    const int compX = compArea.getX()
                    + (compArea.getWidth() - (compKnobWidth * 2 + compGap)) / 2;
    const int bypassTop = compArea.getBottom() - 26;
    const int grTop = compArea.getY() + compresKnob.getHeight() + 16;

    compresKnob.setBounds (compX, compArea.getY() + 4, compKnobWidth,
                           compresKnob.getHeight());
    compSpdKnob.setBounds (compX + compKnobWidth + compGap, compArea.getY() + 4,
                           compKnobWidth, compSpdKnob.getHeight());
    gainReductionMeter.setBounds (compArea.getX(), grTop, compArea.getWidth(),
                                  bypassTop - 8 - grTop);
    bypassButton.setBounds (compArea.getX(), bypassTop, compArea.getWidth(), 26);
}

void BigornaAudioProcessorEditor::updateTelemetry()
{
    const auto snapshot = processor.readMeters();
    const bool bypassed = processor.isBypassed();

    outputMeter.setLevels (snapshot.outPeakL, snapshot.outPeakR);
    outputMeter.setClip (snapshot.outClip);
    gainReductionMeter.setGainReductionDb (snapshot.gainReductionDb, snapshot.gainReductionDb);
    gainReductionMeter.setBypassed (bypassed);

    const float inDb = juce::jmax (forja::ui::linearToDb (snapshot.inPeakL),
                                   forja::ui::linearToDb (snapshot.inPeakR));

    refreshBadge (snapshot.outClip, bypassed, inDb, snapshot.gainReductionDb);
}

void BigornaAudioProcessorEditor::refreshReadouts()
{
    setMetaValues ({
        { "agudos", juce::String (trebleKnob.getSlider().getValue(), 2) },
        { u ("m\u00e9dios"), juce::String (midKnob.getSlider().getValue(), 2) },
        { "graves", juce::String (bassKnob.getSlider().getValue(), 2) },
        { u ("compress\u00e3o"), juce::String (compresKnob.getSlider().getValue(), 2) },
        { u ("sa\u00edda"), juce::String (outputKnob.getSlider().getValue(), 2) }
    });
}

void BigornaAudioProcessorEditor::refreshBadge (bool clipped, bool bypassed, float inputDb,
                                                float reductionDb)
{
    auto kind = forja::ui::BadgeKind::ready;
    juce::String text ("PRONTO");
    juce::String detail (u ("EQ, filtros, compressor e Spiral em um s\u00f3 strip."));

    if (clipped)
    {
        kind = forja::ui::BadgeKind::clip;
        text = "CLIP";
        detail = u ("Sa\u00edda acima de 0 dBFS \u00b7 reduza o ganho de sa\u00edda.");
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
    else if (reductionDb < -0.1f)
    {
        detail = u ("Comprimindo \u00b7 ") + juce::String (reductionDb, 1)
               + u (" dB de redu\u00e7\u00e3o de ganho.");
    }
    else if (compresKnob.getSlider().getValue() < 0.005)
    {
        detail = u ("Compressor desligado \u00b7 s\u00f3 EQ, filtros e sa\u00edda.");
    }

    setBadge (kind, text, detail);
}
