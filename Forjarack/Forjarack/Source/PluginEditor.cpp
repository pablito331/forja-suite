#include "PluginEditor.h"

#include "ForjaInfoText.h"
#include "PluginProcessor.h"
#include "forja/ui/ForjaTheme.h"
#include "ui/RackKnob.h"

namespace
{
constexpr int windowWidth = 1000;
constexpr int windowHeight = 540;
constexpr int masterWidth = 140;

juce::String u (const char* text)
{
    return forja::ui::utf8 (text);
}
} // namespace

namespace forjarackui
{

// Seção master da Serie 500: trim de entrada/saida, inversao de fase e a
// medição estéreo do barramento. Vive no canvas virtual, junto das gavetas.
class MasterSection : public juce::Component
{
public:
    explicit MasterSection (juce::AudioProcessorValueTreeState& state)
    {
        inKnob = std::make_unique<RackKnob> (u ("ENTRADA"), 34.0f);
        outKnob = std::make_unique<RackKnob> (u ("SAIDA"), 34.0f);

        inAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            state, "input_trim", inKnob->getSlider());
        outAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            state, "output_trim", outKnob->getSlider());

        addAndMakeVisible (*inKnob);
        addAndMakeVisible (*outKnob);

        phaseButton.setClickingTogglesState (true);
        phaseButton.setWantsKeyboardFocus (false);
        phaseButton.setButtonText (u ("\u00d8 R"));
        phaseButton.setTooltip (u ("Inverter fase do canal direito"));
        addAndMakeVisible (phaseButton);

        phaseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            state, "phase_invert", phaseButton);
    }

    void setLevels (float inL, float inR, float outL, float outR, bool midiArrived)
    {
        peakInL = std::max (inL, peakInL * 0.85f);
        peakInR = std::max (inR, peakInR * 0.85f);
        peakOutL = std::max (outL, peakOutL * 0.85f);
        peakOutR = std::max (outR, peakOutR * 0.85f);

        // O evento chega em blocos curtos; a leitura e por polling. Segurar o
        // led por um instante faz nota picada e trilha densa aparecerem iguais.
        if (midiArrived)
            midiLitUntilMs = static_cast<double> (juce::Time::getMillisecondCounterHiRes ()) + 220.0;

        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds();

        g.setColour (forja::ui::colour::iron3);
        g.fillRoundedRectangle (bounds.toFloat(), 4.0f);
        g.setColour (forja::ui::colour::ember.withAlpha (0.5f));
        g.drawRoundedRectangle (bounds.toFloat().reduced (0.5f), 4.0f, 1.0f);

        g.setFont (forja::ui::tracked (forja::ui::uiFont (10.0f, juce::Font::bold), 0.14f));
        g.setColour (forja::ui::colour::emberPale);
        g.drawText (u ("MASTER"), bounds.getX(), 6, bounds.getWidth(), 16,
                    juce::Justification::centred, false);

        drawStereoMeter (g, bounds.withY (bounds.getHeight() - 118).withHeight (108));
        drawMidiLed (g);
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        auto top = bounds.removeFromTop (120);

        const int knobW = top.getWidth() / 2;
        inKnob->setBounds (top.withX (0).withWidth (knobW).withY (44).withHeight (60));
        outKnob->setBounds (top.withX (knobW).withWidth (knobW).withY (44).withHeight (60));

        auto mid = bounds.withY (120).withHeight (30).reduced (16, 0);
        const int ledW = 62;
        midiLedArea = mid.withWidth (ledW);
        phaseButton.setBounds (mid.withX (mid.getX() + ledW + 10).withWidth (mid.getWidth() - ledW - 10));
    }

private:
    // Led de atividade MIDI da fileira central: a unica pista de que o MIDI da
    // faixa entrou no rack (o medidor ENTRADA so mostra o audio do barramento).
    void drawMidiLed (juce::Graphics& g)
    {
        if (midiLedArea.isEmpty())
            return;

        const bool lit = static_cast<double> (juce::Time::getMillisecondCounterHiRes ()) < midiLitUntilMs;

        g.setColour (lit ? forja::ui::colour::ember : forja::ui::colour::iron3);
        g.fillRoundedRectangle (midiLedArea.toFloat().reduced (1.0f), 4.0f);
        g.setColour (lit ? forja::ui::colour::emberPale.withAlpha (0.85f)
                         : forja::ui::colour::ember.withAlpha (0.45f));
        g.drawRoundedRectangle (midiLedArea.toFloat().reduced (1.0f), 4.0f, 1.0f);

        g.setFont (forja::ui::tracked (forja::ui::uiFont (9.0f, juce::Font::bold), 0.12f));
        g.setColour (lit ? forja::ui::colour::emberPale : forja::ui::colour::dim);
        g.drawText ("MIDI", midiLedArea, juce::Justification::centred, false);
    }

    void drawStereoMeter (juce::Graphics& g, juce::Rectangle<int> area)
    {
        auto track = area.reduced (16, 0).toFloat();
        const float labelH = 11.0f;
        const float bottom = track.getBottom() - labelH;
        const float height = track.getHeight() - labelH;

        // Quatro barras em dois grupos de esterio: ENTRADA (L|R) e SAIDA
        // (L|R). L e R ficam colados dentro do grupo para leitura classica de
        // medidor estéreo; o grupo largo no meio separa in/saida.
        const float pairGap = 3.0f;
        const float groupGap = 12.0f;
        const float barW = (track.getWidth() - 3.0f * pairGap - groupGap) / 4.0f;

        auto drawBar = [&] (float x, float peak)
        {
            juce::Rectangle<float> bar (x, bottom, barW, -height);
            g.setColour (forja::ui::colour::meterTrack);
            g.fillRoundedRectangle (bar, 2.0f);

            const float level = std::sqrt (juce::jlimit (0.0f, 1.0f, peak)) * height;
            const juce::Colour fill = peak > 0.98f ? forja::ui::colour::meterClip
                              : peak > 0.85f ? forja::ui::colour::meterHot
                                             : forja::ui::colour::success;
            g.setColour (fill);
            g.fillRoundedRectangle (bar.withHeight (-level), 2.0f);
        };

        const float x0 = track.getX();
        const float x1 = x0 + barW + pairGap;
        const float x2 = x1 + barW + groupGap;
        const float x3 = x2 + barW + pairGap;

        drawBar (x0, peakInL);
        drawBar (x1, peakInR);
        drawBar (x2, peakOutL);
        drawBar (x3, peakOutR);

        // Legenda por grupo: L/R vivem juntos dentro de cada par.
        const float groupW = barW * 2.0f + pairGap;
        g.setFont (forja::ui::tracked (forja::ui::uiFont (7.0f, juce::Font::bold), 0.06f));
        g.setColour (forja::ui::colour::dim);
        g.drawText (u ("ENT"), juce::Rectangle<float> (x0, bottom + 2.0f, groupW, labelH),
                    juce::Justification::centred, false);
        g.drawText (u ("SAI"), juce::Rectangle<float> (x2, bottom + 2.0f, groupW, labelH),
                    juce::Justification::centred, false);
    }

    std::unique_ptr<RackKnob> inKnob;
    std::unique_ptr<RackKnob> outKnob;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> inAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outAttachment;

    juce::TextButton phaseButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> phaseAttachment;

    float peakInL = 0.0f;
    float peakInR = 0.0f;
    float peakOutL = 0.0f;
    float peakOutR = 0.0f;

    juce::Rectangle<int> midiLedArea;
    double midiLitUntilMs = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MasterSection)
};

} // namespace forjarackui

ForjarackAudioProcessorEditor::ForjarackAudioProcessorEditor (ForjarackAudioProcessor& owner)
    : ForjaEditorBase (
          owner, owner.apvts,
          { "FORJARACK",
            u ("RACK SERIE 500"),
            "8 SLOTS / SERIE / CADEIA",
            "AIRWINDOWS MIT",
            u ("Rack modular em cadeia · módulos Airwindows MIT"),
            u ("Engine Forja · DSP Airwindows MIT · lat\u00eancia 0 amostras"),
            forjarack::makeForjaInfo () }),
      processor (owner)
{
    for (int slot = 0; slot < forjarack::kNumSlots; ++slot)
    {
        slots[(size_t) slot] = std::make_unique<forjarackui::SlotComponent> (owner.apvts, slot, &owner);
        addAndMakeVisible (*slots[(size_t) slot]);
        slots[(size_t) slot]->refresh();
        lastType[(size_t) slot] = slots[(size_t) slot]->currentTypeIndex();
        lastHostedParams[(size_t) slot] = owner.paramCount (slot);
    }

    master = std::make_unique<forjarackui::MasterSection> (owner.apvts);
    addAndMakeVisible (*master);

    setSize (windowWidth, windowHeight);
}

ForjarackAudioProcessorEditor::~ForjarackAudioProcessorEditor() = default;

void ForjarackAudioProcessorEditor::layoutContent (juce::Rectangle<int> content)
{
    auto masterArea = content.removeFromRight (masterWidth);
    master->setBounds (masterArea);

    content.removeFromRight (8);

    const int gap = 5;
    const int slotWidth = (content.getWidth() - gap * (forjarack::kNumSlots - 1))
                        / forjarack::kNumSlots;

    auto x = content.getX();

    for (int slot = 0; slot < forjarack::kNumSlots; ++slot)
    {
        slots[(size_t) slot]->setBounds (x, content.getY(), slotWidth, content.getHeight());
        x += slotWidth + gap;
    }
}

void ForjarackAudioProcessorEditor::updateTelemetry()
{
    auto& engine = processor.getEngine();

    int activeCount = 0;
    bool anyClip = false;

    for (int slot = 0; slot < forjarack::kNumSlots; ++slot)
    {
        auto& target = *slots[(size_t) slot];

        if (target.currentTypeIndex() != lastType[(size_t) slot])
        {
            lastType[(size_t) slot] = target.currentTypeIndex();
            target.refresh();
        }

        // Gaveta hospedada: a carga do plugin acaba no timer do processor. Quando
        // o numero de params do plugin vivo muda (carregou/esvaziou), relayouta.
        if (processor.isHosted (slot))
        {
            const int params = processor.paramCount (slot);

            if (params != lastHostedParams[(size_t) slot])
            {
                lastHostedParams[(size_t) slot] = params;
                target.refresh();
            }
        }

        // Banda de arquivos: na gaveta de instrumento o seletor mostra o
        // material vivo, que muda em background (idle/carga do kit/bank).
        if (processor.isHosted (slot) || processor.isDrawer (slot))
        {
            const auto bandFile = processor.fileName (slot);

            if (bandFile != lastBandFile[(size_t) slot])
            {
                lastBandFile[(size_t) slot] = bandFile;
                target.refresh();
            }
        }

        // Fileira de INSTRUMENTO: nasce quando o bank assenta (o preset em uso
        // passa de vazio a um nome) e acompanha troca vinda do proprio seletor.
        if (processor.isDrawerWithInstruments (slot))
        {
            const auto instrument = processor.currentInstrument (slot);

            if (instrument != lastInstrument[(size_t) slot])
            {
                lastInstrument[(size_t) slot] = instrument;
                target.refresh();
            }
        }

        target.setOutputPeak (engine.readSlotPeak (slot));
        target.setGainReductionDb (engine.readSlotGainReductionDb (slot));

        if (target.currentTypeIndex() != 0)
            ++activeCount;
    }

    const auto meters = engine.readMasterMeters();
    master->setLevels (meters.inPeakL, meters.inPeakR, meters.outPeakL, meters.outPeakR, meters.midi);

    const double now = juce::Time::getMillisecondCounterHiRes();

    if (meters.clip)
        clipHoldUntilMs = now + 1200.0;

    const bool clipped = now < clipHoldUntilMs;
    anyClip = clipped;
    engine.resetClip();

    const bool bypassed = processor.apvts.getRawParameterValue ("bypass")->load() > 0.5f;

    auto kind = forja::ui::BadgeKind::ready;
    juce::String text ("PRONTO");
    juce::String detail;

    if (bypassed)
    {
        kind = forja::ui::BadgeKind::bypass;
        text = "BYPASS";
        detail = u ("Bypass do host · sa\u00edda com rampa de 10 ms.");
    }
    else if (anyClip)
    {
        kind = forja::ui::BadgeKind::clip;
        text = "CLIP";
        detail = u ("Sa\u00edda acima de 0 dBFS · reduza o trim de sa\u00edda.");
    }
    else if (activeCount == 0)
    {
        kind = forja::ui::BadgeKind::silent;
        text = "VAZIO";
        detail = u ("Nenhum m\u00f3dulo no rack · sinal em pass-through.");
    }
    else
    {
        detail = juce::String (activeCount) + u (" m\u00f3dulo(s) na cadeia.");
    }

    setBadge (kind, text, detail);
    setMetaValues ({
        { "slots", juce::String (activeCount) + "/" + juce::String (forjarack::kNumSlots) }
    });
}
