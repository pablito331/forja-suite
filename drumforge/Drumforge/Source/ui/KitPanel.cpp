#include "KitPanel.h"

#include "PluginProcessor.h"
#include "forja/ui/ForjaTheme.h"

using forja::drumforge::kNumPads;
using forja::ui::colour::dim;
using forja::ui::colour::ember;
using forja::ui::colour::emberSoft;
using forja::ui::colour::iron0;
using forja::ui::colour::iron1;
using forja::ui::colour::iron2;
using forja::ui::colour::line;
using forja::ui::colour::lineStrong;
using forja::ui::colour::muted;
using forja::ui::colour::text;

namespace drumforgeui
{

//==============================================================================
KitDetail::KitDetail()
{
    mixerButton.setTooltip ("Abre a mesa com os 16 canais");
    mixerButton.setWantsKeyboardFocus (false);
    mixerButton.onClick = [this]
    {
        if (onOpenMixer != nullptr)
            onOpenMixer();
    };

    saveButton.setTooltip (forja::ui::utf8 ("Cria uma pasta com o nome do kit, copiando os audios e a imagem junto do .dfkit, com a mix atual"));
    saveButton.setWantsKeyboardFocus (false);
    saveButton.setEnabled (false);
    saveButton.onClick = [this]
    {
        if (onSave != nullptr)
            onSave();
    };

    addAndMakeVisible (mixerButton);
    addAndMakeVisible (saveButton);
}

void KitDetail::setKit (const forja::drumforge::Kit& kit)
{
    kitName = kit.name.isNotEmpty() ? kit.name : juce::String ("Kit vazio");
    kitDescription = kit.description;
    padsWithAudio = kit.padsWithAudio();
    totalSamples = kit.sampleCount();
    layerCount = kit.layerCount();
    roundRobinCount = kit.roundRobinCount();

    for (int pad = 0; pad < kNumPads; ++pad)
    {
        const auto& source = kit.pads[(size_t) pad];
        padNames[(size_t) pad] = source.name.isNotEmpty()
                                     ? source.name
                                     : "PAD " + juce::String (pad + 1);
        padSamples[(size_t) pad] = source.sampleCount();
    }

    // Sem pad com audio nao ha o que salvar.
    saveButton.setEnabled (padsWithAudio > 0);

    repaint();
}

void KitDetail::setImage (const juce::Image& image)
{
    if (preview == image)
        return;

    preview = image;
    repaint();
}

void KitDetail::setStatus (const juce::String& statusText, bool isLoading, int loadProgress)
{
    if (status == statusText && loading == isLoading && progress == loadProgress)
        return;

    status = statusText;
    loading = isLoading;
    progress = juce::jlimit (0, 100, loadProgress);
    repaint();
}

void KitDetail::setLevels (const std::array<forja::drumforge::DrumEngine::PadMeter, kNumPads>& meters)
{
    bool changed = false;

    for (int pad = 0; pad < kNumPads; ++pad)
    {
        const float peak = meters[(size_t) pad].peak;

        if (std::abs (padLevels[(size_t) pad] - peak) > 0.002f)
        {
            padLevels[(size_t) pad] = peak;
            changed = true;
        }
    }

    if (changed)
        repaint();
}

void KitDetail::paint (juce::Graphics& g)
{
    g.setFont (forja::ui::displayFont (19.0f));
    g.setColour (text);
    g.drawText (kitName.toUpperCase(), nameArea, juce::Justification::centredLeft, true);

    g.setFont (forja::ui::uiFont (8.0f));
    g.setColour (loading ? emberSoft : muted);
    g.drawText (status, statusArea, juce::Justification::centredLeft, true);

    paintKitArt (g, artArea);

    const char* const labels[] = { "PADS", "AMOSTRAS", "CAMADAS", "RR" };
    const int values[] = { padsWithAudio, totalSamples, layerCount, roundRobinCount };
    const int chipWidth = (statsArea.getWidth() - 18) / 4;

    for (int i = 0; i < 4; ++i)
    {
        const juce::Rectangle<int> chip (statsArea.getX() + i * (chipWidth + 6),
                                         statsArea.getY(), chipWidth, statsArea.getHeight());

        g.setColour (iron0);
        g.fillRoundedRectangle (chip.toFloat(), 3.0f);
        g.setColour (line);
        g.drawRoundedRectangle (chip.toFloat().reduced (0.5f), 3.0f, 1.0f);

        g.setFont (forja::ui::tracked (forja::ui::uiFont (6.2f, juce::Font::bold), 0.12f));
        g.setColour (dim);
        g.drawText (labels[i], chip.reduced (5, 0), juce::Justification::centredLeft, false);

        g.setFont (forja::ui::uiFont (8.6f, juce::Font::bold));
        g.setColour (text);
        g.drawText (juce::String (values[i]), chip.reduced (5, 0),
                    juce::Justification::centredRight, false);
    }

    g.setColour (forja::ui::colour::meterTrack);
    g.fillRoundedRectangle (progressArea.toFloat(), 1.5f);

    if (progress > 0)
    {
        auto filled = progressArea.toFloat();
        filled.setWidth (filled.getWidth() * (float) progress / 100.0f);
        g.setColour (loading ? ember : forja::ui::colour::success);
        g.fillRoundedRectangle (filled, 1.5f);
    }

    for (int pad = 0; pad < kNumPads; ++pad)
    {
        const auto cell = padCells[(size_t) pad].toFloat();
        const bool hasAudio = padSamples[(size_t) pad] > 0;

        g.setColour (hasAudio ? iron2 : iron1);
        g.fillRoundedRectangle (cell, 3.0f);
        g.setColour (hasAudio ? lineStrong : line);
        g.drawRoundedRectangle (cell.reduced (0.5f), 3.0f, 1.0f);

        if (hasAudio)
        {
            g.setColour (ember);
            g.fillRect (cell.getX(), cell.getY() + 4.0f, 2.0f, cell.getHeight() - 8.0f);
        }

        auto inner = cell.reduced (7.0f, 0.0f);

        g.setFont (forja::ui::tracked (forja::ui::uiFont (6.0f, juce::Font::bold), 0.08f));
        g.setColour (hasAudio ? emberSoft : dim);
        g.drawText (juce::String (pad + 1).paddedLeft ('0', 2),
                    inner.withTrimmedTop (4.0f).withHeight (8.0f),
                    juce::Justification::topLeft, false);

        g.setFont (forja::ui::uiFont (8.0f, juce::Font::bold));
        g.setColour (hasAudio ? text : dim);
        g.drawText (padNames[(size_t) pad].toUpperCase(),
                    inner.withTrimmedTop (12.0f).withHeight (12.0f),
                    juce::Justification::topLeft, true);

        if (! hasAudio)
            continue;

        g.setFont (forja::ui::uiFont (6.2f));
        g.setColour (muted);
        g.drawText (juce::String (padSamples[(size_t) pad]) + " amo",
                    inner.withTrimmedTop (24.0f).withHeight (8.0f),
                    juce::Justification::topLeft, false);

        const auto bar = cell.reduced (7.0f, 0.0f).withTrimmedBottom (5.0f).withHeight (2.0f);
        g.setColour (forja::ui::colour::meterTrack);
        g.fillRect (bar);

        const float level = juce::jlimit (0.0f, 1.0f, padLevels[(size_t) pad]);

        if (level > 0.001f)
        {
            const float db = forja::ui::linearToDb (level, -60.0f);
            const float norm = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);

            g.setColour (level >= 0.99f ? forja::ui::colour::meterClip
                                        : forja::ui::colour::meterSafe);
            g.fillRect (bar.withWidth (bar.getWidth() * norm));
        }
    }

    g.setFont (forja::ui::uiFont (7.0f));
    g.setColour (dim);
    g.drawText (forja::ui::utf8 ("Clique em um pad para ouvir \u00b7 arraste uma pasta, um .dfkit ou um drumkit.txt"),
                hintArea, juce::Justification::centredLeft, true);
}

void KitDetail::resized()
{
    auto area = getLocalBounds();

    nameArea = area.removeFromTop (26);
    statusArea = area.removeFromTop (15);
    statsArea = area.removeFromTop (20);

    area.removeFromTop (4);
    progressArea = area.removeFromTop (5);
    area.removeFromTop (8);

    // A foto do kit ganha o lado esquerdo; o mapa de pads encolhe para a
    // coluna da direita (mesmas 4 linhas, celulas mais estreitas).
    constexpr int gridHeight = 4 * 48 - 6;
    const int artWidth = (area.getWidth() - 8) * 45 / 100;

    artArea = { area.getX(), area.getY(), artWidth, gridHeight };

    const auto grid = area.withTrimmedLeft (artWidth + 8).withHeight (gridHeight);
    const int cellWidth = (grid.getWidth() - 18) / 4;

    for (int pad = 0; pad < kNumPads; ++pad)
    {
        const int row = pad / 4;
        const int column = pad % 4;
        padCells[(size_t) pad] = { grid.getX() + column * (cellWidth + 6),
                                   grid.getY() + row * 48, cellWidth, 42 };
    }

    area.removeFromTop (gridHeight);

    auto footer = area;
    auto buttonsRow = footer.removeFromBottom (26);
    mixerButton.setBounds (buttonsRow.removeFromRight (140));
    buttonsRow.removeFromRight (6);
    saveButton.setBounds (buttonsRow.removeFromRight (140));
    hintArea = footer;
}

void KitDetail::mouseDown (const juce::MouseEvent& event)
{
    for (int pad = 0; pad < kNumPads; ++pad)
    {
        const auto cell = padCells[(size_t) pad];

        if (! cell.contains (event.getPosition()))
            continue;

        const float vertical = juce::jlimit (0.0f, 1.0f,
                                             (float) (event.y - cell.getY()) / (float) cell.getHeight());
        const float velocity = 0.25f + 0.75f * (1.0f - vertical);

        if (onTrigger != nullptr)
            onTrigger (pad, velocity);

        return;
    }
}

void KitDetail::mouseMove (const juce::MouseEvent& event)
{
    for (const auto& cell : padCells)
    {
        if (cell.contains (event.getPosition()))
        {
            setMouseCursor (juce::MouseCursor::PointingHandCursor);
            return;
        }
    }

    setMouseCursor (juce::MouseCursor::NormalCursor);
}

void KitDetail::paintKitArt (juce::Graphics& g, juce::Rectangle<int> area) const
{
    if (area.isEmpty())
        return;

    const auto bounds = area.toFloat();
    g.setColour (iron0.withAlpha (0.7f));
    g.fillRoundedRectangle (bounds, 4.0f);

    if (preview.isValid())
    {
        // Foto do proprio kit (image.jpg/png do drum_sklad ou #image do .dfkit),
        // decodificada no thread de carga; cabe aqui inteira, centrada.
        juce::Graphics::ScopedSaveState save (g);

        juce::Path clip;
        clip.addRoundedRectangle (bounds.reduced (0.5f), 4.0f);
        g.reduceClipRegion (clip);

        const auto inner = area.reduced (1);
        g.drawImageWithin (preview, inner.getX(), inner.getY(), inner.getWidth(), inner.getHeight(),
                            juce::RectanglePlacement (juce::RectanglePlacement::centred));
    }
    else
    {
        paintKitPlaceholder (g, bounds);
    }

    g.setColour (line);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 4.0f, 1.0f);
}

void KitDetail::paintKitPlaceholder (juce::Graphics& g, juce::Rectangle<float> bounds) const
{
    // A bateria vetorial foi desenhada para o quadro 84x72; o fator s leva
    // as mesmas proporcoes para o quadro grande.
    const float s = juce::jmin (bounds.getWidth() / 84.0f, bounds.getHeight() / 72.0f);
    const float cx = bounds.getCentreX();
    const float baseY = bounds.getBottom() - 9.0f * s;

    // Pratos e estantes
    g.setColour (ember.withAlpha (0.55f));
    g.drawEllipse (cx - 30.0f * s, baseY - 46.0f * s, 24.0f * s, 8.0f * s, 1.2f);
    g.drawEllipse (cx + 6.0f * s, baseY - 46.0f * s, 24.0f * s, 8.0f * s, 1.2f);

    g.setColour (lineStrong);
    g.drawLine (cx - 18.0f * s, baseY - 39.0f * s, cx - 24.0f * s, baseY - 6.0f * s, 1.0f);
    g.drawLine (cx + 18.0f * s, baseY - 39.0f * s, cx + 24.0f * s, baseY - 6.0f * s, 1.0f);

    // Caixa e tons
    g.setColour (iron2);
    g.fillEllipse (cx - 31.0f * s, baseY - 33.0f * s, 20.0f * s, 12.0f * s);
    g.setColour (lineStrong);
    g.drawEllipse (cx - 31.0f * s, baseY - 33.0f * s, 20.0f * s, 12.0f * s, 1.0f);

    g.setColour (iron2);
    g.fillEllipse (cx + 11.0f * s, baseY - 33.0f * s, 20.0f * s, 12.0f * s);
    g.setColour (lineStrong);
    g.drawEllipse (cx + 11.0f * s, baseY - 33.0f * s, 20.0f * s, 12.0f * s, 1.0f);

    // Bumbo
    g.setColour (iron1);
    g.fillEllipse (cx - 22.0f * s, baseY - 24.0f * s, 44.0f * s, 24.0f * s);
    g.setColour (ember);
    g.drawEllipse (cx - 22.0f * s, baseY - 24.0f * s, 44.0f * s, 24.0f * s, 1.4f);
    g.setColour (lineStrong);
    g.drawEllipse (cx - 11.0f * s, baseY - 16.0f * s, 22.0f * s, 12.0f * s, 1.0f);
}

//==============================================================================
KitPanel::KitPanel (DrumforgeAudioProcessor& owner)
    : processor (owner)
{
    addAndMakeVisible (libraryCard);
    addAndMakeVisible (kitCard);

    libraryCard.addAndMakeVisible (kitList);
    kitList.setRowHeight (22);
    kitList.setOutlineThickness (1);
    kitList.setColour (juce::ListBox::backgroundColourId, iron0);
    kitList.setColour (juce::ListBox::outlineColourId, line);

    emptyHint.setText ("Nenhum kit na biblioteca", juce::dontSendNotification);
    emptyHint.setFont (forja::ui::uiFont (8.0f));
    emptyHint.setColour (juce::Label::textColourId, dim);
    emptyHint.setJustificationType (juce::Justification::centred);
    emptyHint.setInterceptsMouseClicks (false, false);
    libraryCard.addAndMakeVisible (emptyHint);

    libraryHint.setText (forja::ui::utf8 ("Documentos/Forja/Drumforge/Kits"), juce::dontSendNotification);
    libraryHint.setFont (forja::ui::uiFont (6.8f));
    libraryHint.setColour (juce::Label::textColourId, dim);
    libraryHint.setJustificationType (juce::Justification::centredRight);
    libraryHint.setInterceptsMouseClicks (false, false);
    libraryCard.addAndMakeVisible (libraryHint);

    importButton.setTooltip ("Importa uma pasta de samples e salva o kit na biblioteca");
    openButton.setTooltip ("Carrega um .dfkit ou um drumkit.txt de qualquer pasta");
    refreshButton.setTooltip ("Rele a pasta de kits");

    for (auto* button : { &importButton, &openButton, &refreshButton })
    {
        button->setWantsKeyboardFocus (false);
        libraryCard.addAndMakeVisible (*button);
    }

    importButton.onClick = [this] { chooseImportFolder(); };
    openButton.onClick = [this] { chooseKitFile(); };
    refreshButton.onClick = [this] { updateLibrary(); };

    kitCard.addAndMakeVisible (detail);
    detail.onOpenMixer = [this]
    {
        if (onOpenMixer != nullptr)
            onOpenMixer();
    };
    detail.onSave = [this] { chooseSaveAs(); };
    detail.onTrigger = [this] (int pad, float velocity)
    {
        processor.previewPad (pad, velocity);
    };

    refresh();
}

KitPanel::~KitPanel() = default;

void KitPanel::refresh()
{
    updateLibrary();
    detail.setKit (processor.getKitCopy());

    // A foto de preview vem decodificada dentro do LoadedKit; sem kit pronto,
    // volta a arte vetorial padrao.
    const auto loaded = processor.getLoadedKit();
    detail.setImage (loaded != nullptr ? loaded->preview : juce::Image());

    refreshStatus();
}

void KitPanel::refreshStatus()
{
    detail.setStatus (processor.getKitStatus(), processor.isLoadingKit(),
                      processor.getLoadProgress());
}

void KitPanel::setLevels (
    const std::array<forja::drumforge::DrumEngine::PadMeter, kNumPads>& meters)
{
    detail.setLevels (meters);
}

void KitPanel::updateLibrary()
{
    libraryKits = processor.findLibraryKits();

    updatingSelection = true;
    kitList.updateContent();

    auto loaded = processor.getLoadedKit();
    const auto activePath = loaded != nullptr ? loaded->kit.sourceFile : juce::String();

    int activeRow = -1;

    for (int row = 0; row < libraryKits.size(); ++row)
        if (libraryKits[row].getFullPathName() == activePath)
            activeRow = row;

    if (activeRow >= 0)
        kitList.selectRow (activeRow, true, true);
    else
        kitList.deselectAllRows();

    updatingSelection = false;

    emptyHint.setVisible (libraryKits.isEmpty());
    emptyHint.toFront (false);
}

void KitPanel::loadSelectedRow()
{
    const int row = kitList.getSelectedRow();

    if (juce::isPositiveAndBelow (row, libraryKits.size()))
        processor.loadKitFileAsync (libraryKits[row]);
}

void KitPanel::chooseKitFile()
{
    chooser = std::make_unique<juce::FileChooser> (
        "Abrir kit do Drumforge",
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
        "*.dfkit;*.txt;*.labooh");

    juce::Component::SafePointer<KitPanel> safe (this);

    chooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectFiles,
                          [safe] (const juce::FileChooser& result)
                          {
                              if (safe == nullptr)
                                  return;

                              const auto file = result.getResult();

                              if (file.existsAsFile())
                                  safe->processor.loadKitFileAsync (file);
                          });
}

void KitPanel::chooseImportFolder()
{
    chooser = std::make_unique<juce::FileChooser> (
        "Importar pasta de samples",
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory));

    juce::Component::SafePointer<KitPanel> safe (this);

    chooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectDirectories,
                          [safe] (const juce::FileChooser& result)
                          {
                              if (safe == nullptr)
                                  return;

                              const auto folder = result.getResult();

                              if (folder.isDirectory())
                                  safe->processor.importKitFolderAsync (folder);
                          });
}

void KitPanel::chooseSaveAs()
{
    // Nome padrao sai do kit aberto; a mix e capturada so no aceite do dialog,
    // para pegar os knobs na posicao final (o dialog pode ficar aberto enquanto
    // alguem ajusta a mesa).
    const auto suggestion = juce::File::createLegalFileName (
        processor.getKitCopy().name.isNotEmpty() ? processor.getKitCopy().name
                                                 : juce::String ("Kit"));

    chooser = std::make_unique<juce::FileChooser> (
        "Salvar kit (cria uma pasta com o nome do kit, com os audios)",
        processor.getLibraryDirectory().getChildFile (suggestion),
        "*.dfkit");

    juce::Component::SafePointer<KitPanel> safe (this);

    chooser->launchAsync (juce::FileBrowserComponent::saveMode
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [safe] (const juce::FileChooser& result)
                          {
                              if (safe == nullptr)
                                  return;

                              auto file = result.getResult();

                              if (file == juce::File())
                                  return;

                              const auto kitName = file.getFileNameWithoutExtension();

                              if (kitName.isEmpty())
                                  return;

                              // O pacote e uma pasta com o nome do kit; o .dfkit,
                              // os samples e a imagem vao todos para dentro dela.
                              const auto bundleDir = file.getParentDirectory().getChildFile (kitName);

                              const auto kit = safe->processor.getKitForSaving();

                              if (kit.padsWithAudio() == 0)
                                  return;

                              juce::String error;

                              if (! kit.saveAsBundle (bundleDir, error))
                              {
                                  juce::AlertWindow::showAsync (
                                      juce::MessageBoxOptions()
                                          .withTitle ("Drumforge")
                                          .withMessage (error.isNotEmpty()
                                                            ? error
                                                            : "Falha ao salvar o kit.")
                                          .withButton ("OK"),
                                      nullptr);
                                  return;
                              }

                              safe->processor.markKitSaved (
                                  bundleDir.getChildFile (kitName + ".dfkit"));
                              safe->updateLibrary();
                              safe->refreshStatus();
                          });
}

void KitPanel::paint (juce::Graphics& g)
{
    if (! dragActive)
        return;

    g.setColour (ember.withAlpha (0.9f));
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (2.0f), 6.0f, 2.0f);

    g.setFont (forja::ui::uiFont (9.0f, juce::Font::bold));
    g.setColour (emberSoft);
    g.drawText (forja::ui::utf8 ("Solte para carregar o kit"),
                getLocalBounds().withTrimmedBottom (14), juce::Justification::centred, false);
}

void KitPanel::resized()
{
    constexpr int libraryWidth = 330;

    libraryCard.setBounds (0, 0, libraryWidth, getHeight());
    kitCard.setBounds (libraryWidth + 8, 0, getWidth() - libraryWidth - 8, getHeight());

    auto area = libraryCard.getContentBounds();
    const int buttonsHeight = 24 + 6 + 22;

    kitList.setBounds (area.removeFromTop (area.getHeight() - buttonsHeight));
    emptyHint.setBounds (kitList.getBounds().withHeight (24).translated (0, 34));

    area.removeFromTop (6);

    auto firstRow = area.removeFromTop (24);
    const int halfWidth = (firstRow.getWidth() - 6) / 2;
    importButton.setBounds (firstRow.removeFromLeft (halfWidth));
    firstRow.removeFromLeft (6);
    openButton.setBounds (firstRow);

    area.removeFromTop (6);

    auto secondRow = area.removeFromTop (22);
    refreshButton.setBounds (secondRow.removeFromLeft (100));
    secondRow.removeFromLeft (6);
    libraryHint.setBounds (secondRow);

    detail.setBounds (kitCard.getContentBounds());
}

int KitPanel::getNumRows()
{
    return libraryKits.size();
}

void KitPanel::paintListBoxItem (int rowNumber, juce::Graphics& g, int width, int height,
                                 bool rowIsSelected)
{
    if (! juce::isPositiveAndBelow (rowNumber, libraryKits.size()))
        return;

    if (rowIsSelected)
    {
        g.setColour (ember.withAlpha (0.12f));
        g.fillRect (0, 0, width, height);
        g.setColour (ember);
        g.fillRect (0, 0, 2, height);
    }

    g.setFont (forja::ui::uiFont (8.4f, juce::Font::bold));
    g.setColour (rowIsSelected ? emberSoft : text);

    // Kit de texto e uma pasta (mostra o nome da pasta); .dfkit e arquivo
    // (mostra o nome sem a extensao).
    const auto& entry = libraryKits[rowNumber];
    const auto label = entry.isDirectory() ? entry.getFileName()
                                          : entry.getFileNameWithoutExtension();
    g.drawText (label.toUpperCase(),
                9, 0, width - 14, height, juce::Justification::centredLeft, true);
}

void KitPanel::selectedRowsChanged (int)
{
    if (updatingSelection)
        return;

    loadSelectedRow();
}

bool KitPanel::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& path : files)
    {
        const juce::File file (path);

        if (file.isDirectory() || file.hasFileExtension ("dfkit") || file.hasFileExtension ("txt"))
            return true;
    }

    return false;
}

void KitPanel::filesDropped (const juce::StringArray& files, int, int)
{
    dragActive = false;
    repaint();

    for (const auto& path : files)
    {
        const juce::File file (path);

        if (file.isDirectory())
        {
            processor.importKitFolderAsync (file);
            return;
        }

        if (file.hasFileExtension ("dfkit") || file.hasFileExtension ("txt")
            || file.hasFileExtension ("labooh"))
        {
            processor.loadKitFileAsync (file);
            return;
        }
    }
}

void KitPanel::fileDragEnter (const juce::StringArray&, int, int)
{
    dragActive = true;
    repaint();
}

void KitPanel::fileDragExit (const juce::StringArray&)
{
    dragActive = false;
    repaint();
}

} // namespace drumforgeui
