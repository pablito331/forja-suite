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

TornoAudioProcessorEditor::TornoAudioProcessorEditor (TornoAudioProcessor& p)
    : ForjaEditorBase (
          p, p.apvts,
          { "TORNO",
            "DE EQUALIZADORES",
            "SHELVES DE TOM",
            "AIRWINDOWS BAXANDALL3",
            u ("Equalizador de tom \u00b7 DSP Airwindows Baxandall3"),
            u ("DSP Airwindows Baxandall3 \u00b7 MIT \u00b7 lat\u00eancia 0 amostras"),
            forja::torno::makeForjaInfo() }),
      processor (p)
{
    configureNote (trebleNote, "0,50 = neutro");
    configureNote (bassNote, u ("Neutro apenas com os tr\u00eas em 0,50."));
    addAndMakeVisible (inputCard);
    addAndMakeVisible (trebleCard);
    addAndMakeVisible (bassCard);
    addAndMakeVisible (outputCard);

    inputCard.addAndMakeVisible (inputKnob);
    inputCard.addAndMakeVisible (bypassButton);
    trebleCard.addAndMakeVisible (trebleKnob);
    trebleCard.addAndMakeVisible (trebleNote);
    bassCard.addAndMakeVisible (bassKnob);
    bassCard.addAndMakeVisible (bassNote);
    bassCard.addAndMakeVisible (planoButton);
    outputCard.addAndMakeVisible (outputMeter);

    inputAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "input", inputKnob.getSlider());
    trebleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "treble", trebleKnob.getSlider());
    bassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "bass", bassKnob.getSlider());

    bypassButton.setClickingTogglesState (true);
    bypassButton.setTooltip ("Bypass com rampa de 10 ms");
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, "bypass", bypassButton);

    planoButton.setTooltip ("Volta Entrada, Agudos e Graves ao neutro (0,50)");
    planoButton.onClick = [this]
    {
        inputKnob.getSlider().setValue (0.5, juce::sendNotificationSync);
        trebleKnob.getSlider().setValue (0.5, juce::sendNotificationSync);
        bassKnob.getSlider().setValue (0.5, juce::sendNotificationSync);
    };

    outputMeter.onClipReset = [this]
    {
        processor.resetClip();
        outputMeter.setClip (false);
    };

    const auto onKnobMoved = [this] { refreshReadouts(); };
    inputKnob.onValueChange = onKnobMoved;
    trebleKnob.onValueChange = onKnobMoved;
    bassKnob.onValueChange = onKnobMoved;

    refreshReadouts();
    setSize (windowWidth, windowHeight);
}

void TornoAudioProcessorEditor::layoutContent (juce::Rectangle<int> content)
{
    const int y = content.getY();

    inputCard.setBounds (content.getX(), y, 190, cardHeight);
    trebleCard.setBounds (content.getX() + 198, y, 236, cardHeight);
    bassCard.setBounds (content.getX() + 442, y, 294, cardHeight);
    outputCard.setBounds (content.getX() + 744, y, 242, cardHeight);

    const auto inputArea = inputCard.getContentBounds();
    inputKnob.setBounds (inputArea.getCentreX() - inputKnob.getWidth() / 2,
                         inputArea.getY() + 20, inputKnob.getWidth(), inputKnob.getHeight());
    bypassButton.setBounds (inputArea.getX(), inputArea.getBottom() - 26,
                            inputArea.getWidth(), 26);

    const auto trebleArea = trebleCard.getContentBounds();
    trebleKnob.setBounds (trebleArea.getCentreX() - trebleKnob.getWidth() / 2,
                          trebleArea.getY() + 20, trebleKnob.getWidth(), trebleKnob.getHeight());
    trebleNote.setBounds (trebleArea.getX(), trebleArea.getBottom() - 60,
                          trebleArea.getWidth(), 40);

    const auto bassArea = bassCard.getContentBounds();
    bassKnob.setBounds (bassArea.getCentreX() - bassKnob.getWidth() / 2,
                        bassArea.getY() + 20, bassKnob.getWidth(), bassKnob.getHeight());
    bassNote.setBounds (bassArea.getX() + 12, bassArea.getBottom() - 112,
                        bassArea.getWidth() - 24, 36);
    planoButton.setBounds (bassArea.getX() + 12, bassArea.getBottom() - 66,
                           bassArea.getWidth() - 24, 28);

    const auto outputArea = outputCard.getContentBounds();
    outputMeter.setBounds (outputArea.getX(), outputArea.getBottom() - 180,
                           outputArea.getWidth(), 180);
}

void TornoAudioProcessorEditor::updateTelemetry()
{
    const auto snapshot = processor.readMeters();
    const bool bypassed = processor.isBypassed();

    outputMeter.setLevels (snapshot.outPeakL, snapshot.outPeakR);
    outputMeter.setClip (snapshot.outClip);

    const float inDb = juce::jmax (forja::ui::linearToDb (snapshot.inPeakL),
                                   forja::ui::linearToDb (snapshot.inPeakR));

    refreshBadge (snapshot.outClip, bypassed, inDb);
}

void TornoAudioProcessorEditor::refreshReadouts()
{
    setMetaValues ({
        { "entrada", juce::String (inputKnob.getSlider().getValue(), 2) },
        { "agudos", juce::String (trebleKnob.getSlider().getValue(), 2) },
        { "graves", juce::String (bassKnob.getSlider().getValue(), 2) }
    });
}

void TornoAudioProcessorEditor::refreshBadge (bool clipped, bool bypassed, float inputDb)
{
    auto kind = forja::ui::BadgeKind::ready;
    juce::String text ("PRONTO");
    juce::String detail (u ("Shelves Bessel entrela\u00e7ados \u00b7 neutro em 0,50."));

    if (clipped)
    {
        kind = forja::ui::BadgeKind::clip;
        text = "CLIP";
        detail = u ("Sa\u00edda acima de 0 dBFS \u00b7 reduza a entrada.");
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

    setBadge (kind, text, detail);
}
