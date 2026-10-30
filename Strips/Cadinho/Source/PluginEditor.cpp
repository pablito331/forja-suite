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

CadinhoAudioProcessorEditor::CadinhoAudioProcessorEditor (CadinhoAudioProcessor& p)
    : ForjaEditorBase (
          p, p.apvts,
          { "CADINHO",
            u ("DE CHANNEL STRIPS"),
            u ("STRIP DE BUS"),
            "AIRWINDOWS X2BUSS",
            u ("Strip de bus \u00b7 DSP Airwindows X2Buss"),
            u ("DSP Airwindows X2Buss \u00b7 MIT \u00b7 lat\u00eancia 1 amostra"),
            forja::cadinho::makeForjaInfo() }),
      processor (p)
{
    configureNote (highNote, u ("0,50 no ganho = neutro.\nA frequ\u00eancia move o foco."));
    configureNote (midNoteA, u ("Ganho 0,50 = neutro;\na frequ\u00eancia move o foco."));
    configureNote (midNoteB, u ("As frequ\u00eancias tamb\u00e9m\ninclinam o Q das bandas vizinhas."));
    configureNote (bassNote, u ("0,50 = neutro."));
    configureNote (bassFreqNote, u ("Move o split."));

    addAndMakeVisible (highCard);
    addAndMakeVisible (midCard);
    addAndMakeVisible (bassCard);
    addAndMakeVisible (dynCard);

    highCard.addAndMakeVisible (highKnob);
    highCard.addAndMakeVisible (highFreqKnob);
    highCard.addAndMakeVisible (highNote);

    midCard.addAndMakeVisible (highMidKnob);
    midCard.addAndMakeVisible (highMidFreqKnob);
    midCard.addAndMakeVisible (lowMidKnob);
    midCard.addAndMakeVisible (lowMidFreqKnob);
    midCard.addAndMakeVisible (midNoteA);
    midCard.addAndMakeVisible (midNoteB);

    bassCard.addAndMakeVisible (bassKnob);
    bassCard.addAndMakeVisible (bassFreqKnob);
    bassCard.addAndMakeVisible (bassNote);
    bassCard.addAndMakeVisible (bassFreqNote);
    bassCard.addAndMakeVisible (outputMeter);

    dynCard.addAndMakeVisible (threshKnob);
    dynCard.addAndMakeVisible (faderKnob);
    dynCard.addAndMakeVisible (gainReductionMeter);
    dynCard.addAndMakeVisible (bypassButton);

    highAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "high", highKnob.getSlider());
    highFreqAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "highFreq", highFreqKnob.getSlider());
    highMidAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "highMid", highMidKnob.getSlider());
    highMidFreqAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "highMidFreq", highMidFreqKnob.getSlider());
    lowMidAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "lowMid", lowMidKnob.getSlider());
    lowMidFreqAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "lowMidFreq", lowMidFreqKnob.getSlider());
    bassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "bass", bassKnob.getSlider());
    bassFreqAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "bassFreq", bassFreqKnob.getSlider());
    threshAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "thresh", threshKnob.getSlider());
    faderAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "fader", faderKnob.getSlider());

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

    highKnob.onValueChange = onKnobMoved;
    highFreqKnob.onValueChange = onKnobMoved;
    highMidKnob.onValueChange = onKnobMoved;
    highMidFreqKnob.onValueChange = onKnobMoved;
    lowMidKnob.onValueChange = onKnobMoved;
    lowMidFreqKnob.onValueChange = onKnobMoved;
    bassKnob.onValueChange = onKnobMoved;
    bassFreqKnob.onValueChange = onKnobMoved;
    threshKnob.onValueChange = onKnobMoved;
    faderKnob.onValueChange = onKnobMoved;

    refreshReadouts();

    setSize (windowWidth, windowHeight);
}

void CadinhoAudioProcessorEditor::layoutContent (juce::Rectangle<int> content)
{
    const int y = content.getY();

    highCard.setBounds (content.getX(), y, 190, cardHeight);
    midCard.setBounds (content.getX() + 198, y, 236, cardHeight);
    bassCard.setBounds (content.getX() + 442, y, 294, cardHeight);
    dynCard.setBounds (content.getX() + 744, y, 236, cardHeight);

    const auto highArea = highCard.getContentBounds();
    const int highWidth = highKnob.getWidth();
    const int highX = highArea.getX() + (highArea.getWidth() - highWidth) / 2;

    highKnob.setBounds (highX, highArea.getY() + 8, highWidth, highKnob.getHeight());
    highFreqKnob.setBounds (highX, highArea.getY() + 139, highWidth, highFreqKnob.getHeight());
    highNote.setBounds (highArea.getX(), highArea.getY() + 268, highArea.getWidth(), 44);

    const auto midArea = midCard.getContentBounds();
    const int midWidth = highMidKnob.getWidth();
    const int midHeight = highMidKnob.getHeight();
    const int columnStep = midWidth + 4;
    const int gridX = midArea.getX() + (midArea.getWidth() - columnStep * 2 + 4) / 2;
    const int gridY = midArea.getY() + 6;

    highMidKnob.setBounds (gridX, gridY, midWidth, midHeight);
    highMidFreqKnob.setBounds (gridX + columnStep, gridY, midWidth, midHeight);
    lowMidKnob.setBounds (gridX, gridY + midHeight + 6, midWidth, midHeight);
    lowMidFreqKnob.setBounds (gridX + columnStep, gridY + midHeight + 6, midWidth, midHeight);
    midNoteA.setBounds (midArea.getX(), midArea.getY() + 206, midArea.getWidth(), 38);
    midNoteB.setBounds (midArea.getX(), midArea.getY() + 248, midArea.getWidth(), 44);

    const auto bassArea = bassCard.getContentBounds();
    const int bassWidth = bassKnob.getWidth();

    bassKnob.setBounds (bassArea.getX(), bassArea.getY() + 8, bassWidth, bassKnob.getHeight());
    bassFreqKnob.setBounds (bassArea.getX() + bassWidth, bassArea.getY() + 8, bassWidth,
                            bassFreqKnob.getHeight());
    bassNote.setBounds (bassArea.getX(), bassArea.getY() + 131, bassWidth, 26);
    bassFreqNote.setBounds (bassArea.getX() + bassWidth, bassArea.getY() + 131, bassWidth, 26);
    outputMeter.setBounds (bassArea.getX(), bassArea.getY() + 172, bassArea.getWidth(), 140);

    const auto dynArea = dynCard.getContentBounds();
    const int dynKnobWidth = threshKnob.getWidth();
    constexpr int dynGap = 10;
    const int dynX = dynArea.getX()
                   + (dynArea.getWidth() - (dynKnobWidth * 2 + dynGap)) / 2;
    const int bypassTop = dynArea.getBottom() - 26;
    const int grTop = dynArea.getY() + threshKnob.getHeight() + 16;

    threshKnob.setBounds (dynX, dynArea.getY() + 4, dynKnobWidth, threshKnob.getHeight());
    faderKnob.setBounds (dynX + dynKnobWidth + dynGap, dynArea.getY() + 4,
                         dynKnobWidth, faderKnob.getHeight());
    gainReductionMeter.setBounds (dynArea.getX(), grTop, dynArea.getWidth(),
                                  bypassTop - 8 - grTop);
    bypassButton.setBounds (dynArea.getX(), bypassTop, dynArea.getWidth(), 26);
}

void CadinhoAudioProcessorEditor::updateTelemetry()
{
    const auto snapshot = processor.readMeters();
    const bool bypassed = processor.isBypassed();

    outputMeter.setLevels (snapshot.outPeakL, snapshot.outPeakR);
    outputMeter.setClip (snapshot.outClip);
    gainReductionMeter.setGainReductionDb (snapshot.gainReductionDb, snapshot.gainReductionDb);
    gainReductionMeter.setBypassed (bypassed);

    const float inDb = juce::jmax (forja::ui::linearToDb (snapshot.inPeakL),
                                   forja::ui::linearToDb (snapshot.inPeakR));

    refreshBadge (snapshot.outClip, bypassed, inDb, snapshot.gainReductionDb);
}

void CadinhoAudioProcessorEditor::refreshReadouts()
{
    setMetaValues ({
        { "agudos", juce::String (highKnob.getSlider().getValue(), 2) },
        { "m. alta", juce::String (highMidKnob.getSlider().getValue(), 2) },
        { "m. baixa", juce::String (lowMidKnob.getSlider().getValue(), 2) },
        { "graves", juce::String (bassKnob.getSlider().getValue(), 2) },
        { "fader", juce::String (faderKnob.getSlider().getValue(), 2) }
    });
}

void CadinhoAudioProcessorEditor::refreshBadge (bool clipped, bool bypassed, float inputDb,
                                                float reductionDb)
{
    auto kind = forja::ui::BadgeKind::ready;
    juce::String text ("PRONTO");
    juce::String detail (u ("Quatro bandas entrela\u00e7adas \u00b7 ClipOnly3 na sa\u00edda."));

    if (clipped)
    {
        kind = forja::ui::BadgeKind::clip;
        text = "CLIP";
        detail = u ("Sa\u00edda acima de 0 dBFS \u00b7 abaixe o fader.");
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
    else if (reductionDb < -0.1f)
    {
        detail = u ("Din\u00e2mica ativa \u00b7 ") + juce::String (reductionDb, 1)
               + u (" dB de redu\u00e7\u00e3o de ganho.");
    }
    else if (threshKnob.getSlider().getValue() > 0.995)
    {
        detail = u ("DIN\u00c2MICA em 1,00 \u00b7 sem compress\u00e3o de bandas.");
    }

    setBadge (kind, text, detail);
}
