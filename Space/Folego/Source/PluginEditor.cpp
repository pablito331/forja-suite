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

FolegoAudioProcessorEditor::FolegoAudioProcessorEditor (FolegoAudioProcessor& p)
    : ForjaEditorBase (
          p, p.apvts,
          { u ("F\u00d4LEGO"),
            "DE AMBIENTE",
            "REVERB DE CLUBE",
            "AIRWINDOWS KSTATION",
            u ("Ambiente pequeno \u00b7 DSP Airwindows kStation"),
            u ("DSP Airwindows kStation \u00b7 MIT \u00b7 atraso musical, PDC 0"),
            forja::folego::makeForjaInfo() }),
      processor (p)
{
    configureNote (clubNote, u ("Clube de 272 lugares:\nreflex\u00f5es iniciais curtas\n"
                                "e cauda controlada,\ncom f\u00f4lego de ambiente."));
    configureNote (bodyNote, u ("DEREZ reduz a resolu\u00e7\u00e3o\ndo corpo (B\u00e9zier).\n"
                                "Acima de 0,50 o\noriginal muda de modo."));
    configureNote (filterNote, u ("FILTRO escurece a cauda\npor dentro da subamostragem.\n"
                                  "REFLEX\u00d5ES define o peso\ndas primeiras reflex\u00f5es."));
    configureNote (mixNote, u ("0,50 j\u00e1 \u00e9 metade molhado.\n1,00 = s\u00f3 ambiente."));

    addAndMakeVisible (clubCard);
    addAndMakeVisible (bodyCard);
    addAndMakeVisible (filterCard);
    addAndMakeVisible (outCard);

    clubCard.addAndMakeVisible (clubNote);

    bodyCard.addAndMakeVisible (regenKnob);
    bodyCard.addAndMakeVisible (derezKnob);

    filterCard.addAndMakeVisible (filterKnob);
    filterCard.addAndMakeVisible (earlyKnob);
    filterCard.addAndMakeVisible (positionKnob);
    filterCard.addAndMakeVisible (filterNote);

    outCard.addAndMakeVisible (mixKnob);
    outCard.addAndMakeVisible (mixNote);
    outCard.addAndMakeVisible (outputMeter);
    outCard.addAndMakeVisible (bypassButton);

    regenAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "regen", regenKnob.getSlider());
    derezAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "derez", derezKnob.getSlider());
    filterAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "filter", filterKnob.getSlider());
    earlyAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "early", earlyKnob.getSlider());
    positionAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "position", positionKnob.getSlider());
    mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "mix", mixKnob.getSlider());

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

    regenKnob.onValueChange = onKnobMoved;
    derezKnob.onValueChange = onKnobMoved;
    filterKnob.onValueChange = onKnobMoved;
    earlyKnob.onValueChange = onKnobMoved;
    positionKnob.onValueChange = onKnobMoved;
    mixKnob.onValueChange = onKnobMoved;

    refreshReadouts();

    setSize (windowWidth, windowHeight);
}

void FolegoAudioProcessorEditor::layoutContent (juce::Rectangle<int> content)
{
    const int y = content.getY();

    clubCard.setBounds (content.getX(), y, 190, cardHeight);
    bodyCard.setBounds (content.getX() + 198, y, 236, cardHeight);
    filterCard.setBounds (content.getX() + 442, y, 294, cardHeight);
    outCard.setBounds (content.getX() + 744, y, 236, cardHeight);

    const auto clubArea = clubCard.getContentBounds();
    clubNote.setBounds (clubArea.getX(), clubArea.getY() + 70, clubArea.getWidth(), 110);

    const auto bodyArea = bodyCard.getContentBounds();
    const int bodyX = bodyArea.getX() + (bodyArea.getWidth() - regenKnob.getWidth()) / 2;
    regenKnob.setBounds (bodyX, bodyArea.getY() + 8, regenKnob.getWidth(), regenKnob.getHeight());
    derezKnob.setBounds (bodyX, bodyArea.getY() + 131, derezKnob.getWidth(),
                         derezKnob.getHeight());
    bodyNote.setBounds (bodyArea.getX(), bodyArea.getY() + 258, bodyArea.getWidth(), 70);

    const auto filterArea = filterCard.getContentBounds();
    const int filterX = filterArea.getX()
                        + (filterArea.getWidth() - filterKnob.getWidth() * 2 - 8) / 2;
    filterKnob.setBounds (filterX, filterArea.getY() + 8, filterKnob.getWidth(),
                          filterKnob.getHeight());
    earlyKnob.setBounds (filterX + filterKnob.getWidth() + 8, filterArea.getY() + 8,
                         earlyKnob.getWidth(), earlyKnob.getHeight());
    positionKnob.setBounds (filterX, filterArea.getY() + 131, positionKnob.getWidth(),
                            positionKnob.getHeight());
    filterNote.setBounds (filterArea.getX(), filterArea.getY() + 258, filterArea.getWidth(), 70);

    const auto outArea = outCard.getContentBounds();
    const int mixX = outArea.getX() + (outArea.getWidth() - mixKnob.getWidth()) / 2;
    mixKnob.setBounds (mixX, outArea.getY() + 8, mixKnob.getWidth(), mixKnob.getHeight());
    mixNote.setBounds (outArea.getX(), outArea.getY() + 139, outArea.getWidth(), 30);

    const int meterTop = outArea.getY() + 172;
    const int bypassTop = outArea.getBottom() - 26;

    outputMeter.setBounds (outArea.getX(), meterTop, outArea.getWidth(), bypassTop - 8 - meterTop);
    bypassButton.setBounds (outArea.getX(), bypassTop, outArea.getWidth(), 26);
}

void FolegoAudioProcessorEditor::updateTelemetry()
{
    const auto snapshot = processor.readMeters();
    const bool bypassed = processor.isBypassed();

    outputMeter.setLevels (snapshot.outPeakL, snapshot.outPeakR);
    outputMeter.setClip (snapshot.outClip);

    const float inDb = juce::jmax (forja::ui::linearToDb (snapshot.inPeakL),
                                   forja::ui::linearToDb (snapshot.inPeakR));

    refreshBadge (snapshot.outClip, bypassed, inDb);
}

void FolegoAudioProcessorEditor::refreshReadouts()
{
    setMetaValues ({
        { u ("posi\u00e7\u00e3o"), juce::String (positionKnob.getSlider().getValue() * 100.0, 0) + " %" },
        { "regen", juce::String (regenKnob.getSlider().getValue() * 100.0, 0) + " %" },
        { "filtro", juce::String (filterKnob.getSlider().getValue() * 100.0, 0) + " %" },
        { u ("reflex\u00f5es"), juce::String (earlyKnob.getSlider().getValue() * 100.0, 0) + " %" },
        { u ("mix"), juce::String (mixKnob.getSlider().getValue() * 100.0, 0) + " %" }
    });
}

void FolegoAudioProcessorEditor::refreshBadge (bool clipped, bool bypassed, float inputDb)
{
    auto kind = forja::ui::BadgeKind::ready;
    juce::String text ("PRONTO");
    juce::String detail (u ("Clube de 272 lugares \u00b7 36 linhas por canal \u00b7 PDC 0."));

    const float regen = processor.apvts.getRawParameterValue ("regen")->load();
    const float mix = processor.apvts.getRawParameterValue ("mix")->load();

    if (clipped)
    {
        kind = forja::ui::BadgeKind::clip;
        text = "CLIP";
        detail = u ("Sa\u00edda acima de 0 dBFS \u00b7 reduza a regenera\u00e7\u00e3o ou a mistura.");
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
    else if (mix < 0.01f)
    {
        detail = u ("Mistura em zero \u00b7 s\u00f3 o sinal seco sai.");
    }
    else if (regen > 0.95f)
    {
        detail = u ("Regenera\u00e7\u00e3o m\u00e1xima \u00b7 a cauda pode durar muito.");
    }

    setBadge (kind, text, detail);
}
