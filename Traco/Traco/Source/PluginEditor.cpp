#include "PluginEditor.h"

#include <algorithm>
#include <array>
#include <cmath>

#include "ForjaInfoText.h"
#include "PluginProcessor.h"
#include "engine/TracoEngine.h"
#include "forja/ui/ForjaChoice.h"
#include "forja/ui/ForjaKnob.h"
#include "forja/ui/ForjaTheme.h"

namespace
{
juce::String u (const char* text)
{
    return forja::ui::utf8 (text);
}

constexpr int windowWidth = 1000;
constexpr int windowHeight = 540;
constexpr int meterColumnWidth = 78;
constexpr int globalBarHeight = 34;
constexpr int panelWidth = 470;
constexpr int panelHeight = 152;

// Inversos das escalas do engine: o grafico devolve grandeza fisica, o APVTS
// guarda 0..1. Cada par (forward no engine, inverso aqui) e a unica moeda entre
// a UI e o DSP, entao ficam colados um do outro para nunca divergir.
float normForFreq (double hz) noexcept
{
    return static_cast<float> (forja::traco::freqToNorm (hz));
}

float normForGain (double db) noexcept
{
    return static_cast<float> ((db + 18.0) / 36.0);
}

float normForEssFreq (double hz) noexcept
{
    return static_cast<float> (juce::jlimit (0.0, 1.0, (hz - 4000.0) / 8000.0));
}

juce::String formatFreq (double hz)
{
    if (hz >= 1000.0)
        return juce::String (hz / 1000.0, hz < 10000.0 ? 2 : 1) + " kHz";

    return juce::String (juce::roundToInt (hz)) + " Hz";
}

// Identificadores por secao: 0 HP, 1 LP, 2..6 bandas, 7 de-esser.
juce::String idForSection (int section)
{
    if (section == 0)
        return "HP";
    if (section == 1)
        return "LP";
    if (section == 7)
        return "DE";
    return "B" + juce::String (section - 1);
}
} // namespace

TracoAudioProcessorEditor::TracoAudioProcessorEditor (TracoAudioProcessor& p)
    : ForjaEditorBase (
          p, p.apvts,
          { "TRACO",
            u ("EQ GR\u00c1FICO"),
            u ("PARAM\u00c9TRICO DE BANCADA"),
            "DSP ORIGINAL FORJA",
            u ("5 bandas + de-esser \u00b7 lat\u00eancia 0"),
            u ("DSP Forja sobre o Audio EQ Cookbook \u00b7 RTA de entrada e sa\u00edda \u00b7 lat\u00eancia 0 amostras"),
            forja::traco::makeForjaInfo() }),
      processor (p)
{
    addAndMakeVisible (graph);
    addAndMakeVisible (inputMeter);
    addAndMakeVisible (outputMeter);
    addAndMakeVisible (bypassButton);
    addChildComponent (panelCard);

    // O painel flutua: o usuario arrasta pelo cabecalho para tira-lo da frente
    // do RTA e puxa o canto para reduce-lo. Uma vez movido por ele, o layout
    // para de reposicionar o painel no resize da janela.
    panelCard.setFloatable (true);
    panelCard.onGeometryChanged = [this]
    {
        panelPlacedByUser = true;
        layoutPanelContents();
    };

    graph.responseSampler = [this] (float frequencyHz) -> float
    {
        const auto params = TracoAudioProcessor::paramsFromValues (
            [this] (const juce::String& id)
            {
                return processor.apvts.getRawParameterValue (id)->load();
            });
        return static_cast<float> (forja::traco::TracoEngine::compositeDbAt (
            params, processor.displaySampleRate(), static_cast<double> (frequencyHz)));
    };

    graph.onNodeSelected = [this] (const juce::String& id) { selectSection (sectionForId (id)); };
    graph.onBackgroundClicked = [this] { selectSection (-1); };
    graph.onNodeDrag = [this] (const juce::String& id, float frequency, float gain,
                               float widthNorm, bool widthDrag)
    {
        handleNodeDrag (id, frequency, gain, widthNorm, widthDrag);
    };
    graph.onNodeReset = [this] (const juce::String& id) { handleNodeReset (id); };
    graph.onNodeGlyph = [this] (const juce::String& id, int glyph) { handleNodeGlyph (id, glyph); };

    outputMeter.onClipReset = [this]
    {
        processor.resetClip();
        outputMeter.setClip (false);
    };

    bypassButton.setClickingTogglesState (true);
    bypassButton.setTooltip (u ("Bypass com rampa de 10 ms"));
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, "bypass", bypassButton);

    refreshGraph();
    selectSection (2); // banda 1 ligada por default: abre o painel de cara

    setSize (windowWidth, windowHeight);
}

TracoAudioProcessorEditor::~TracoAudioProcessorEditor()
{
}

void TracoAudioProcessorEditor::layoutContent (juce::Rectangle<int> content)
{
    const int graphWidth = content.getWidth() - meterColumnWidth - 8;
    const int graphHeight = content.getHeight() - globalBarHeight - 4;

    const juce::Rectangle<int> graphArea (content.getX(), content.getY(), graphWidth, graphHeight);
    graph.setBounds (graphArea);

    const int meterHeight = (graphHeight - 6) / 2;
    const int meterX = content.getX() + graphWidth + 8;
    inputMeter.setBounds (meterX, content.getY(), meterColumnWidth, meterHeight);
    outputMeter.setBounds (meterX, content.getY() + meterHeight + 6, meterColumnWidth, meterHeight);

    bypassButton.setBounds (content.getX(), content.getBottom() - globalBarHeight,
                            110, globalBarHeight - 4);

    // Painel contextual flutua centrado sobre a parte de baixo do grafico,
    // mas so ate o usuario move-lo ou redimensiona-lo pela primeira vez.
    if (! panelPlacedByUser)
    {
        const int panelX = graphArea.getCentreX() - panelWidth / 2;
        const int panelY = graphArea.getBottom() - panelHeight - 10;
        panelCard.setBounds (juce::jmax (graphArea.getX(), panelX), panelY, panelWidth, panelHeight);
    }

    layoutPanelContents();
}

void TracoAudioProcessorEditor::updateTelemetry()
{
    const auto snapshot = processor.readMeters();
    const bool bypassed = processor.isBypassed();

    inputMeter.setLevels (snapshot.inPeakL, snapshot.inPeakR);
    inputMeter.setClip (snapshot.inClip);
    outputMeter.setLevels (snapshot.outPeakL, snapshot.outPeakR);
    outputMeter.setClip (snapshot.outClip);

    std::array<float, static_cast<std::size_t> (TracoAudioProcessor::spectrumBands)> bands;
    processor.readSpectrumIn (bands.data(), TracoAudioProcessor::spectrumBands);
    graph.setSpectrum (bands.data(), static_cast<int> (bands.size()));

    refreshGraph();

    setMetaValues ({ { u ("ENTRADA"), juce::String (snapshot.inPeakL, 3) },
                     { u ("SA\u00cdDA"), juce::String (snapshot.outPeakL, 3) },
                     { u ("DE-ESSER"), juce::String (snapshot.essGainReductionDb, 1) + " dB" } });

    refreshBadge (snapshot.inClip || snapshot.outClip, bypassed, snapshot.inPeakL);
}

void TracoAudioProcessorEditor::refreshGraph()
{
    const auto params = TracoAudioProcessor::paramsFromValues (
        [this] (const juce::String& id)
        {
            return processor.apvts.getRawParameterValue (id)->load();
        });

    using namespace forja::traco;
    std::vector<forja::ui::ForjaEqGraph::Node> nodes;
    nodes.reserve (8);

    const auto glyphForType = [] (int type) -> int
    {
        return juce::jlimit (0, static_cast<int> (forja::ui::ForjaEqGraph::notch), type);
    };

    nodes.push_back ({ idForSection (0), static_cast<float> (params.hpFreq), 0.0f,
                       forja::ui::ForjaEqGraph::highPass, params.hpOn, false, true, false });
    nodes.push_back ({ idForSection (1), static_cast<float> (params.lpFreq), 0.0f,
                       forja::ui::ForjaEqGraph::lowPass, params.lpOn, false, true, false });

    // Cada banda ganha uma cor propria para as bolinhas se distinguirem no
    // grafico; o de-esser fica numa faixa abaixo da linha, fora da curva.
    const juce::Colour bandPalette[5] = {
        juce::Colour (0xffff7a18), juce::Colour (0xffe0b34a), juce::Colour (0xff5fb8a8),
        juce::Colour (0xff5b9fd6), juce::Colour (0xffa878d0)
    };

    for (int i = 0; i < TracoEngine::numBands; ++i)
    {
        const auto& band = params.bands[static_cast<std::size_t> (i)];
        forja::ui::ForjaEqGraph::Node node { idForSection (2 + i), static_cast<float> (band.freq),
                                             static_cast<float> (band.gainDb), glyphForType (band.type),
                                             band.on, false, false, true };
        node.accent = bandPalette[i];
        nodes.push_back (std::move (node));
    }

    forja::ui::ForjaEqGraph::Node deesser { idForSection (7), static_cast<float> (params.essFreq),
                                            0.0f, forja::ui::ForjaEqGraph::deEsser, params.essOn,
                                            false, true, true };
    deesser.accent = juce::Colour (0xffdb6d60);
    deesser.laneDb = -14.0f;
    nodes.push_back (std::move (deesser));

    for (std::size_t i = 0; i < nodes.size(); ++i)
        nodes[i].selected = (static_cast<int> (i) == selectedSection);

    graph.setBypassed (processor.isBypassed());
    graph.setNodes (std::move (nodes));
}

void TracoAudioProcessorEditor::selectSection (int section)
{
    selectedSection = section;
    rebuildPanel();
    refreshGraph();
}

void TracoAudioProcessorEditor::setParam (const juce::String& id, float norm)
{
    if (auto* parameter = processor.apvts.getParameter (id))
        parameter->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, norm));
}

int TracoAudioProcessorEditor::sectionForId (const juce::String& id)
{
    if (id == "HP")
        return 0;
    if (id == "LP")
        return 1;
    if (id == "DE")
        return 7;
    if (id.startsWith ("B"))
        return 1 + id.substring (1).getIntValue();
    return -1;
}

namespace
{
// Constroi um knob de valor normalizado 0..1 preso a um parametro do APVTS, com
// formatador fisico e anexo guardado no editor para durar ate a proxima selecao.
forja::ui::ForjaKnob* makeKnob (
    std::vector<std::unique_ptr<juce::Component>>& widgets,
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>>& attachments,
    juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId,
    const juce::String& label, const juce::String& hint,
    std::function<juce::String (double)> formatter, double defaultNorm)
{
    auto knob = std::make_unique<forja::ui::ForjaKnob> (
        label, hint, 62.0f, 0.0, 1.0, 0.0, defaultNorm, std::move (formatter),
        forja::ui::ForjaKnob::Options { 116, true, true, true });

    auto* raw = knob.get();
    attachments.push_back (
        std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            apvts, paramId, raw->getSlider()));
    widgets.push_back (std::move (knob));
    return raw;
}

forja::ui::ForjaChoice* makeChoice (
    std::vector<std::unique_ptr<juce::Component>>& widgets, const juce::String& caption,
    const juce::StringArray& items, int initialIndex, std::function<void (int)> onChosen)
{
    auto choice = std::make_unique<forja::ui::ForjaChoice> (caption, items);
    auto* raw = choice.get();
    raw->getComboBox().setSelectedItemIndex (initialIndex, juce::dontSendNotification);
    raw->getComboBox().onChange = [raw, onChosen]
    {
        if (onChosen != nullptr)
            onChosen (raw->getComboBox().getSelectedItemIndex());
    };
    widgets.push_back (std::move (choice));
    return raw;
}

juce::ToggleButton* makeToggle (
    std::vector<std::unique_ptr<juce::Component>>& widgets,
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>>& attachments,
    juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId, const juce::String& text)
{
    auto toggle = std::make_unique<juce::ToggleButton> (text);
    auto* raw = toggle.get();
    raw->setClickingTogglesState (true);
    attachments.push_back (
        std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            apvts, paramId, *raw));
    widgets.push_back (std::move (toggle));
    return raw;
}
} // namespace

void TracoAudioProcessorEditor::rebuildPanel()
{
    panelSliderAttachments.clear();
    panelButtonAttachments.clear();

    for (auto& widget : panelWidgets)
        panelCard.removeChildComponent (widget.get());
    panelWidgets.clear();

    panelKnobA = panelKnobB = panelKnobC = nullptr;
    panelChoice = nullptr;
    panelOnToggle = panelListenToggle = nullptr;

    if (selectedSection < 0)
    {
        panelCard.setVisible (false);
        return;
    }

    const auto params = TracoAudioProcessor::paramsFromValues (
        [this] (const juce::String& id)
        {
            return processor.apvts.getRawParameterValue (id)->load();
        });

    using namespace forja::traco;
    auto& state = processor.apvts;

    forja::ui::ForjaKnob* knobA = nullptr;
    forja::ui::ForjaKnob* knobB = nullptr;
    forja::ui::ForjaKnob* knobC = nullptr;
    forja::ui::ForjaChoice* choice = nullptr;
    juce::ToggleButton* onToggle = nullptr;
    juce::ToggleButton* listenToggle = nullptr;

    if (selectedSection == 0 || selectedSection == 1)
    {
        const bool isHp = selectedSection == 0;
        const juce::String onId = isHp ? "hpOn" : "lpOn";
        const juce::String freqId = isHp ? "hpFreq" : "lpFreq";
        const juce::String slopeId = isHp ? "hpSlope" : "lpSlope";

        panelCard.setTitle (isHp ? u ("PASSA-ALTA") : u ("PASSA-BAIXA"));
        panelCard.setIndex (selectedSection);

        onToggle = makeToggle (panelWidgets, panelButtonAttachments, state, onId, u ("LIGAR"));
        knobA = makeKnob (panelWidgets, panelSliderAttachments, state, freqId, u ("FREQ"),
                          isHp ? "corte" : u ("corte"),
                          [] (double v) { return formatFreq (scaleFreq (v)); },
                          isHp ? 0.06 : 0.95);
        choice = makeChoice (
            panelWidgets, u ("INCLINACAO"), { "12 dB/oct", "24 dB/oct" },
            (isHp ? params.hpSlope24 : params.lpSlope24) ? 1 : 0,
            [this, slopeId] (int index) { setParam (slopeId, index > 0 ? 1.0f : 0.0f); });
    }
    else if (selectedSection == 7)
    {
        panelCard.setTitle (u ("DE-ESSER"));
        panelCard.setIndex (selectedSection);

        onToggle = makeToggle (panelWidgets, panelButtonAttachments, state, "essOn", u ("LIGAR"));
        listenToggle = makeToggle (panelWidgets, panelButtonAttachments, state, "essListen", u ("OUVIR"));
        knobA = makeKnob (panelWidgets, panelSliderAttachments, state, "essFreq", u ("CENTRO"), "split",
                          [] (double v) { return formatFreq (scaleEssFreq (v)); }, 0.31);
        knobB = makeKnob (panelWidgets, panelSliderAttachments, state, "essWidth", u ("LARGURA"), "Q",
                          [] (double v) { return juce::String (scaleEssWidth (v), 2); }, 0.5);
        knobC = makeKnob (panelWidgets, panelSliderAttachments, state, "essRange", u ("ALCANCE"),
                          u ("m\u00e1x dB"),
                          [] (double v) { return juce::String (scaleEssRange (v), 1) + " dB"; }, 0.38);
    }
    else
    {
        const int band = selectedSection - 2;
        const juce::String n = "b" + juce::String (band + 1);
        const auto& bandParams = params.bands[static_cast<std::size_t> (band)];

        panelCard.setTitle (u ("BANDA ") + juce::String (band + 1));
        panelCard.setIndex (selectedSection);

        onToggle = makeToggle (panelWidgets, panelButtonAttachments, state, n + "On", u ("LIGAR"));
        listenToggle = makeToggle (panelWidgets, panelButtonAttachments, state, n + "Listen", u ("OUVIR"));

        const juce::StringArray types { u ("Campainha"), u ("Prat. baixa"), u ("Prat. alta"),
                                        u ("Passa-alta"), u ("Passa-baixa"), "Notch" };
        choice = makeChoice (
            panelWidgets, u ("TIPO"), types, bandParams.type,
            [this, id = n + "Type"] (int index) { setParam (id, static_cast<float> (index) / 5.0f); });

        knobA = makeKnob (panelWidgets, panelSliderAttachments, state, n + "Freq", u ("FREQ"), "Hz",
                          [] (double v) { return formatFreq (scaleFreq (v)); },
                          processor.bandDefaultFreqNorm[static_cast<std::size_t> (band)]);
        knobB = makeKnob (panelWidgets, panelSliderAttachments, state, n + "Gain", u ("GAIN"),
                          u ("ganho"),
                          [] (double v) { return forja::ui::formatDb (static_cast<float> (scaleGain (v))); },
                          0.5);
        knobC = makeKnob (panelWidgets, panelSliderAttachments, state, n + "Q", "Q", u ("largura"),
                          [] (double v) { return juce::String (scaleQ (v), 2); }, 0.5);
    }

    panelKnobA = knobA;
    panelKnobB = knobB;
    panelKnobC = knobC;
    panelChoice = choice;
    panelOnToggle = onToggle;
    panelListenToggle = listenToggle;

    for (auto& widget : panelWidgets)
    {
        widget->setInterceptsMouseClicks (true, true);
        panelCard.addAndMakeVisible (*widget);
    }

    panelCard.setVisible (true);
    panelCard.toFront (false);
    layoutPanelContents();
}

void TracoAudioProcessorEditor::layoutPanelContents()
{
    // Layout interno do painel: toggles + choice na faixa de cima, knobs embaixo.
    const auto area = panelCard.getContentBounds();
    const int topY = area.getY();
    const int choiceWidth = 150;
    const int toggleWidth = 62;

    int pen = area.getX();
    if (panelOnToggle != nullptr)
    {
        panelOnToggle->setBounds (pen, topY, toggleWidth, 22);
        pen += toggleWidth + 6;
    }
    if (panelListenToggle != nullptr)
    {
        panelListenToggle->setBounds (pen, topY, toggleWidth, 22);
        pen += toggleWidth + 6;
    }

    if (panelChoice != nullptr)
        panelChoice->setBounds (area.getRight() - choiceWidth, topY - 2, choiceWidth, 28);

    forja::ui::ForjaKnob* knobs[] = { panelKnobA, panelKnobB, panelKnobC };
    int count = 0;
    for (auto* k : knobs)
        if (k != nullptr)
            ++count;

    if (count > 0)
    {
        const int knobZoneY = topY + 30;
        const int zoneHeight = area.getBottom() - knobZoneY;
        const int slot = area.getWidth() / count;

        int placed = 0;
        for (auto* k : knobs)
        {
            if (k == nullptr)
                continue;

            const int kw = k->getWidth();
            const int kh = k->getHeight();
            const int x = area.getX() + placed * slot + (slot - kw) / 2;
            k->setBounds (x, knobZoneY + (zoneHeight - kh) / 2, kw, kh);
            ++placed;
        }
    }
}

void TracoAudioProcessorEditor::handleNodeDrag (const juce::String& id, float frequencyHz,
                                                float gainDb, float widthNorm, bool widthDrag)
{
    const int section = sectionForId (id);

    if (section == 0)
    {
        setParam ("hpFreq", normForFreq (frequencyHz));
    }
    else if (section == 1)
    {
        setParam ("lpFreq", normForFreq (frequencyHz));
    }
    else if (section >= 2 && section <= 6)
    {
        const juce::String n = "b" + juce::String (section - 1);
        setParam (n + "Freq", normForFreq (frequencyHz));

        if (widthDrag)
            setParam (n + "Q", widthNorm);
        else
            setParam (n + "Gain", normForGain (gainDb));
    }
    else if (section == 7)
    {
        setParam ("essFreq", normForEssFreq (frequencyHz));

        if (widthDrag)
            setParam ("essWidth", widthNorm);
    }
}

void TracoAudioProcessorEditor::handleNodeReset (const juce::String& id)
{
    const int section = sectionForId (id);

    if (section >= 2 && section <= 6)
    {
        const juce::String n = "b" + juce::String (section - 1);
        setParam (n + "Gain", 0.5f); // 0 dB
    }
    else if (section == 0)
    {
        setParam ("hpFreq", 0.06f);
    }
    else if (section == 1)
    {
        setParam ("lpFreq", 0.95f);
    }
    else if (section == 7)
    {
        setParam ("essFreq", 0.31f);
        setParam ("essWidth", 0.5f);
    }
}

void TracoAudioProcessorEditor::handleNodeGlyph (const juce::String& id, int glyph)
{
    const int section = sectionForId (id);

    if (section < 2 || section > 6)
        return;

    setParam ("b" + juce::String (section - 1) + "Type", static_cast<float> (glyph) / 5.0f);
}

void TracoAudioProcessorEditor::refreshBadge (bool clipped, bool bypassed, float inPeak)
{
    auto kind = forja::ui::BadgeKind::ready;
    juce::String text ("PRONTO");
    juce::String detail;

    if (clipped)
    {
        kind = forja::ui::BadgeKind::clip;
        text = "CLIP";
        detail = u ("Sa\u00edda acima de 0 dBFS \u00b7 reduza o ganho das bandas.");
    }
    else if (bypassed)
    {
        kind = forja::ui::BadgeKind::bypass;
        text = "BYPASS";
        detail = u ("Bypass ativo \u00b7 sinal seco com rampa de 10 ms.");
    }
    else if (inPeak <= 0.001f)
    {
        kind = forja::ui::BadgeKind::silent;
        text = "SEM SINAL";
        detail = u ("Sem sinal na entrada \u00b7 aguardando \u00e1udio da DAW.");
    }
    else
    {
        detail = u ("Clique numa banda para abrir os controles dela.");
    }

    setBadge (kind, text, detail);
}
