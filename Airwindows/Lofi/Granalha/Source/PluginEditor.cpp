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

GranalhaAudioProcessorEditor::GranalhaAudioProcessorEditor (GranalhaAudioProcessor& p)
    : ForjaEditorBase (
          p, p.apvts,
          { u ("GRANALHA"),
            u ("DE LO-FI"),
            u ("REDUTOR DE BITS"),
            "AIRWINDOWS DEREZ5",
            u ("Lo-fi \u00b7 DSP Airwindows DeRez5"),
            u ("DSP Airwindows DeRez5 \u00b7 MIT \u00b7 lat\u00eancia 0 amostras"),
            forja::granalha::makeForjaInfo() }),
      processor (p)
{
    configureNote (grainNote, u ("Gr\u00e3o de palavra:\na onda volta a ter\ndegraus e curvas\nem vez de linha\ncont\u00ednua."));
    configureNote (bitNote, u ("DEREZ escolhe a\nresolu\u00e7\u00e3o. TRIM\ndosa quanto do\ngr\u00e3o entra."));
    configureNote (shapeNote, u ("QUADRADO endurece o\ndegrau. BRILHO e\nGRAVES s\u00e3o Beziers\nseparados em cima\ne embaixo."));
    configureNote (outputNote, u ("Ganho final depois\ndo gr\u00e3o. 1,00 mant\u00e9m\no n\u00edvel do original."));

    addAndMakeVisible (grainCard);
    addAndMakeVisible (bitCard);
    addAndMakeVisible (shapeCard);
    addAndMakeVisible (outCard);

    grainCard.addAndMakeVisible (grainNote);

    bitCard.addAndMakeVisible (derezKnob);
    bitCard.addAndMakeVisible (trimKnob);
    bitCard.addAndMakeVisible (bitNote);

    shapeCard.addAndMakeVisible (squareKnob);
    shapeCard.addAndMakeVisible (brightKnob);
    shapeCard.addAndMakeVisible (bassisKnob);
    shapeCard.addAndMakeVisible (shapeNote);

    outCard.addAndMakeVisible (outputKnob);
    outCard.addAndMakeVisible (outputNote);
    outCard.addAndMakeVisible (outputMeter);
    outCard.addAndMakeVisible (bypassButton);

    derezAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "derez", derezKnob.getSlider());
    trimAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "trim", trimKnob.getSlider());
    squareAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "square", squareKnob.getSlider());
    brightAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "bright", brightKnob.getSlider());
    bassisAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "bassis", bassisKnob.getSlider());
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

    derezKnob.onValueChange = onKnobMoved;
    trimKnob.onValueChange = onKnobMoved;
    squareKnob.onValueChange = onKnobMoved;
    brightKnob.onValueChange = onKnobMoved;
    bassisKnob.onValueChange = onKnobMoved;
    outputKnob.onValueChange = onKnobMoved;

    refreshReadouts();

    setSize (windowWidth, windowHeight);
}

void GranalhaAudioProcessorEditor::layoutContent (juce::Rectangle<int> content)
{
    const int y = content.getY();

    grainCard.setBounds (content.getX(), y, 190, cardHeight);
    bitCard.setBounds (content.getX() + 198, y, 236, cardHeight);
    shapeCard.setBounds (content.getX() + 442, y, 294, cardHeight);
    outCard.setBounds (content.getX() + 744, y, 236, cardHeight);

    const auto grainArea = grainCard.getContentBounds();
    grainNote.setBounds (grainArea.getX(), grainArea.getY() + 70, grainArea.getWidth(), 110);

    const auto bitArea = bitCard.getContentBounds();
    const int bitX = bitArea.getX() + (bitArea.getWidth() - derezKnob.getWidth()) / 2;
    derezKnob.setBounds (bitX, bitArea.getY() + 8, derezKnob.getWidth(), derezKnob.getHeight());
    trimKnob.setBounds (bitX, bitArea.getY() + 131, trimKnob.getWidth(), trimKnob.getHeight());
    bitNote.setBounds (bitArea.getX(), bitArea.getY() + 258, bitArea.getWidth(), 70);

    const auto shapeArea = shapeCard.getContentBounds();
    const int shapeX = shapeArea.getX()
                       + (shapeArea.getWidth() - squareKnob.getWidth() * 2 - 8) / 2;
    squareKnob.setBounds (shapeX, shapeArea.getY() + 8, squareKnob.getWidth(),
                          squareKnob.getHeight());
    brightKnob.setBounds (shapeX + squareKnob.getWidth() + 8, shapeArea.getY() + 8,
                          brightKnob.getWidth(), brightKnob.getHeight());
    bassisKnob.setBounds (shapeArea.getX() + (shapeArea.getWidth() - bassisKnob.getWidth()) / 2,
                          shapeArea.getY() + 131, bassisKnob.getWidth(), bassisKnob.getHeight());
    shapeNote.setBounds (shapeArea.getX(), shapeArea.getY() + 258, shapeArea.getWidth(), 70);

    const auto outArea = outCard.getContentBounds();
    const int outputX = outArea.getX() + (outArea.getWidth() - outputKnob.getWidth()) / 2;
    outputKnob.setBounds (outputX, outArea.getY() + 8, outputKnob.getWidth(),
                          outputKnob.getHeight());
    outputNote.setBounds (outArea.getX(), outArea.getY() + 139, outArea.getWidth(), 30);

    const int meterTop = outArea.getY() + 172;
    const int bypassTop = outArea.getBottom() - 26;

    outputMeter.setBounds (outArea.getX(), meterTop, outArea.getWidth(), bypassTop - 8 - meterTop);
    bypassButton.setBounds (outArea.getX(), bypassTop, outArea.getWidth(), 26);
}

void GranalhaAudioProcessorEditor::updateTelemetry()
{
    const auto snapshot = processor.readMeters();
    const bool bypassed = processor.isBypassed();

    outputMeter.setLevels (snapshot.outPeakL, snapshot.outPeakR);
    outputMeter.setClip (snapshot.outClip);

    const float inDb = juce::jmax (forja::ui::linearToDb (snapshot.inPeakL),
                                   forja::ui::linearToDb (snapshot.inPeakR));

    refreshBadge (snapshot.outClip, bypassed, inDb);
}

void GranalhaAudioProcessorEditor::refreshReadouts()
{
    setMetaValues ({
        { "derez", juce::String (derezKnob.getSlider().getValue() * 100.0, 0) + " %" },
        { "trim", juce::String (trimKnob.getSlider().getValue() * 100.0, 0) + " %" },
        { "brilho", juce::String (brightKnob.getSlider().getValue() * 100.0, 0) + " %" },
        { "graves", juce::String (bassisKnob.getSlider().getValue() * 100.0, 0) + " %" },
        { u ("sa\u00edda"), juce::String (outputKnob.getSlider().getValue() * 100.0, 0) + " %" }
    });
}

void GranalhaAudioProcessorEditor::refreshBadge (bool clipped, bool bypassed, float inputDb)
{
    auto kind = forja::ui::BadgeKind::ready;
    juce::String text ("PRONTO");
    juce::String detail (u ("Palavra redesenhada em B\u00e9zier \u00b7 lat\u00eancia 0."));

    const float derez = processor.apvts.getRawParameterValue ("derez")->load();
    const float trim = processor.apvts.getRawParameterValue ("trim")->load();

    if (clipped)
    {
        kind = forja::ui::BadgeKind::clip;
        text = "CLIP";
        detail = u ("Sa\u00edda acima de 0 dBFS \u00b7 reduza a sa\u00edda ou o trim.");
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
    else if (derez > 0.999f)
    {
        detail = u ("DEREZ em 1,00 \u00b7 resolu\u00e7\u00e3o intacta, s\u00f3 o trim atua.");
    }
    else if (trim < 0.005f)
    {
        detail = u ("TRIM em 0 \u00b7 o gr\u00e3o n\u00e3o entra no sinal.");
    }
    else
    {
        detail = u ("Gr\u00e3o ativo \u00b7 reduzindo a resolu\u00e7\u00e3o da onda.");
    }

    setBadge (kind, text, detail);
}
