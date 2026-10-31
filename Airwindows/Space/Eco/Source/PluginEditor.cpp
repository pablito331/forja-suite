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

EcoAudioProcessorEditor::EcoAudioProcessorEditor (EcoAudioProcessor& p)
    : ForjaEditorBase (
          p, p.apvts,
          { "ECO",
            u ("DE ESPA\u00c7O"),
            u ("ESPA\u00c7OS DE ECOJAM"),
            "AIRWINDOWS ECHOJAM",
            u ("Reverb de espa\u00e7os \u00b7 DSP Airwindows Echojam"),
            u ("DSP Airwindows Echojam \u00b7 MIT \u00b7 atraso musical, PDC 0"),
            forja::eco::makeForjaInfo() }),
      processor (p)
{
    configureNote (spaceNote, u ("17 espa\u00e7os fixos, cada um com\nseu pr\u00f3prio tempo de\n"
                                 "parede e n\u00famero de lugares."));
    configureNote (echoNote, u ("REGEN em 0 deixa uma\npassagem \u00fanica.\n"
                                "VELOCIDADE define o\nn\u00famero de amostras internas."));
    configureNote (mixNote, u ("0,50 j\u00e1 \u00e9 metade molhado.\n1,00 = s\u00f3 espa\u00e7o \u00b7 0,00 = s\u00f3\nseco."));

    addAndMakeVisible (spaceCard);
    addAndMakeVisible (echoCard);
    addAndMakeVisible (mixCard);
    addAndMakeVisible (outCard);

    spaceCard.addAndMakeVisible (spaceChoice);
    spaceCard.addAndMakeVisible (spaceNote);

    echoCard.addAndMakeVisible (regenKnob);
    echoCard.addAndMakeVisible (speedKnob);
    echoCard.addAndMakeVisible (echoNote);
    echoCard.addAndMakeVisible (syncButton);
    echoCard.addAndMakeVisible (divisionChoice);

    mixCard.addAndMakeVisible (mixKnob);
    mixCard.addAndMakeVisible (mixNote);

    outCard.addAndMakeVisible (outputMeter);
    outCard.addAndMakeVisible (bypassButton);

    regenAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "regen", regenKnob.getSlider());
    speedAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "speed", speedKnob.getSlider());
    mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "mix", mixKnob.getSlider());

    spaceChoice.getComboBox().onChange = [this]
    {
        if (syncingSpace)
            return;

        const int index = spaceChoice.getComboBox().getSelectedItemIndex();
        if (index >= 0)
            processor.apvts.getParameter ("select")->setValueNotifyingHost ((float) index / 16.0f);
    };

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

    regenKnob.onValueChange = onKnobMoved;
    speedKnob.onValueChange = onKnobMoved;
    mixKnob.onValueChange = onKnobMoved;

    syncSpace();
    syncDivision();
    updateSyncUi();
    refreshReadouts();

    setSize (windowWidth, windowHeight);
}

void EcoAudioProcessorEditor::syncSpace()
{
    const int space = processor.selectedSpace();
    auto& combo = spaceChoice.getComboBox();

    if (combo.getSelectedItemIndex() == space || combo.isPopupActive())
        return;

    syncingSpace = true;
    combo.setSelectedItemIndex (space, juce::dontSendNotification);
    syncingSpace = false;
}

juce::String EcoAudioProcessorEditor::formatSyncTime() const
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

void EcoAudioProcessorEditor::syncDivision()
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

void EcoAudioProcessorEditor::updateSyncUi()
{
    const bool sync = processor.isSyncEnabled();
    const bool locked = sync && processor.hostTempo() > 0.0;

    if (syncUiInitialized && sync == syncUiSync && locked == syncUiLocked)
        return;

    syncUiInitialized = true;
    syncUiSync = sync;
    syncUiLocked = locked;

    speedKnob.getSlider().setEnabled (! locked);
    speedKnob.setAlpha (locked ? 0.45f : 1.0f);
    divisionChoice.getComboBox().setEnabled (sync);
    divisionChoice.setAlpha (sync ? 1.0f : 0.45f);

    refreshReadouts();
}

void EcoAudioProcessorEditor::layoutContent (juce::Rectangle<int> content)
{
    const int y = content.getY();

    spaceCard.setBounds (content.getX(), y, 190, cardHeight);
    echoCard.setBounds (content.getX() + 198, y, 236, cardHeight);
    mixCard.setBounds (content.getX() + 442, y, 294, cardHeight);
    outCard.setBounds (content.getX() + 744, y, 236, cardHeight);

    const auto spaceArea = spaceCard.getContentBounds();
    spaceChoice.setBounds (spaceArea.getX(), spaceArea.getY() + 8, spaceArea.getWidth(), 58);
    spaceNote.setBounds (spaceArea.getX(), spaceArea.getY() + 86, spaceArea.getWidth(), 70);

    const auto echoArea = echoCard.getContentBounds();
    const int echoWidth = regenKnob.getWidth();
    const int echoStep = echoWidth + 4;
    const int echoX = echoArea.getX() + (echoArea.getWidth() - echoStep * 2 + 4) / 2;

    regenKnob.setBounds (echoX, echoArea.getY() + 8, echoWidth, regenKnob.getHeight());
    speedKnob.setBounds (echoX + echoStep, echoArea.getY() + 8, echoWidth,
                         speedKnob.getHeight());
    echoNote.setBounds (echoArea.getX(), echoArea.getY() + 139, echoArea.getWidth(), 90);
    syncButton.setBounds (echoArea.getX(), echoArea.getY() + 240, echoArea.getWidth(), 24);
    divisionChoice.setBounds (echoArea.getX(), echoArea.getY() + 268, echoArea.getWidth(), 46);

    const auto mixArea = mixCard.getContentBounds();
    const int mixWidth = mixKnob.getWidth();
    const int mixX = mixArea.getX() + (mixArea.getWidth() - mixWidth) / 2;

    mixKnob.setBounds (mixX, mixArea.getY() + 8, mixWidth, mixKnob.getHeight());
    mixNote.setBounds (mixArea.getX(), mixArea.getY() + 139, mixArea.getWidth(), 60);

    const auto outArea = outCard.getContentBounds();
    const int bypassTop = outArea.getBottom() - 26;

    outputMeter.setBounds (outArea.getX(), outArea.getY() + 8, outArea.getWidth(),
                           bypassTop - 16 - (outArea.getY() + 8));
    bypassButton.setBounds (outArea.getX(), bypassTop, outArea.getWidth(), 26);
}

void EcoAudioProcessorEditor::updateTelemetry()
{
    const auto snapshot = processor.readMeters();
    const bool bypassed = processor.isBypassed();

    syncSpace();
    syncDivision();
    updateSyncUi();

    outputMeter.setLevels (snapshot.outPeakL, snapshot.outPeakR);
    outputMeter.setClip (snapshot.outClip);

    const float inDb = juce::jmax (forja::ui::linearToDb (snapshot.inPeakL),
                                   forja::ui::linearToDb (snapshot.inPeakR));

    refreshBadge (snapshot.outClip, bypassed, inDb);
}

void EcoAudioProcessorEditor::refreshReadouts()
{
    const int space = processor.selectedSpace() + 1;

    setMetaValues ({
        { u ("espa\u00e7o"), juce::String (space).paddedLeft ('0', 2) + " / 17" },
        { u ("regen"), juce::String (regenKnob.getSlider().getValue() * 100.0, 0) + " %" },
        { "speed", processor.isSyncEnabled()
                       ? formatSyncTime()
                       : juce::String (speedKnob.getSlider().getValue() * 100.0, 0) + " %" },
        { u ("mix"), juce::String (mixKnob.getSlider().getValue() * 100.0, 0) + " %" }
    });
}

void EcoAudioProcessorEditor::refreshBadge (bool clipped, bool bypassed, float inputDb)
{
    auto kind = forja::ui::BadgeKind::ready;
    juce::String text ("PRONTO");
    juce::String detail (u ("Reverb de espa\u00e7os \u00b7 16 taps por canal \u00b7 PDC 0."));

    const float regen = processor.apvts.getRawParameterValue ("regen")->load();
    const float speed = processor.apvts.getRawParameterValue ("speed")->load();
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
    else if (processor.isSyncEnabled())
    {
        const float division = processor.apvts.getRawParameterValue ("division")->load();
        const juce::String div (forja::dsp::syncDivisionLabels[
            forja::dsp::syncDivisionIndex (division)]);
        const double bpm = processor.hostTempo();

        if (bpm <= 0.0)
            detail = u ("SYNC ligado, mas o host n\u00e3o informou BPM \u00b7 velocidade manual em uso.");
        else
            detail = u ("Sincronizado ao host \u00b7 ") + juce::String (bpm, 1)
                   + " BPM \u00b7 " + div + " \u00b7 parede mais longa.";
    }
    else if (regen > 0.95f)
    {
        detail = u ("Regenera\u00e7\u00e3o m\u00e1xima \u00b7 a cauda pode durar muito.");
    }
    else if (speed < 0.05f)
    {
        detail = u ("Velocidade m\u00ednima \u00b7 o espa\u00e7o perde resolu\u00e7\u00e3o.");
    }

    setBadge (kind, text, detail);
}
