#include "MixerPanel.h"

#include "PluginProcessor.h"
#include "MiniKnob.h"
#include "forja/ui/ForjaMeter.h"
#include "forja/ui/ForjaTheme.h"

using forja::drumforge::kNumOutputBuses;
using forja::drumforge::kNumPads;
using forja::drumforge::mapping::eqDb;
using forja::drumforge::mapping::hpfHz;
using forja::drumforge::mapping::ratio;
using forja::drumforge::mapping::thresholdDb;
using forja::drumforge::mapping::trimDb;

namespace drumforgeui
{

namespace
{

juce::String padParameterId (int padIndex, const char* suffix)
{
    return "pad" + juce::String (padIndex + 1).paddedLeft ('0', 2) + "_" + suffix;
}

juce::String signedDb (float db, int decimals = 1)
{
    return (db > 0.0f ? "+" : "") + juce::String (db, decimals);
}

juce::String formatKnobValue (int knob, double value)
{
    const auto unit = (float) value;

    switch (knob)
    {
        case 0:  return signedDb (trimDb (unit));
        case 1:
        {
            const int percent = juce::roundToInt ((unit - 0.5f) * 200.0f);

            if (percent == 0)
                return "C";

            return (percent < 0 ? "L" : "R") + juce::String (std::abs (percent));
        }
        case 2:
        {
            const auto hz = hpfHz (unit);
            return hz < 25.0f ? juce::String ("OFF") : juce::String (juce::roundToInt (hz));
        }
        case 3:
        case 4:
        case 5:  return signedDb (eqDb (unit));
        case 6:  return juce::String (juce::roundToInt (thresholdDb (unit)));
        case 7:  return juce::String (ratio (unit), 1) + ":1";
        default: return juce::String (juce::roundToInt (unit * 100.0f));
    }
}

juce::String formatMasterKnobValue (int knob, double value)
{
    switch (knob)
    {
        case 0:  return signedDb (trimDb ((float) value));
        case 1:  return juce::String (juce::roundToInt (value * 100.0));
        case 2:
        case 3:  return juce::String (juce::roundToInt (value * 100.0));
        default: return juce::String (juce::roundToInt (value * 100.0));
    }
}

// Fader vertical desenhado como pista fina com capa, no lugar da barra
// padrao do JUCE, para o canal lembrar uma mesa analógica.
class FaderLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        if (style != juce::Slider::LinearVertical)
        {
            LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos,
                                              minSliderPos, maxSliderPos, style, slider);
            return;
        }

        const auto area = juce::Rectangle<float> ((float) x, (float) y,
                                                  (float) width, (float) height);
        const float centreX = area.getCentreX();
        constexpr float trackWidth = 4.0f;

        const juce::Rectangle<float> track (centreX - trackWidth * 0.5f, area.getY() + 4.0f,
                                            trackWidth, area.getHeight() - 8.0f);

        g.setColour (forja::ui::colour::meterTrack);
        g.fillRoundedRectangle (track, 2.0f);

        const float filledTop = juce::jlimit (track.getY(), track.getBottom(), sliderPos);
        g.setColour (forja::ui::colour::ember.withAlpha (0.85f));
        g.fillRoundedRectangle (track.withTop (filledTop), 2.0f);

        const juce::Rectangle<float> cap (area.getX() + 1.0f, sliderPos - 5.0f,
                                          area.getWidth() - 2.0f, 10.0f);
        g.setColour (juce::Colour (0xff2c333d));
        g.fillRoundedRectangle (cap, 2.0f);
        g.setColour (forja::ui::colour::lineStrong);
        g.drawRoundedRectangle (cap, 2.0f, 1.0f);
        g.setColour (forja::ui::colour::emberPale);
        g.fillRect (cap.getX() + 3.0f, cap.getCentreY() - 0.5f, cap.getWidth() - 6.0f, 1.0f);
    }
};

FaderLookAndFeel& faderStyle()
{
    static FaderLookAndFeel instance;
    return instance;
}

void paintLevelMeter (juce::Graphics& g, juce::Rectangle<int> area, float peak)
{
    if (area.isEmpty())
        return;

    const float db = forja::ui::linearToDb (peak, -60.0f);
    const float norm = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 66.0f);

    const auto bounds = area.toFloat();
    g.setColour (forja::ui::colour::meterTrack);
    g.fillRoundedRectangle (bounds, 2.0f);
    g.setColour (forja::ui::colour::line);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 2.0f, 1.0f);

    if (norm <= 0.001f)
        return;

    auto filled = bounds.withTop (bounds.getBottom() - bounds.getHeight() * norm);
    juce::ColourGradient gradient (forja::ui::colour::meterSafe, 0.0f, bounds.getBottom(),
                                   forja::ui::colour::meterHot, 0.0f, bounds.getY(), false);
    gradient.addColour (0.55, forja::ui::colour::meterCaution);
    g.setGradientFill (gradient);
    g.fillRoundedRectangle (filled, 2.0f);
}

void configureToggle (juce::TextButton& button, const juce::String& text,
                      const juce::String& tooltip)
{
    button.setButtonText (text);
    button.setClickingTogglesState (true);
    button.setTooltip (tooltip);
    button.setWantsKeyboardFocus (false);
}

} // namespace

//==============================================================================
class MixerStrip final : public juce::Component
{
public:
    MixerStrip (juce::AudioProcessorValueTreeState& state, int padIndex)
        : apvts (state), pad (padIndex)
    {
        setInterceptsMouseClicks (false, true);

        static constexpr const char* ids[] = { "trim", "pan", "hpf", "eqLow", "eqMid",
                                               "eqHigh", "compThr", "compRatio",
                                               "send1", "send2" };
        static constexpr const char* labels[] = { "TRIM", "PAN", "HPF", "LO", "MID",
                                                  "HI", "THR", "RAT", "S1", "S2" };

        for (int i = 0; i < numKnobs; ++i)
        {
            auto knob = std::make_unique<MiniKnob> (labels[i], 20.0f);
            knob->setFormatter ([i] (double value) { return formatKnobValue (i, value); });
            addAndMakeVisible (*knob);

            sliderAttachments.push_back (
                std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                    apvts, padParameterId (pad, ids[i]), knob->getSlider()));

            knobs[(size_t) i] = knob.get();
            knobStorage[(size_t) i] = std::move (knob);
        }

        configureToggle (muteButton, "M", "Mute do pad");
        configureToggle (soloButton, "S", "Solo do pad");
        configureToggle (phaseButton, forja::ui::utf8 ("\u00d8"), "Inverte a fase do pad");

        addAndMakeVisible (muteButton);
        addAndMakeVisible (soloButton);
        addAndMakeVisible (phaseButton);

        buttonAttachments.push_back (
            std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
                apvts, padParameterId (pad, "mute"), muteButton));
        buttonAttachments.push_back (
            std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
                apvts, padParameterId (pad, "solo"), soloButton));
        buttonAttachments.push_back (
            std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
                apvts, padParameterId (pad, "phase"), phaseButton));

        outputButton.setTooltip ("Saida do canal: MAIN ou AUX 1-4");
        outputButton.setWantsKeyboardFocus (false);
        outputButton.onClick = [this] { cycleOutput(); };
        addAndMakeVisible (outputButton);
        updateOutputText();

        fader.setSliderStyle (juce::Slider::LinearVertical);
        fader.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        fader.setLookAndFeel (&faderStyle());
        fader.setWantsKeyboardFocus (false);
        addAndMakeVisible (fader);

        sliderAttachments.push_back (
            std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                apvts, padParameterId (pad, "fader"), fader));
    }

    ~MixerStrip() override
    {
        fader.setLookAndFeel (nullptr);
    }

    void setPadName (const juce::String& padName)
    {
        if (name == padName)
            return;

        name = padName;
        repaint (getLocalBounds());
    }

    void setMeter (float peak, float reductionDb)
    {
        meterPeak = peak;
        meterReduction = reductionDb;
        updateOutputText();
        repaint (meterArea.getUnion (grArea).expanded (1));
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (0.5f);

        juce::ColourGradient background (forja::ui::colour::iron3, bounds.getTopLeft(),
                                         forja::ui::colour::iron2, bounds.getBottomRight(), false);
        g.setGradientFill (background);
        g.fillRoundedRectangle (bounds, 4.0f);
        g.setColour (forja::ui::colour::line);
        g.drawRoundedRectangle (bounds, 4.0f, 1.0f);

        g.setColour (forja::ui::colour::ember.withAlpha (0.9f));
        g.fillRect (bounds.getX() + 5.0f, (float) headerArea.getBottom() - 2.0f,
                    bounds.getWidth() - 10.0f, 2.0f);

        auto header = headerArea.reduced (1, 0);

        g.setFont (forja::ui::tracked (forja::ui::uiFont (6.8f, juce::Font::bold), 0.06f));
        g.setColour (forja::ui::colour::emberSoft);
        g.drawText (juce::String (pad + 1).paddedLeft ('0', 2), header.removeFromLeft (13),
                    juce::Justification::centred, false);

        g.setFont (forja::ui::uiFont (7.2f, juce::Font::bold));
        g.setColour (forja::ui::colour::text);
        g.drawText (name.toUpperCase(), header, juce::Justification::centred, true);

        paintLevelMeter (g, meterArea, meterPeak);

        if (! grArea.isEmpty())
        {
            g.setFont (forja::ui::tracked (forja::ui::uiFont (6.4f, juce::Font::bold), 0.08f));
            g.setColour (meterReduction < -0.1f ? forja::ui::colour::emberSoft
                                                : forja::ui::colour::dim);
            g.drawText (meterReduction < -0.1f
                            ? "GR " + juce::String (meterReduction, 1)
                            : juce::String ("GR"),
                        grArea, juce::Justification::centred, false);
        }
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (4, 3);

        headerArea = area.removeFromTop (16);

        auto toggleRow = area.removeFromTop (16);
        const int toggleWidth = (toggleRow.getWidth() - 4) / 3;
        muteButton.setBounds (toggleRow.removeFromLeft (toggleWidth));
        toggleRow.removeFromLeft (2);
        soloButton.setBounds (toggleRow.removeFromLeft (toggleWidth));
        toggleRow.removeFromLeft (2);
        phaseButton.setBounds (toggleRow);

        outputButton.setBounds (area.removeFromTop (14));

        const int cellWidth = area.getWidth() / 2;

        for (int i = 0; i < numKnobs; ++i)
        {
            const int row = i / 2;
            const int column = i % 2;
            knobs[(size_t) i]->setBounds (area.getX() + column * cellWidth,
                                          area.getY() + row * 32, cellWidth, 32);
        }

        area.removeFromTop (5 * 32);

        grArea = area.removeFromBottom (9);
        meterArea = area.removeFromRight (16).reduced (1, 1);
        fader.setBounds (area.reduced (6, 0));
    }

private:
    static constexpr int numKnobs = 10;

    void updateOutputText()
    {
        if (auto* raw = apvts.getRawParameterValue (padParameterId (pad, "out")))
        {
            static constexpr const char* names[] = { "MAIN", "AUX 1", "AUX 2", "AUX 3", "AUX 4" };
            const int index = juce::jlimit (0, 4, (int) raw->load());

            outputButton.setButtonText (juce::String (names[index]).toUpperCase());
        }
    }

    void cycleOutput()
    {
        auto* parameter = apvts.getParameter (padParameterId (pad, "out"));
        auto* raw = apvts.getRawParameterValue (padParameterId (pad, "out"));

        if (parameter == nullptr || raw == nullptr)
            return;

        int count = kNumOutputBuses;

        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (parameter))
            count = juce::jmax (1, choice->choices.size());

        const int current = juce::jlimit (0, count - 1, (int) raw->load());
        const int next = (current + 1) % count;

        parameter->setValueNotifyingHost ((float) next / (float) (count - 1));
        updateOutputText();
    }

    juce::AudioProcessorValueTreeState& apvts;
    int pad = 0;

    juce::String name { "PAD" };
    std::array<std::unique_ptr<MiniKnob>, numKnobs> knobStorage;
    std::array<MiniKnob*, numKnobs> knobs {};

    juce::TextButton muteButton, soloButton, phaseButton, outputButton;
    juce::Slider fader;

    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAttachments;

    float meterPeak = 0.0f;
    float meterReduction = 0.0f;

    juce::Rectangle<int> headerArea, meterArea, grArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixerStrip)
};

//==============================================================================
class MasterStrip final : public juce::Component
{
public:
    explicit MasterStrip (juce::AudioProcessorValueTreeState& state)
        : apvts (state)
    {
        static constexpr const char* ids[] = { "master_trim", "master_comp",
                                               "master_fx1Regen", "master_fx1Position",
                                               "master_fx2Drive" };
        static constexpr const char* labels[] = { "TRIM", "COMP", "REGEN", "POS", "DRIVE" };

        for (int i = 0; i < numKnobs; ++i)
        {
            auto knob = std::make_unique<MiniKnob> (labels[i], 26.0f);
            knob->setFormatter ([i] (double value) { return formatMasterKnobValue (i, value); });
            addAndMakeVisible (*knob);

            sliderAttachments.push_back (
                std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                    apvts, ids[i], knob->getSlider()));

            knobs[(size_t) i] = knob.get();
            knobStorage[(size_t) i] = std::move (knob);
        }

        configureToggle (limiterButton, "LIMITER", "Limitador de pico a -0,3 dBFS");
        addAndMakeVisible (limiterButton);

        buttonAttachments.push_back (
            std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
                apvts, "master_limiter", limiterButton));

        addAndMakeVisible (meter);
        meter.onClipReset = [this]
        {
            meter.setClip (false);
        };
    }

    ~MasterStrip() override = default;

    void setMeter (const forja::drumforge::DrumEngine::MasterMeter& snapshot)
    {
        meter.setLevels (snapshot.peakLeft, snapshot.peakRight);

        if (snapshot.clip)
            meter.setClip (true);

        reductionDb = snapshot.reductionDb;
        repaint (grArea.expanded (1));
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (0.5f);

        juce::ColourGradient background (juce::Colour (0xff232933), bounds.getTopLeft(),
                                         juce::Colour (0xff171b22), bounds.getBottomRight(), false);
        g.setGradientFill (background);
        g.fillRoundedRectangle (bounds, 4.0f);
        g.setColour (forja::ui::colour::lineStrong);
        g.drawRoundedRectangle (bounds, 4.0f, 1.0f);

        g.setColour (forja::ui::colour::ember);
        g.fillRect (bounds.getX() + 5.0f, (float) headerArea.getBottom() - 2.0f,
                    bounds.getWidth() - 10.0f, 2.0f);

        g.setFont (forja::ui::tracked (forja::ui::uiFont (7.6f, juce::Font::bold), 0.16f));
        g.setColour (forja::ui::colour::emberPale);
        g.drawText ("MASTER", headerArea, juce::Justification::centred, false);

        if (! grArea.isEmpty())
        {
            g.setFont (forja::ui::tracked (forja::ui::uiFont (6.6f, juce::Font::bold), 0.08f));
            g.setColour (reductionDb < -0.1f ? forja::ui::colour::emberSoft
                                             : forja::ui::colour::dim);
            g.drawText (reductionDb < -0.1f
                            ? "GR " + juce::String (reductionDb, 1) + " dB"
                            : juce::String ("GR 0,0 dB"),
                        grArea, juce::Justification::centred, false);
        }
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (5, 3);

        headerArea = area.removeFromTop (18);

        const int cellWidth = area.getWidth() / 2;

        for (int i = 0; i < numKnobs; ++i)
        {
            const int row = i / 2;
            const int column = i % 2;
            knobs[(size_t) i]->setBounds (area.getX() + column * cellWidth,
                                          area.getY() + row * 38, cellWidth, 37);
        }

        area.removeFromTop (3 * 38);

        limiterButton.setBounds (area.removeFromTop (20));
        area.removeFromTop (6);

        grArea = area.removeFromBottom (12);
        meter.setBounds (area);
    }

private:
    static constexpr int numKnobs = 5;

    juce::AudioProcessorValueTreeState& apvts;
    std::array<std::unique_ptr<MiniKnob>, numKnobs> knobStorage;
    std::array<MiniKnob*, numKnobs> knobs {};

    juce::TextButton limiterButton;
    forja::ui::ForjaMeter meter;

    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAttachments;

    float reductionDb = 0.0f;
    juce::Rectangle<int> headerArea, grArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MasterStrip)
};

//==============================================================================
MixerPanel::MixerPanel (DrumforgeAudioProcessor& owner)
    : processor (owner)
{
    addAndMakeVisible (card);

    card.addAndMakeVisible (viewport);
    viewport.setViewedComponent (&stripHolder, false);
    viewport.setScrollBarsShown (false, true);
    viewport.setScrollBarThickness (7);
    viewport.setScrollOnDragMode (juce::Viewport::ScrollOnDragMode::never);

    auto& hScroll = viewport.getHorizontalScrollBar();
    hScroll.setColour (juce::ScrollBar::thumbColourId,
                       forja::ui::colour::ember.withAlpha (0.45f));
    hScroll.setColour (juce::ScrollBar::trackColourId, forja::ui::colour::iron0);
    hScroll.setColour (juce::ScrollBar::backgroundColourId, juce::Colours::transparentBlack);

    for (int pad = 0; pad < kNumPads; ++pad)
    {
        strips[(size_t) pad] = std::make_unique<MixerStrip> (processor.apvts, pad);
        stripHolder.addAndMakeVisible (*strips[(size_t) pad]);
    }

    master = std::make_unique<MasterStrip> (processor.apvts);
    card.addAndMakeVisible (*master);

    refreshNames();
}

MixerPanel::~MixerPanel() = default;

void MixerPanel::refreshNames()
{
    for (int pad = 0; pad < kNumPads; ++pad)
        strips[(size_t) pad]->setPadName (processor.getPadName (pad));
}

void MixerPanel::setMeters (
    const std::array<forja::drumforge::DrumEngine::PadMeter, kNumPads>& padMeters,
    const forja::drumforge::DrumEngine::MasterMeter& masterMeter)
{
    for (int pad = 0; pad < kNumPads; ++pad)
        strips[(size_t) pad]->setMeter (padMeters[(size_t) pad].peak,
                                        padMeters[(size_t) pad].reductionDb);

    master->setMeter (masterMeter);
}

void MixerPanel::resized()
{
    card.setBounds (getLocalBounds());

    auto content = card.getContentBounds();
    auto masterArea = content.removeFromRight (144);
    content.removeFromRight (8);

    master->setBounds (masterArea);

    viewport.setBounds (content);

    constexpr int stripWidth = 92;
    constexpr int stripGap = 4;
    const int stripHeight = content.getHeight() - 8;

    stripHolder.setSize (kNumPads * (stripWidth + stripGap) - stripGap, stripHeight);

    for (int pad = 0; pad < kNumPads; ++pad)
        strips[(size_t) pad]->setBounds (pad * (stripWidth + stripGap), 0, stripWidth, stripHeight);
}

} // namespace drumforgeui
