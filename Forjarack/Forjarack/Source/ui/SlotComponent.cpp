#include "SlotComponent.h"

#include <algorithm>
#include <cmath>

namespace forjarackui
{

namespace
{
constexpr int headerH = 20;
// Dois seletores empilhados (categoria em cima, modulo embaixo), mais baixos
// para nao espremer a coluna de knobs.
constexpr int selectorY = 24;
constexpr int categoryH = 20;
constexpr int selectorGap = 2;
constexpr int selectorH = 22;
// As duas chaves dividem a largura da gaveta (~100 px): cada uma fica com
// ~42 px de area de clique e sobra altura para a coluna de knobs.
constexpr int toggleY = selectorY + categoryH + selectorGap + selectorH + 4;
constexpr int toggleH = 28;
constexpr int toggleGap = 4;
constexpr int knobsTop = toggleY + toggleH + toggleGap + 6;
constexpr int meterH = 12;
// Gaveta hospedada: abaixo das chaves vao o seletor de arquivo e a fileira de
// botoes (BUSCAR revarre a pasta, EDITAR abre a UI nativa/gerica). Os knobs
// comecam mais abaixo que nos modulos fixos.
constexpr int hostedFileH = 20;
constexpr int hostedBtnH = 20;
constexpr int hostedGap = 3;
constexpr int hostedKnobsTop = knobsTop + hostedFileH + hostedGap + hostedBtnH + 4;
// Barra de reducao de ganho (compressores/limitadores): fica acima do led de
// saida e so entra no layout quando o modulo atual mede GR real.
constexpr int grH = 14;
constexpr int grGap = 4;

// Rotulo do filtro de categoria. So da UI (filtro visual); o parametro 'type'
// continua gravando o indice do catalogo, entao a ordem das categorias aqui
// nao afeta presets. 'vazio' e a casa do slot cego; os demais espelham as
// familias do catalogo.
juce::String categoryLabel (forjarack::ModuleCategory category)
{
    switch (category)
    {
    case forjarack::ModuleCategory::color:     return forja::ui::utf8 ("Cor / calor");
    case forjarack::ModuleCategory::drive:     return forja::ui::utf8 ("Drive / fita");
    case forjarack::ModuleCategory::dynamics:  return forja::ui::utf8 ("Dinamica");
    case forjarack::ModuleCategory::eq:        return forja::ui::utf8 ("Equalizacao");
    case forjarack::ModuleCategory::space:     return forja::ui::utf8 ("Espaco / delay");
    case forjarack::ModuleCategory::lofi:      return forja::ui::utf8 ("Lo-fi");
    case forjarack::ModuleCategory::instrument:
        return forja::ui::utf8 ("Instrumento");
    case forjarack::ModuleCategory::hosted:
        return forja::ui::utf8 ("Hospedado");
    case forjarack::ModuleCategory::numCategories:
        break;
    }

    return forja::ui::utf8 ("Vazio");
}

// juce::ComboBox nao expoe lookup por itemId nesta versao do JUCE.
bool hasItemId (const juce::ComboBox& box, int itemId)
{
    for (int i = 0; i < box.getNumItems(); ++i)
        if (box.getItemId (i) == itemId)
            return true;

    return false;
}

// ---- marca d'agua de identidade (desenho esmaecido atras dos knobs) ----
// Todo face usa a mesma linguagem: linha fina na cor de acento, alpha baixo,
// mesma caixa e mesmo traco. So o motivo muda por face, mantendo a coerencia
// do console. Nao intercepta clique nem mexe no layout dos controles.

// Um rolo de fita: aro, cubo, tres raios e fuso central.
void faceReel (juce::Graphics& g, juce::Point<float> c, float r,
               juce::Colour ink, juce::Colour inkSoft)
{
    g.setColour (ink);
    g.drawEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f, 1.2f);
    g.setColour (inkSoft);
    g.fillEllipse (c.x - r * 0.30f, c.y - r * 0.30f, r * 0.6f, r * 0.6f);

    g.setColour (ink);
    for (int s = 0; s < 3; ++s)
    {
        const float a = juce::MathConstants<float>::twoPi * static_cast<float> (s) / 3.0f;
        g.drawLine (c.x + std::cos (a) * r * 0.3f, c.y + std::sin (a) * r * 0.3f,
                    c.x + std::cos (a) * r * 0.88f, c.y + std::sin (a) * r * 0.88f, 1.0f);
    }

    g.setColour (inkSoft);
    g.fillEllipse (c.x - 1.8f, c.y - 1.8f, 3.6f, 3.6f);
}

// Arco aberto (juce::Graphics nao tem drawArc): addArc + strokePath.
void strokeArc (juce::Graphics& g, juce::Rectangle<float> r, float a0, float a1, float thickness)
{
    juce::Path p;
    p.addArc (r.getX(), r.getY(), r.getWidth(), r.getHeight(), a0, a1);
    g.strokePath (p, juce::PathStrokeType (thickness));
}

inline constexpr float kFaceAlpha = 0.16f;

void paintFaceWatermark (juce::Graphics& g, juce::Rectangle<float> area,
                         juce::Colour accent, forjarack::ModuleFace face, bool dimmed)
{
    using forjarack::ModuleFace;

    if (face == ModuleFace::generic)
        return;

    if (area.getWidth() < 30.0f || area.getHeight() < 30.0f)
        return;

    const float alpha = dimmed ? kFaceAlpha * 0.40f : kFaceAlpha;
    const juce::Colour ink = accent.withAlpha (alpha);
    const juce::Colour inkSoft = accent.withAlpha (alpha * 0.55f);
    const float stroke = 1.2f;

    const auto box = area.reduced (7.0f, 5.0f);
    const float cx = box.getCentreX();
    const float cy = box.getCentreY();
    const float w = box.getWidth();
    const float h = box.getHeight();
    constexpr float pi = juce::MathConstants<float>::pi;

    // Fundo com material: fita ganha chapa metalica acinzentada, lo-fi um
    // painel embutido tipo visor. Os demais ficam sobre o ferro padrao.
    if (face == forjarack::ModuleFace::tape)
    {
        const float plate = dimmed ? 0.28f : 0.55f;
        juce::ColourGradient steel (forja::ui::colour::iron4.withAlpha (plate),
                                    0.0f, box.getY(),
                                    forja::ui::colour::iron2.withAlpha (plate),
                                    0.0f, box.getBottom(), false);
        g.setGradientFill (steel);
        g.fillRoundedRectangle (box, 5.0f);
        g.setColour (forja::ui::colour::line.withAlpha (plate));
        g.drawRoundedRectangle (box.reduced (0.5f), 5.0f, 1.0f);
    }
    else if (face == forjarack::ModuleFace::digital)
    {
        g.setColour (forja::ui::colour::iron0.withAlpha (dimmed ? 0.30f : 0.60f));
        g.fillRoundedRectangle (box, 3.0f);
    }

    switch (face)
    {
    case ModuleFace::generic:
        break;

    case ModuleFace::tape:
    {
        const float r = juce::jmin (w * 0.22f, h * 0.16f);
        const float ry = box.getY() + r + 3.0f;
        faceReel (g, { box.getX() + w * 0.30f, ry }, r, ink, inkSoft);
        faceReel (g, { box.getX() + w * 0.70f, ry }, r, ink, inkSoft);

        const float guideY = ry + r + h * 0.10f;
        const float headY = guideY + 8.0f;
        juce::Path tape;
        tape.startNewSubPath (box.getX() + w * 0.30f, ry + r);
        tape.lineTo (box.getX() + w * 0.30f, guideY);
        tape.lineTo (cx - 12.0f, guideY);
        tape.lineTo (cx - 6.0f, headY);
        tape.lineTo (cx + 6.0f, headY);
        tape.lineTo (cx + 12.0f, guideY);
        tape.lineTo (box.getX() + w * 0.70f, guideY);
        tape.lineTo (box.getX() + w * 0.70f, ry + r);
        g.setColour (ink);
        g.strokePath (tape, juce::PathStrokeType (stroke));
        g.setColour (inkSoft);
        g.drawRoundedRectangle (cx - 13.0f, headY - 4.0f, 26.0f, 10.0f, 2.0f, stroke);
        break;
    }

    case ModuleFace::dynamics:
    {
        const float vr = w * 0.44f;
        const juce::Point<float> pivot (cx, box.getY() + vr * 0.72f);
        const juce::Rectangle<float> dial (pivot.x - vr, pivot.y - vr, vr * 2.0f, vr * 2.0f);
        g.setColour (ink);
        strokeArc (g, dial, juce::degreesToRadians (200.0f),
                   juce::degreesToRadians (340.0f), stroke);

        for (int t = 0; t <= 6; ++t)
        {
            const float a = juce::degreesToRadians (200.0f + 140.0f * static_cast<float> (t) / 6.0f);
            const auto dir = juce::Point<float> (std::cos (a), std::sin (a));
            g.drawLine ((pivot + dir * (vr * 0.82f)).x, (pivot + dir * (vr * 0.82f)).y,
                        (pivot + dir * (vr * 0.95f)).x, (pivot + dir * (vr * 0.95f)).y, stroke);
        }

        const float na = juce::degreesToRadians (258.0f);
        g.setColour (inkSoft);
        g.drawLine (pivot.x, pivot.y, pivot.x + std::cos (na) * vr * 0.78f,
                    pivot.y + std::sin (na) * vr * 0.78f, 1.4f);
        g.setColour (ink);
        g.fillEllipse (pivot.x - 2.2f, pivot.y - 2.2f, 4.4f, 4.4f);
        break;
    }

    case ModuleFace::eq:
    {
        g.setColour (inkSoft);
        g.drawLine (box.getX(), cy, box.getRight(), cy, stroke);
        const float amp = h * 0.20f;
        juce::Path curve;
        curve.startNewSubPath (box.getX(), cy);
        curve.cubicTo (box.getX() + w * 0.2f, cy - amp, box.getX() + w * 0.3f, cy - amp, cx, cy);
        curve.cubicTo (box.getX() + w * 0.7f, cy + amp, box.getX() + w * 0.8f, cy + amp,
                       box.getRight(), cy);
        g.setColour (ink);
        g.strokePath (curve, juce::PathStrokeType (1.5f));
        break;
    }

    case ModuleFace::space:
    {
        const juce::Point<float> src (box.getX() + w * 0.26f, cy);
        g.setColour (ink);
        g.fillEllipse (src.x - 2.0f, src.y - 2.0f, 4.0f, 4.0f);
        const float maxR = juce::jmin (w * 0.72f, h * 0.9f);
        for (int i = 1; i <= 4; ++i)
        {
            const float r = maxR * static_cast<float> (i) / 4.0f;
            const juce::Rectangle<float> ring (src.x - r, src.y - r, r * 2.0f, r * 2.0f);
            g.setColour (accent.withAlpha (alpha * (1.0f - 0.17f * static_cast<float> (i))));
            strokeArc (g, ring, juce::degreesToRadians (-42.0f),
                       juce::degreesToRadians (42.0f), stroke);
        }
        break;
    }

    case ModuleFace::digital:
    {
        const float cell = 11.0f;
        const float sq = 4.0f;
        int k = 0;
        for (float y = box.getY() + 3.0f; y < box.getBottom() - sq; y += cell)
        {
            for (float x = box.getX() + 3.0f; x < box.getRight() - sq; x += cell, ++k)
            {
                g.setColour ((k % 3) != 0 ? ink : inkSoft);
                g.fillRect (juce::Rectangle<float> (x, y, sq, sq));
            }
        }
        break;
    }

    case ModuleFace::warmth:
    {
        const float base = box.getBottom() - h * 0.30f;
        for (int i = 0; i < 3; ++i)
        {
            const float r = w * 0.16f + static_cast<float> (i) * w * 0.13f;
            const juce::Rectangle<float> arc (cx - r, base - r, r * 2.0f, r * 2.0f);
            g.setColour (accent.withAlpha (alpha * (1.0f - 0.2f * static_cast<float> (i))));
            strokeArc (g, arc, pi, pi * 2.0f, stroke);
        }
        break;
    }

    case ModuleFace::drive:
    {
        const float amp = h * 0.18f;
        const float top = cy - amp;
        const float bot = cy + amp;
        juce::Path wave;
        bool first = true;
        for (float x = 0.0f; x <= w; x += 3.0f)
        {
            const float phase = (x / w) * juce::MathConstants<float>::twoPi * 2.5f;
            float y = cy - std::sin (phase) * amp * 1.9f;
            y = juce::jlimit (top, bot, y);   // topo/fundo achatados = saturacao
            const float px = box.getX() + x;
            if (first) { wave.startNewSubPath (px, y); first = false; }
            else       { wave.lineTo (px, y); }
        }
        g.setColour (ink);
        g.strokePath (wave, juce::PathStrokeType (1.4f));
        break;
    }

    case ModuleFace::valve:
    {
        const float bw = w * 0.34f;
        const float bh = h * 0.42f;
        const juce::Rectangle<float> bulb (cx - bw * 0.5f, cy - bh * 0.5f, bw, bh);
        g.setColour (ink);
        g.drawRoundedRectangle (bulb, bw * 0.42f, stroke);
        g.drawLine (cx - bw * 0.18f, bulb.getY() + bh * 0.28f,
                    cx - bw * 0.18f, bulb.getBottom() - bh * 0.2f, stroke);
        g.drawLine (cx + bw * 0.18f, bulb.getY() + bh * 0.28f,
                    cx + bw * 0.18f, bulb.getBottom() - bh * 0.2f, stroke);
        for (int p = -1; p <= 1; ++p)
            g.drawLine (cx + static_cast<float> (p) * bw * 0.28f, bulb.getBottom(),
                        cx + static_cast<float> (p) * bw * 0.28f,
                        bulb.getBottom() + h * 0.08f, 1.4f);
        break;
    }

    case ModuleFace::drum:
    {
        const float r = juce::jmin (w, h) * 0.28f;
        const juce::Point<float> c (cx, cy);
        g.setColour (ink);
        g.drawEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f, stroke);
        g.setColour (inkSoft);
        g.drawEllipse (c.x - r * 0.7f, c.y - r * 0.7f, r * 1.4f, r * 1.4f, stroke);
        for (int l = 0; l < 6; ++l)
        {
            const float a = juce::MathConstants<float>::twoPi * static_cast<float> (l) / 6.0f;
            const auto lug = c + juce::Point<float> (std::cos (a), std::sin (a)) * r;
            g.setColour (ink);
            g.fillEllipse (lug.x - 1.6f, lug.y - 1.6f, 3.2f, 3.2f);
        }
        break;
    }
    }
}
} // namespace

juce::String SlotComponent::slotId (int slot, const juce::String& suffix)
{
    return "slot" + juce::String (slot + 1) + "_" + suffix;
}

juce::String SlotComponent::knobId (int slot, int knob)
{
    return slotId (slot, "knob" + juce::String (knob));
}

juce::AudioParameterChoice* SlotComponent::typeParameter() const
{
    return dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (slotId (slot, "type")));
}

void SlotComponent::selectCatalogIndex (int index)
{
    auto* type = typeParameter();
    if (type == nullptr)
        return;

    const int chosen = juce::jmax (0, index);
    const int last = juce::jmax (1, forjarack::ModuleCatalog::count() - 1);

    type->setValueNotifyingHost (static_cast<float> (chosen) / static_cast<float> (last));
}

void SlotComponent::populateModuleChoices()
{
    const auto category = static_cast<forjarack::ModuleCategory> (
        juce::jmax (1, categorySelector.getSelectedId()) - 1);
    const int current = currentTypeIndex();

    selector.clear (juce::dontSendNotification);

    // VAZIO (indice 0) e o unico modo de esvaziar a gaveta: vive sempre no
    // filtro, nao so na categoria 'cor' onde o registro o hospeda.
    const auto& voidSpec = forjarack::ModuleCatalog::specByIndex (0);

    if (voidSpec.category != category)
        selector.addItem (voidSpec.name, 1);

    for (int i = 0; i < forjarack::ModuleCatalog::count(); ++i)
    {
        const auto& spec = forjarack::ModuleCatalog::specByIndex (i);

        // Modulo atual nunca some do filtro (presets antigos seguem legiveis).
        if (spec.category == category || i == current)
            selector.addItem (spec.name, i + 1);
    }

    if (hasItemId (selector, current + 1))
        selector.setSelectedId (current + 1, juce::dontSendNotification);
    else if (selector.getNumItems() > 0)
        selector.setSelectedId (selector.getItemId (0), juce::dontSendNotification);
}

void SlotComponent::syncCategoryChoice()
{
    const auto& spec = forjarack::ModuleCatalog::specByIndex (currentTypeIndex());

    if (hasItemId (categorySelector, static_cast<int> (spec.category) + 1))
        categorySelector.setSelectedId (static_cast<int> (spec.category) + 1,
                                        juce::dontSendNotification);
}

SlotComponent::SlotComponent (juce::AudioProcessorValueTreeState& state, int slotIndex,
                              IHostedSlot* hostedSlot)
    : apvts (state), slot (slotIndex),
      slotLabel (juce::String (slotIndex + 1).paddedLeft ('0', 2)),
      hosted (hostedSlot)
{
    // Filtro visual da gaveta: lista as categorias do catalogo e refiltra o
    // seletor de modulos. Nunca toca no parametro 'type' diretamente.
    categorySelector.setWantsKeyboardFocus (false);
    categorySelector.setTooltip (forja::ui::utf8 ("Filtro por familia de modulo"));

    for (int c = 0; c < static_cast<int> (forjarack::ModuleCategory::numCategories); ++c)
    {
        const auto category = static_cast<forjarack::ModuleCategory> (c);
        bool usado = false;

        for (int i = 0; i < forjarack::ModuleCatalog::count(); ++i)
            if (forjarack::ModuleCatalog::specByIndex (i).category == category)
                usado = true;

        if (usado)
            categorySelector.addItem (categoryLabel (category), c + 1);
    }

    categorySelector.onChange = [this]
    {
        const auto chosen = static_cast<forjarack::ModuleCategory> (
            juce::jmax (1, categorySelector.getSelectedId ()) - 1);

        populateModuleChoices();

        // O modulo ja pertence a categoria escolhida: o filtro so redesenha a
        // lista de modulos, nao se toca na gaveta.
        if (forjarack::ModuleCatalog::specByIndex (currentTypeIndex ()).category == chosen)
            return;

        // Vai para o primeiro modulo REAL da categoria escolhida. O VAZIO
        // (indice 0, familia cor) e apenas a casa do slot cego no filtro: se ele
        // for a escolha automatica, o syncCategoryChoice do refresh realinha o
        // filtro para "cor" e a troca de categoria parece nao acontecer (a caixa
        // volta sozinha). Por isso se salta o indice 0 e se procura pela familia.
        for (int i = 1; i < forjarack::ModuleCatalog::count (); ++i)
        {
            if (forjarack::ModuleCatalog::specByIndex (i).category == chosen)
            {
                selectCatalogIndex (i);
                refresh ();
                return;
            }
        }
    };
    addAndMakeVisible (categorySelector);

    selector.setWantsKeyboardFocus (false);
    selector.setTooltip (forja::ui::utf8 ("Modulo da gaveta"));
    selector.onChange = [this]
    {
        if (const int id = selector.getSelectedId(); id > 0)
        {
            selectCatalogIndex (id - 1);
            refresh();
        }
    };
    addAndMakeVisible (selector);

    auto configure = [this] (juce::TextButton& button, const char* tip)
    {
        button.setClickingTogglesState (true);
        button.setWantsKeyboardFocus (false);
        button.setTooltip (forja::ui::utf8 (tip));
        addAndMakeVisible (button);
    };

    configure (bypassButton, "Bypass do modulo (pass-through)");
    configure (soloButton, "Solo: silencia as demais gavetas");

    toggleAttachments[0] = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        state, slotId (slot, "bypass"), bypassButton);
    toggleAttachments[1] = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        state, slotId (slot, "solo"), soloButton);

    for (int knob = 0; knob < forjarack::kMaxModuleParams; ++knob)
    {
        knobs[(size_t) knob] = std::make_unique<RackKnob> ("");
        knobAttachments[(size_t) knob] =
            std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                state, knobId (slot, knob), knobs[(size_t) knob]->getSlider());

        addChildComponent (*knobs[(size_t) knob]);
    }

    // Controles da banda de arquivos (HOSPEDADO e gavetas de instrumento com
    // biblioteca): o seletor escreve via IHostedSlot::chooseFile; BUSCAR revarre
    // a pasta de plugins; ARQUIVO abre o FileChooser do kit/bank; EDITAR abre a
    // UI nativa do plugin hospedado em janela nova.
    fileSelector.setWantsKeyboardFocus (false);
    fileSelector.setTooltip (forja::ui::utf8 ("Arquivos da biblioteca da gaveta"));
    fileSelector.onChange = [this]
    {
        if (hosted == nullptr)
            return;

        if (hosted->isHosted (slot))
            hosted->chooseFile (slot, fileSelector.getSelectedId() <= 1
                                           ? juce::String() : fileSelector.getText());
        else
            hosted->chooseFile (slot, fileSelector.getSelectedId() > 1
                                           ? fileSelector.getText() : juce::String());

        refresh();
    };
    addChildComponent (fileSelector);

    procurarButton.setButtonText ("BUSCAR");
    procurarButton.setWantsKeyboardFocus (false);
    procurarButton.setClickingTogglesState (false);
    procurarButton.setTooltip (forja::ui::utf8 ("Reescanear a pasta Documents/Forja/Rack"));
    procurarButton.onClick = [this]
    {
        if (hosted != nullptr)
            hosted->rescan();

        refresh();
    };
    addChildComponent (procurarButton);

    editarButton.setWantsKeyboardFocus (false);
    editarButton.setClickingTogglesState (false);
    editarButton.onClick = [this]
    {
        if (hosted == nullptr)
            return;

        if (editarButton.getButtonText() == "EDITAR")
            hosted->openEditor (slot);
        else
            hosted->browseFile (slot);   // ARQUIVO: FileChooser do kit/bank
    };
    addChildComponent (editarButton);
}

SlotComponent::~SlotComponent() = default;

int SlotComponent::currentTypeIndex() const
{
    if (auto* choice = typeParameter())
        return choice->getIndex();

    return 0;
}

void SlotComponent::refresh()
{
    const auto& spec = forjarack::ModuleCatalog::specByIndex (currentTypeIndex());

    accentColour = juce::Colour (spec.accent);

    const bool isHost = hosted != nullptr && hosted->isHosted (slot);
    const bool isBand = hosted != nullptr && (isHost || hosted->isDrawer (slot));

    // Barra de GR so aparece em compressor/limitador que mede reducao real.
    // Gaveta hospedada nunca mostra GR: nao medimos a reducao de um plugin alheio.
    const bool grNow = ! isBand && spec.showsGainReduction;

    if (grNow != showsGr)
    {
        showsGr = grNow;

        if (! showsGr)
        {
            grDb = 0.0f;
            grHold = 0.0f;
        }
    }

    // Trocas externas (preset, host) realinham o filtro a categoria do modulo;
    // o usuario mexendo no filtro nao passa por aqui (onChange nao da refresh
    // na categoria quando o modulo ja pertence a ela).
    syncCategoryChoice();
    populateModuleChoices();

    // Banda de arquivos: seletor + botoes. HOSPEDADO: BUSCAR (revarre a pasta)
    // e EDITAR (UI nativa). Gaveta de instrumento: so ARQUIVO (FileChooser),
    // ocupando a fileira inteira; o nome carregado e o material vivo do modulo.
    fileSelector.setVisible (isBand);
    procurarButton.setVisible (isHost);
    editarButton.setVisible (isBand);

    int knobCount = spec.numParams;

    if (isBand)
    {
        editarButton.setButtonText (isHost ? "EDITAR" : "ARQUIVO");
        editarButton.setTooltip (isHost
            ? forja::ui::utf8 ("Abrir a UI do plugin em janela separada")
            : forja::ui::utf8 ("Abrir um kit/soundfont fora da biblioteca"));

        fileSelector.clear (juce::dontSendNotification);

        const auto current = hosted->fileName (slot);
        int selectedId = 0;

        if (isHost)
        {
            fileSelector.addItem (forja::ui::utf8 ("(nenhum)"), 1);

            const auto files = hosted->fileList();

            for (int i = 0; i < files.size(); ++i)
                fileSelector.addItem (files[i], i + 2);

            for (int i = 0; i < files.size(); ++i)
                if (files[i] == current)
                    selectedId = i + 2;

            fileSelector.setSelectedId (selectedId != 0 ? selectedId : 1,
                                         juce::dontSendNotification);
            editarButton.setEnabled (hosted->canOpenEditor (slot));
        }
        else
        {
            const auto paths = hosted->drawerFileNames (slot);

            for (int i = 0; i < paths.size(); ++i)
                fileSelector.addItem (juce::File (paths[i]).getFileName(), i + 2);

            // Nome do material vivo (ou da escolha externa em transito): se nao
            // esta na lista, entra como item extra para o seletor poder mostra-lo.
            if (! current.isEmpty())
            {
                for (int i = 0; i < paths.size(); ++i)
                    if (juce::File (paths[i]).getFileName() == current)
                        selectedId = i + 2;

                if (selectedId == 0)
                {
                    fileSelector.addItem (current, paths.size() + 2);
                    selectedId = paths.size() + 2;
                }
            }

            fileSelector.setSelectedId (selectedId, juce::dontSendNotification);
        }

        if (isHost)
            knobCount = hosted->paramCount (slot);
    }

    const int knobsShown = std::clamp (knobCount, 0, forjarack::kMaxModuleParams);

    for (int knob = 0; knob < forjarack::kMaxModuleParams; ++knob)
    {
        auto& target = knobs[(size_t) knob];
        const bool visible = knob < knobsShown;

        target->setVisible (visible);

        if (visible)
        {
            const juce::String label = isHost
                ? hosted->paramLabel (slot, knob)
                : juce::String (spec.knobs[(size_t) knob].label);

            target->setLabelText (label);
            target->setAccent (accentColour);
        }
    }

    resized();
    repaint();
}

void SlotComponent::setOutputPeak (float peak)
{
    const float next = std::max (peak, outputPeak * 0.82f);

    if (std::abs (next - outputPeak) > 0.001f)
    {
        outputPeak = next;
        repaint();
    }
}

int SlotComponent::bottomReserved() const
{
    return meterH + 2 + (showsGr ? grH + grGap : 0);
}

void SlotComponent::setGainReductionDb (float db)
{
    if (! showsGr)
        return;

    // db chega negativo (0 = sem compressao). Trabalhamos com a magnitude.
    const float magnitude = juce::jlimit (0.0f, 24.0f, -db);
    const float now = static_cast<float> (juce::Time::getMillisecondCounterHiRes());

    grDb = magnitude;

    // Hold: marca o pico e segura 900 ms antes de deixar cair, para o olho
    // acompanhar o bombeio mesmo com blocos curtos.
    if (magnitude >= grHold)
    {
        grHold = magnitude;
        grHoldUntilMs = now + 900.0;
    }
    else if (now > grHoldUntilMs)
    {
        grHold = std::max (magnitude, grHold * 0.85f);
    }

    repaint();
}

void SlotComponent::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    g.setColour (forja::ui::colour::iron2);
    g.fillRoundedRectangle (bounds.toFloat(), 4.0f);
    g.setColour (forja::ui::colour::line);
    g.drawRoundedRectangle (bounds.toFloat().reduced (0.5f), 4.0f, 1.0f);

    // Cabecalho: barra de acento + numero do slot. Fosca quando em bypass.
    const bool bypassed = bypassButton.getToggleState();

    // Marca d'agua de identidade no vao atras dos knobs (os filhos desenham
    // depois, entao o motivo fica sempre por baixo dos controles).
    const auto& spec = forjarack::ModuleCatalog::specByIndex (currentTypeIndex());
    auto faceArea = getLocalBounds().toFloat()
                        .withY (static_cast<float> (knobsTop))
                        .withBottomY (static_cast<float> (getHeight() - bottomReserved() - 4));
    paintFaceWatermark (g, faceArea, accentColour, spec.face, bypassed);

    auto header = bounds.withHeight (headerH).toFloat().reduced (1.0f);

    g.setColour (accentColour.withAlpha (bypassed ? 0.30f : 0.92f));
    g.fillRoundedRectangle (header, 3.0f);

    g.setFont (forja::ui::tracked (forja::ui::uiFont (9.0f, juce::Font::bold), 0.08f));
    g.setColour (bypassed ? forja::ui::colour::muted : forja::ui::colour::iron0);
    g.drawText (slotLabel, 0, 0, bounds.getWidth(), headerH, juce::Justification::centred, false);

    // Gavetas de instrumento (sf2/sf3, mini drum, expansoes) ganham chapa.
    if (spec.role == forjarack::ModuleRole::instrument)
    {
        g.setFont (forja::ui::tracked (forja::ui::uiFont (8.0f, juce::Font::bold), 0.10f));
        g.setColour (forja::ui::colour::iron0.withAlpha (0.90f));
        g.drawText ("INSTR", bounds.getWidth() - 50, 0, 44, headerH,
                    juce::Justification::centredRight, false);
    }

    // Led de saida no rodape.
    auto meter = bounds.withY (getHeight() - meterH - 2).withHeight (meterH)
                     .toFloat()
                     .reduced (6.0f, 2.0f);

    g.setColour (forja::ui::colour::meterTrack);
    g.fillRoundedRectangle (meter, 2.0f);

    const float level = std::sqrt (juce::jlimit (0.0f, 1.0f, outputPeak));
    g.setColour (level > 0.98f ? forja::ui::colour::meterClip : accentColour);
    g.fillRoundedRectangle (meter.withWidth (meter.getWidth() * level), 2.0f);

    // Barra de reducao de ganho: grafico horizontal 0..-24 dB acima do led,
    // preenchido da esquerda para a direita conforme o compressor trabalha.
    if (showsGr)
    {
        const int grY = getHeight() - meterH - 2 - grGap - grH;
        auto gr = bounds.toFloat().withY (static_cast<float> (grY))
                      .withHeight (static_cast<float> (grH)).reduced (6.0f, 1.0f);

        constexpr float labelW = 16.0f;
        const auto track = gr.withX (gr.getX() + labelW).reduced (2.0f, 0.0f);

        g.setFont (forja::ui::tracked (forja::ui::uiFont (7.0f, juce::Font::bold), 0.06f));
        g.setColour (bypassed ? forja::ui::colour::dim : forja::ui::colour::muted);
        g.drawText ("GR", juce::Rectangle<float> (gr.getX(), track.getY(), labelW, track.getHeight()),
                    juce::Justification::centredLeft, false);

        g.setColour (forja::ui::colour::meterTrack);
        g.fillRoundedRectangle (track, 2.0f);

        // trilho de referencia: marca de -12 dB no meio da barra.
        g.setColour (forja::ui::colour::graphGrid);
        g.drawLine (track.getX() + track.getWidth() * 0.5f, track.getY() + 1.0f,
                    track.getX() + track.getWidth() * 0.5f, track.getBottom() - 1.0f, 1.0f);

        const float norm = juce::jlimit (0.0f, 1.0f, grDb / 24.0f);

        if (norm > 0.0f && ! bypassed)
        {
            juce::ColourGradient grad (forja::ui::colour::emberSoft, track.getX(), 0.0f,
                                       forja::ui::colour::error, track.getRight(), 0.0f, false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (track.withWidth (track.getWidth() * norm), 2.0f);
        }

        const float holdNorm = juce::jlimit (0.0f, 1.0f, grHold / 24.0f);

        if (holdNorm > 0.001f && ! bypassed)
        {
            const float hx = track.getX() + track.getWidth() * holdNorm;
            g.setColour (forja::ui::colour::meterPeak.withAlpha (0.85f));
            g.fillRect (hx - 1.0f, track.getY(), 2.0f, track.getHeight());
        }

        g.setColour (forja::ui::colour::line);
        g.drawRoundedRectangle (track.reduced (0.5f), 2.0f, 1.0f);
    }
}

void SlotComponent::resized()
{
    auto selectorArea = getLocalBounds().withY (selectorY).reduced (6, 0);
    categorySelector.setBounds (selectorArea.withHeight (categoryH));
    selector.setBounds (selectorArea.withY (selectorY + categoryH + selectorGap)
                              .withHeight (selectorH));

    auto toggles = getLocalBounds().withY (toggleY).withHeight (toggleH).reduced (6, 0);
    const int each = (toggles.getWidth() - toggleGap) / 2;

    bypassButton.setBounds (toggles.withX (toggles.getX()).withWidth (each));
    soloButton.setBounds (toggles.withX (toggles.getX() + each + toggleGap)
                                .withWidth (toggles.getWidth() - each - toggleGap));

    const bool isHost = hosted != nullptr && hosted->isHosted (slot);
    const bool isBand = hosted != nullptr && (isHost || hosted->isDrawer (slot));

    // Banda de arquivos: seletor + fileira de botoes ocupam a faixa que nos
    // modulos fixos seria dos knobs; os knobs comecam mais abaixo.
    if (isBand)
    {
        auto hostArea = getLocalBounds().withY (knobsTop).reduced (6, 0);
        fileSelector.setBounds (hostArea.withHeight (hostedFileH));

        auto btnRow = hostArea.withY (knobsTop + hostedFileH + hostedGap).withHeight (hostedBtnH);

        if (isHost)
        {
            const int bEach = (btnRow.getWidth() - toggleGap) / 2;

            procurarButton.setBounds (btnRow.withX (btnRow.getX()).withWidth (bEach));
            editarButton.setBounds (btnRow.withX (btnRow.getX() + bEach + toggleGap)
                                         .withWidth (btnRow.getWidth() - bEach - toggleGap));
        }
        else
        {
            // ARQUIVO sozinho na fileira (nao ha BUSCAR nem EDITAR aqui).
            procurarButton.setBounds (btnRow.withWidth (0));
            editarButton.setBounds (btnRow.withX (btnRow.getX())
                                         .withWidth (btnRow.getWidth()));
        }
    }

    const int knobTop = isBand ? hostedKnobsTop : knobsTop;

    // A coluna de knobs usa o tanto de knobs que o modulo tem (no hospedado,
    // quantos params o plugin vivo expoe): um de um knob nao espreme oito
    // fileiras, e um de sete nao deixa rotulo sobre knob.
    const int knobCount = isHost ? hosted->paramCount (slot)
                                 : static_cast<int> (forjarack::ModuleCatalog::specByIndex (currentTypeIndex()).numParams);
    const int rows = juce::jlimit (1, forjarack::kMaxModuleParams, knobCount);
    const int areaHeight = getHeight() - knobTop - bottomReserved() - 4;

    // Modulo denso (mais de seis knobs) nao cabe em coluna unica: o rotulo
    // passaria a invadir o knob da fileira de baixo. Vira duas colunas.
    const bool twoCols = rows > 6;
    const int gridRows = twoCols ? (rows + 1) / 2 : rows;

    const int rowH = std::max (1, std::min (areaHeight / gridRows, 46));
    const int startY = knobTop + std::max (0, (areaHeight - gridRows * rowH) / 2);
    const int labelH = std::min (11, rowH / 3);
    const int diameter = juce::jlimit (16, 34, rowH - labelH - 2);
    const int colW = getWidth() / 2;

    for (int knob = 0; knob < rows; ++knob)
    {
        auto& target = knobs[(size_t) knob];
        target->setDiameter (static_cast<float> (diameter));
        target->setLabelHeight (labelH);

        const int col = twoCols ? knob / gridRows : 0;
        const int row = twoCols ? knob % gridRows : knob;

        target->setBounds (col * colW, startY + row * rowH, colW, rowH);
    }
}

} // namespace forjarackui
