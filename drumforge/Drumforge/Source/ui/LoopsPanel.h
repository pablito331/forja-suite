#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <functional>
#include <memory>
#include <vector>

#include "engine/DrumEngine.h"
#include "forja/ui/ForjaCard.h"
#include "forja/ui/ForjaChoice.h"
#include "loops/LoopLibrary.h"
#include "ui/MiniKnob.h"

class DrumforgeAudioProcessor;

namespace drumforgeui
{

// Grade do loop: uma faixa por pad que o arquivo realmente toca e uma coluna
// por 1/16. Nada e inventado: as celulas vem dos golpes do MIDI.
class LoopGrid : public juce::Component
{
public:
    LoopGrid() = default;

    void setData (std::vector<juce::String> laneNames,
                  std::vector<std::vector<float>> velocities,
                  int stepCount, int barsShown, int unmappedHits, int barsTotal);
    void setPlayhead (int step, bool isPlaying);

    int getUnmappedHits() const noexcept { return unmapped; }

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    std::vector<juce::String> lanes;
    std::vector<std::vector<float>> cells;
    int steps = 16;
    int bars = 1;
    int totalBars = 1;
    int unmapped = 0;
    int playStep = -1;
    bool playing = false;

    juce::Rectangle<int> rulerArea, laneArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoopGrid)
};

// Botao que entrega o .mid selecionado para a DAW por arraste externo.
class DragOutButton : public juce::TextButton
{
public:
    DragOutButton();

    std::function<juce::StringArray()> getFiles;

    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    bool dragging = false;
    bool awaitingDrop = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DragOutButton)
};

// Detalhe do loop: titulo, transporte, BPM, grade de passos e arraste.
class LoopDetail : public juce::Component
{
public:
    explicit LoopDetail (DrumforgeAudioProcessor& processor);

    void refresh();
    void tick();
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    void updateBpmAffordance();

    DrumforgeAudioProcessor& processor;

    LoopGrid grid;
    juce::TextButton playButton { "TOCAR" };
    juce::TextButton syncButton { "SYNC DAW" };
    MiniKnob bpmKnob { "BPM", 26.0f };
    DragOutButton dragButton;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bpmAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncAttachment;

    juce::String loopName;
    juce::String originText;
    juce::String statusText;
    juce::String lastPositionText { "--" };
    int totalHits = 0;
    int totalBars = 1;
    int lastBpmShown = -1;
    bool lastSynced = true;

    juce::Rectangle<int> titleArea, statusArea, metaArea, bpmArea, positionArea, hintArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoopDetail)
};

// Aba LOOPS MIDI: biblioteca a esquerda, loop selecionado a direita.
class LoopsPanel : public juce::Component,
                   public juce::ListBoxModel,
                   public juce::FileDragAndDropTarget
{
public:
    explicit LoopsPanel (DrumforgeAudioProcessor& processor);
    ~LoopsPanel() override;

    void refresh();
    void refreshStatus();

    void paint (juce::Graphics& g) override;
    void resized() override;

    int getNumRows() override;
    void paintListBoxItem (int rowNumber, juce::Graphics& g, int width, int height,
                           bool rowIsSelected) override;
    void selectedRowsChanged (int lastRowSelected) override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;
    void fileDragEnter (const juce::StringArray& files, int x, int y) override;
    void fileDragExit (const juce::StringArray& files) override;

private:
    void updateLibrary();
    void rebuildFilter();
    void selectActiveRow();
    void loadSelectedRow();
    void importFiles (const juce::StringArray& files);
    void chooseLoopFiles();
    void openLoopFolder();

    DrumforgeAudioProcessor& processor;

    forja::ui::ForjaCard libraryCard { 1, "BIBLIOTECA" };
    forja::ui::ForjaCard loopCard { 2, "LOOP MIDI", true };

    juce::TextEditor search;
    forja::ui::ForjaChoice packChoice { "PACOTE", {} };
    juce::ListBox list { "loops", this };
    juce::Label emptyHint;
    juce::Label libraryHint;
    juce::TextButton importButton { "IMPORTAR MIDI" };
    juce::TextButton folderButton { "ABRIR PASTA" };
    juce::TextButton refreshButton { "ATUALIZAR" };

    LoopDetail detail;

    std::vector<forja::drumforge::loops::Entry> library;
    std::vector<int> visibleRows;
    juce::StringArray packs;

    std::unique_ptr<juce::FileChooser> chooser;
    bool updatingSelection = false;
    bool dragActive = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoopsPanel)
};

} // namespace drumforgeui
