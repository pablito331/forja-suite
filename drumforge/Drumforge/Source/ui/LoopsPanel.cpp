#include "LoopsPanel.h"

#include "PluginProcessor.h"
#include "forja/ui/ForjaTheme.h"

#include <cmath>

using forja::drumforge::kNumPads;
using forja::drumforge::loops::Entry;

namespace drumforgeui
{

namespace
{

constexpr int laneLabelWidth = 62;
constexpr int maxStepsShown = 64;

juce::String u (const char* text)
{
    return forja::ui::utf8 (text);
}

void configureToggle (juce::TextButton& button, const juce::String& text, const juce::String& tooltip)
{
    button.setButtonText (text);
    button.setClickingTogglesState (true);
    button.setTooltip (tooltip);
    button.setWantsKeyboardFocus (false);
}

juce::String formatBarBeat (double beats)
{
    const int bar = 1 + (int) std::floor (beats / 4.0);
    const int beat = 1 + (int) std::floor (std::fmod (beats, 4.0));

    return juce::String (bar) + "." + juce::String (beat);
}

} // namespace

//==============================================================================
void LoopGrid::setData (std::vector<juce::String> laneNames,
                        std::vector<std::vector<float>> velocities,
                        int stepCount, int barsShown, int unmappedHits, int barsTotal)
{
    lanes = std::move (laneNames);
    cells = std::move (velocities);
    steps = juce::jmax (4, stepCount);
    bars = juce::jmax (1, barsShown);
    totalBars = juce::jmax (1, barsTotal);
    unmapped = juce::jmax (0, unmappedHits);
    playStep = -1;
    repaint();
}

void LoopGrid::setPlayhead (int step, bool isPlaying)
{
    const int clamped = step >= 0 && step < steps ? step : -1;

    if (clamped == playStep && isPlaying == playing)
        return;

    playStep = clamped;
    playing = isPlaying;
    repaint (laneArea.expanded (1));
}

void LoopGrid::resized()
{
    auto area = getLocalBounds();
    rulerArea = area.removeFromTop (12);
    laneArea = area;
}

void LoopGrid::paint (juce::Graphics& g)
{
    auto area = laneArea;

    if (area.isEmpty())
    {
        area = getLocalBounds().withTrimmedTop (12);
        rulerArea = getLocalBounds().removeFromTop (12);
    }

    if (cells.empty())
    {
        g.setFont (forja::ui::uiFont (8.0f));
        g.setColour (forja::ui::colour::dim);
        g.drawText (unmapped > 0 ? u ("Os golpes do loop n\u00e3o casam com o kit atual")
                                 : u ("Escolha um loop na biblioteca"),
                    area, juce::Justification::centred, true);
        return;
    }

    g.setColour (forja::ui::colour::iron0);
    g.fillRoundedRectangle (area.toFloat(), 3.0f);

    const float cellWidth = (float) (area.getWidth() - laneLabelWidth) / (float) steps;
    const float laneHeight = (float) area.getHeight() / (float) lanes.size();
    const int cellsX = area.getX() + laneLabelWidth;

    for (int bar = 0; bar < bars; ++bar)
    {
        const float x = (float) cellsX + (float) bar * 16.0f * cellWidth;

        g.setFont (forja::ui::monoFont (6.4f));
        g.setColour (forja::ui::colour::dim);
        g.drawText (juce::String (bar + 1),
                    juce::Rectangle<float> (x + 2.0f, (float) rulerArea.getY(), 12.0f, 10.0f),
                    juce::Justification::centredLeft, false);

        g.setColour (forja::ui::colour::line);
        g.fillRect (x, (float) area.getY(), 1.0f, (float) area.getHeight());
    }

    for (size_t lane = 0; lane < lanes.size(); ++lane)
    {
        const float top = (float) area.getY() + (float) lane * laneHeight;

        g.setFont (forja::ui::uiFont (7.0f, juce::Font::bold));
        g.setColour (forja::ui::colour::muted);
        g.drawText (lanes[lane],
                    juce::Rectangle<float> ((float) area.getX() + 5.0f, top, (float) laneLabelWidth - 9.0f,
                                            laneHeight),
                    juce::Justification::centredLeft, false);

        for (int step = 0; step < steps; ++step)
        {
            const juce::Rectangle<float> cell ((float) cellsX + (float) step * cellWidth,
                                               top + 1.5f, cellWidth - 1.0f, laneHeight - 3.0f);

            const bool beatStart = step % 4 == 0;

            g.setColour (beatStart ? forja::ui::colour::iron2 : forja::ui::colour::iron1);
            g.fillRoundedRectangle (cell, 1.5f);

            const float velocity = cells[lane][(size_t) step];

            if (velocity <= 0.0f)
                continue;

            g.setColour (forja::ui::colour::ember.withAlpha (0.32f + 0.68f * juce::jlimit (0.0f, 1.0f, velocity)));
            g.fillRoundedRectangle (cell.reduced (0.5f), 1.5f);
        }
    }

    if (playing && playStep >= 0)
    {
        const juce::Rectangle<float> column ((float) cellsX + (float) playStep * cellWidth,
                                             (float) area.getY(), cellWidth,
                                             (float) area.getHeight());

        g.setColour (forja::ui::colour::emberPale.withAlpha (0.12f));
        g.fillRect (column);
        g.setColour (forja::ui::colour::emberSoft);
        g.fillRect (column.withHeight (1.5f));
    }
}

//==============================================================================
DragOutButton::DragOutButton()
    : juce::TextButton ("ARRASTAR MIDI")
{
    setWantsKeyboardFocus (false);
}

void DragOutButton::mouseDrag (const juce::MouseEvent& event)
{
    if (dragging || awaitingDrop || event.getDistanceFromDragStart() < 8)
        return;

    const auto files = getFiles != nullptr ? getFiles() : juce::StringArray();

    if (files.isEmpty() || ! juce::File (files[0]).existsAsFile())
        return;

    dragging = true;
    awaitingDrop = true;

    juce::DragAndDropContainer::performExternalDragDropOfFiles (
        files, false, this,
        [safe = juce::Component::SafePointer<DragOutButton> (this)]
        {
            if (safe != nullptr)
            {
                safe->dragging = false;
                safe->awaitingDrop = false;
            }
        });
}

void DragOutButton::mouseUp (const juce::MouseEvent&)
{
    dragging = false;
}

//==============================================================================
LoopDetail::LoopDetail (DrumforgeAudioProcessor& owner)
    : processor (owner)
{
    addAndMakeVisible (grid);

    playButton.setTooltip (u ("Toca o loop selecionado no BPM do host ou no BPM manual"));
    playButton.setWantsKeyboardFocus (false);
    playButton.onClick = [this]
    {
        if (processor.isLoopPreviewing())
            processor.stopLoopPreview();
        else
            processor.startLoopPreview();

        tick();
    };
    addAndMakeVisible (playButton);

    configureToggle (syncButton, "SYNC DAW", u ("Segue o BPM do host"));
    syncButton.onClick = [this] { updateBpmAffordance(); };
    addAndMakeVisible (syncButton);

    bpmKnob.setFormatter ([] (double value) { return juce::String (juce::roundToInt (40.0 + 200.0 * value)); });
    bpmKnob.getSlider().setDoubleClickReturnValue (true, 0.4);
    bpmKnob.getSlider().setTooltip (u ("BPM manual do preview, de 40 a 240"));
    addAndMakeVisible (bpmKnob);

    dragButton.setTooltip (u ("Arraste para a DAW para levar o .mid da biblioteca"));
    dragButton.getFiles = [this]
    {
        const auto path = processor.getSelectedLoopPath();

        return path.isEmpty() ? juce::StringArray() : juce::StringArray (path);
    };
    addAndMakeVisible (dragButton);

    bpmAttachment =
        std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            processor.apvts, "loop_bpm", bpmKnob.getSlider());
    syncAttachment =
        std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            processor.apvts, "loop_sync", syncButton);

    updateBpmAffordance();
}

void LoopDetail::refresh()
{
    auto sequence = processor.getPreviewLoop();

    loopName.clear();
    originText.clear();
    totalHits = 0;
    totalBars = 1;

    std::vector<juce::String> laneNames;
    std::vector<std::vector<float>> velocities;
    int steps = 16;
    int barsShown = 1;
    int unmapped = 0;

    if (sequence != nullptr)
    {
        loopName = sequence->name;
        totalHits = (int) sequence->hits.size();

        const int totalSteps = juce::jmax (4, juce::roundToInt (sequence->beats * 4.0));
        totalBars = juce::jmax (1, totalSteps / 16);
        barsShown = juce::jlimit (1, totalBars, maxStepsShown / 16);
        steps = juce::jmin (totalSteps, maxStepsShown);

        const auto noteMap = processor.noteToPadMap();
        std::array<int, kNumPads> laneOf {};
        std::array<int, kNumPads> counts {};
        laneOf.fill (-1);

        for (const auto& hit : sequence->hits)
        {
            const int pad = juce::isPositiveAndBelow (hit.note, 128) ? noteMap[(size_t) hit.note] : -1;

            if (pad < 0)
            {
                ++unmapped;
                continue;
            }

            ++counts[(size_t) pad];
        }

        for (int pad = 0; pad < kNumPads; ++pad)
        {
            if (counts[(size_t) pad] == 0)
                continue;

            laneOf[(size_t) pad] = (int) laneNames.size();
            laneNames.push_back (juce::String (pad + 1).paddedLeft ('0', 2) + " "
                                 + processor.getPadName (pad).toUpperCase());
        }

        velocities.assign (laneNames.size(), std::vector<float> ((size_t) steps, 0.0f));

        for (const auto& hit : sequence->hits)
        {
            const int pad = juce::isPositiveAndBelow (hit.note, 128) ? noteMap[(size_t) hit.note] : -1;

            if (pad < 0)
                continue;

            const int lane = laneOf[(size_t) pad];
            const int step = (int) std::floor (hit.beat * 4.0 + 1.0e-6);

            if (lane < 0 || ! juce::isPositiveAndBelow (step, steps))
                continue;

            auto& target = velocities[(size_t) lane][(size_t) step];
            target = juce::jmax (target, hit.velocity);
        }

        originText = juce::String (juce::roundToInt (sequence->bpm)) + " BPM - "
                   + juce::String (totalBars) + (totalBars == 1 ? " compasso" : " compassos")
                   + " - " + juce::String (totalHits) + " golpes";
    }

    statusText = processor.getLoopStatus();

    grid.setData (std::move (laneNames), std::move (velocities), steps, barsShown, unmapped, totalBars);

    lastBpmShown = -1;
    tick();
    repaint();
}

void LoopDetail::tick()
{
    const bool playing = processor.isLoopPreviewing();
    const double beats = processor.getLoopPositionBeats();
    const int step = playing ? (int) std::floor (beats * 4.0 + 1.0e-6) : -1;

    grid.setPlayhead (step, playing);

    const auto status = processor.getLoopStatus();

    if (status != statusText)
    {
        statusText = status;
        repaint (statusArea);
    }

    const juce::String text (playing ? "PARAR" : "TOCAR");

    if (playButton.getButtonText() != text)
        playButton.setButtonText (text);

    // O transporte so repinta quando o compasso muda, e nao a 30 Hz.
    const auto positionText = playing ? formatBarBeat (beats) : juce::String ("--");

    if (positionText != lastPositionText)
    {
        lastPositionText = positionText;
        repaint (positionArea);
    }

    const int bpm = juce::roundToInt (processor.getEffectiveBpm());
    const bool synced = processor.apvts.getRawParameterValue ("loop_sync")->load() > 0.5f;

    if (bpm != lastBpmShown || synced != lastSynced)
    {
        lastBpmShown = bpm;
        lastSynced = synced;
        repaint (bpmArea.getUnion (positionArea).expanded (2));
    }

    updateBpmAffordance();
}

void LoopDetail::updateBpmAffordance()
{
    const bool synced = processor.apvts.getRawParameterValue ("loop_sync")->load() > 0.5f;
    const float alpha = synced ? 0.35f : 1.0f;

    if (std::abs (bpmKnob.getAlpha() - alpha) > 0.001f)
        bpmKnob.setAlpha (alpha);
}

void LoopDetail::resized()
{
    auto area = getLocalBounds();

    auto titleRow = area.removeFromTop (22);
    metaArea = titleRow.removeFromRight (juce::jmax (0, titleRow.getWidth() - 240));
    titleArea = titleRow;

    statusArea = area.removeFromTop (14);
    area.removeFromTop (6);

    auto transport = area.removeFromTop (38);

    playButton.setBounds (transport.removeFromLeft (72).withSizeKeepingCentre (72, 24));
    transport.removeFromLeft (6);
    syncButton.setBounds (transport.removeFromLeft (82).withSizeKeepingCentre (82, 24));
    transport.removeFromLeft (8);
    bpmKnob.setBounds (transport.removeFromLeft (34).withSizeKeepingCentre (34, 38));
    transport.removeFromLeft (4);
    bpmArea = transport.removeFromLeft (86).withSizeKeepingCentre (86, 26);
    dragButton.setBounds (transport.removeFromRight (118).withSizeKeepingCentre (118, 24));
    transport.removeFromRight (8);
    positionArea = transport;

    area.removeFromTop (6);
    hintArea = area.removeFromBottom (12);
    grid.setBounds (area);
}

void LoopDetail::paint (juce::Graphics& g)
{
    const bool empty = loopName.isEmpty();

    g.setFont (forja::ui::displayFont (18.0f));
    g.setColour (empty ? forja::ui::colour::dim : forja::ui::colour::text);
    g.drawText (empty ? juce::String ("NENHUM LOOP") : loopName.toUpperCase(),
                titleArea, juce::Justification::centredLeft, true);

    g.setFont (forja::ui::uiFont (7.6f));
    g.setColour (forja::ui::colour::muted);
    g.drawText (originText, metaArea, juce::Justification::centredRight, true);

    g.setFont (forja::ui::uiFont (7.8f));
    g.setColour (empty ? forja::ui::colour::dim : forja::ui::colour::emberSoft);
    g.drawText (statusText, statusArea, juce::Justification::centredLeft, true);

    g.setFont (forja::ui::uiFont (8.4f, juce::Font::bold));
    g.setColour (forja::ui::colour::text);
    g.drawText (juce::String (juce::jmax (0, lastBpmShown)) + " BPM",
                bpmArea.withHeight (13), juce::Justification::centredLeft, false);

    g.setFont (forja::ui::tracked (forja::ui::uiFont (6.2f, juce::Font::bold), 0.10f));
    g.setColour (forja::ui::colour::dim);
    g.drawText (lastSynced ? "DO HOST" : "MANUAL", bpmArea.withTrimmedTop (13),
                juce::Justification::centredLeft, false);

    const bool playing = processor.isLoopPreviewing();
    const double beats = processor.getLoopPositionBeats();

    g.setFont (forja::ui::monoFont (11.0f));
    g.setColour (playing ? forja::ui::colour::emberPale : forja::ui::colour::dim);
    g.drawText (playing ? formatBarBeat (beats) : juce::String ("--"),
                positionArea, juce::Justification::centredRight, false);

    const int unmapped = grid.getUnmappedHits();
    const bool truncated = totalBars > juce::jmax (1, maxStepsShown / 16);

    juce::String hint = u ("Arraste um .mid para a biblioteca \u00b7 arraste ARRASTAR MIDI para a DAW");

    if (unmapped > 0)
        hint = juce::String (unmapped) + u (" golpe(s) fora do mapa do kit atual");
    else if (truncated)
        hint = u ("Grade limitada aos primeiros 4 compassos do loop");

    g.setFont (forja::ui::uiFont (6.8f));
    g.setColour (forja::ui::colour::dim);
    g.drawText (hint, hintArea, juce::Justification::centredLeft, true);
}

//==============================================================================
LoopsPanel::LoopsPanel (DrumforgeAudioProcessor& owner)
    : processor (owner),
      detail (owner)
{
    addAndMakeVisible (libraryCard);
    addAndMakeVisible (loopCard);

    libraryCard.addAndMakeVisible (packChoice);
    packChoice.getComboBox().setTooltip (u ("Filtra por pacote ou categoria (subpastas: Fills, Chorus...)"));
    packChoice.getComboBox().onChange = [this] { rebuildFilter(); };

    search.setTextToShowWhenEmpty (u ("Buscar loop"), forja::ui::colour::dim);
    search.setFont (forja::ui::uiFont (8.6f));
    search.setColour (juce::TextEditor::backgroundColourId, forja::ui::colour::iron1);
    search.setColour (juce::TextEditor::textColourId, forja::ui::colour::text);
    search.setColour (juce::TextEditor::outlineColourId, forja::ui::colour::lineStrong);
    search.setColour (juce::TextEditor::focusedOutlineColourId, forja::ui::colour::ember);
    search.setColour (juce::TextEditor::highlightColourId, forja::ui::colour::ember.withAlpha (0.25f));
    search.setColour (juce::TextEditor::highlightedTextColourId, forja::ui::colour::emberPale);
    search.onTextChange = [this] { rebuildFilter(); };
    libraryCard.addAndMakeVisible (search);

    libraryCard.addAndMakeVisible (list);
    list.setRowHeight (26);
    list.setOutlineThickness (1);
    list.setColour (juce::ListBox::backgroundColourId, forja::ui::colour::iron0);
    list.setColour (juce::ListBox::outlineColourId, forja::ui::colour::line);

    emptyHint.setText (u ("Nenhum loop na biblioteca"), juce::dontSendNotification);
    emptyHint.setFont (forja::ui::uiFont (8.0f));
    emptyHint.setColour (juce::Label::textColourId, forja::ui::colour::dim);
    emptyHint.setJustificationType (juce::Justification::centred);
    emptyHint.setInterceptsMouseClicks (false, false);
    libraryCard.addAndMakeVisible (emptyHint);

    libraryHint.setText ("Documentos/Forja/Drumforge/Loops", juce::dontSendNotification);
    libraryHint.setFont (forja::ui::uiFont (6.4f));
    libraryHint.setColour (juce::Label::textColourId, forja::ui::colour::dim);
    libraryHint.setJustificationType (juce::Justification::centredRight);
    libraryHint.setInterceptsMouseClicks (false, false);
    libraryCard.addAndMakeVisible (libraryHint);

    importButton.setTooltip (u ("Copia .mid ou pastas inteiras para a biblioteca, preservando subpastas"));
    folderButton.setTooltip (u ("Abre a pasta da biblioteca no sistema"));
    refreshButton.setTooltip (u ("Rele a pasta de loops"));

    for (auto* button : { &importButton, &folderButton, &refreshButton })
    {
        button->setWantsKeyboardFocus (false);
        libraryCard.addAndMakeVisible (*button);
    }

    importButton.onClick = [this] { chooseLoopFiles(); };
    folderButton.onClick = [this] { openLoopFolder(); };
    refreshButton.onClick = [this] { refresh(); };

    loopCard.addAndMakeVisible (detail);

    refresh();
}

LoopsPanel::~LoopsPanel() = default;

void LoopsPanel::refresh()
{
    updateLibrary();
    detail.refresh();
}

void LoopsPanel::refreshStatus()
{
    detail.tick();
}

void LoopsPanel::updateLibrary()
{
    library = processor.findLoops();

    juce::StringArray found;

    for (const auto& entry : library)
    {
        if (entry.folder.isEmpty())
            continue;

        // Cada nivel do caminho vira categoria: "Ritmos/Fills" entra com
        // "Ritmos" (prefixo) e "Ritmos/Fills" (folha).
        juce::String path;

        for (const auto& part : juce::StringArray::fromTokens (entry.folder, "/", {}))
        {
            if (part.isEmpty())
                continue;

            path = path.isEmpty() ? part : path + "/" + part;

            if (! found.contains (path))
                found.add (path);
        }
    }

    found.sort (true);

    if (found != packs)
    {
        auto& combo = packChoice.getComboBox();
        const auto previous = combo.getText();

        packs = found;
        combo.clear (juce::dontSendNotification);
        combo.addItem ("TODOS OS PACOTES", 1);

        for (int index = 0; index < packs.size(); ++index)
            combo.addItem (packs[index], index + 2);

        const int restored = packs.indexOf (previous);
        combo.setSelectedId (restored >= 0 ? restored + 2 : 1, juce::dontSendNotification);
    }

    rebuildFilter();
}

void LoopsPanel::rebuildFilter()
{
    const auto query = search.getText().trim().toLowerCase();
    auto& combo = packChoice.getComboBox();
    const auto pack = combo.getSelectedId() > 1 ? combo.getText() : juce::String();

    visibleRows.clear();

    for (int index = 0; index < (int) library.size(); ++index)
    {
        const auto& entry = library[(size_t) index];

        // Categoria-pai traz toda a subarvore: "Ritmos" inclui "Ritmos/Fills".
        if (pack.isNotEmpty() && entry.folder != pack
            && ! entry.folder.startsWith (pack + "/"))
            continue;

        if (query.isNotEmpty()
            && ! entry.name.toLowerCase().contains (query)
            && ! entry.folder.toLowerCase().contains (query))
            continue;

        visibleRows.push_back (index);
    }

    list.updateContent();
    emptyHint.setVisible (visibleRows.empty());

    if (emptyHint.isVisible())
        emptyHint.toFront (false);

    selectActiveRow();
}

void LoopsPanel::selectActiveRow()
{
    const auto activePath = processor.getSelectedLoopPath();
    int row = -1;

    for (int index = 0; index < (int) visibleRows.size(); ++index)
        if (library[(size_t) visibleRows[(size_t) index]].file.getFullPathName() == activePath)
            row = index;

    updatingSelection = true;

    if (row >= 0)
        list.selectRow (row, true, true);
    else
        list.deselectAllRows();

    updatingSelection = false;
}

void LoopsPanel::loadSelectedRow()
{
    const int row = list.getSelectedRow();

    if (! juce::isPositiveAndBelow (row, (int) visibleRows.size()))
        return;

    const auto& entry = library[(size_t) visibleRows[(size_t) row]];

    processor.loadLoopFile (entry.file);
    detail.refresh();
}

void LoopsPanel::importFiles (const juce::StringArray& files)
{
    bool imported = false;

    for (const auto& path : files)
    {
        const juce::File file (path);

        if (file.isDirectory())
        {
            if (processor.importLoopFolder (file) > 0)
                imported = true;

            continue;
        }

        if (! file.hasFileExtension ("mid;midi"))
            continue;

        processor.importLoopFile (file);
        imported = true;
    }

    if (imported)
        refresh();
}

void LoopsPanel::chooseLoopFiles()
{
    chooser = std::make_unique<juce::FileChooser> (
        "Importar loops MIDI",
        juce::File::getSpecialLocation (juce::File::userMusicDirectory), "*.mid;*.midi");

    juce::Component::SafePointer<LoopsPanel> safe (this);

    chooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::canSelectDirectories
                              | juce::FileBrowserComponent::canSelectMultipleItems,
                          [safe] (const juce::FileChooser& result)
                          {
                              if (safe == nullptr)
                                  return;

                              juce::StringArray paths;

                              for (const auto& file : result.getResults())
                                  paths.add (file.getFullPathName());

                              safe->importFiles (paths);
                          });
}

void LoopsPanel::openLoopFolder()
{
    auto directory = processor.getLoopDirectory();

    if (! directory.isDirectory())
        directory.createDirectory();

    directory.revealToUser();
}

void LoopsPanel::paint (juce::Graphics& g)
{
    if (! dragActive)
        return;

    g.setColour (forja::ui::colour::ember.withAlpha (0.9f));
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (2.0f), 6.0f, 2.0f);

    g.setFont (forja::ui::uiFont (9.0f, juce::Font::bold));
    g.setColour (forja::ui::colour::emberSoft);
    g.drawText (u ("Solte para importar o loop MIDI"),
                getLocalBounds().withTrimmedBottom (14), juce::Justification::centred, false);
}

void LoopsPanel::resized()
{
    constexpr int libraryWidth = 330;

    libraryCard.setBounds (0, 0, libraryWidth, getHeight());
    loopCard.setBounds (libraryWidth + 8, 0, getWidth() - libraryWidth - 8, getHeight());

    auto area = libraryCard.getContentBounds();
    const int buttonsHeight = 24 + 6 + 22;

    packChoice.setBounds (area.removeFromTop (36));
    area.removeFromTop (6);
    search.setBounds (area.removeFromTop (24));
    area.removeFromTop (6);

    list.setBounds (area.removeFromTop (area.getHeight() - buttonsHeight));
    emptyHint.setBounds (list.getBounds().withSizeKeepingCentre (juce::jmax (0, list.getWidth() - 16), 24));

    area.removeFromTop (6);

    auto firstRow = area.removeFromTop (24);
    const int halfWidth = (firstRow.getWidth() - 6) / 2;
    importButton.setBounds (firstRow.removeFromLeft (halfWidth));
    firstRow.removeFromLeft (6);
    folderButton.setBounds (firstRow);

    area.removeFromTop (6);

    auto secondRow = area.removeFromTop (22);
    refreshButton.setBounds (secondRow.removeFromLeft (100));
    secondRow.removeFromLeft (6);
    libraryHint.setBounds (secondRow);

    detail.setBounds (loopCard.getContentBounds());
}

int LoopsPanel::getNumRows()
{
    return (int) visibleRows.size();
}

void LoopsPanel::paintListBoxItem (int rowNumber, juce::Graphics& g, int width, int height,
                                   bool rowIsSelected)
{
    if (! juce::isPositiveAndBelow (rowNumber, (int) visibleRows.size()))
        return;

    const auto& entry = library[(size_t) visibleRows[(size_t) rowNumber]];
    const bool active = entry.file.getFullPathName() == processor.getSelectedLoopPath();

    if (rowIsSelected)
    {
        g.setColour (forja::ui::colour::ember.withAlpha (0.12f));
        g.fillRect (0, 0, width, height);
    }

    if (rowIsSelected || active)
    {
        g.setColour (active ? forja::ui::colour::ember : forja::ui::colour::lineStrong);
        g.fillRect (0, 0, 2, height);
    }

    g.setFont (forja::ui::uiFont (8.4f, juce::Font::bold));
    g.setColour (active ? forja::ui::colour::emberPale
                        : (rowIsSelected ? forja::ui::colour::emberSoft
                                         : forja::ui::colour::text));
    g.drawText (entry.name.toUpperCase(), 9, 3, width - 88, height - 12,
                juce::Justification::centredLeft, true);

    g.setFont (forja::ui::uiFont (6.4f));
    g.setColour (forja::ui::colour::dim);
    g.drawText ((entry.folder.isNotEmpty() ? entry.folder + " - " : juce::String())
                    + entry.bpmText() + " BPM - "
                    + juce::String (entry.bars()) + (entry.bars() == 1 ? " comp" : " comps") + " - "
                    + juce::String (entry.hits) + " golpes",
                9, height - 11, width - 88, height - 12, juce::Justification::centredLeft, true);

    g.setFont (forja::ui::tracked (forja::ui::uiFont (6.2f, juce::Font::bold), 0.10f));
    g.setColour (active ? forja::ui::colour::emberSoft : forja::ui::colour::dim);
    g.drawText (active ? "NO PLUGIN" : "MIDI", width - 76, 0, 68, height,
                juce::Justification::centredRight, false);
}

void LoopsPanel::selectedRowsChanged (int)
{
    if (updatingSelection)
        return;

    loadSelectedRow();
}

bool LoopsPanel::isInterestedInFileDrag (const juce::StringArray& files)
{
    using forja::drumforge::loops::hasMidiFiles;

    for (const auto& path : files)
    {
        const juce::File file (path);

        if (file.hasFileExtension ("mid;midi") || (file.isDirectory() && hasMidiFiles (file)))
            return true;
    }

    return false;
}

void LoopsPanel::filesDropped (const juce::StringArray& files, int, int)
{
    dragActive = false;
    repaint();
    importFiles (files);
}

void LoopsPanel::fileDragEnter (const juce::StringArray&, int, int)
{
    dragActive = true;
    repaint();
}

void LoopsPanel::fileDragExit (const juce::StringArray&)
{
    dragActive = false;
    repaint();
}

} // namespace drumforgeui
