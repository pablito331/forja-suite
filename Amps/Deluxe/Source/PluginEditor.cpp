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

DeluxeAudioProcessorEditor::DeluxeAudioProcessorEditor (DeluxeAudioProcessor& p)
    : ForjaEditorBase (
          p, p.apvts,
          { "DELUXE",
            u ("DI COMPRIMIDO"),
            u ("CONDICIONADOR DE DI"),
            "AIRWINDOWS CHIMEYDELUXE",
            u ("DI comprimido \u00b7 DSP Airwindows ChimeyDeluxe"),
            u ("DSP Airwindows ChimeyDeluxe \u00b7 MIT \u00b7 lat\u00eancia 0 amostras"),
            forja::deluxe::makeForjaInfo() }),
      processor (p)
{
    configureNote (ampNote, u ("Hell, Fizz e Pick definem\no topo e o ataque; o maior\ndeles tamb\u00e9m abre o drive."));
    configureNote (toneNote, u ("Satan, Danger e Critical\ns\u00e3o as bandas m\u00e9dias\ndo DI."));
    configureNote (bodyNote, u ("H Meat e L Meat d\u00e3o corpo;\nSwing e Rarely s\u00e3o o\npeso dos graves."));
    configureNote (signalNote, u ("Sa\u00edda do DI.\nSem gate: a compress\u00e3o\nsegura o ru\u00eddo."));

    addAndMakeVisible (ampCard);
    addAndMakeVisible (toneCard);
    addAndMakeVisible (bodyCard);
    addAndMakeVisible (signalCard);

    ampCard.addAndMakeVisible (hellKnob);
    ampCard.addAndMakeVisible (fizzKnob);
    ampCard.addAndMakeVisible (pickKnob);
    ampCard.addAndMakeVisible (ampNote);

    toneCard.addAndMakeVisible (satanKnob);
    toneCard.addAndMakeVisible (dangerKnob);
    toneCard.addAndMakeVisible (criticalKnob);
    toneCard.addAndMakeVisible (toneNote);

    bodyCard.addAndMakeVisible (hMeatKnob);
    bodyCard.addAndMakeVisible (lMeatKnob);
    bodyCard.addAndMakeVisible (swingKnob);
    bodyCard.addAndMakeVisible (rarelyKnob);
    bodyCard.addAndMakeVisible (bodyNote);

    signalCard.addAndMakeVisible (outputMeter);
    signalCard.addAndMakeVisible (signalNote);
    signalCard.addAndMakeVisible (bypassButton);

    hellAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "hell", hellKnob.getSlider());
    fizzAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "fizz", fizzKnob.getSlider());
    pickAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "pick", pickKnob.getSlider());
    satanAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "satan", satanKnob.getSlider());
    dangerAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "danger", dangerKnob.getSlider());
    criticalAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "critical", criticalKnob.getSlider());
    hMeatAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "hMeat", hMeatKnob.getSlider());
    lMeatAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "lMeat", lMeatKnob.getSlider());
    swingAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "swing", swingKnob.getSlider());
    rarelyAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "rarely", rarelyKnob.getSlider());

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
    hellKnob.onValueChange = onKnobMoved;
    fizzKnob.onValueChange = onKnobMoved;
    pickKnob.onValueChange = onKnobMoved;
    satanKnob.onValueChange = onKnobMoved;
    dangerKnob.onValueChange = onKnobMoved;
    criticalKnob.onValueChange = onKnobMoved;
    hMeatKnob.onValueChange = onKnobMoved;
    lMeatKnob.onValueChange = onKnobMoved;
    swingKnob.onValueChange = onKnobMoved;
    rarelyKnob.onValueChange = onKnobMoved;

    refreshReadouts();

    setSize (windowWidth, windowHeight);
}

void DeluxeAudioProcessorEditor::layoutContent (juce::Rectangle<int> content)
{
    const int y = content.getY();

    ampCard.setBounds (content.getX(), y, 190, cardHeight);
    toneCard.setBounds (content.getX() + 198, y, 236, cardHeight);
    bodyCard.setBounds (content.getX() + 442, y, 294, cardHeight);
    signalCard.setBounds (content.getX() + 744, y, 236, cardHeight);

    const auto ampArea = ampCard.getContentBounds();
    const int ampX = ampArea.getX() + (ampArea.getWidth() - hellKnob.getWidth()) / 2;
    hellKnob.setBounds (ampX, ampArea.getY() + 2, hellKnob.getWidth(), hellKnob.getHeight());
    fizzKnob.setBounds (ampX, ampArea.getY() + 97, fizzKnob.getWidth(), fizzKnob.getHeight());
    pickKnob.setBounds (ampX, ampArea.getY() + 192, pickKnob.getWidth(), pickKnob.getHeight());
    ampNote.setBounds (ampArea.getX(), ampArea.getBottom() - 38, ampArea.getWidth(), 36);

    const auto toneArea = toneCard.getContentBounds();
    const int toneX = toneArea.getX() + (toneArea.getWidth() - satanKnob.getWidth()) / 2;
    satanKnob.setBounds (toneX, toneArea.getY() + 2, satanKnob.getWidth(), satanKnob.getHeight());
    dangerKnob.setBounds (toneX, toneArea.getY() + 105, dangerKnob.getWidth(), dangerKnob.getHeight());
    criticalKnob.setBounds (toneX, toneArea.getY() + 208, criticalKnob.getWidth(), criticalKnob.getHeight());
    toneNote.setBounds (toneArea.getX(), toneArea.getBottom() - 24, toneArea.getWidth(), 22);

    const auto bodyArea = bodyCard.getContentBounds();
    const int bodyLeft = bodyArea.getX() + 6;
    const int bodyRight = bodyLeft + hMeatKnob.getWidth() + 8;
    hMeatKnob.setBounds (bodyLeft, bodyArea.getY() + 2, hMeatKnob.getWidth(), hMeatKnob.getHeight());
    lMeatKnob.setBounds (bodyRight, bodyArea.getY() + 2, lMeatKnob.getWidth(), lMeatKnob.getHeight());
    swingKnob.setBounds (bodyLeft, bodyArea.getY() + 122, swingKnob.getWidth(), swingKnob.getHeight());
    rarelyKnob.setBounds (bodyRight, bodyArea.getY() + 122, rarelyKnob.getWidth(), rarelyKnob.getHeight());
    bodyNote.setBounds (bodyArea.getX(), bodyArea.getY() + 250, bodyArea.getWidth(), 60);

    const auto signalArea = signalCard.getContentBounds();
    outputMeter.setBounds (signalArea.getX() + 6, signalArea.getY() + 6, signalArea.getWidth() - 12, 150);
    signalNote.setBounds (signalArea.getX(), signalArea.getY() + 168, signalArea.getWidth(), 60);
    bypassButton.setBounds (signalArea.getX(), signalArea.getBottom() - 26,
                            signalArea.getWidth(), 26);
}

void DeluxeAudioProcessorEditor::updateTelemetry()
{
    const auto snapshot = processor.readMeters();
    const bool bypassed = processor.isBypassed();

    outputMeter.setLevels (snapshot.outPeakL, snapshot.outPeakR);
    outputMeter.setClip (snapshot.outClip);

    const float inDb = juce::jmax (forja::ui::linearToDb (snapshot.inPeakL),
                                   forja::ui::linearToDb (snapshot.inPeakR));

    refreshBadge (snapshot.outClip, bypassed, inDb);
}

void DeluxeAudioProcessorEditor::refreshReadouts()
{
    setMetaValues ({
        { "hell", juce::String (hellKnob.getSlider().getValue(), 2) },
        { "fizz", juce::String (fizzKnob.getSlider().getValue(), 2) },
        { "pick", juce::String (pickKnob.getSlider().getValue(), 2) }
    });
}

void DeluxeAudioProcessorEditor::refreshBadge (bool clipped, bool bypassed, float inputDb)
{
    auto kind = forja::ui::BadgeKind::ready;
    juce::String text ("PRONTO");
    juce::String detail (u ("DI comprimido \u00b7 lat\u00eancia 0."));

    if (clipped)
    {
        kind = forja::ui::BadgeKind::clip;
        text = "CLIP";
        detail = u ("Sa\u00edda no limite \u00b1 1 \u00b7 reduza as bandas de corpo.");
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
    else
    {
        detail = u ("DI comprimido \u00b7 dez bandas fixas moldando o sinal.");
    }

    setBadge (kind, text, detail);
}
