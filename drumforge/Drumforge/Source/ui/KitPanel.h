#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <functional>
#include <memory>

#include "engine/DrumEngine.h"
#include "engine/Kit.h"
#include "forja/ui/ForjaCard.h"

class DrumforgeAudioProcessor;

namespace drumforgeui
{

// Detalhe do kit: nome, numeros e o mapa dos 16 pads (clique dispara o sample).
class KitDetail : public juce::Component
{
public:
    KitDetail();

    void setKit (const forja::drumforge::Kit& kit);
    void setImage (const juce::Image& image);
    void setStatus (const juce::String& text, bool isLoading, int loadProgress);
    void setLevels (const std::array<forja::drumforge::DrumEngine::PadMeter,
                                     forja::drumforge::kNumPads>& meters);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

    std::function<void (int padIndex, float velocity)> onTrigger;
    std::function<void()> onOpenMixer;
    std::function<void()> onSave;

private:
    void paintKitArt (juce::Graphics& g, juce::Rectangle<int> area) const;
    void paintKitPlaceholder (juce::Graphics& g, juce::Rectangle<float> bounds) const;

    juce::String kitName { "Nenhum kit" };
    juce::String kitDescription;
    juce::String status { "Arraste uma pasta de samples, um .dfkit ou um drumkit.txt." };
    juce::Image preview;
    bool loading = false;
    int progress = 0;

    std::array<juce::String, forja::drumforge::kNumPads> padNames;
    std::array<int, forja::drumforge::kNumPads> padSamples {};
    std::array<float, forja::drumforge::kNumPads> padLevels {};

    int padsWithAudio = 0;
    int totalSamples = 0;
    int layerCount = 0;
    int roundRobinCount = 0;

    std::array<juce::Rectangle<int>, forja::drumforge::kNumPads> padCells;
    juce::Rectangle<int> nameArea, statusArea, statsArea, artArea, hintArea, progressArea;

    juce::TextButton mixerButton { "ABRIR NO MIXER" };
    juce::TextButton saveButton { "SALVAR KIT" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KitDetail)
};

// Aba KITS: biblioteca de kits salvos a esquerda, detalhe do kit a direita.
class KitPanel : public juce::Component,
                 public juce::ListBoxModel,
                 public juce::FileDragAndDropTarget
{
public:
    explicit KitPanel (DrumforgeAudioProcessor& processor);
    ~KitPanel() override;

    void refresh();
    void refreshStatus();
    void setLevels (const std::array<forja::drumforge::DrumEngine::PadMeter,
                                     forja::drumforge::kNumPads>& meters);

    std::function<void()> onOpenMixer;

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
    void loadSelectedRow();
    void chooseKitFile();
    void chooseImportFolder();
    void chooseSaveAs();

    DrumforgeAudioProcessor& processor;

    forja::ui::ForjaCard libraryCard { 1, "BIBLIOTECA" };
    forja::ui::ForjaCard kitCard { 2, "KIT", true };

    juce::ListBox kitList { "kits", this };
    juce::Label libraryHint;
    juce::Label emptyHint;
    juce::TextButton importButton { "IMPORTAR PASTA" };
    juce::TextButton openButton { "ABRIR KIT..." };
    juce::TextButton refreshButton { "ATUALIZAR" };

    juce::Array<juce::File> libraryKits;
    KitDetail detail;

    std::unique_ptr<juce::FileChooser> chooser;
    bool updatingSelection = false;
    bool dragActive = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KitPanel)
};

} // namespace drumforgeui
