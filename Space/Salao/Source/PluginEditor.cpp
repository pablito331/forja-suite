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

SalaoAudioProcessorEditor::SalaoAudioProcessorEditor (SalaoAudioProcessor& p)
    : ForjaEditorBase (
          p, p.apvts,
          { u ("SAL\u00c3O"),
            "DE SALA",
            "REVERB DE SALA GRANDE",
            "AIRWINDOWS KROROCKSTAR2",
            u ("Reverb grande \u00b7 DSP Airwindows kRockstar2"),
            u ("DSP Airwindows kRockstar2 \u00b7 MIT \u00b7 atraso musical, PDC 0"),
            forja::salao::makeForjaInfo() }),
      processor (p)
{
    configureNote (hallNote, u ("Sala grande de concerto:\nreflex\u00f5es iniciais,\n"
                                "corpo de sala e piso\nde ru\u00eddo pr\u00f3prio."));
    configureNote (spaceNote, u ("DOWNREZ reduz a resolu\u00e7\u00e3o\ninterna do corpo da sala\n"
                                 "(degraus de B\u00e9zier).\nDIST\u00c2NCIA aproxima ou\nafasta a fonte."));
    configureNote (mixNote, u ("0,50 j\u00e1 \u00e9 metade molhado.\n1,00 = s\u00f3 sala."));

    addAndMakeVisible (hallCard);
    addAndMakeVisible (timeCard);
    addAndMakeVisible (spaceCard);
    addAndMakeVisible (outCard);

    hallCard.addAndMakeVisible (hallNote);

    timeCard.addAndMakeVisible (regenKnob);
    timeCard.addAndMakeVisible (positionKnob);

    spaceCard.addAndMakeVisible (derezKnob);
    spaceCard.addAndMakeVisible (distanceKnob);
    spaceCard.addAndMakeVisible (spaceNote);

    outCard.addAndMakeVisible (mixKnob);
    outCard.addAndMakeVisible (mixNote);
    outCard.addAndMakeVisible (outputMeter);
    outCard.addAndMakeVisible (bypassButton);

    regenAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "regen", regenKnob.getSlider());
    positionAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "position", positionKnob.getSlider());
    derezAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "derez", derezKnob.getSlider());
    distanceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "distance", distanceKnob.getSlider());
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
    positionKnob.onValueChange = onKnobMoved;
    derezKnob.onValueChange = onKnobMoved;
    distanceKnob.onValueChange = onKnobMoved;
    mixKnob.onValueChange = onKnobMoved;

    refreshReadouts();

    setSize (windowWidth, windowHeight);
}

void SalaoAudioProcessorEditor::layoutContent (juce::Rectangle<int> content)
{
    const int y = content.getY();

    hallCard.setBounds (content.getX(), y, 190, cardHeight);
    timeCard.setBounds (content.getX() + 198, y, 236, cardHeight);
    spaceCard.setBounds (content.getX() + 442, y, 294, cardHeight);
    outCard.setBounds (content.getX() + 744, y, 236, cardHeight);

    const auto hallArea = hallCard.getContentBounds();
    hallNote.setBounds (hallArea.getX(), hallArea.getY() + 70, hallArea.getWidth(), 110);

    const auto timeArea = timeCard.getContentBounds();
    const int regenX = timeArea.getX() + (timeArea.getWidth() - regenKnob.getWidth()) / 2;
    regenKnob.setBounds (regenX, timeArea.getY() + 8, regenKnob.getWidth(),
                         regenKnob.getHeight());
    positionKnob.setBounds (regenX, timeArea.getY() + 8 + regenKnob.getHeight() + 4,
                            positionKnob.getWidth(), positionKnob.getHeight());

    const auto spaceArea = spaceCard.getContentBounds();
    const int spaceX = spaceArea.getX() + (spaceArea.getWidth() - distanceKnob.getWidth() * 2 - 8) / 2;
    derezKnob.setBounds (spaceX, spaceArea.getY() + 8, derezKnob.getWidth(), derezKnob.getHeight());
    distanceKnob.setBounds (spaceX + derezKnob.getWidth() + 8, spaceArea.getY() + 8,
                            distanceKnob.getWidth(), distanceKnob.getHeight());
    spaceNote.setBounds (spaceArea.getX(), spaceArea.getY() + 139, spaceArea.getWidth(), 110);

    const auto outArea = outCard.getContentBounds();
    const int mixX = outArea.getX() + (outArea.getWidth() - mixKnob.getWidth()) / 2;
    mixKnob.setBounds (mixX, outArea.getY() + 8, mixKnob.getWidth(), mixKnob.getHeight());
    mixNote.setBounds (outArea.getX(), outArea.getY() + 139, outArea.getWidth(), 30);

    const int meterTop = outArea.getY() + 172;
    const int bypassTop = outArea.getBottom() - 26;

    outputMeter.setBounds (outArea.getX(), meterTop, outArea.getWidth(), bypassTop - 8 - meterTop);
    bypassButton.setBounds (outArea.getX(), bypassTop, outArea.getWidth(), 26);
}

void SalaoAudioProcessorEditor::updateTelemetry()
{
    const auto snapshot = processor.readMeters();
    const bool bypassed = processor.isBypassed();

    outputMeter.setLevels (snapshot.outPeakL, snapshot.outPeakR);
    outputMeter.setClip (snapshot.outClip);

    const float inDb = juce::jmax (forja::ui::linearToDb (snapshot.inPeakL),
                                   forja::ui::linearToDb (snapshot.inPeakR));

    refreshBadge (snapshot.outClip, bypassed, inDb);
}

void SalaoAudioProcessorEditor::refreshReadouts()
{
    setMetaValues ({
        { u ("posi\u00e7\u00e3o"), juce::String (positionKnob.getSlider().getValue() * 100.0, 0) + " %" },
        { "regen", juce::String (regenKnob.getSlider().getValue() * 100.0, 0) + " %" },
        { "derez", juce::String (derezKnob.getSlider().getValue() * 100.0, 0) + " %" },
        { u ("dist\u00e2ncia"), juce::String (distanceKnob.getSlider().getValue() * 100.0, 0) + " %" },
        { u ("mix"), juce::String (mixKnob.getSlider().getValue() * 100.0, 0) + " %" }
    });
}

void SalaoAudioProcessorEditor::refreshBadge (bool clipped, bool bypassed, float inputDb)
{
    auto kind = forja::ui::BadgeKind::ready;
    juce::String text ("PRONTO");
    juce::String detail (u ("Sala grande \u00b7 36 linhas por canal \u00b7 PDC 0."));

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
