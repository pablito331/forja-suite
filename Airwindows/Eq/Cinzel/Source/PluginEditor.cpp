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
} // namespace

CinzelAudioProcessorEditor::CinzelAudioProcessorEditor (CinzelAudioProcessor& p)
    : ForjaEditorBase (
          p, p.apvts,
          { u ("CINZ\u00c9L"),
            "DE EQUALIZADORES",
            u ("EQUALIZADOR PARAM\u00c9TRICO DIN\u00c2MICO"),
            "AIRWINDOWS PARAMETRIC",
            u ("EQ param\u00e9trico din\u00e2mico \u00b7 DSP Airwindows Parametric"),
            u ("DSP Airwindows Parametric + camada din\u00e2mica Forja \u00b7 MIT \u00b7 lat\u00eancia 0 amostras"),
            forja::cinzel::makeForjaInfo() }),
      processor (p)
{
    addAndMakeVisible (curveCard);
    addAndMakeVisible (bandCard);
    addAndMakeVisible (dynamicsCard);

    curveCard.addAndMakeVisible (curve);
    curveCard.addAndMakeVisible (outputMeter);

    bandCard.addAndMakeVisible (highFreqKnob);
    bandCard.addAndMakeVisible (highGainKnob);
    bandCard.addAndMakeVisible (highResoKnob);
    bandCard.addAndMakeVisible (highMidFreqKnob);
    bandCard.addAndMakeVisible (highMidGainKnob);
    bandCard.addAndMakeVisible (highMidResoKnob);
    bandCard.addAndMakeVisible (lowMidFreqKnob);
    bandCard.addAndMakeVisible (lowMidGainKnob);
    bandCard.addAndMakeVisible (lowMidResoKnob);

    dynamicsCard.addAndMakeVisible (mixKnob);
    dynamicsCard.addAndMakeVisible (dynKnob);
    dynamicsCard.addAndMakeVisible (gainReductionMeter);
    dynamicsCard.addAndMakeVisible (bypassButton);

    highFreqAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "highFreq", highFreqKnob.getSlider());
    highGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "highGain", highGainKnob.getSlider());
    highResoAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "highReso", highResoKnob.getSlider());
    highMidFreqAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "highMidFreq", highMidFreqKnob.getSlider());
    highMidGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "highMidGain", highMidGainKnob.getSlider());
    highMidResoAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "highMidReso", highMidResoKnob.getSlider());
    lowMidFreqAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "lowMidFreq", lowMidFreqKnob.getSlider());
    lowMidGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "lowMidGain", lowMidGainKnob.getSlider());
    lowMidResoAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "lowMidReso", lowMidResoKnob.getSlider());
    mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "mix", mixKnob.getSlider());
    dynAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "dyn", dynKnob.getSlider());

    bypassButton.setClickingTogglesState (true);
    bypassButton.setTooltip ("Bypass com rampa de 10 ms");
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, "bypass", bypassButton);

    outputMeter.onClipReset = [this]
    {
        processor.resetClip();
        outputMeter.setClip (false);
    };

    curve.onMarkerDragStart = [this] (const juce::String& id) { beginMarkerDrag (id); };
    curve.onMarkerDrag = [this] (const juce::String& id, float frequency, float dbDelta)
    {
        applyMarkerDrag (id, frequency, dbDelta);
    };
    curve.onMarkerDragEnd = [this] { refreshCurve (true); };

    const auto onKnobMoved = [this] { refreshReadouts(); };

    highFreqKnob.onValueChange = onKnobMoved;
    highGainKnob.onValueChange = onKnobMoved;
    highResoKnob.onValueChange = onKnobMoved;
    highMidFreqKnob.onValueChange = onKnobMoved;
    highMidGainKnob.onValueChange = onKnobMoved;
    highMidResoKnob.onValueChange = onKnobMoved;
    lowMidFreqKnob.onValueChange = onKnobMoved;
    lowMidGainKnob.onValueChange = onKnobMoved;
    lowMidResoKnob.onValueChange = onKnobMoved;
    mixKnob.onValueChange = onKnobMoved;
    dynKnob.onValueChange = onKnobMoved;

    refreshReadouts();
    refreshCurve (true);

    setSize (windowWidth, windowHeight);
}

void CinzelAudioProcessorEditor::layoutContent (juce::Rectangle<int> content)
{
    const int y = content.getY();
    const int height = cardHeight;

    curveCard.setBounds (content.getX(), y, curveCardWidth, height);
    bandCard.setBounds (content.getX() + curveCardWidth + cardGap, y, sideCardWidth, height);
    dynamicsCard.setBounds (content.getX() + curveCardWidth + sideCardWidth + cardGap * 2, y,
                            sideCardWidth, height);

    const auto curveArea = curveCard.getContentBounds();
    curve.setBounds (curveArea.withHeight (juce::jmin (240, curveArea.getHeight() - 93)));
    outputMeter.setBounds (curveArea.getX(), curve.getBottom() + 8,
                           curveArea.getWidth(), curveArea.getBottom() - curve.getBottom() - 8);

    const auto bandArea = bandCard.getContentBounds();
    const int knobWidth = highFreqKnob.getWidth();
    const int knobHeight = highFreqKnob.getHeight();
    const int columnStep = bandArea.getWidth() / 3;

    for (int row = 0; row < 3; ++row)
    {
        const int rowY = bandArea.getY() + row * (knobHeight + 31);
        for (int column = 0; column < 3; ++column)
        {
            const int x = bandArea.getX() + column * columnStep + (columnStep - knobWidth) / 2;
            if (row == 0 && column == 0) highFreqKnob.setBounds (x, rowY, knobWidth, knobHeight);
            if (row == 0 && column == 1) highGainKnob.setBounds (x, rowY, knobWidth, knobHeight);
            if (row == 0 && column == 2) highResoKnob.setBounds (x, rowY, knobWidth, knobHeight);
            if (row == 1 && column == 0) highMidFreqKnob.setBounds (x, rowY, knobWidth, knobHeight);
            if (row == 1 && column == 1) highMidGainKnob.setBounds (x, rowY, knobWidth, knobHeight);
            if (row == 1 && column == 2) highMidResoKnob.setBounds (x, rowY, knobWidth, knobHeight);
            if (row == 2 && column == 0) lowMidFreqKnob.setBounds (x, rowY, knobWidth, knobHeight);
            if (row == 2 && column == 1) lowMidGainKnob.setBounds (x, rowY, knobWidth, knobHeight);
            if (row == 2 && column == 2) lowMidResoKnob.setBounds (x, rowY, knobWidth, knobHeight);
        }
    }

    const auto dynamicsArea = dynamicsCard.getContentBounds();
    const int half = dynamicsArea.getWidth() / 2;
    const int wideKnobWidth = mixKnob.getWidth();
    const int wideKnobHeight = mixKnob.getHeight();

    mixKnob.setBounds (dynamicsArea.getX() + (half - wideKnobWidth) / 2, dynamicsArea.getY(),
                       wideKnobWidth, wideKnobHeight);
    dynKnob.setBounds (dynamicsArea.getX() + half + (half - wideKnobWidth) / 2,
                       dynamicsArea.getY(), wideKnobWidth, wideKnobHeight);

    const int meterY = dynamicsArea.getY() + wideKnobHeight + 16;
    gainReductionMeter.setBounds (dynamicsArea.getX(), meterY, dynamicsArea.getWidth(),
                                  dynamicsArea.getBottom() - 34 - meterY);

    bypassButton.setBounds (dynamicsArea.getX(), dynamicsArea.getBottom() - 26,
                            dynamicsArea.getWidth(), 26);
}

void CinzelAudioProcessorEditor::updateTelemetry()
{
    const auto snapshot = processor.readMeters();
    const bool bypassed = processor.isBypassed();

    outputMeter.setLevels (snapshot.outPeakL, snapshot.outPeakR);
    outputMeter.setClip (snapshot.outClip);
    gainReductionMeter.setGainReductionDb (snapshot.gainReductionDb, snapshot.gainReductionDb);
    gainReductionMeter.setBypassed (bypassed);
    curve.setBypassed (bypassed);

    const float inDb = juce::jmax (forja::ui::linearToDb (snapshot.inPeakL),
                                   forja::ui::linearToDb (snapshot.inPeakR));

    refreshBadge (snapshot.outClip, bypassed, inDb, snapshot.gainReductionDb);
    refreshCurve (false);
}

void CinzelAudioProcessorEditor::refreshReadouts()
{
    setMetaValues ({
        { "mistura", juce::String (mixKnob.getSlider().getValue(), 2) },
        { u ("din\u00e2mica"), juce::String (dynKnob.getSlider().getValue(), 2) },
        { "alta", juce::String (highGainKnob.getSlider().getValue(), 2) },
        { "m. alta", juce::String (highMidGainKnob.getSlider().getValue(), 2) },
        { "m. baixa", juce::String (lowMidGainKnob.getSlider().getValue(), 2) }
    });
}

void CinzelAudioProcessorEditor::refreshCurve (bool force)
{
    const double sampleRate = processor.getSampleRate() > 0.0 ? processor.getSampleRate() : 48000.0;

    const std::array<float, 9> values {
        (float) highFreqKnob.getSlider().getValue(),
        (float) highGainKnob.getSlider().getValue(),
        (float) highResoKnob.getSlider().getValue(),
        (float) highMidFreqKnob.getSlider().getValue(),
        (float) highMidGainKnob.getSlider().getValue(),
        (float) highMidResoKnob.getSlider().getValue(),
        (float) lowMidFreqKnob.getSlider().getValue(),
        (float) lowMidGainKnob.getSlider().getValue(),
        (float) lowMidResoKnob.getSlider().getValue()
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
        [&values] (forja::dsp::CinzelDSP& adapter)
        {
            forja::dsp::CinzelDSP::Parameters parameters;
            parameters.highFreq = values[0];
            parameters.highGain = values[1];
            parameters.highReso = values[2];
            parameters.highMidFreq = values[3];
            parameters.highMidGain = values[4];
            parameters.highMidReso = values[5];
            parameters.lowMidFreq = values[6];
            parameters.lowMidGain = values[7];
            parameters.lowMidReso = values[8];
            parameters.mix = 1.0f;
            parameters.dyn = 0.0f;
            parameters.bypass = false;
            adapter.setParameters (parameters);
        },
        240);

    curve.setCurve (points);

    const auto bandFrequency = [] (float value, double span, double base)
    {
        return (float) juce::jlimit (20.0, 20000.0, std::pow ((double) value, 3.0) * span + base);
    };

    curve.setMarkers ({
        { bandFrequency (values[0], 14500.0, 1500.0), 0.0f, "A", "high", true },
        { bandFrequency (values[3], 6400.0, 600.0), 0.0f, "MA", "highMid", true },
        { bandFrequency (values[6], 2200.0, 20.0), 0.0f, "MB", "lowMid", true }
    });

    curve.setCaption (u ("curva est\u00e1tica \u00b7 arraste os n\u00f3s \u00b7 dyn = 0"));
}

void CinzelAudioProcessorEditor::beginMarkerDrag (const juce::String& id)
{
    if (id == "high")
        dragStartGain = highGainKnob.getSlider().getValue();
    else if (id == "highMid")
        dragStartGain = highMidGainKnob.getSlider().getValue();
    else if (id == "lowMid")
        dragStartGain = lowMidGainKnob.getSlider().getValue();
}

void CinzelAudioProcessorEditor::applyMarkerDrag (const juce::String& id, float frequency,
                                                  float dbDelta)
{
    juce::Slider* frequencySlider = nullptr;
    juce::Slider* gainSlider = nullptr;
    double base = 0.0;
    double span = 1.0;

    if (id == "high")
    {
        frequencySlider = &highFreqKnob.getSlider();
        gainSlider = &highGainKnob.getSlider();
        base = 1500.0;
        span = 14500.0;
    }
    else if (id == "highMid")
    {
        frequencySlider = &highMidFreqKnob.getSlider();
        gainSlider = &highMidGainKnob.getSlider();
        base = 600.0;
        span = 6400.0;
    }
    else if (id == "lowMid")
    {
        frequencySlider = &lowMidFreqKnob.getSlider();
        gainSlider = &lowMidGainKnob.getSlider();
        base = 20.0;
        span = 2200.0;
    }

    if (frequencySlider == nullptr)
        return;

    const double ratio = ((double) frequency - base) / span;
    frequencySlider->setValue (juce::jlimit (0.0, 1.0, std::cbrt (juce::jlimit (0.0, 1.0, ratio))),
                               juce::sendNotificationSync);

    if (gainSlider != nullptr)
        gainSlider->setValue (juce::jlimit (0.0, 1.0, dragStartGain + (double) dbDelta / 36.0),
                              juce::sendNotificationSync);
}

void CinzelAudioProcessorEditor::refreshBadge (bool clipped, bool bypassed, float inputDb,
                                               float reductionDb)
{
    auto kind = forja::ui::BadgeKind::ready;
    juce::String text ("PRONTO");
    juce::String detail (u ("Tr\u00eas bandas param\u00e9tricas sobre o sinal seco."));

    if (clipped)
    {
        kind = forja::ui::BadgeKind::clip;
        text = "CLIP";
        detail = u ("Sa\u00edda acima de 0 dBFS \u00b7 reduza o ganho de alguma banda.");
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
        detail = u ("Din\u00e2mica ativa \u00b7 ") + juce::String (reductionDb, 1) + u (" dB de redu\u00e7\u00e3o.");
    }
    else if (dynKnob.getSlider().getValue() < 0.01)
    {
        detail = u ("DIN\u00c2MICA em 0 \u00b7 caminho id\u00eantico ao Parametric.");
    }

    setBadge (kind, text, detail);
}
