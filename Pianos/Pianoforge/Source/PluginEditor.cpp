#include "PluginEditor.h"

#include "ForjaInfoText.h"
#include "PluginProcessor.h"
#include "forja/ui/ForjaTheme.h"

namespace
{
using namespace forja::pianoforge;

juce::String u (const char* text)
{
    return forja::ui::utf8 (text);
}

constexpr int windowWidth = 1000;
constexpr int windowHeight = 540;
constexpr int tabRowHeight = 24;

// Uma linha por grupo, na ordem dos ParamId. O editor le o rotulo e o default da
// tabela kParamDefs; aqui so mora a dica curta (utf-8) de cada disco.
struct KnobPlacement
{
    int paramId;
    const char* hint;
};

const KnobPlacement* groupKnobs (const forja::ui::ForjaCard& card, int& count) noexcept
{
    // Cinco grupos na tela PIANO, no maximo tres discos por card (o card de um
    // terco de coluna tem ~200 px de conteudo; um disco compacto cabe em ~98 px,
    // entao 3 cabem sem sobrepor). A tampa e a curva de velocity sairam do PIANO e
    // moram agora no MOTOR, que tem espaco de sobra, para abrir vaga ao card MOLHO.
    static const KnobPlacement dyn[] = {
        { Param_Volume, "nivel de saida" },
        { Param_Hammer, "ataque do martelo" }
    };
    static const KnobPlacement corpo[] = {
        { Param_Body, "curva de radiacao do tampo" },
        { Param_Ring, "a madeira vibrando" }
    };
    static const KnobPlacement eq[] = {
        { Param_BassEQ, "graves" },
        { Param_MidEQ, "medios" },
        { Param_HighEQ, "agudos" }
    };
    static const KnobPlacement molho[] = {
        { Param_Compressor, "compressor de um knob" },
        { Param_Molho, "nivel do halo +12 do shimmer" },
        { Param_Quinta, "dose da 12a acima da oitava" }
    };
    static const KnobPlacement espaco[] = {
        { Param_SympRes, "ressonancia das cordas" },
        { Param_Chorus, "chorus" },
        { Param_Reverb, "ambiencia" }
    };
    static const KnobPlacement motor[] = {
        { Param_Lid, "abertura da tampa" },
        { Param_VelCurve, "curva de velocity" },
        { Param_ReverbType, "tipo de sala" },
        { Param_TrimDb, "trim db" },
        { Param_Width, "largura esterio" },
        { Param_ReleaseDamp, "amortecer soltura" },
        { Param_HammerTone, "brilho do martelo" },
        { Param_SympTune, "afinacao ressonancia" },
        { Param_EngineTune, "afinacao global (cents)" },
        { Param_Tail, "tempo da cauda do shimmer" }
    };

    // O JUCE 9 guarda o componentID como String: a chave do layout e uma etiqueta
    // curta, nao um inteiro. Os cards sem etiqueta (banco, saida) caem no default.
    const auto tag = card.getComponentID();

    if (tag == "dyn")    { count = 2; return dyn; }    // Dinamica
    if (tag == "corpo")  { count = 2; return corpo; }  // Corpo
    if (tag == "eq")     { count = 3; return eq; }     // Equalizacao
    if (tag == "molho")  { count = 3; return molho; }  // Molho (shimmer espectral)
    if (tag == "espaco") { count = 3; return espaco; } // Espaco
    if (tag == "motor")  { count = 10; return motor; } // Motor (2 fileiras x 5)

    count = 0;
    return nullptr;
}
} // namespace

PianoforgeAudioProcessorEditor::PianoforgeAudioProcessorEditor (PianoforgeAudioProcessor& p)
    : ForjaEditorBase (
          p, p.apvts,
          { "PIANOFORGE",
            u ("MODELO F\u00cdSICO"),
            "23 PARAMETROS / FDN / SHIMMER",
            "TINYSOUNDFONT MIT",
            u ("Soundfont de piano + comportamento fisico"),
            u ("TinySoundFont v0.9 \u00b7 DSP portado do projeto-irmao \u00b7 \u00b7 "
               "lat\u00eancia medida por impulso"),
            forja::pianoforge::makeForjaInfo() }),
      processor (p)
{
    // Etiquetas para o layout reconhecer cada card (o componentID e uma String no JUCE 9).
    dynCard.setComponentID ("dyn");
    corpoCard.setComponentID ("corpo");
    eqCard.setComponentID ("eq");
    molhoCard.setComponentID ("molho");
    spaceCard.setComponentID ("espaco");
    motorCard.setComponentID ("motor");

    auto addKnobTo = [this] (forja::ui::ForjaCard& card, int paramId, const char* hint)
    {
        auto knob = makeKnob (paramId, u (hint));
        auto* raw = knob.get();
        knobs[static_cast<std::size_t> (paramId)] = std::move (knob);
        card.addAndMakeVisible (*raw);
    };

    addKnobTo (dynCard, Param_Volume, "nivel de saida");
    addKnobTo (dynCard, Param_Hammer, "ataque do martelo");

    addKnobTo (corpoCard, Param_Body, "curva de radiacao do tampo");
    addKnobTo (corpoCard, Param_Ring, "a madeira vibrando");

    addKnobTo (eqCard, Param_BassEQ, "graves");
    addKnobTo (eqCard, Param_MidEQ, "medios");
    addKnobTo (eqCard, Param_HighEQ, "agudos");

    // Compressor: disco INVERTIDO (gire a esquerda = achata mais). Sem attachment;
    // o editor costura slider<->parametro em makeInvertedKnob.
    {
        auto knob = makeInvertedKnob (Param_Compressor, "achata picos / gire a esquerda");
        auto* raw = knob.get();
        molhoCard.addAndMakeVisible (*raw);
        knobs[static_cast<std::size_t> (Param_Compressor)] = std::move (knob);
    }
    addKnobTo (molhoCard, Param_Molho, "nivel do halo +12 do shimmer");
    addKnobTo (molhoCard, Param_Quinta, "dose da 12a acima da oitava");

    addKnobTo (spaceCard, Param_SympRes, "ressonancia das cordas");
    addKnobTo (spaceCard, Param_Chorus, "chorus");
    addKnobTo (spaceCard, Param_Reverb, "ambiencia");

    // MOTOR: ajuste fino + a tampa e a curva realocadas + o tempo da cauda do shimmer.
    addKnobTo (motorCard, Param_Lid, "abertura da tampa");
    addKnobTo (motorCard, Param_VelCurve, "curva de velocity");
    addKnobTo (motorCard, Param_ReverbType, "tipo de sala");
    addKnobTo (motorCard, Param_TrimDb, "trim db");
    addKnobTo (motorCard, Param_Width, "largura esterio");
    addKnobTo (motorCard, Param_ReleaseDamp, "amortecer soltura");
    addKnobTo (motorCard, Param_HammerTone, "brilho do martelo");
    addKnobTo (motorCard, Param_SympTune, "afinacao ressonancia");
    addKnobTo (motorCard, Param_EngineTune, "afinacao global (cents)");
    addKnobTo (motorCard, Param_Tail, "tempo da cauda do shimmer");

    // Banco e saida.
    bankCard.addAndMakeVisible (bankChoice);
    bankCard.addAndMakeVisible (instrumentChoice);
    bankCard.addAndMakeVisible (refreshButton);
    bankCard.addAndMakeVisible (statusLabel);

    outCard.addAndMakeVisible (outputMeter);
    outCard.addAndMakeVisible (bypassButton);

    // Cards da tela PIANO ficam visiveis; o card MOTOR começa escondido.
    addAndMakeVisible (dynCard);
    addAndMakeVisible (corpoCard);
    addAndMakeVisible (eqCard);
    addAndMakeVisible (molhoCard);
    addAndMakeVisible (spaceCard);
    addAndMakeVisible (bankCard);
    addAndMakeVisible (outCard);
    addAndMakeVisible (pianoArt);
    addChildComponent (motorCard);

    // Abas no estilo Drumforge: radio group + troca de visibilidade.
    for (auto* tab : { &pianoTab, &motorTab })
    {
        tab->setClickingTogglesState (true);
        tab->setRadioGroupId (0x4a4e);
        tab->setWantsKeyboardFocus (false);
        addAndMakeVisible (*tab);
    }

    pianoTab.onClick = [this] { setActiveTab (0); };
    motorTab.onClick = [this] { setActiveTab (1); };

    // Instrumento do banco (preset unico). O choice nao e um parametro APVTS: a
    // lista muda com cada banco, entao dirige direto o engine, como no CANIVETE.
    instrumentChoice.getComboBox().onChange = [this]
    {
        if (const int id = instrumentChoice.getComboBox().getSelectedId(); id > 0)
            processor.setPresetIndex (id - 1);
    };

    refreshButton.onClick = [this]
    {
        processor.refreshLibrary();
        rebuildBankChoice();
        refreshBankViews();
    };

    statusLabel.setJustificationType (juce::Justification::centredLeft);
    statusLabel.setFont (forja::ui::uiFont (8.5f));
    statusLabel.setColour (juce::Label::textColourId, juce::Colour (0xff8a97a5));
    statusLabel.setInterceptsMouseClicks (false, false);

    rebuildBankChoice();
    rebuildPresetChoice();

    bypassButton.setClickingTogglesState (true);
    bypassButton.setTooltip (u ("Bypass com rampa de 10 ms"));
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, "bypass", bypassButton);

    outputMeter.onClipReset = [this] { outputMeter.setClip (false); };

    refreshBankViews();
    setActiveTab (0);

    // Estado inicial da arte da tampa: pinta ja na abertura de fabrica (0,52)
    // em vez de piscar fechada ate o primeiro tick do timer de telemetria, e
    // escreve o banco ja carregado na partitura.
    pianoArt.update (engineParam01 (Param_Lid), engineParam01 (Param_Body),
                     engineParam01 (Param_Ring));
    pianoArt.setName (processor.getBankName());

    setSize (windowWidth, windowHeight);
}

PianoforgeAudioProcessorEditor::~PianoforgeAudioProcessorEditor()
{
    // O disco invertido do Compressor e o unico com costura manual: desinscreve o
    // listener antes de qualquer destruicao de slider.
    processor.apvts.removeParameterListener (forja::pianoforge::kParamDefs[forja::pianoforge::Param_Compressor].id, this);
    compressorSlider = nullptr;

    bankAttachment.reset();   // o choice e dono do combo; o attachment primeiro
}

std::unique_ptr<forja::ui::ForjaKnob> PianoforgeAudioProcessorEditor::makeKnob (int paramId,
                                                                                const juce::String& hint)
{
    const auto& def = kParamDefs[paramId];
    const double interval = (paramId == Param_ReverbType) ? (1.0 / 3.0) : 0.01;

    // Disco compacto: 44 px de cabeca. A pilha do knob e diametro + 27 px de
    // rotulo/dica, e comeca abaixo do cabecalho de 44 px do card; num card rowH
    // (~115 px) isso exige diametro <= 44 para o NOME caber inteiro (58 cortava o
    // rotulo na borda de baixo). Largura minima 84 px: tres discos cabem na celula
    // de ~101 px da meia coluna sem sobrepor.
    auto knob = std::make_unique<forja::ui::ForjaKnob> (
        forja::ui::utf8 (def.label), hint, 44.0f,
        0.0, 1.0, interval, def.default01,
        [this, paramId] (double value) { return PianoforgeAudioProcessor::paramText (paramId, value); },
        forja::ui::ForjaKnobOptions { 84, true, true, true });

    attachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, def.id, knob->getSlider()));

    return knob;
}

std::unique_ptr<forja::ui::ForjaKnob> PianoforgeAudioProcessorEditor::makeInvertedKnob (
    int paramId, const juce::String& hint)
{
    const auto& def = kParamDefs[paramId];

    // O espaco do SLIDER e espelhado em relacao ao parametro: slider 1 = off (lado
    // direito, o default honesto), slider 0 = achata no talo (lado esquerdo). O
    // valor exibido e o amount (0..1) = 1 - slider, entao a leitura continua
    // "0%..100% de squash" mesmo com a mao invertida.
    auto knob = std::make_unique<forja::ui::ForjaKnob> (
        forja::ui::utf8 (def.label), hint, 44.0f,
        0.0, 1.0, 0.01, 1.0,
        [this, paramId] (double sliderValue)
        {
            return PianoforgeAudioProcessor::paramText (paramId, 1.0 - sliderValue);
        },
        forja::ui::ForjaKnobOptions { 84, true, true, true });

    auto& slider = knob->getSlider();
    compressorSlider = &slider;

    // Estado inicial espelhando o APVTS (default 0 = off -> slider 1), sem notificacao
    // para nao reescrever o parametro na abertura.
    if (auto* prm = processor.apvts.getParameter (def.id))
        slider.setValue (1.0 - static_cast<double> (prm->getValue()), juce::dontSendNotification);

    // Slider -> parametro (drag do usuario). A trava corta o eco com o listener.
    knob->onValueChange = [this, paramId]
    {
        if (syncingCompressor || compressorSlider == nullptr)
            return;
        syncingCompressor = true;
        if (auto* prm = processor.apvts.getParameter (kParamDefs[paramId].id))
            prm->setValueNotifyingHost (static_cast<float> (1.0 - compressorSlider->getValue()));
        syncingCompressor = false;
    };

    // So este disco usa costura manual; o resto da suite segue no SliderAttachment.
    processor.apvts.addParameterListener (def.id, this);

    return knob;
}

void PianoforgeAudioProcessorEditor::parameterChanged (const juce::String& parameterID, float newValue)
{
    // Chegou do host (automacao) ou de um preset: mantem o disco espelhado. O unico
    // parametro escutado aqui e o "compressor".
    if (parameterID != forja::pianoforge::kParamDefs[forja::pianoforge::Param_Compressor].id)
        return;
    if (syncingCompressor || compressorSlider == nullptr)
        return;

    syncingCompressor = true;
    compressorSlider->setValue (1.0 - static_cast<double> (newValue), juce::sendNotificationSync);
    syncingCompressor = false;
}

void PianoforgeAudioProcessorEditor::setActiveTab (int tabIndex)
{
    activeTab = juce::jlimit (0, 1, tabIndex);

    pianoTab.setToggleState (activeTab == 0, juce::dontSendNotification);
    motorTab.setToggleState (activeTab == 1, juce::dontSendNotification);

    dynCard.setVisible (activeTab == 0);
    corpoCard.setVisible (activeTab == 0);
    eqCard.setVisible (activeTab == 0);
    molhoCard.setVisible (activeTab == 0);
    spaceCard.setVisible (activeTab == 0);
    bankCard.setVisible (activeTab == 0);
    outCard.setVisible (activeTab == 0);
    pianoArt.setVisible (activeTab == 0);
    motorCard.setVisible (activeTab == 1);
}

float PianoforgeAudioProcessorEditor::engineParam01 (int paramId) const
{
    if (paramId < 0 || paramId >= forja::pianoforge::Param_Count)
        return 0.0f;

    auto* parameter = processor.apvts.getParameter (forja::pianoforge::kParamDefs[paramId].id);
    return parameter != nullptr ? static_cast<float> (parameter->getValue()) : 0.0f;
}

void PianoforgeAudioProcessorEditor::rebuildBankChoice()
{
    bankAttachment.reset();

    auto& combo = bankChoice.getComboBox();
    combo.clear (juce::sendNotificationSync);

    int id = 1;
    combo.addItem ("none", id++);

    for (const auto& file : processor.getBankFiles())
        combo.addItem (file.getFileNameWithoutExtension(), id++);

    if (processor.hasBank())
    {
        const auto name = processor.getBankName();

        for (int index = 0; index < combo.getNumItems(); ++index)
            if (combo.getItemText (index) == name)
            {
                combo.setSelectedId (combo.getItemId (index));
                break;
            }
    }
    else
    {
        combo.setSelectedId (1);   // none
    }

    bankAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processor.apvts, "bank", combo);
}

void PianoforgeAudioProcessorEditor::rebuildPresetChoice()
{
    auto& combo = instrumentChoice.getComboBox();
    combo.clear (juce::dontSendNotification);

    const int count = processor.getNumPresets();

    for (int index = 0; index < count; ++index)
    {
        auto name = processor.getPresetName (index);
        if (name.isEmpty())
            name = "Preset " + juce::String (index + 1);
        combo.addItem (name, index + 1);
    }

    const int selected = processor.getSelectedPresetIndex();

    if (selected >= 0 && selected < count)
        combo.setSelectedId (selected + 1, juce::dontSendNotification);
    else if (count > 0)
        combo.setSelectedId (1, juce::dontSendNotification);

    instrumentChoice.setVisible (count > 0);
    lastPresetCount = count;
}

void PianoforgeAudioProcessorEditor::layoutContent (juce::Rectangle<int> content)
{
    auto tabs = content.removeFromTop (tabRowHeight);
    pianoTab.setBounds (tabs.removeFromLeft (92));
    tabs.removeFromLeft (6);
    motorTab.setBounds (tabs.removeFromLeft (92));
    content.removeFromTop (6);

    // Coluna esquerda: dois tercos, com CINCO cards de modelagem/molho empilhados
    // em tres fileiras de duas colunas (Dinamica | Corpo / Equalizacao | Molho /
    // Espaco | vazio). Cada card de meia coluna (~325 px) da celula de ~102 px,
    // folgada para os tres discos de 98 px (a grade de tres colunas apertava e
    // fazia sobrepor). Coluna direita: banco em cima, saida embaixo. O card MOTOR
    // ocupa tudo quando a aba dele esta ativa.
    const int gap = 6;
    const auto left = content.withWidth (content.getWidth() * 66 / 100);
    const auto right = content.withTrimmedLeft (left.getWidth() + gap);

    const int rowH = (left.getHeight() - 2 * gap) / 3;
    const int cardW = (left.getWidth() - gap) / 2;
    const int x0 = left.getX();
    const int y0 = left.getY();
    const int xR = x0 + cardW + gap;
    const int yM = y0 + rowH + gap;
    const int yB = y0 + 2 * (rowH + gap);

    dynCard.setBounds (x0, y0, cardW, rowH);
    eqCard.setBounds (x0, yM, cardW, rowH);
    spaceCard.setBounds (x0, yB, cardW, rowH);

    // Coluna direita: Corpo, Molho e a arte do piano dividem as MESMAS tres
    // alturas rowH da esquerda (nao ha mais achatamento). O card achatado de antes
    // nao comportava a pilha do disco (diametro + 27px de rotulo/dica abaixo do
    // cabeca, que comeca ja abaixo do cabecalho de 44px do card): o NOME do knob
    // saia cortado pela borda de baixo. Com rowH inteiro e disco compacto, o
    // rotulo cabe. O piano encolhe para rowH (antes recebia o sobra dos cards
    // achatados) -- era o espaco que estava sobrando.
    corpoCard.setBounds (xR, y0, cardW, rowH);
    molhoCard.setBounds (xR, yM, cardW, rowH);
    pianoArt.setBounds (xR, yB, cardW, rowH);

    const int halfRH = (right.getHeight() - gap) / 2;
    bankCard.setBounds (right.getX(), right.getY(), right.getWidth(), halfRH);
    outCard.setBounds (right.getX(), right.getY() + halfRH + gap,
                       right.getWidth(), right.getHeight() - halfRH - gap);

    motorCard.setBounds (content);

    // Discos por card em grade (cols colunas x rows fileiras), cada disco
    // centralizado na sua celula. No PIANO cada card e uma fileira (1..3 discos)
    // e a meia coluna (~325 px) da celula ~102 px, folgada para o disco de 98 px.
    // No MOTOR os dez discos vao em 3x4 (card largo, sobra celula).
    auto layOutKnobs = [this] (forja::ui::ForjaCard& card, int rows, int cols)
    {
        int count = 0;
        const auto* placement = groupKnobs (card, count);

        if (placement == nullptr || count == 0)
            return;

        if (cols <= 0)
            cols = count;
        if (rows <= 0)
            rows = (count + cols - 1) / cols;

        const auto area = card.getContentBounds();
        const int cellW = area.getWidth() / cols;
        const int cellH = area.getHeight() / rows;

        for (int i = 0; i < count; ++i)
        {
            auto* knob = knobs[static_cast<std::size_t> (placement[i].paramId)].get();
            if (knob == nullptr)
                continue;

            const int c = i % cols;
            const int r = i / cols;
            const int w = knob->getWidth();
            const int h = knob->getHeight();

            const int cellX = area.getX() + cellW * c;
            const int cellY = area.getY() + cellH * r;

            int x = cellX + (cellW - w) / 2;
            int y = cellY + (cellH - h) / 2;
            x = juce::jlimit (area.getX(), juce::jmax (area.getX(), area.getRight() - w), x);
            y = juce::jlimit (area.getY(), juce::jmax (area.getY(), area.getBottom() - h), y);

            knob->setBounds (x, y, w, h);
        }
    };

    layOutKnobs (dynCard, 1, 2);
    layOutKnobs (corpoCard, 1, 2);
    layOutKnobs (eqCard, 1, 3);
    layOutKnobs (molhoCard, 1, 3);
    layOutKnobs (spaceCard, 1, 3);
    layOutKnobs (motorCard, 3, 4);

    const auto bankArea = bankCard.getContentBounds();
    bankChoice.setBounds (bankArea.getX() + 2, bankArea.getY(), bankArea.getWidth() - 4, 30);
    instrumentChoice.setBounds (bankArea.getX() + 2, bankChoice.getBottom() + 4,
                                bankArea.getWidth() - 4, 30);
    refreshButton.setBounds (bankArea.getX() + 2, instrumentChoice.getBottom() + 5,
                             bankArea.getWidth() / 2 - 4, 24);
    statusLabel.setBounds (refreshButton.getRight() + 4, refreshButton.getY(),
                           bankArea.getRight() - refreshButton.getRight() - 4, refreshButton.getHeight());

    const auto outArea = outCard.getContentBounds();
    outputMeter.setBounds (outArea.getX() + 4, outArea.getY(),
                           juce::jmin (90, outArea.getWidth() / 3), outArea.getHeight() - 34);
    bypassButton.setBounds (outArea.getRight() - 120, outArea.getBottom() - 28, 118, 26);
}

void PianoforgeAudioProcessorEditor::refreshBankViews()
{
    statusLabel.setText (processor.getBankStatus(), juce::dontSendNotification);
}

void PianoforgeAudioProcessorEditor::updateTelemetry()
{
    const auto meter = processor.getEngine().readMeter();
    outputMeter.setLevels (meter.peakLeft, meter.peakRight);
    outputMeter.setClip (meter.clip);

    // Mantem a arte da tampa sincronizada com o estado (30 Hz). O componente so
    // repinta quando um dos tres valores sai do lugar.
    pianoArt.update (engineParam01 (Param_Lid), engineParam01 (Param_Body),
                     engineParam01 (Param_Ring));

    const auto revision = processor.getBankRevision();

    if (revision != lastBankRevision)
    {
        lastBankRevision = revision;
        refreshBankViews();
        rebuildPresetChoice();
        pianoArt.setName (processor.getBankName());   // partitura mostra o banco em uso
    }
    else if (processor.isLoadingBank())
    {
        statusLabel.setText (processor.getBankStatus(), juce::dontSendNotification);
    }
    else if (processor.hasBank())
    {
        if (processor.getNumPresets() != lastPresetCount)
            rebuildPresetChoice();

        auto& combo = instrumentChoice.getComboBox();
        const int selected = processor.getSelectedPresetIndex();

        if (selected >= 0 && selected < processor.getNumPresets()
            && combo.getSelectedId() != selected + 1)
            combo.setSelectedId (selected + 1, juce::dontSendNotification);
    }

    refreshBadge (meter.clip, processor.isBypassed(), processor.hasBank());

    setMetaValues ({ { u ("BANCO"), processor.getBankName() },
                     { u ("VOZES"), juce::String (meter.activeVoices) } });
}

void PianoforgeAudioProcessorEditor::refreshBadge (bool clipped, bool bypassed, bool hasBank)
{
    auto kind = forja::ui::BadgeKind::ready;
    juce::String text ("PRONTO");
    juce::String detail;

    if (clipped)
    {
        kind = forja::ui::BadgeKind::clip;
        text = "CLIP";
        detail = u ("Sa\u00edda acima de 0 dBFS \u00b7 baixe o volume ou o trim.");
    }
    else if (bypassed)
    {
        kind = forja::ui::BadgeKind::bypass;
        text = "BYPASS";
        detail = u ("Bypass ativo \u00b7 sa\u00edda silenciada com rampa de 10 ms.");
    }
    else if (! hasBank)
    {
        kind = forja::ui::BadgeKind::silent;
        text = "SEM BANCO";
        detail = u ("Nenhum piano carregado \u00b7 deposite um .sf2 ou .pforge em "
                    "Documentos/Forja/Pianoforge e toque ATUALIZAR.");
    }
    else
    {
        detail = u ("Tocando \u00b7 cadeia f\u00edsica ativa \u00b7 aba MOTOR para "
                    "ajuste fino.");
    }

    setBadge (kind, text, detail);
}
