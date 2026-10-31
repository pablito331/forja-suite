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

PontaAudioProcessorEditor::PontaAudioProcessorEditor (PontaAudioProcessor& p)
    : ForjaEditorBase (
          p, p.apvts,
          { "PONTA",
            u ("DE AMP"),
            "AMP DE GUITARRA",
            "AIRWINDOWS POINTYGUITAR",
            u ("Amp de guitarra \u00b7 DSP Airwindows PointyGuitar"),
            u ("DSP Airwindows PointyGuitar \u00b7 MIT \u00b7 lat\u00eancia 0 amostras"),
            forja::ponta::makeForjaInfo() }),
      processor (p)
{
    configureNote (powerNote, u ("Drive abre a distor\u00e7\u00e3o;\nGate limpa o ru\u00eddo entre\nfrases."));
    configureNote (toneNote, u ("Presen\u00e7a, agudos e m\u00e9dios\ndo pr\u00e9 amplificador."));
    configureNote (speakerNote, u ("Graves e sub do amp;\nHSpeakr/LSpeakr s\u00e3o\no gabinete."));
    configureNote (signalNote, u ("Sa\u00edda do amp.\nO gate \u00e9 o \u00fanico\ncontrole din\u00e2mico."));

    addAndMakeVisible (powerCard);
    addAndMakeVisible (toneCard);
    addAndMakeVisible (speakerCard);
    addAndMakeVisible (signalCard);

    powerCard.addAndMakeVisible (driveKnob);
    powerCard.addAndMakeVisible (gateKnob);
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

    driveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "drive", driveKnob.getSlider());
    gateAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "gate", gateKnob.getSlider());
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
    driveKnob.onValueChange = onKnobMoved;
    gateKnob.onValueChange = onKnobMoved;
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

void PontaAudioProcessorEditor::layoutContent (juce::Rectangle<int> content)
{
    const int y = content.getY();

    powerCard.setBounds (content.getX(), y, 190, cardHeight);
    toneCard.setBounds (content.getX() + 198, y, 236, cardHeight);
    speakerCard.setBounds (content.getX() + 442, y, 294, cardHeight);
    signalCard.setBounds (content.getX() + 744, y, 236, cardHeight);

    const auto powerArea = powerCard.getContentBounds();
    const int powerX = powerArea.getX() + (powerArea.getWidth() - driveKnob.getWidth()) / 2;
    driveKnob.setBounds (powerX, powerArea.getY() + 2, driveKnob.getWidth(), driveKnob.getHeight());
    gateKnob.setBounds (powerX, powerArea.getY() + 97, gateKnob.getWidth(), gateKnob.getHeight());
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

void PontaAudioProcessorEditor::updateTelemetry()
{
    const auto snapshot = processor.readMeters();
    const bool bypassed = processor.isBypassed();

    outputMeter.setLevels (snapshot.outPeakL, snapshot.outPeakR);
    outputMeter.setClip (snapshot.outClip);

    const float inDb = juce::jmax (forja::ui::linearToDb (snapshot.inPeakL),
                                   forja::ui::linearToDb (snapshot.inPeakR));

    refreshBadge (snapshot.outClip, bypassed, inDb);
}

void PontaAudioProcessorEditor::refreshReadouts()
{
    setMetaValues ({
        { "drive", juce::String (driveKnob.getSlider().getValue(), 2) },
        { "gate", juce::String (gateKnob.getSlider().getValue(), 2) },
        { u ("sa\u00edda"), juce::String (outputKnob.getSlider().getValue(), 2) }
    });
}

void PontaAudioProcessorEditor::refreshBadge (bool clipped, bool bypassed, float inputDb)
{
    auto kind = forja::ui::BadgeKind::ready;
    juce::String text ("PRONTO");
    juce::String detail (u ("Amp de guitarra \u00b7 lat\u00eancia 0."));

    if (clipped)
    {
        kind = forja::ui::BadgeKind::clip;
        text = "CLIP";
        detail = u ("Sa\u00edda acima de 0 dBFS \u00b7 reduza o drive ou a sa\u00edda.");
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
    else if (driveKnob.getSlider().getValue() < 0.35)
    {
        detail = u ("Amp limpo \u00b7 o drive ainda n\u00e3o abriu a distor\u00e7\u00e3o.");
    }
    else
    {
        detail = u ("Amp distorcendo \u00b7 gate e alto-falante moldando o resultado.");
    }

    setBadge (kind, text, detail);
}
