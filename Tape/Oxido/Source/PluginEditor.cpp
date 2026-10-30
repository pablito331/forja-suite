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

OxidoAudioProcessorEditor::OxidoAudioProcessorEditor (OxidoAudioProcessor& p)
    : ForjaEditorBase (
          p, p.apvts,
          { u ("\u00d3XIDO"),
            u ("DE FITA"),
            "FITA CASSETE",
            "AIRWINDOWS IRONOXIDE5",
            u ("Fita cassete \u00b7 DSP Airwindows IronOxide5"),
            u ("DSP Airwindows IronOxide5 \u00b7 MIT \u00b7 lat\u00eancia 0 amostras"),
            forja::oxido::makeForjaInfo() }),
      processor (p)
{
    configureNote (inputNote, u ("Ganho de \u221218 a +18 dB.\n0,50 \u00e9 unit\u00e1rio."));
    configureNote (tapeNote, u ("FITA AGUDO e GRAVE\najustam a velocidade\nda fita em IPS."));
    configureNote (textureNote, u ("Flutter em 0 desliga.\nRu\u00eddo em 0 deixa a\nfita em sil\u00eancio."));
    configureNote (mixNote, u ("0,50 = s\u00f3 seco \u00b7 1,00 = s\u00f3 fita.\nAbaixo de 0,50 a fita\nentra invertida."));

    addAndMakeVisible (ioCard);
    addAndMakeVisible (tapeCard);
    addAndMakeVisible (textureCard);
    addAndMakeVisible (mixCard);

    ioCard.addAndMakeVisible (inputKnob);
    ioCard.addAndMakeVisible (outputKnob);
    ioCard.addAndMakeVisible (inputNote);

    tapeCard.addAndMakeVisible (cassetteArt);
    tapeCard.addAndMakeVisible (tapeHighKnob);
    tapeCard.addAndMakeVisible (tapeLowKnob);
    tapeCard.addAndMakeVisible (tapeNote);

    textureCard.addAndMakeVisible (flutterKnob);
    textureCard.addAndMakeVisible (noiseKnob);
    textureCard.addAndMakeVisible (textureNote);

    mixCard.addAndMakeVisible (mixKnob);
    mixCard.addAndMakeVisible (mixNote);
    mixCard.addAndMakeVisible (outputMeter);
    mixCard.addAndMakeVisible (bypassButton);

    inputAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "input", inputKnob.getSlider());
    outputAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "output", outputKnob.getSlider());
    tapeHighAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "tapeHigh", tapeHighKnob.getSlider());
    tapeLowAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "tapeLow", tapeLowKnob.getSlider());
    flutterAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "flutter", flutterKnob.getSlider());
    noiseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "noise", noiseKnob.getSlider());
    mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "invDryWet", mixKnob.getSlider());

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

    inputKnob.onValueChange = onKnobMoved;
    outputKnob.onValueChange = onKnobMoved;
    tapeHighKnob.onValueChange = onKnobMoved;
    tapeLowKnob.onValueChange = onKnobMoved;
    flutterKnob.onValueChange = onKnobMoved;
    noiseKnob.onValueChange = onKnobMoved;
    mixKnob.onValueChange = onKnobMoved;

    refreshReadouts();

    setSize (windowWidth, windowHeight);
}

void OxidoAudioProcessorEditor::layoutContent (juce::Rectangle<int> content)
{
    const int y = content.getY();

    ioCard.setBounds (content.getX(), y, 190, cardHeight);
    tapeCard.setBounds (content.getX() + 198, y, 236, cardHeight);
    textureCard.setBounds (content.getX() + 442, y, 294, cardHeight);
    mixCard.setBounds (content.getX() + 744, y, 236, cardHeight);

    const auto ioArea = ioCard.getContentBounds();
    const int ioWidth = inputKnob.getWidth();
    const int ioX = ioArea.getX() + (ioArea.getWidth() - ioWidth) / 2;

    inputKnob.setBounds (ioX, ioArea.getY() + 8, ioWidth, inputKnob.getHeight());
    outputKnob.setBounds (ioX, ioArea.getY() + 139, ioWidth, outputKnob.getHeight());
    inputNote.setBounds (ioArea.getX(), ioArea.getY() + 268, ioArea.getWidth(), 44);

    const auto tapeArea = tapeCard.getContentBounds();
    const int tapeWidth = tapeHighKnob.getWidth();
    const int tapeHeight = tapeHighKnob.getHeight();
    const int tapeStep = tapeWidth + 4;
    const int tapeX = tapeArea.getX() + (tapeArea.getWidth() - tapeStep * 2 + 4) / 2;

    tapeHighKnob.setBounds (tapeX, tapeArea.getY() + 8, tapeWidth, tapeHeight);
    tapeLowKnob.setBounds (tapeX + tapeStep, tapeArea.getY() + 8, tapeWidth, tapeHeight);
    tapeNote.setBounds (tapeArea.getX(), tapeArea.getY() + 139, tapeArea.getWidth(), 70);

    const int artTop = tapeArea.getY() + 216;
    cassetteArt.setBounds (tapeArea.getX() + 10, artTop, tapeArea.getWidth() - 20,
                           juce::jmax (48, tapeArea.getBottom() - artTop - 4));

    const auto textureArea = textureCard.getContentBounds();
    const int textureWidth = flutterKnob.getWidth();
    const int textureGap = textureArea.getWidth() - textureWidth * 2;

    flutterKnob.setBounds (textureArea.getX(), textureArea.getY() + 8, textureWidth,
                           flutterKnob.getHeight());
    noiseKnob.setBounds (textureArea.getX() + textureWidth + textureGap, textureArea.getY() + 8,
                         textureWidth, noiseKnob.getHeight());
    textureNote.setBounds (textureArea.getX(), textureArea.getY() + 139, textureArea.getWidth(),
                           70);

    const auto mixArea = mixCard.getContentBounds();
    const int mixWidth = mixKnob.getWidth();
    const int mixX = mixArea.getX() + (mixArea.getWidth() - mixWidth) / 2;
    const int bypassTop = mixArea.getBottom() - 26;
    const int meterTop = mixArea.getY() + 178;

    mixKnob.setBounds (mixX, mixArea.getY() + 8, mixWidth, mixKnob.getHeight());
    mixNote.setBounds (mixArea.getX(), mixArea.getY() + 132, mixArea.getWidth(), 42);
    outputMeter.setBounds (mixArea.getX(), meterTop, mixArea.getWidth(),
                           bypassTop - 8 - meterTop);
    bypassButton.setBounds (mixArea.getX(), bypassTop, mixArea.getWidth(), 26);
}

void OxidoAudioProcessorEditor::updateTelemetry()
{
    const auto snapshot = processor.readMeters();
    const bool bypassed = processor.isBypassed();

    outputMeter.setLevels (snapshot.outPeakL, snapshot.outPeakR);
    outputMeter.setClip (snapshot.outClip);

    const float inDb = juce::jmax (forja::ui::linearToDb (snapshot.inPeakL),
                                   forja::ui::linearToDb (snapshot.inPeakR));

    refreshBadge (snapshot.outClip, bypassed, inDb);
}

void OxidoAudioProcessorEditor::refreshReadouts()
{
    const auto ips = [] (forja::ui::ForjaKnob& knob)
    {
        const double value = knob.getSlider().getValue();
        const double speed = (((value * value) * (value * value)) * 148.5) + 1.5;
        return juce::String (speed, 1) + " ips";
    };

    setMetaValues ({
        { "entrada", juce::String ((inputKnob.getSlider().getValue() * 36.0) - 18.0, 1) + " dB" },
        { u ("agudo"), ips (tapeHighKnob) },
        { "grave", ips (tapeLowKnob) },
        { "flutter", juce::String (flutterKnob.getSlider().getValue(), 2) },
        { u ("ru\u00eddo"), juce::String (noiseKnob.getSlider().getValue(), 2) },
        { u ("sa\u00edda"), juce::String ((outputKnob.getSlider().getValue() * 36.0) - 18.0, 1)
                          + " dB" }
    });
}

void OxidoAudioProcessorEditor::refreshBadge (bool clipped, bool bypassed, float inputDb)
{
    auto kind = forja::ui::BadgeKind::ready;
    juce::String text ("PRONTO");
    juce::String detail (u ("Fita cassete \u00b7 gr\u00e3o, flutter e lim\u00edtador \u00b7 "
                            "lat\u00eancia 0."));

    const float flutter = processor.apvts.getRawParameterValue ("flutter")->load();
    const float noise = processor.apvts.getRawParameterValue ("noise")->load();
    const float mix = processor.apvts.getRawParameterValue ("invDryWet")->load();

    if (clipped)
    {
        kind = forja::ui::BadgeKind::clip;
        text = "CLIP";
        detail = u ("Sa\u00edda acima de 0 dBFS \u00b7 reduza ENTRADA ou SA\u00cdDA.");
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
    else if (mix <= 0.5f)
    {
        detail = u ("Mistura em 0,50 ou abaixo \u00b7 a fita est\u00e1 fora do caminho ou invertida.");
    }
    else if (flutter < 0.005f)
    {
        detail = u ("Fita est\u00e1vel \u00b7 flutter desligado.");
    }
    else if (noise < 0.005f)
    {
        detail = u ("Ru\u00eddo de fita em zero \u00b7 s\u00f3 a satura\u00e7\u00e3o est\u00e1 ativa.");
    }

    setBadge (kind, text, detail);
}
