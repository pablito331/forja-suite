#include "PluginEditor.h"

#include "ForjaInfoText.h"
#include "PluginProcessor.h"

#include <cmath>

namespace
{
namespace colour = forja::ui::colour;
}

BrasaAudioProcessorEditor::BrasaAudioProcessorEditor (BrasaAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lookAndFeel);
    setOpaque (true);
    forja::ui::configureResizableEditor (*this, constrainer, windowWidth, windowHeight);
    setSize (windowWidth, windowHeight);

    addAndMakeVisible (topBar);
    addAndMakeVisible (statusBar);

    topBar.setIdentity ("Brasa",
                        forja::ui::utf8 ("DE VÁLVULAS"),
                        forja::ui::utf8 ("SATURADOR DE VÁLVULA"),
                        "AIRWINDOWS TUBE2");
    statusBar.setIdentity ("Brasa",
                           forja::ui::utf8 ("Saturador de válvula · DSP Airwindows Tube2"));

    infoPanel = std::make_unique<forja::ui::ForjaInfoPanel> (forja::brasa::makeForjaInfo());
    addChildComponent (*infoPanel);
    infoPanel->onClosed = [this] { topBar.grabKeyboardFocus(); };

    topBar.onFundamentos = [this]
    {
        infoPanel->setBounds (0, 0, windowWidth, windowHeight);
        infoPanel->showPanel();
        infoPanel->toFront (true);
    };

    addAndMakeVisible (inputCard);
    addAndMakeVisible (valveCard);
    addAndMakeVisible (curveCard);
    addAndMakeVisible (outputCard);

    inputCard.addAndMakeVisible (inputKnob);
    inputCard.addAndMakeVisible (inputMeter);
    valveCard.addAndMakeVisible (valve);
    valveCard.addAndMakeVisible (tubeKnob);
    valveCard.addAndMakeVisible (valveCaption);
    curveCard.addAndMakeVisible (curve);
    outputCard.addAndMakeVisible (outputKnob);
    outputCard.addAndMakeVisible (outputMeter);
    outputCard.addAndMakeVisible (bypassButton);

    valveCaption.setText (forja::ui::utf8 ("SATURAÇÃO ASIMÉTRICA"), juce::dontSendNotification);
    valveCaption.setFont (forja::ui::tracked (forja::ui::uiFont (7.0f, juce::Font::bold), 0.16f));
    valveCaption.setColour (juce::Label::textColourId, juce::Colour (0xff64707d));
    valveCaption.setJustificationType (juce::Justification::centred);
    valveCaption.setInterceptsMouseClicks (false, false);

    inputAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "input", inputKnob.getSlider());
    tubeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "tube", tubeKnob.getSlider());
    outputAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "output", outputKnob.getSlider());

    bypassButton.setClickingTogglesState (true);
    bypassButton.setTooltip ("Bypass com rampa de 10 ms");
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, "bypass", bypassButton);

    const auto resetClips = [this]
    {
        processor.resetClip();
        inputMeter.setClip (false);
        outputMeter.setClip (false);
        refreshBadge (false, processor.isBypassed(), -100.0f);
    };

    inputMeter.onClipReset = resetClips;
    outputMeter.onClipReset = resetClips;

    const auto onKnobMoved = [this]
    {
        curve.setState ((float) inputKnob.getSlider().getValue(),
                        (float) tubeKnob.getSlider().getValue(),
                        processor.isBypassed());
        refreshMeta();
    };

    inputKnob.onValueChange = onKnobMoved;
    tubeKnob.onValueChange = onKnobMoved;
    outputKnob.onValueChange = onKnobMoved;

    bypassButton.onClick = [this]
    {
        lastBypass = processor.isBypassed();
        topBar.setBypassed (lastBypass);
        curve.setState ((float) inputKnob.getSlider().getValue(),
                        (float) tubeKnob.getSlider().getValue(),
                        lastBypass);
    };

    lastBypass = processor.isBypassed();
    lastSampleRate = processor.getSampleRate();
    lastBlockSize = processor.getBlockSize();
    topBar.setBypassed (lastBypass);
    refreshMeta();
    refreshBadge (false, lastBypass, -100.0f);

    startTimerHz (30);
}

BrasaAudioProcessorEditor::~BrasaAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void BrasaAudioProcessorEditor::paint (juce::Graphics& g)
{
    juce::ColourGradient background (juce::Colour (0xff1c212a), 500.0f, 0.0f,
                                     colour::iron1, 500.0f, (float) getHeight(), false);
    g.setGradientFill (background);
    g.fillAll();

    g.setColour (juce::Colour (0xff424954));
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 5.0f, 1.0f);
}

void BrasaAudioProcessorEditor::resized()
{
    topBar.setBounds (topBarBounds);
    statusBar.setBounds (statusBounds);

    inputCard.setBounds (7, 63, 190, 388);
    valveCard.setBounds (207, 63, 236, 388);
    curveCard.setBounds (453, 63, 324, 388);
    outputCard.setBounds (787, 63, 206, 388);

    inputKnob.setBounds ((inputCard.getWidth() - inputKnob.getWidth()) / 2, 70,
                         inputKnob.getWidth(), inputKnob.getHeight());
    inputMeter.setBounds (9, inputCard.getHeight() - 11 - 152,
                          inputCard.getWidth() - 18, 152);

    tubeKnob.setBounds ((valveCard.getWidth() - tubeKnob.getWidth()) / 2, 224,
                        tubeKnob.getWidth(), tubeKnob.getHeight());
    valve.setBounds (9, 44, valveCard.getWidth() - 18,
                     tubeKnob.getY() - 8 - 44);
    valveCaption.setBounds (9, valveCard.getHeight() - 21, valveCard.getWidth() - 18, 10);

    curve.setBounds (9, 44, curveCard.getWidth() - 18, curveCard.getHeight() - 55);

    outputKnob.setBounds ((outputCard.getWidth() - outputKnob.getWidth()) / 2, 56,
                          outputKnob.getWidth(), outputKnob.getHeight());
    outputMeter.setBounds (9, outputCard.getHeight() - 11 - 26 - 8 - 152,
                           outputCard.getWidth() - 18, 152);
    bypassButton.setBounds (9, outputCard.getHeight() - 11 - 26,
                            outputCard.getWidth() - 18, 26);

    if (infoPanel != nullptr)
        infoPanel->setBounds (0, 0, windowWidth, windowHeight);

    forja::ui::applyCanvasScale (*this, windowWidth, windowHeight);
}

void BrasaAudioProcessorEditor::timerCallback()
{
    const auto snapshot = processor.readMeters();

    inputMeter.setLevels (snapshot.inPeakL, snapshot.inPeakR);
    inputMeter.setClip (snapshot.inClip);
    outputMeter.setLevels (snapshot.outPeakL, snapshot.outPeakR);
    outputMeter.setClip (snapshot.outClip);

    const bool bypassed = processor.isBypassed();
    const bool clipped = snapshot.inClip || snapshot.outClip;

    const float inDb = juce::jmax (forja::ui::linearToDb (snapshot.inPeakL),
                                   forja::ui::linearToDb (snapshot.inPeakR));

    float heat = 0.1f;
    if (! bypassed)
    {
        const float linear = std::pow (10.0f, juce::jmax (-100.0f, inDb) * 0.05f);
        heat = juce::jlimit (0.1f, 0.95f,
                             0.1f + (float) tubeKnob.getSlider().getValue() * 0.42f
                                 + std::sqrt (linear) * 0.55f);
    }
    valve.setHeat (heat);

    curve.setState ((float) inputKnob.getSlider().getValue(),
                    (float) tubeKnob.getSlider().getValue(),
                    bypassed);

    refreshBadge (clipped, bypassed, inDb);

    if (bypassed != lastBypass)
    {
        lastBypass = bypassed;
        topBar.setBypassed (bypassed);
    }

    if (processor.getSampleRate() != lastSampleRate || processor.getBlockSize() != lastBlockSize)
    {
        lastSampleRate = processor.getSampleRate();
        lastBlockSize = processor.getBlockSize();
        refreshMeta();
    }
}

void BrasaAudioProcessorEditor::refreshBadge (bool clipped, bool bypassed, float inputDb)
{
    auto kind = forja::ui::BadgeKind::ready;
    juce::String text ("PRONTO");
    juce::String detail = forja::ui::utf8 ("Processando na DAW · DSP Airwindows Tube2.");

    if (clipped)
    {
        kind = forja::ui::BadgeKind::clip;
        text = "CLIP";
        detail = forja::ui::utf8 ("Saída acima de 0 dBFS · reduza a saída ou a válvula.");
    }
    else if (bypassed)
    {
        kind = forja::ui::BadgeKind::bypass;
        text = "BYPASS";
        detail = forja::ui::utf8 ("Bypass ativo · sinal seco com rampa de 10 ms.");
    }
    else if (inputDb <= -60.0f)
    {
        kind = forja::ui::BadgeKind::silent;
        text = "SEM SINAL";
        detail = forja::ui::utf8 ("Sem sinal na entrada · aguardando áudio da DAW.");
    }

    statusBar.setBadge (kind, text, detail);
}

void BrasaAudioProcessorEditor::refreshMeta()
{
    statusBar.setMeta (processor.getSampleRate(),
                       processor.getBlockSize(),
                       {
                           { "entrada", juce::String (inputKnob.getSlider().getValue(), 2) },
                           { forja::ui::utf8 ("válvula"), juce::String (tubeKnob.getSlider().getValue(), 2) },
                           { forja::ui::utf8 ("saída"), outputKnob.getSlider().getTextFromValue (outputKnob.getSlider().getValue()) }
                       });
    statusBar.setFooter (forja::ui::utf8 ("DSP Airwindows Tube2 · MIT · latência 0 amostras"));
}
