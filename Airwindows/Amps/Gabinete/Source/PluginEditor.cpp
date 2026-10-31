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

GabineteAudioProcessorEditor::GabineteAudioProcessorEditor (GabineteAudioProcessor& p)
    : ForjaEditorBase (
          p, p.apvts,
          { "GABINETE",
            u ("DE AMP"),
            "GABINETE FASEADO",
            "AIRWINDOWS CABS2",
            u ("Gabinete \u00b7 DSP Airwindows Cabs2"),
            u ("DSP Airwindows Cabs2 \u00b7 MIT \u00b7 lat\u00eancia 0 amostras"),
            forja::gabinete::makeForjaInfo() }),
      processor (p)
{
    configureNote (lowNote, u ("1,00 = passa-baixos aberto.\nBaixando, o topo do\namplificador fecha."));
    configureNote (highNote, u ("0,00 = sem corte grave.\nSubindo, o corpo do\ngabinete afina."));
    configureNote (signalNote, u ("Filtros em cascata, sem\ndistor\u00e7\u00e3o nem ganho.\n"
                                  "Lat\u00eancia 0 amostras."));

    addAndMakeVisible (lowCard);
    addAndMakeVisible (highCard);
    addAndMakeVisible (signalCard);

    lowCard.addAndMakeVisible (lowpassKnob);
    lowCard.addAndMakeVisible (lowNote);

    highCard.addAndMakeVisible (hipassKnob);
    highCard.addAndMakeVisible (highNote);

    signalCard.addAndMakeVisible (outputMeter);
    signalCard.addAndMakeVisible (signalNote);
    signalCard.addAndMakeVisible (bypassButton);

    lowpassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "lowpass", lowpassKnob.getSlider());
    hipassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "hipass", hipassKnob.getSlider());

    bypassButton.setClickingTogglesState (true);
    bypassButton.setTooltip ("Bypass com rampa de 10 ms");
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, "bypass", bypassButton);

    outputMeter.onClipReset = [this]
    {
        processor.resetClip();
        outputMeter.setClip (false);
    };

    lowpassKnob.onValueChange = [this] { refreshReadouts(); };
    hipassKnob.onValueChange = [this] { refreshReadouts(); };

    refreshReadouts();

    setSize (windowWidth, windowHeight);
}

void GabineteAudioProcessorEditor::layoutContent (juce::Rectangle<int> content)
{
    const int y = content.getY();

    lowCard.setBounds (content.getX(), y, 294, cardHeight);
    highCard.setBounds (content.getX() + 302, y, 294, cardHeight);
    signalCard.setBounds (content.getX() + 604, y, 374, cardHeight);

    const auto lowArea = lowCard.getContentBounds();
    lowpassKnob.setBounds (lowArea.getX() + (lowArea.getWidth() - lowpassKnob.getWidth()) / 2,
                           lowArea.getY() + 16, lowpassKnob.getWidth(), lowpassKnob.getHeight());
    lowNote.setBounds (lowArea.getX(), lowArea.getY() + 176, lowArea.getWidth(), 56);

    const auto highArea = highCard.getContentBounds();
    hipassKnob.setBounds (highArea.getX() + (highArea.getWidth() - hipassKnob.getWidth()) / 2,
                          highArea.getY() + 16, hipassKnob.getWidth(), hipassKnob.getHeight());
    highNote.setBounds (highArea.getX(), highArea.getY() + 176, highArea.getWidth(), 56);

    const auto signalArea = signalCard.getContentBounds();
    outputMeter.setBounds (signalArea.getX() + 10, signalArea.getY() + 10, 150, 150);
    signalNote.setBounds (signalArea.getX() + 174, signalArea.getY() + 14,
                          signalArea.getWidth() - 174, 70);
    bypassButton.setBounds (signalArea.getX(), signalArea.getBottom() - 26,
                            signalArea.getWidth(), 26);
}

void GabineteAudioProcessorEditor::updateTelemetry()
{
    const auto snapshot = processor.readMeters();
    const bool bypassed = processor.isBypassed();

    outputMeter.setLevels (snapshot.outPeakL, snapshot.outPeakR);
    outputMeter.setClip (snapshot.outClip);

    const float inDb = juce::jmax (forja::ui::linearToDb (snapshot.inPeakL),
                                   forja::ui::linearToDb (snapshot.inPeakR));

    refreshBadge (snapshot.outClip, bypassed, inDb);
}

void GabineteAudioProcessorEditor::refreshReadouts()
{
    setMetaValues ({
        { "passa-baixos", juce::String (lowpassKnob.getSlider().getValue(), 2) },
        { "passa-altos", juce::String (hipassKnob.getSlider().getValue(), 2) }
    });
}

void GabineteAudioProcessorEditor::refreshBadge (bool clipped, bool bypassed, float inputDb)
{
    auto kind = forja::ui::BadgeKind::ready;
    juce::String text ("PRONTO");
    juce::String detail (u ("Gabinete faseado \u00b7 lat\u00eancia 0."));

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
    else if (lowpassKnob.getSlider().getValue() > 0.995
             && hipassKnob.getSlider().getValue() < 0.005)
    {
        detail = u ("Gabinete aberto \u00b7 os dois filtros est\u00e3o em repouso.");
    }
    else
    {
        detail = u ("Gabinete ativo \u00b7 colorindo o topo e o corpo.");
    }

    setBadge (kind, text, detail);
}
