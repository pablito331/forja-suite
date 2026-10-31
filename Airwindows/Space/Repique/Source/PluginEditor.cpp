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

RepiqueAudioProcessorEditor::RepiqueAudioProcessorEditor (RepiqueAudioProcessor& p)
    : ForjaEditorBase (
          p, p.apvts,
          { "REPIQUE",
            u ("DE ESPA\u00c7O"),
            "DELAY DE FITA",
            "AIRWINDOWS TAPEDELAY2",
            u ("Delay de fita \u00b7 DSP Airwindows TapeDelay2"),
            u ("DSP Airwindows TapeDelay2 \u00b7 MIT \u00b7 atraso musical, PDC 0") ,
            forja::repique::makeForjaInfo() }),
      processor (p)
{
    configureNote (timeNote, u ("1,00 \u2248 70 ms \u00b7 0,00 \u2248 1,8 s.\nO controle \u00e9 a velocidade\nda cabe\u00e7a, n\u00e3o o atraso."));
    configureNote (tapeNote, u ("REGEN realimenta o filtro.\nFLUTTER modula a velocidade.\nEm 0, os dois ficam limpos."));
    configureNote (filterNote, u ("O filtro fica dentro da\nrealimenta\u00e7\u00e3o. O filtro de\nsa\u00edda \u00e9 1,618\u00d7 mais aberto."));
    configureNote (mixNote, u ("1,00 = s\u00f3 eco \u00b7 0,50 = seco +\neco cheio. \u00c9 submix, n\u00e3o\ncrossfade."));

    addAndMakeVisible (timeCard);
    addAndMakeVisible (tapeCard);
    addAndMakeVisible (filterCard);
    addAndMakeVisible (mixCard);

    timeCard.addAndMakeVisible (timeKnob);
    timeCard.addAndMakeVisible (timeNote);
    timeCard.addAndMakeVisible (syncButton);
    timeCard.addAndMakeVisible (divisionChoice);

    tapeCard.addAndMakeVisible (regenKnob);
    tapeCard.addAndMakeVisible (flutterKnob);
    tapeCard.addAndMakeVisible (tapeNote);
    tapeCard.addAndMakeVisible (reelArt);

    filterCard.addAndMakeVisible (freqKnob);
    filterCard.addAndMakeVisible (resoKnob);
    filterCard.addAndMakeVisible (filterNote);

    mixCard.addAndMakeVisible (mixKnob);
    mixCard.addAndMakeVisible (mixNote);
    mixCard.addAndMakeVisible (outputMeter);
    mixCard.addAndMakeVisible (bypassButton);

    timeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "time", timeKnob.getSlider());
    regenAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "regen", regenKnob.getSlider());
    flutterAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "flutter", flutterKnob.getSlider());
    freqAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "freq", freqKnob.getSlider());
    resoAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "reso", resoKnob.getSlider());
    mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "mix", mixKnob.getSlider());

    bypassButton.setClickingTogglesState (true);
    bypassButton.setTooltip ("Bypass com rampa de 10 ms");
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, "bypass", bypassButton);

    syncButton.setClickingTogglesState (true);
    syncButton.setTooltip (forja::ui::utf8 ("Segue o BPM do host e usa a divisão escolhida"));
    syncAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, "sync", syncButton);

    divisionChoice.getComboBox().onChange = [this]
    {
        if (syncingDivision)
            return;

        const int index = divisionChoice.getComboBox().getSelectedItemIndex();
        if (index >= 0)
            processor.apvts.getParameter ("division")->setValueNotifyingHost (
                (float) index / 5.0f);
    };

    outputMeter.onClipReset = [this]
    {
        processor.resetClip();
        outputMeter.setClip (false);
    };

    const auto onKnobMoved = [this] { refreshReadouts(); };

    timeKnob.onValueChange = onKnobMoved;
    regenKnob.onValueChange = onKnobMoved;
    flutterKnob.onValueChange = onKnobMoved;
    freqKnob.onValueChange = onKnobMoved;
    resoKnob.onValueChange = onKnobMoved;
    mixKnob.onValueChange = onKnobMoved;

    syncDivision();
    updateSyncUi();
    refreshReadouts();

    setSize (windowWidth, windowHeight);
}

juce::String RepiqueAudioProcessorEditor::formatTime (double value) const
{
    double sampleRate = processor.getSampleRate();
    if (sampleRate <= 0.0)
        sampleRate = 48000.0;

    const double speed = (std::pow (value, 4.0) * 25.0) + 1.0;
    const double milliseconds = 88200.0 / speed / sampleRate * 1000.0;

    if (milliseconds >= 1000.0)
        return juce::String (milliseconds / 1000.0, 2) + " s";

    return juce::String (juce::roundToInt (milliseconds)) + " ms";
}

juce::String RepiqueAudioProcessorEditor::formatFrequency (double value) const
{
    double sampleRate = processor.getSampleRate();
    if (sampleRate <= 0.0)
        sampleRate = 48000.0;

    const double hz = (std::pow (value, 3.0) * 0.4 + 0.0001) * sampleRate * 0.5;

    if (hz >= 1000.0)
        return juce::String (hz / 1000.0, 2) + " kHz";

    return juce::String (juce::roundToInt (hz)) + " Hz";
}

juce::String RepiqueAudioProcessorEditor::formatSyncTime() const
{
    const float division = processor.apvts.getRawParameterValue ("division")->load();
    const juce::String label (forja::dsp::syncDivisionLabels[
        forja::dsp::syncDivisionIndex (division)]);

    const double bpm = processor.hostTempo();
    if (bpm <= 0.0)
        return label + " \u00b7 --";

    const double wanted = forja::dsp::syncDivisionSeconds (division, bpm);
    const double actual = processor.syncDelaySeconds();

    juce::String text = label + " \u00b7 ";
    text << (actual >= 1.0 ? juce::String (actual, 2) + " s"
                           : juce::String (juce::roundToInt (actual * 1000.0)) + " ms");

    if (wanted > actual * 1.01)
        text << " m\u00e1x";

    return text;
}

void RepiqueAudioProcessorEditor::syncDivision()
{
    const int index = forja::dsp::syncDivisionIndex (
        processor.apvts.getRawParameterValue ("division")->load());
    auto& combo = divisionChoice.getComboBox();

    if (combo.getSelectedItemIndex() == index || combo.isPopupActive())
        return;

    syncingDivision = true;
    combo.setSelectedItemIndex (index, juce::dontSendNotification);
    syncingDivision = false;
}

void RepiqueAudioProcessorEditor::updateSyncUi()
{
    const bool sync = processor.isSyncEnabled();
    const bool locked = sync && processor.hostTempo() > 0.0;

    if (syncUiInitialized && sync == syncUiSync && locked == syncUiLocked)
        return;

    syncUiInitialized = true;
    syncUiSync = sync;
    syncUiLocked = locked;

    timeKnob.getSlider().setEnabled (! locked);
    timeKnob.setAlpha (locked ? 0.45f : 1.0f);
    divisionChoice.getComboBox().setEnabled (sync);
    divisionChoice.setAlpha (sync ? 1.0f : 0.45f);

    refreshReadouts();
}

void RepiqueAudioProcessorEditor::layoutContent (juce::Rectangle<int> content)
{
    const int y = content.getY();

    timeCard.setBounds (content.getX(), y, 190, cardHeight);
    tapeCard.setBounds (content.getX() + 198, y, 236, cardHeight);
    filterCard.setBounds (content.getX() + 442, y, 294, cardHeight);
    mixCard.setBounds (content.getX() + 744, y, 236, cardHeight);

    const auto timeArea = timeCard.getContentBounds();
    const int timeWidth = timeKnob.getWidth();
    timeKnob.setBounds (timeArea.getX() + (timeArea.getWidth() - timeWidth) / 2,
                        timeArea.getY() + 8, timeWidth, timeKnob.getHeight());
    syncButton.setBounds (timeArea.getX(), timeArea.getY() + 148, timeArea.getWidth(), 24);
    divisionChoice.setBounds (timeArea.getX(), timeArea.getY() + 178, timeArea.getWidth(), 46);
    timeNote.setBounds (timeArea.getX(), timeArea.getY() + 268, timeArea.getWidth(), 44);

    const auto tapeArea = tapeCard.getContentBounds();
    const int tapeWidth = regenKnob.getWidth();
    const int tapeStep = tapeWidth + 4;
    const int tapeX = tapeArea.getX() + (tapeArea.getWidth() - tapeStep * 2 + 4) / 2;

    regenKnob.setBounds (tapeX, tapeArea.getY() + 8, tapeWidth, regenKnob.getHeight());
    flutterKnob.setBounds (tapeX + tapeStep, tapeArea.getY() + 8, tapeWidth,
                           flutterKnob.getHeight());
    tapeNote.setBounds (tapeArea.getX(), tapeArea.getY() + 139, tapeArea.getWidth(), 56);

    const int artTop = tapeArea.getY() + 200;
    reelArt.setBounds (tapeArea.getX() + 6, artTop, tapeArea.getWidth() - 12,
                       juce::jmax (64, tapeArea.getBottom() - artTop - 4));

    const auto filterArea = filterCard.getContentBounds();
    const int filterWidth = freqKnob.getWidth();
    const int filterGap = filterArea.getWidth() - filterWidth * 2;

    freqKnob.setBounds (filterArea.getX(), filterArea.getY() + 8, filterWidth,
                        freqKnob.getHeight());
    resoKnob.setBounds (filterArea.getX() + filterWidth + filterGap, filterArea.getY() + 8,
                        filterWidth, resoKnob.getHeight());
    filterNote.setBounds (filterArea.getX(), filterArea.getY() + 139, filterArea.getWidth(), 70);

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

void RepiqueAudioProcessorEditor::updateTelemetry()
{
    const auto snapshot = processor.readMeters();
    const bool bypassed = processor.isBypassed();

    syncDivision();
    updateSyncUi();

    outputMeter.setLevels (snapshot.outPeakL, snapshot.outPeakR);
    outputMeter.setClip (snapshot.outClip);

    const float inDb = juce::jmax (forja::ui::linearToDb (snapshot.inPeakL),
                                   forja::ui::linearToDb (snapshot.inPeakR));

    refreshBadge (snapshot.outClip, bypassed, inDb);
}

void RepiqueAudioProcessorEditor::refreshReadouts()
{
    const bool sync = processor.isSyncEnabled();

    setMetaValues ({
        { "tempo", sync ? formatSyncTime()
                        : formatTime (timeKnob.getSlider().getValue()) },
        { u ("regen"), juce::String (regenKnob.getSlider().getValue()
                                     * regenKnob.getSlider().getValue() * 100.0, 0) + " %" },
        { "flutter", juce::String (flutterKnob.getSlider().getValue()
                                   * flutterKnob.getSlider().getValue() * 100.0, 0) + " %" },
        { u ("freq"), formatFrequency (freqKnob.getSlider().getValue()) },
        { u ("reso"), juce::String (resoKnob.getSlider().getValue()
                                    * resoKnob.getSlider().getValue() + 0.01, 2) },
        { u ("mix"), juce::String (mixKnob.getSlider().getValue() * 100.0, 0) + " %" }
    });
}

void RepiqueAudioProcessorEditor::refreshBadge (bool clipped, bool bypassed, float inputDb)
{
    auto kind = forja::ui::BadgeKind::ready;
    juce::String text ("PRONTO");
    juce::String detail (u ("Delay de fita \u00b7 regenera\u00e7\u00e3o ressonante e flutter \u00b7 "
                            "PDC 0."));

    const float regen = processor.apvts.getRawParameterValue ("regen")->load();
    const float mix = processor.apvts.getRawParameterValue ("mix")->load();
    const float time = processor.apvts.getRawParameterValue ("time")->load();

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
    else if (regen > 0.95f)
    {
        detail = u ("Regenera\u00e7\u00e3o m\u00e1xima \u00b7 o eco pode crescer sem parar.");
    }
    else if (mix < 0.01f)
    {
        detail = u ("Mistura em zero \u00b7 s\u00f3 o sinal seco sai.");
    }
    else if (processor.isSyncEnabled())
    {
        const float division = processor.apvts.getRawParameterValue ("division")->load();
        const juce::String div (forja::dsp::syncDivisionLabels[
            forja::dsp::syncDivisionIndex (division)]);
        const double bpm = processor.hostTempo();

        if (bpm <= 0.0)
            detail = u ("SYNC ligado, mas o host n\u00e3o informou BPM \u00b7 tempo manual em uso.");
        else
            detail = u ("Sincronizado ao host \u00b7 ") + juce::String (bpm, 1)
                   + " BPM \u00b7 " + div + ".";
    }
    else if (time > 0.98f)
    {
        detail = u ("Tempo curto \u00b7 o delay est\u00e1 em cerca de 70 ms.");
    }

    setBadge (kind, text, detail);
}
