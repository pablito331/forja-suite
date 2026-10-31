#include "PluginEditor.h"

#include <algorithm>
#include <cmath>

#include "ForjaInfoText.h"
#include "PluginProcessor.h"
#include "forja/ui/ForjaEqResponse.h"
#include "forja/ui/ForjaTheme.h"

namespace
{
juce::String u (const char* text)
{
    return forja::ui::utf8 (text);
}

constexpr int windowWidth = 1000;
constexpr int windowHeight = 540;

constexpr int curveCardWidth = 386;
constexpr int sideCardWidth = 292;
constexpr int cardGap = 8;
constexpr int cardHeight = 388;

float bandFrequency (double reference)
{
    return (float) juce::jlimit (20.0, 20000.0, std::pow (reference, 3.0) * 20000.0);
}
} // namespace

LimaAudioProcessorEditor::LimaAudioProcessorEditor (LimaAudioProcessor& p)
    : ForjaEditorBase (
          p, p.apvts,
          { "LIMA",
            "DE EQUALIZADORES",
            "EQUALIZADOR DE BUS",
            "AIRWINDOWS SMOOTHEQ2",
            u ("EQ de bus e master \u00b7 DSP Airwindows SmoothEQ2"),
            u ("DSP Airwindows SmoothEQ2 \u00b7 MIT \u00b7 lat\u00eancia 0 amostras"),
            forja::lima::makeForjaInfo() }),
      processor (p)
{
    addAndMakeVisible (curveCard);
    addAndMakeVisible (gainCard);
    addAndMakeVisible (freqCard);

    curveCard.addAndMakeVisible (curve);
    curveCard.addAndMakeVisible (outputMeter);

    gainCard.addAndMakeVisible (highGainKnob);
    gainCard.addAndMakeVisible (highMidGainKnob);
    gainCard.addAndMakeVisible (lowMidGainKnob);
    gainCard.addAndMakeVisible (bassGainKnob);

    freqCard.addAndMakeVisible (highFreqKnob);
    freqCard.addAndMakeVisible (highMidFreqKnob);
    freqCard.addAndMakeVisible (lowMidFreqKnob);
    freqCard.addAndMakeVisible (bassFreqKnob);
    freqCard.addAndMakeVisible (bypassButton);

    highGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "high", highGainKnob.getSlider());
    highMidGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "highMid", highMidGainKnob.getSlider());
    lowMidGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "lowMid", lowMidGainKnob.getSlider());
    bassGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "bass", bassGainKnob.getSlider());
    highFreqAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "highFreq", highFreqKnob.getSlider());
    highMidFreqAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "highMidFreq", highMidFreqKnob.getSlider());
    lowMidFreqAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "lowMidFreq", lowMidFreqKnob.getSlider());
    bassFreqAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "bassFreq", bassFreqKnob.getSlider());

    bypassButton.setClickingTogglesState (true);
    bypassButton.setTooltip ("Bypass com rampa de 10 ms");
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, "bypass", bypassButton);

    outputMeter.onClipReset = [this]
    {
        processor.resetClip();
        outputMeter.setClip (false);
    };

    curve.onMarkerDrag = [this] (const juce::String& id, float frequency, float)
    {
        applyMarkerDrag (id, frequency);
    };
    curve.onMarkerDragEnd = [this] { refreshCurve (true); };

    const auto onKnobMoved = [this] { refreshReadouts(); };
    highGainKnob.onValueChange = onKnobMoved;
    highMidGainKnob.onValueChange = onKnobMoved;
    lowMidGainKnob.onValueChange = onKnobMoved;
    bassGainKnob.onValueChange = onKnobMoved;
    highFreqKnob.onValueChange = onKnobMoved;
    highMidFreqKnob.onValueChange = onKnobMoved;
    lowMidFreqKnob.onValueChange = onKnobMoved;
    bassFreqKnob.onValueChange = onKnobMoved;

    refreshReadouts();
    refreshCurve (true);

    setSize (windowWidth, windowHeight);
}

void LimaAudioProcessorEditor::layoutContent (juce::Rectangle<int> content)
{
    const int y = content.getY();

    curveCard.setBounds (content.getX(), y, curveCardWidth, cardHeight);
    gainCard.setBounds (content.getX() + curveCardWidth + cardGap, y, sideCardWidth, cardHeight);
    freqCard.setBounds (content.getX() + curveCardWidth + sideCardWidth + cardGap * 2, y,
                        sideCardWidth, cardHeight);

    const auto curveArea = curveCard.getContentBounds();
    curve.setBounds (curveArea.withHeight (juce::jmin (240, curveArea.getHeight() - 93)));
    outputMeter.setBounds (curveArea.getX(), curve.getBottom() + 8,
                           curveArea.getWidth(), curveArea.getBottom() - curve.getBottom() - 8);

    const auto gainArea = gainCard.getContentBounds();
    const int knobWidth = highGainKnob.getWidth();
    const int knobHeight = highGainKnob.getHeight();
    const int half = gainArea.getWidth() / 2;
    const int columnX[2] { gainArea.getX() + (half - knobWidth) / 2,
                           gainArea.getX() + half + (half - knobWidth) / 2 };
    const int rowY[2] { gainArea.getY() + 18, gainArea.getY() + 18 + knobHeight + 20 };

    highGainKnob.setBounds (columnX[0], rowY[0], knobWidth, knobHeight);
    highMidGainKnob.setBounds (columnX[1], rowY[0], knobWidth, knobHeight);
    lowMidGainKnob.setBounds (columnX[0], rowY[1], knobWidth, knobHeight);
    bassGainKnob.setBounds (columnX[1], rowY[1], knobWidth, knobHeight);

    const auto freqArea = freqCard.getContentBounds();
    const int freqHalf = freqArea.getWidth() / 2;
    const int freqColumnX[2] { freqArea.getX() + (freqHalf - knobWidth) / 2,
                               freqArea.getX() + freqHalf + (freqHalf - knobWidth) / 2 };

    highFreqKnob.setBounds (freqColumnX[0], rowY[0], knobWidth, knobHeight);
    highMidFreqKnob.setBounds (freqColumnX[1], rowY[0], knobWidth, knobHeight);
    lowMidFreqKnob.setBounds (freqColumnX[0], rowY[1], knobWidth, knobHeight);
    bassFreqKnob.setBounds (freqColumnX[1], rowY[1], knobWidth, knobHeight);

    bypassButton.setBounds (freqArea.getX(), freqArea.getBottom() - 26, freqArea.getWidth(), 26);
}

void LimaAudioProcessorEditor::updateTelemetry()
{
    const auto snapshot = processor.readMeters();
    const bool bypassed = processor.isBypassed();

    outputMeter.setLevels (snapshot.outPeakL, snapshot.outPeakR);
    outputMeter.setClip (snapshot.outClip);
    curve.setBypassed (bypassed);

    const float inDb = juce::jmax (forja::ui::linearToDb (snapshot.inPeakL),
                                   forja::ui::linearToDb (snapshot.inPeakR));

    refreshBadge (snapshot.outClip, bypassed, inDb);
    refreshCurve (false);
}

void LimaAudioProcessorEditor::refreshReadouts()
{
    setMetaValues ({
        { "alta", juce::String (highGainKnob.getSlider().getValue(), 2) },
        { "m. alta", juce::String (highMidGainKnob.getSlider().getValue(), 2) },
        { "m. baixa", juce::String (lowMidGainKnob.getSlider().getValue(), 2) },
        { "graves", juce::String (bassGainKnob.getSlider().getValue(), 2) }
    });
}

void LimaAudioProcessorEditor::refreshCurve (bool force)
{
    const double sampleRate = processor.getSampleRate() > 0.0 ? processor.getSampleRate() : 48000.0;

    const std::array<float, 8> values {
        (float) highGainKnob.getSlider().getValue(),
        (float) highMidGainKnob.getSlider().getValue(),
        (float) lowMidGainKnob.getSlider().getValue(),
        (float) bassGainKnob.getSlider().getValue(),
        (float) highFreqKnob.getSlider().getValue(),
        (float) highMidFreqKnob.getSlider().getValue(),
        (float) lowMidFreqKnob.getSlider().getValue(),
        (float) bassFreqKnob.getSlider().getValue()
    };

    bool changed = force || ! curveInitialised || sampleRate != lastCurveSampleRate;

    for (std::size_t i = 0; i < values.size(); ++i)
        changed = changed || std::abs (values[i] - lastCurveParams[i]) > 1.0e-6f;

    if (! changed)
        return;

    lastCurveParams = values;
    lastCurveSampleRate = sampleRate;
    curveInitialised = true;

    const auto points = forja::ui::measureEqResponse (
        curveAdapter, sampleRate,
        [&values] (forja::dsp::LimaDSP& adapter)
        {
            forja::dsp::LimaDSP::Parameters parameters;
            parameters.high = values[0];
            parameters.highMid = values[1];
            parameters.lowMid = values[2];
            parameters.bass = values[3];
            parameters.highFreq = values[4];
            parameters.highMidFreq = values[5];
            parameters.lowMidFreq = values[6];
            parameters.bassFreq = values[7];
            parameters.bypass = false;
            adapter.setParameters (parameters);
        },
        240);

    curve.setCurve (points);

    const double trebleRef = (double) values[4] - 0.5;
    const double highmidRef = (double) values[5] - 0.5;
    const double lowmidRef = (double) values[6] - 0.5;
    const double bassRef = (double) values[7] - 0.5;

    const double highF = 0.75 + ((trebleRef + trebleRef + trebleRef + highmidRef) * 0.125);
    const double bassF = 0.25 + ((lowmidRef + bassRef + bassRef + bassRef) * 0.125);
    const double midF = (highF * 0.5) + (bassF * 0.5) + ((highmidRef + lowmidRef) * 0.125);

    curve.setMarkers ({
        { bandFrequency (highF), 0.0f, "A", "highF", true, true },
        { bandFrequency (midF), 0.0f, "M", "midF", true, true },
        { bandFrequency (bassF), 0.0f, "G", "bassF", true, true }
    });

    curve.setCaption (u ("curva est\u00e1tica \u00b7 arraste os n\u00f3s \u00b7 4 bandas"));
}

void LimaAudioProcessorEditor::applyMarkerDrag (const juce::String& id, float frequency)
{
    const double target = std::cbrt (juce::jlimit (20.0, 20000.0, (double) frequency) / 20000.0);

    const double trebleRef = highFreqKnob.getSlider().getValue() - 0.5;
    const double highmidRef = highMidFreqKnob.getSlider().getValue() - 0.5;
    const double lowmidRef = lowMidFreqKnob.getSlider().getValue() - 0.5;
    const double bassRef = bassFreqKnob.getSlider().getValue() - 0.5;

    if (id == "highF")
    {
        const double solved = (8.0 * (target - 0.75) - highmidRef) / 3.0;
        highFreqKnob.getSlider().setValue (juce::jlimit (-0.5, 0.5, solved) + 0.5,
                                           juce::sendNotificationSync);
    }
    else if (id == "bassF")
    {
        const double solved = (8.0 * (target - 0.25) - lowmidRef) / 3.0;
        bassFreqKnob.getSlider().setValue (juce::jlimit (-0.5, 0.5, solved) + 0.5,
                                           juce::sendNotificationSync);
    }
    else if (id == "midF")
    {
        const double highF = 0.75 + (3.0 * trebleRef + highmidRef) / 8.0;
        const double bassF = 0.25 + (lowmidRef + 3.0 * bassRef) / 8.0;
        const double sum = 8.0 * (target - 0.5 * (highF + bassF));
        const double diff = highmidRef - lowmidRef;

        highMidFreqKnob.getSlider().setValue (
            juce::jlimit (-0.5, 0.5, (sum + diff) / 2.0) + 0.5, juce::sendNotificationSync);
        lowMidFreqKnob.getSlider().setValue (
            juce::jlimit (-0.5, 0.5, (sum - diff) / 2.0) + 0.5, juce::sendNotificationSync);
    }
}

void LimaAudioProcessorEditor::refreshBadge (bool clipped, bool bypassed, float inputDb)
{
    auto kind = forja::ui::BadgeKind::ready;
    juce::String text ("PRONTO");
    juce::String detail (u ("Quatro bandas largas \u00b7 crossover suave em tr\u00eas est\u00e1gios."));

    if (clipped)
    {
        kind = forja::ui::BadgeKind::clip;
        text = "CLIP";
        detail = u ("Sa\u00edda acima de 0 dBFS \u00b7 reduza algum ganho de banda.");
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
