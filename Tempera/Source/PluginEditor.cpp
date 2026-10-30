#include "PluginEditor.h"

#include "ForjaInfoText.h"
#include "PluginProcessor.h"

namespace
{
namespace colour = forja::ui::colour;
}

TemperaAudioProcessorEditor::TemperaAudioProcessorEditor (TemperaAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lookAndFeel);
    setOpaque (true);
    forja::ui::configureResizableEditor (*this, constrainer, windowWidth, windowHeight);
    setSize (windowWidth, windowHeight);

    addAndMakeVisible (topBar);
    addAndMakeVisible (statusBar);

    topBar.setIdentity (forja::ui::utf8 ("TÊMPERA"),
                        forja::ui::utf8 ("DE COMPRESSORES"),
                        "COLA DE BUS",
                        "AIRWINDOWS BUTTERCOMP2");
    statusBar.setIdentity (forja::ui::utf8 ("TÊMPERA"),
                           forja::ui::utf8 ("Compressor de bus · DSP Airwindows ButterComp2"));

    infoPanel = std::make_unique<forja::ui::ForjaInfoPanel> (forja::tempera::makeForjaInfo());
    addChildComponent (*infoPanel);
    infoPanel->onClosed = [this] { topBar.grabKeyboardFocus(); };

    topBar.onFundamentos = [this]
    {
        infoPanel->setBounds (0, 0, windowWidth, windowHeight);
        infoPanel->showPanel();
        infoPanel->toFront (true);
    };

    addAndMakeVisible (compressCard);
    addAndMakeVisible (outputCard);
    addAndMakeVisible (motionCard);
    addAndMakeVisible (mixCard);

    compressCard.addAndMakeVisible (compressKnob);
    compressCard.addAndMakeVisible (gainReductionMeter);
    outputCard.addAndMakeVisible (outputKnob);
    outputCard.addAndMakeVisible (outputMeter);
    motionCard.addAndMakeVisible (grHistory);
    mixCard.addAndMakeVisible (mixKnob);
    mixCard.addAndMakeVisible (bypassButton);

    compressAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "compress", compressKnob.getSlider());
    outputAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "output", outputKnob.getSlider());
    mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "mix", mixKnob.getSlider());

    bypassButton.setClickingTogglesState (true);
    bypassButton.setTooltip ("Bypass com rampa de 10 ms");
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, "bypass", bypassButton);

    const auto resetClips = [this]
    {
        processor.resetClip();
        outputMeter.setClip (false);
        refreshBadge (false, processor.isBypassed(), -100.0f, 0.0f);
    };

    outputMeter.onClipReset = resetClips;

    const auto onKnobMoved = [this] { refreshMeta(); };
    compressKnob.onValueChange = onKnobMoved;
    outputKnob.onValueChange = onKnobMoved;
    mixKnob.onValueChange = onKnobMoved;

    bypassButton.onClick = [this]
    {
        lastBypass = processor.isBypassed();
        topBar.setBypassed (lastBypass);
    };

    lastBypass = processor.isBypassed();
    lastSampleRate = processor.getSampleRate();
    lastBlockSize = processor.getBlockSize();
    topBar.setBypassed (lastBypass);
    refreshMeta();
    refreshBadge (false, lastBypass, -100.0f, 0.0f);

    startTimerHz (30);
}

TemperaAudioProcessorEditor::~TemperaAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void TemperaAudioProcessorEditor::paint (juce::Graphics& g)
{
    juce::ColourGradient background (juce::Colour (0xff1c212a), 500.0f, 0.0f,
                                     colour::iron1, 500.0f, (float) getHeight(), false);
    g.setGradientFill (background);
    g.fillAll();

    g.setColour (juce::Colour (0xff424954));
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 5.0f, 1.0f);
}

void TemperaAudioProcessorEditor::resized()
{
    topBar.setBounds (topBarBounds);
    statusBar.setBounds (statusBounds);

    compressCard.setBounds (7, 63, 190, 388);
    outputCard.setBounds (205, 63, 236, 388);
    motionCard.setBounds (449, 63, 294, 388);
    mixCard.setBounds (751, 63, 236, 388);

    compressKnob.setBounds ((compressCard.getWidth() - compressKnob.getWidth()) / 2, 70,
                            compressKnob.getWidth(), compressKnob.getHeight());
    gainReductionMeter.setBounds (9, compressCard.getHeight() - 11 - 152,
                                  compressCard.getWidth() - 18, 152);

    outputKnob.setBounds ((outputCard.getWidth() - outputKnob.getWidth()) / 2, 56,
                          outputKnob.getWidth(), outputKnob.getHeight());
    outputMeter.setBounds (9, outputCard.getHeight() - 11 - 152,
                           outputCard.getWidth() - 18, 152);

    grHistory.setBounds (9, 44, motionCard.getWidth() - 18, motionCard.getHeight() - 55);

    mixKnob.setBounds ((mixCard.getWidth() - mixKnob.getWidth()) / 2, 56,
                       mixKnob.getWidth(), mixKnob.getHeight());
    bypassButton.setBounds (9, mixCard.getHeight() - 11 - 26,
                            mixCard.getWidth() - 18, 26);

    if (infoPanel != nullptr)
        infoPanel->setBounds (0, 0, windowWidth, windowHeight);

    forja::ui::applyCanvasScale (*this, windowWidth, windowHeight);
}

void TemperaAudioProcessorEditor::timerCallback()
{
    const auto snapshot = processor.readMeters();

    gainReductionMeter.setGainReductionDb (snapshot.gainReductionDb, snapshot.gainReductionDb);
    gainReductionMeter.setBypassed (processor.isBypassed());
    grHistory.pushGainReductionDb (snapshot.gainReductionDb, snapshot.gainReductionDb);
    grHistory.setBypassed (processor.isBypassed());
    outputMeter.setLevels (snapshot.outPeakL, snapshot.outPeakR);
    outputMeter.setClip (snapshot.outClip);

    const bool bypassed = processor.isBypassed();
    const bool clipped = snapshot.outClip;

    const float inDb = juce::jmax (forja::ui::linearToDb (snapshot.inPeakL),
                                   forja::ui::linearToDb (snapshot.inPeakR));

    refreshBadge (clipped, bypassed, inDb, snapshot.gainReductionDb);

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

void TemperaAudioProcessorEditor::refreshBadge (bool clipped, bool bypassed, float inputDb, float grDb)
{
    auto kind = forja::ui::BadgeKind::ready;
    juce::String text ("PRONTO");
    juce::String detail = forja::ui::utf8 ("Quatro compressores paralelos em ação · DSP Airwindows ButterComp2.");

    if (clipped)
    {
        kind = forja::ui::BadgeKind::clip;
        text = "CLIP";
        detail = forja::ui::utf8 ("Saída acima de 0 dBFS · abaixe a saída ou a compressão.");
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
    else if (grDb < -0.1f)
    {
        detail = forja::ui::utf8 ("Comprimindo · ")
               + juce::String (grDb, 1) + " dB de redução de ganho.";
    }

    statusBar.setBadge (kind, text, detail);
}

void TemperaAudioProcessorEditor::refreshMeta()
{
    statusBar.setMeta (processor.getSampleRate(),
                       processor.getBlockSize(),
                       {
                           { forja::ui::utf8 ("compressão"), juce::String (compressKnob.getSlider().getValue(), 2) },
                           { forja::ui::utf8 ("saída"), juce::String (outputKnob.getSlider().getValue(), 2) },
                           { "mistura", juce::String (mixKnob.getSlider().getValue(), 2) }
                       });
    statusBar.setFooter (forja::ui::utf8 ("DSP Airwindows ButterComp2 · MIT · latência 0 amostras"));
}
