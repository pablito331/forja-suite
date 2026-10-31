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

LaminadorAudioProcessorEditor::LaminadorAudioProcessorEditor (LaminadorAudioProcessor& p)
    : ForjaEditorBase (
          p, p.apvts,
          { "LAMINADOR",
            u ("DE FITA"),
            u ("FITA DE EST\u00daDIO"),
            "AIRWINDOWS TOTAPE9",
            u ("Fita 2 pistas \u00b7 DSP Airwindows ToTape9"),
            u ("DSP Airwindows ToTape9 \u00b7 MIT \u00b7 lat\u00eancia 1 amostra"),
            forja::laminador::makeForjaInfo() }),
      processor (p)
{
    configureNote (inputNote, u ("Entrada alimenta o Dubly;\n0,50 \u00e9 ganho unit\u00e1rio."));
    configureNote (colorNote, u ("FORMA esculpe o Dubly.\nCABE\u00c7OTE refor\u00e7a entre\n"
                                 "25 e 200 Hz.\nBIAS em 0,50 n\u00e3o satura."));
    configureNote (flutterNote, u ("Profundidade da modula\u00e7\u00e3o\nde velocidade. Em 0\no "
                                   "flutter desliga."));

    addAndMakeVisible (inputCard);
    addAndMakeVisible (colorCard);
    addAndMakeVisible (flutterCard);
    addAndMakeVisible (outputCard);

    inputCard.addAndMakeVisible (inputKnob);
    inputCard.addAndMakeVisible (tiltKnob);
    inputCard.addAndMakeVisible (inputNote);

    colorCard.addAndMakeVisible (shapeKnob);
    colorCard.addAndMakeVisible (biasKnob);
    colorCard.addAndMakeVisible (headBmpKnob);
    colorCard.addAndMakeVisible (headFrqKnob);
    colorCard.addAndMakeVisible (colorNote);

    flutterCard.addAndMakeVisible (tapeArt);
    flutterCard.addAndMakeVisible (flutterKnob);
    flutterCard.addAndMakeVisible (flutSpdKnob);
    flutterCard.addAndMakeVisible (flutterNote);

    outputCard.addAndMakeVisible (outputKnob);
    outputCard.addAndMakeVisible (outputMeter);
    outputCard.addAndMakeVisible (bypassButton);

    inputAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "input", inputKnob.getSlider());
    tiltAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "tilt", tiltKnob.getSlider());
    shapeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "shape", shapeKnob.getSlider());
    biasAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "bias", biasKnob.getSlider());
    headBmpAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "headBmp", headBmpKnob.getSlider());
    headFrqAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "headFrq", headFrqKnob.getSlider());
    flutterAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "flutter", flutterKnob.getSlider());
    flutSpdAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "flutSpd", flutSpdKnob.getSlider());
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

    inputKnob.onValueChange = onKnobMoved;
    tiltKnob.onValueChange = onKnobMoved;
    shapeKnob.onValueChange = onKnobMoved;
    biasKnob.onValueChange = onKnobMoved;
    headBmpKnob.onValueChange = onKnobMoved;
    headFrqKnob.onValueChange = onKnobMoved;
    flutterKnob.onValueChange = onKnobMoved;
    flutSpdKnob.onValueChange = onKnobMoved;
    outputKnob.onValueChange = onKnobMoved;

    refreshReadouts();

    setSize (windowWidth, windowHeight);
}

void LaminadorAudioProcessorEditor::layoutContent (juce::Rectangle<int> content)
{
    const int y = content.getY();

    inputCard.setBounds (content.getX(), y, 190, cardHeight);
    colorCard.setBounds (content.getX() + 198, y, 236, cardHeight);
    flutterCard.setBounds (content.getX() + 442, y, 294, cardHeight);
    outputCard.setBounds (content.getX() + 744, y, 236, cardHeight);

    const auto inputArea = inputCard.getContentBounds();
    const int inputWidth = inputKnob.getWidth();
    const int inputX = inputArea.getX() + (inputArea.getWidth() - inputWidth) / 2;

    inputKnob.setBounds (inputX, inputArea.getY() + 8, inputWidth, inputKnob.getHeight());
    tiltKnob.setBounds (inputX, inputArea.getY() + 139, inputWidth, tiltKnob.getHeight());
    inputNote.setBounds (inputArea.getX(), inputArea.getY() + 268, inputArea.getWidth(), 44);

    const auto colorArea = colorCard.getContentBounds();
    const int colorWidth = shapeKnob.getWidth();
    const int colorHeight = shapeKnob.getHeight();
    const int columnStep = colorWidth + 4;
    const int gridX = colorArea.getX() + (colorArea.getWidth() - columnStep * 2 + 4) / 2;
    const int gridY = colorArea.getY() + 6;

    shapeKnob.setBounds (gridX, gridY, colorWidth, colorHeight);
    biasKnob.setBounds (gridX + columnStep, gridY, colorWidth, colorHeight);
    headBmpKnob.setBounds (gridX, gridY + colorHeight + 6, colorWidth, colorHeight);
    headFrqKnob.setBounds (gridX + columnStep, gridY + colorHeight + 6, colorWidth, colorHeight);
    colorNote.setBounds (colorArea.getX(), colorArea.getY() + 206, colorArea.getWidth(), 76);

    const auto flutterArea = flutterCard.getContentBounds();
    const int flutterWidth = flutterKnob.getWidth();
    const int flutterGap = flutterArea.getWidth() - flutterWidth * 2;

    flutterKnob.setBounds (flutterArea.getX(), flutterArea.getY() + 8, flutterWidth,
                           flutterKnob.getHeight());
    flutSpdKnob.setBounds (flutterArea.getX() + flutterWidth + flutterGap, flutterArea.getY() + 8,
                           flutterWidth, flutSpdKnob.getHeight());
    flutterNote.setBounds (flutterArea.getX(), flutterArea.getY() + 139, flutterArea.getWidth(),
                           76);

    const int artTop = flutterArea.getY() + 226;
    tapeArt.setBounds (flutterArea.getX() + 12, artTop, flutterArea.getWidth() - 24,
                       juce::jmax (48, flutterArea.getBottom() - artTop - 4));

    const auto outputArea = outputCard.getContentBounds();
    const int outputWidth = outputKnob.getWidth();
    const int outputX = outputArea.getX() + (outputArea.getWidth() - outputWidth) / 2;
    const int bypassTop = outputArea.getBottom() - 26;
    const int meterTop = outputArea.getY() + 140;

    outputKnob.setBounds (outputX, outputArea.getY() + 8, outputWidth, outputKnob.getHeight());
    outputMeter.setBounds (outputArea.getX(), meterTop, outputArea.getWidth(),
                           bypassTop - 8 - meterTop);
    bypassButton.setBounds (outputArea.getX(), bypassTop, outputArea.getWidth(), 26);
}

void LaminadorAudioProcessorEditor::updateTelemetry()
{
    const auto snapshot = processor.readMeters();
    const bool bypassed = processor.isBypassed();

    outputMeter.setLevels (snapshot.outPeakL, snapshot.outPeakR);
    outputMeter.setClip (snapshot.outClip);

    const float inDb = juce::jmax (forja::ui::linearToDb (snapshot.inPeakL),
                                   forja::ui::linearToDb (snapshot.inPeakR));

    refreshBadge (snapshot.outClip, bypassed, inDb);
}

void LaminadorAudioProcessorEditor::refreshReadouts()
{
    const auto headHz = juce::String (25.0 + headFrqKnob.getSlider().getValue()
                                              * headFrqKnob.getSlider().getValue() * 175.0,
                                      0);

    setMetaValues ({
        { "entrada", juce::String (inputKnob.getSlider().getValue(), 2) },
        { u ("inclina\u00e7\u00e3o"), juce::String (tiltKnob.getSlider().getValue(), 2) },
        { "forma", juce::String (shapeKnob.getSlider().getValue(), 2) },
        { "flutter", juce::String (flutterKnob.getSlider().getValue(), 2) },
        { u ("cabe\u00e7ote"), headHz + " Hz" },
        { u ("sa\u00edda"), juce::String (outputKnob.getSlider().getValue(), 2) }
    });
}

void LaminadorAudioProcessorEditor::refreshBadge (bool clipped, bool bypassed, float inputDb)
{
    auto kind = forja::ui::BadgeKind::ready;
    juce::String text ("PRONTO");
    juce::String detail (u ("Dubly, cabe\u00e7ote e ClipOnly3 \u00b7 lat\u00eancia 1 amostra."));

    const float flutter = processor.apvts.getRawParameterValue ("flutter")->load();
    const float bias = processor.apvts.getRawParameterValue ("bias")->load();

    if (clipped)
    {
        kind = forja::ui::BadgeKind::clip;
        text = "CLIP";
        detail = u ("Sa\u00edda acima de 0 dBFS \u00b7 reduza a ENTRADA ou SA\u00cdDA.");
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
    else if (flutter < 0.005f)
    {
        detail = u ("Fita est\u00e1vel \u00b7 flutter desligado.");
    }
    else if (bias > 0.65f)
    {
        detail = u ("Bias alto \u00b7 satura\u00e7\u00e3o de fita forte no material.");
    }

    setBadge (kind, text, detail);
}
