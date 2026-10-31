#include "PluginEditor.h"

#include "ForjaInfoText.h"
#include "PluginProcessor.h"

namespace
{
namespace colour = forja::ui::colour;
}

PrensaAudioProcessorEditor::PrensaAudioProcessorEditor (PrensaAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lookAndFeel);
    setOpaque (true);
    forja::ui::configureResizableEditor (*this, constrainer, windowWidth, windowHeight);
    setSize (windowWidth, windowHeight);

    addAndMakeVisible (topBar);
    addAndMakeVisible (statusBar);

    topBar.setIdentity (forja::ui::utf8 ("PRENSA"),
                        forja::ui::utf8 ("DE COMPRESSORES"),
                        "VARI-MU",
                        "AIRWINDOWS PRESSURE5");
    statusBar.setIdentity (forja::ui::utf8 ("PRENSA"),
                           forja::ui::utf8 ("Compressor vari-mu · DSP Airwindows Pressure5"));

    infoPanel = std::make_unique<forja::ui::ForjaInfoPanel> (forja::prensa::makeForjaInfo());
    addChildComponent (*infoPanel);
    infoPanel->onClosed = [this] { topBar.grabKeyboardFocus(); };

    topBar.onFundamentos = [this]
    {
        infoPanel->setBounds (0, 0, windowWidth, windowHeight);
        infoPanel->showPanel();
        infoPanel->toFront (true);
    };

    addAndMakeVisible (pressureCard);
    addAndMakeVisible (timeCard);
    addAndMakeVisible (clawCard);
    addAndMakeVisible (outputCard);

    pressureCard.addAndMakeVisible (pressureKnob);
    pressureCard.addAndMakeVisible (gainReductionMeter);
    timeCard.addAndMakeVisible (speedKnob);
    timeCard.addAndMakeVisible (mewinessKnob);
    clawCard.addAndMakeVisible (pawclawKnob);
    clawCard.addAndMakeVisible (grHistory);
    outputCard.addAndMakeVisible (outputKnob);
    outputCard.addAndMakeVisible (mixKnob);
    outputCard.addAndMakeVisible (outputMeter);
    outputCard.addAndMakeVisible (bypassButton);

    pressureAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "pressure", pressureKnob.getSlider());
    speedAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "speed", speedKnob.getSlider());
    mewinessAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "mewiness", mewinessKnob.getSlider());
    pawclawAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "pawclaw", pawclawKnob.getSlider());
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
    pressureKnob.onValueChange = onKnobMoved;
    speedKnob.onValueChange = onKnobMoved;
    mewinessKnob.onValueChange = onKnobMoved;
    pawclawKnob.onValueChange = onKnobMoved;
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

PrensaAudioProcessorEditor::~PrensaAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void PrensaAudioProcessorEditor::paint (juce::Graphics& g)
{
    juce::ColourGradient background (juce::Colour (0xff1c212a), 500.0f, 0.0f,
                                     colour::iron1, 500.0f, (float) getHeight(), false);
    g.setGradientFill (background);
    g.fillAll();

    g.setColour (juce::Colour (0xff424954));
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 5.0f, 1.0f);
}

void PrensaAudioProcessorEditor::resized()
{
    topBar.setBounds (topBarBounds);
    statusBar.setBounds (statusBounds);

    pressureCard.setBounds (7, 63, 190, 388);
    timeCard.setBounds (205, 63, 236, 388);
    clawCard.setBounds (449, 63, 294, 388);
    outputCard.setBounds (751, 63, 236, 388);

    pressureKnob.setBounds ((pressureCard.getWidth() - pressureKnob.getWidth()) / 2, 70,
                            pressureKnob.getWidth(), pressureKnob.getHeight());
    gainReductionMeter.setBounds (9, pressureCard.getHeight() - 11 - 152,
                                  pressureCard.getWidth() - 18, 152);

    speedKnob.setBounds ((timeCard.getWidth() - speedKnob.getWidth()) / 2, 56,
                         speedKnob.getWidth(), speedKnob.getHeight());
    mewinessKnob.setBounds ((timeCard.getWidth() - mewinessKnob.getWidth()) / 2, 181,
                            mewinessKnob.getWidth(), mewinessKnob.getHeight());

    pawclawKnob.setBounds ((clawCard.getWidth() - pawclawKnob.getWidth()) / 2, 44,
                           pawclawKnob.getWidth(), pawclawKnob.getHeight());
    grHistory.setBounds (9, 165, clawCard.getWidth() - 18, 190);

    outputKnob.setBounds ((outputCard.getWidth() - outputKnob.getWidth()) / 2, 44,
                          outputKnob.getWidth(), outputKnob.getHeight());
    mixKnob.setBounds ((outputCard.getWidth() - mixKnob.getWidth()) / 2, 151,
                       mixKnob.getWidth(), mixKnob.getHeight());
    outputMeter.setBounds (9, outputCard.getHeight() - 11 - 26 - 8 - 80,
                           outputCard.getWidth() - 18, 80);
    bypassButton.setBounds (9, outputCard.getHeight() - 11 - 26,
                            outputCard.getWidth() - 18, 26);

    if (infoPanel != nullptr)
        infoPanel->setBounds (0, 0, windowWidth, windowHeight);

    forja::ui::applyCanvasScale (*this, windowWidth, windowHeight);
}

void PrensaAudioProcessorEditor::timerCallback()
{
    const auto snapshot = processor.readMeters();
    const bool bypassed = processor.isBypassed();

    gainReductionMeter.setGainReductionDb (snapshot.gainReductionDb, snapshot.gainReductionDb);
    gainReductionMeter.setBypassed (bypassed);
    grHistory.pushGainReductionDb (snapshot.gainReductionDb, snapshot.gainReductionDb);
    grHistory.setBypassed (bypassed);
    outputMeter.setLevels (snapshot.outPeakL, snapshot.outPeakR);
    outputMeter.setClip (snapshot.outClip);

    const float inDb = juce::jmax (forja::ui::linearToDb (snapshot.inPeakL),
                                   forja::ui::linearToDb (snapshot.inPeakR));

    refreshBadge (snapshot.outClip, bypassed, inDb, snapshot.gainReductionDb);

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

void PrensaAudioProcessorEditor::refreshBadge (bool clipped, bool bypassed, float inputDb, float grDb)
{
    auto kind = forja::ui::BadgeKind::ready;
    juce::String text ("PRONTO");
    juce::String detail = forja::ui::utf8 ("Vari-mu em ação · detector estéreo linkado.");

    if (clipped)
    {
        kind = forja::ui::BadgeKind::clip;
        text = "CLIP";
        detail = forja::ui::utf8 ("Saída acima de 0 dBFS · abaixe a saída ou a pressão.");
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

void PrensaAudioProcessorEditor::refreshMeta()
{
    statusBar.setMeta (processor.getSampleRate(),
                       processor.getBlockSize(),
                       {
                           { forja::ui::utf8 ("pressão"), juce::String (pressureKnob.getSlider().getValue(), 2) },
                           { "velocidade", juce::String (speedKnob.getSlider().getValue(), 2) },
                           { "miado", juce::String (mewinessKnob.getSlider().getValue(), 2) },
                           { "garra", juce::String (pawclawKnob.getSlider().getValue(), 2) },
                           { forja::ui::utf8 ("saída"), juce::String (outputKnob.getSlider().getValue(), 2) }
                       });
    statusBar.setFooter (forja::ui::utf8 ("DSP Airwindows Pressure5 · MIT · latência 1 amostra"));
}
