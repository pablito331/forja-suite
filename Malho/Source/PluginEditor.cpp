#include "PluginEditor.h"

#include "ForjaInfoText.h"
#include "PluginProcessor.h"

namespace
{
namespace colour = forja::ui::colour;
}

MalhoAudioProcessorEditor::MalhoAudioProcessorEditor (MalhoAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lookAndFeel);
    setOpaque (true);
    forja::ui::configureResizableEditor (*this, constrainer, windowWidth, windowHeight);
    setSize (windowWidth, windowHeight);

    addAndMakeVisible (topBar);
    addAndMakeVisible (statusBar);

    topBar.setIdentity ("MALHO",
                        forja::ui::utf8 ("DE COMPRESSORES"),
                        "IMPACTO",
                        "AIRWINDOWS POP2");
    statusBar.setIdentity ("MALHO",
                           forja::ui::utf8 ("Compressor de impacto · DSP Airwindows Pop2"));

    infoPanel = std::make_unique<forja::ui::ForjaInfoPanel> (forja::malho::makeForjaInfo());
    addChildComponent (*infoPanel);
    infoPanel->onClosed = [this] { topBar.grabKeyboardFocus(); };

    topBar.onFundamentos = [this]
    {
        infoPanel->setBounds (0, 0, windowWidth, windowHeight);
        infoPanel->showPanel();
        infoPanel->toFront (true);
    };

    addAndMakeVisible (compressCard);
    addAndMakeVisible (timeCard);
    addAndMakeVisible (driveCard);
    addAndMakeVisible (mixCard);

    compressCard.addAndMakeVisible (compressKnob);
    compressCard.addAndMakeVisible (gainReductionMeter);
    timeCard.addAndMakeVisible (attackKnob);
    timeCard.addAndMakeVisible (releaseKnob);
    driveCard.addAndMakeVisible (driveKnob);
    driveCard.addAndMakeVisible (grHistory);
    mixCard.addAndMakeVisible (mixKnob);
    mixCard.addAndMakeVisible (outputMeter);
    mixCard.addAndMakeVisible (bypassButton);

    compressAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "compress", compressKnob.getSlider());
    attackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "attack", attackKnob.getSlider());
    releaseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "release", releaseKnob.getSlider());
    driveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "drive", driveKnob.getSlider());
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
    attackKnob.onValueChange = onKnobMoved;
    releaseKnob.onValueChange = onKnobMoved;
    driveKnob.onValueChange = onKnobMoved;
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

MalhoAudioProcessorEditor::~MalhoAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void MalhoAudioProcessorEditor::paint (juce::Graphics& g)
{
    juce::ColourGradient background (juce::Colour (0xff1c212a), 500.0f, 0.0f,
                                     colour::iron1, 500.0f, (float) getHeight(), false);
    g.setGradientFill (background);
    g.fillAll();

    g.setColour (juce::Colour (0xff424954));
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 5.0f, 1.0f);
}

void MalhoAudioProcessorEditor::resized()
{
    topBar.setBounds (topBarBounds);
    statusBar.setBounds (statusBounds);

    compressCard.setBounds (7, 63, 190, 388);
    timeCard.setBounds (205, 63, 236, 388);
    driveCard.setBounds (449, 63, 294, 388);
    mixCard.setBounds (751, 63, 236, 388);

    compressKnob.setBounds ((compressCard.getWidth() - compressKnob.getWidth()) / 2, 70,
                            compressKnob.getWidth(), compressKnob.getHeight());
    gainReductionMeter.setBounds (9, compressCard.getHeight() - 11 - 152,
                                  compressCard.getWidth() - 18, 152);

    attackKnob.setBounds ((timeCard.getWidth() - attackKnob.getWidth()) / 2, 56,
                          attackKnob.getWidth(), attackKnob.getHeight());
    releaseKnob.setBounds ((timeCard.getWidth() - releaseKnob.getWidth()) / 2, 181,
                           releaseKnob.getWidth(), releaseKnob.getHeight());

    driveKnob.setBounds ((driveCard.getWidth() - driveKnob.getWidth()) / 2, 44,
                         driveKnob.getWidth(), driveKnob.getHeight());
    grHistory.setBounds (9, 165, driveCard.getWidth() - 18, 190);

    mixKnob.setBounds ((mixCard.getWidth() - mixKnob.getWidth()) / 2, 56,
                       mixKnob.getWidth(), mixKnob.getHeight());
    outputMeter.setBounds (9, mixCard.getHeight() - 11 - 26 - 8 - 152,
                           mixCard.getWidth() - 18, 152);
    bypassButton.setBounds (9, mixCard.getHeight() - 11 - 26,
                            mixCard.getWidth() - 18, 26);

    if (infoPanel != nullptr)
        infoPanel->setBounds (0, 0, windowWidth, windowHeight);

    forja::ui::applyCanvasScale (*this, windowWidth, windowHeight);
}

void MalhoAudioProcessorEditor::timerCallback()
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

void MalhoAudioProcessorEditor::refreshBadge (bool clipped, bool bypassed, float inputDb, float grDb)
{
    auto kind = forja::ui::BadgeKind::ready;
    juce::String text ("PRONTO");
    juce::String detail = forja::ui::utf8 ("Envelopes independentes L/R · ataque e soltura separados.");

    if (clipped)
    {
        kind = forja::ui::BadgeKind::clip;
        text = "CLIP";
        detail = forja::ui::utf8 ("Saída acima de 0 dBFS · abaixe a mistura ou a compressão.");
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

void MalhoAudioProcessorEditor::refreshMeta()
{
    statusBar.setMeta (processor.getSampleRate(),
                       processor.getBlockSize(),
                       {
                           { forja::ui::utf8 ("compressão"), juce::String (compressKnob.getSlider().getValue(), 2) },
                           { "ataque", juce::String (attackKnob.getSlider().getValue(), 2) },
                           { "soltura", juce::String (releaseKnob.getSlider().getValue(), 2) },
                           { "drive", juce::String (driveKnob.getSlider().getValue(), 2) },
                           { "mistura", juce::String (mixKnob.getSlider().getValue(), 2) }
                       });
    statusBar.setFooter (forja::ui::utf8 ("DSP Airwindows Pop2 · MIT · latência 1 amostra"));
}
