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

CrickAudioProcessorEditor::CrickAudioProcessorEditor (CrickAudioProcessor& p)
    : ForjaEditorBase (
          p, p.apvts,
          { "CRICK",
            u ("DE AMP"),
            "BAIXO DE DOIS CAPTADORES",
            "AIRWINDOWS CRICKBASS",
            u ("Amp de baixo \u00b7 DSP Airwindows CrickBass"),
            u ("DSP Airwindows CrickBass \u00b7 MIT \u00b7 lat\u00eancia 1 a 5 amostras por taxa"),
            forja::crick::makeForjaInfo() }),
      processor (p)
{
    configureNote (driveNote, u ("Ganho dos seis est\u00e1gios\nde satura\u00e7\u00e3o do baixo.\n"
                                  "0,50 = corpo cheio."));
    configureNote (toneNote, u ("Tom dos dois captadores:\n0,00 = escuro e grave;\n"
                                 "1,00 = aberto e brilhante."));
    configureNote (signalNote, u ("A sa\u00edda passa por ClipOnly.\nPDC: 1 amostra em\n"
                                  "44,1/48 kHz; 3 e 5 acima."));

    addAndMakeVisible (driveCard);
    addAndMakeVisible (toneCard);
    addAndMakeVisible (signalCard);

    driveCard.addAndMakeVisible (driveKnob);
    driveCard.addAndMakeVisible (driveNote);

    toneCard.addAndMakeVisible (toneKnob);
    toneCard.addAndMakeVisible (toneNote);

    signalCard.addAndMakeVisible (outputMeter);
    signalCard.addAndMakeVisible (signalNote);
    signalCard.addAndMakeVisible (bypassButton);

    driveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "drive", driveKnob.getSlider());
    toneAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "tone", toneKnob.getSlider());

    bypassButton.setClickingTogglesState (true);
    bypassButton.setTooltip ("Bypass com rampa de 10 ms");
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, "bypass", bypassButton);

    outputMeter.onClipReset = [this]
    {
        processor.resetClip();
        outputMeter.setClip (false);
    };

    driveKnob.onValueChange = [this] { refreshReadouts(); };
    toneKnob.onValueChange = [this] { refreshReadouts(); };

    refreshReadouts();

    setSize (windowWidth, windowHeight);
}

void CrickAudioProcessorEditor::layoutContent (juce::Rectangle<int> content)
{
    const int y = content.getY();

    driveCard.setBounds (content.getX(), y, 294, cardHeight);
    toneCard.setBounds (content.getX() + 302, y, 294, cardHeight);
    signalCard.setBounds (content.getX() + 604, y, 374, cardHeight);

    const auto driveArea = driveCard.getContentBounds();
    driveKnob.setBounds (driveArea.getX() + (driveArea.getWidth() - driveKnob.getWidth()) / 2,
                         driveArea.getY() + 16, driveKnob.getWidth(), driveKnob.getHeight());
    driveNote.setBounds (driveArea.getX(), driveArea.getY() + 176, driveArea.getWidth(), 56);

    const auto toneArea = toneCard.getContentBounds();
    toneKnob.setBounds (toneArea.getX() + (toneArea.getWidth() - toneKnob.getWidth()) / 2,
                        toneArea.getY() + 16, toneKnob.getWidth(), toneKnob.getHeight());
    toneNote.setBounds (toneArea.getX(), toneArea.getY() + 176, toneArea.getWidth(), 56);

    const auto signalArea = signalCard.getContentBounds();
    outputMeter.setBounds (signalArea.getX() + 10, signalArea.getY() + 10, 150, 150);
    signalNote.setBounds (signalArea.getX() + 174, signalArea.getY() + 14,
                          signalArea.getWidth() - 174, 90);
    bypassButton.setBounds (signalArea.getX(), signalArea.getBottom() - 26,
                            signalArea.getWidth(), 26);
}

void CrickAudioProcessorEditor::updateTelemetry()
{
    const auto snapshot = processor.readMeters();
    const bool bypassed = processor.isBypassed();

    outputMeter.setLevels (snapshot.outPeakL, snapshot.outPeakR);
    outputMeter.setClip (snapshot.outClip);

    const float inDb = juce::jmax (forja::ui::linearToDb (snapshot.inPeakL),
                                   forja::ui::linearToDb (snapshot.inPeakR));

    refreshBadge (snapshot.outClip, bypassed, inDb);
}

void CrickAudioProcessorEditor::refreshReadouts()
{
    setMetaValues ({
        { "drive", juce::String (driveKnob.getSlider().getValue(), 2) },
        { "tom", juce::String (toneKnob.getSlider().getValue(), 2) },
        { "pdc", juce::String (processor.getLatencySamples()) + " am." }
    });
}

void CrickAudioProcessorEditor::refreshBadge (bool clipped, bool bypassed, float inputDb)
{
    auto kind = forja::ui::BadgeKind::ready;
    juce::String text ("PRONTO");
    juce::String detail (u ("Amp de baixo \u00b7 dois captadores."));

    if (clipped)
    {
        kind = forja::ui::BadgeKind::clip;
        text = "CLIP";
        detail = u ("Sa\u00edda acima de 0 dBFS \u00b7 reduza o drive.");
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
    else if (driveKnob.getSlider().getValue() < 0.25)
    {
        detail = u ("Baixo limpo \u00b7 s\u00f3 o car\u00e1ter dos captadores.");
    }
    else
    {
        detail = u ("Baixo saturado \u00b7 seis est\u00e1gios em cascata.");
    }

    setBadge (kind, text, detail);
}
