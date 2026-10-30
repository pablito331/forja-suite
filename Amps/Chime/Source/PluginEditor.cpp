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

ChimeAudioProcessorEditor::ChimeAudioProcessorEditor (ChimeAudioProcessor& p)
    : ForjaEditorBase (
          p, p.apvts,
          { "CHIME",
            u ("DE AMP"),
            "AMP CHIME",
            "AIRWINDOWS CHIMEYGUITAR2",
            u ("Amp chime \u00b7 DSP Airwindows ChimeyGuitar2"),
            u ("DSP Airwindows ChimeyGuitar2 \u00b7 MIT \u00b7 lat\u00eancia 0 amostras"),
            forja::chime::makeForjaInfo() }),
      processor (p)
{
    configureNote (powerNote, u ("Compress\u00e3o do amp;\nvelocidade define qu\u00e3o\nr\u00e1pido ele responde."));
    configureNote (toneNote, u ("Presen\u00e7a, agudos e m\u00e9dios\ndo pr\u00e9 amplificador."));
    configureNote (speakerNote, u ("Graves e sub do amp;\nHSpeakr/LSpeakr s\u00e3o\no gabinete."));
    configureNote (signalNote, u ("Sa\u00edda do amp.\nO limitador de seno\nfecha o caminho."));

    addAndMakeVisible (powerCard);
    addAndMakeVisible (toneCard);
    addAndMakeVisible (speakerCard);
    addAndMakeVisible (signalCard);

    powerCard.addAndMakeVisible (compresKnob);
    powerCard.addAndMakeVisible (speedKnob);
    powerCard.addAndMakeVisible (outputKnob);
    powerCard.addAndMakeVisible (powerNote);

    toneCard.addAndMakeVisible (presenceKnob);
    toneCard.addAndMakeVisible (highKnob);
    toneCard.addAndMakeVisible (midKnob);
    toneCard.addAndMakeVisible (toneNote);

    speakerCard.addAndMakeVisible (lowKnob);
    speakerCard.addAndMakeVisible (subKnob);
    speakerCard.addAndMakeVisible (hspeakerKnob);
    speakerCard.addAndMakeVisible (lspeakerKnob);
    speakerCard.addAndMakeVisible (speakerNote);

    signalCard.addAndMakeVisible (outputMeter);
    signalCard.addAndMakeVisible (signalNote);
    signalCard.addAndMakeVisible (bypassButton);

    compresAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "compres", compresKnob.getSlider());
    speedAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "speed", speedKnob.getSlider());
    outputAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "output", outputKnob.getSlider());
    presenceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "presence", presenceKnob.getSlider());
    highAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "high", highKnob.getSlider());
    midAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "mid", midKnob.getSlider());
    lowAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "low", lowKnob.getSlider());
    subAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "sub", subKnob.getSlider());
    hspeakerAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "hspeaker", hspeakerKnob.getSlider());
    lspeakerAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "lspeaker", lspeakerKnob.getSlider());

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
    compresKnob.onValueChange = onKnobMoved;
    speedKnob.onValueChange = onKnobMoved;
    outputKnob.onValueChange = onKnobMoved;
    presenceKnob.onValueChange = onKnobMoved;
    highKnob.onValueChange = onKnobMoved;
    midKnob.onValueChange = onKnobMoved;
    lowKnob.onValueChange = onKnobMoved;
    subKnob.onValueChange = onKnobMoved;
    hspeakerKnob.onValueChange = onKnobMoved;
    lspeakerKnob.onValueChange = onKnobMoved;

    refreshReadouts();

    setSize (windowWidth, windowHeight);
}

void ChimeAudioProcessorEditor::layoutContent (juce::Rectangle<int> content)
{
    const int y = content.getY();

    powerCard.setBounds (content.getX(), y, 190, cardHeight);
    toneCard.setBounds (content.getX() + 198, y, 236, cardHeight);
    speakerCard.setBounds (content.getX() + 442, y, 294, cardHeight);
    signalCard.setBounds (content.getX() + 744, y, 236, cardHeight);

    const auto powerArea = powerCard.getContentBounds();
    const int powerX = powerArea.getX() + (powerArea.getWidth() - compresKnob.getWidth()) / 2;
    compresKnob.setBounds (powerX, powerArea.getY() + 2, compresKnob.getWidth(), compresKnob.getHeight());
    speedKnob.setBounds (powerX, powerArea.getY() + 97, speedKnob.getWidth(), speedKnob.getHeight());
    outputKnob.setBounds (powerX, powerArea.getY() + 192, outputKnob.getWidth(), outputKnob.getHeight());
    powerNote.setBounds (powerArea.getX(), powerArea.getBottom() - 38, powerArea.getWidth(), 36);

    const auto toneArea = toneCard.getContentBounds();
    const int toneX = toneArea.getX() + (toneArea.getWidth() - presenceKnob.getWidth()) / 2;
    presenceKnob.setBounds (toneX, toneArea.getY() + 2, presenceKnob.getWidth(), presenceKnob.getHeight());
    highKnob.setBounds (toneX, toneArea.getY() + 105, highKnob.getWidth(), highKnob.getHeight());
    midKnob.setBounds (toneX, toneArea.getY() + 208, midKnob.getWidth(), midKnob.getHeight());
    toneNote.setBounds (toneArea.getX(), toneArea.getBottom() - 24, toneArea.getWidth(), 22);

    const auto speakerArea = speakerCard.getContentBounds();
    const int speakerLeft = speakerArea.getX() + 6;
    const int speakerRight = speakerLeft + lowKnob.getWidth() + 8;
    lowKnob.setBounds (speakerLeft, speakerArea.getY() + 2, lowKnob.getWidth(), lowKnob.getHeight());
    subKnob.setBounds (speakerRight, speakerArea.getY() + 2, subKnob.getWidth(), subKnob.getHeight());
    hspeakerKnob.setBounds (speakerLeft, speakerArea.getY() + 122, hspeakerKnob.getWidth(), hspeakerKnob.getHeight());
    lspeakerKnob.setBounds (speakerRight, speakerArea.getY() + 122, lspeakerKnob.getWidth(), lspeakerKnob.getHeight());
    speakerNote.setBounds (speakerArea.getX(), speakerArea.getY() + 250, speakerArea.getWidth(), 60);

    const auto signalArea = signalCard.getContentBounds();
    outputMeter.setBounds (signalArea.getX() + 6, signalArea.getY() + 6, signalArea.getWidth() - 12, 150);
    signalNote.setBounds (signalArea.getX(), signalArea.getY() + 168, signalArea.getWidth(), 60);
    bypassButton.setBounds (signalArea.getX(), signalArea.getBottom() - 26,
                            signalArea.getWidth(), 26);
}

void ChimeAudioProcessorEditor::updateTelemetry()
{
    const auto snapshot = processor.readMeters();
    const bool bypassed = processor.isBypassed();

    outputMeter.setLevels (snapshot.outPeakL, snapshot.outPeakR);
    outputMeter.setClip (snapshot.outClip);

    const float inDb = juce::jmax (forja::ui::linearToDb (snapshot.inPeakL),
                                   forja::ui::linearToDb (snapshot.inPeakR));

    refreshBadge (snapshot.outClip, bypassed, inDb);
}

void ChimeAudioProcessorEditor::refreshReadouts()
{
    setMetaValues ({
        { u ("compress\u00e3o"), juce::String (compresKnob.getSlider().getValue(), 2) },
        { "velocidade", juce::String (speedKnob.getSlider().getValue(), 2) },
        { u ("sa\u00edda"), juce::String (outputKnob.getSlider().getValue(), 2) }
    });
}

void ChimeAudioProcessorEditor::refreshBadge (bool clipped, bool bypassed, float inputDb)
{
    auto kind = forja::ui::BadgeKind::ready;
    juce::String text ("PRONTO");
    juce::String detail (u ("Amp chime \u00b7 lat\u00eancia 0."));

    if (clipped)
    {
        kind = forja::ui::BadgeKind::clip;
        text = "CLIP";
        detail = u ("Sa\u00edda acima de 0 dBFS \u00b7 reduza a compress\u00e3o ou a sa\u00edda.");
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
    else if (compresKnob.getSlider().getValue() < 0.15)
    {
        detail = u ("Amp quase limpo \u00b7 a compress\u00e3o ainda n\u00e3o entrou.");
    }
    else
    {
        detail = u ("Amp comprimindo \u00b7 o chime abre com o ataque.");
    }

    setBadge (kind, text, detail);
}
