#include "PluginEditor.h"

#include "ForjaInfoText.h"
#include "PluginProcessor.h"
#include "canivete/BankLibrary.h"
#include "forja/ui/ForjaTheme.h"

namespace
{
juce::String u (const char* text)
{
    return forja::ui::utf8 (text);
}

constexpr int windowWidth = 560;
constexpr int windowHeight = 260;

juce::String volumeText (double value)
{
    const auto db = (static_cast<float> (value) - 0.5f) * 48.0f;
    return (db > 0.0f ? "+" : "") + juce::String (db, 1) + " dB";
}
} // namespace

CaniveteAudioProcessorEditor::CaniveteAudioProcessorEditor (CaniveteAudioProcessor& p)
    : ForjaEditorBase (
          p, p.apvts,
          { "CANIVETE",
            u ("PLAYER SF2/SF3"),
            "SOUNDFONT MULTI-TIMBRAL",
            "TINYSOUNDFONT MIT",
            u ("Bank da biblioteca \u00b7 16 canais GM"),
            u ("TinySoundFont v0.9 \u00b7 DSP MIT \u00b7 lat\u00eancia 0 amostras"),
            forja::canivete::makeForjaInfo() },
          true),
      processor (p),
      bankCard (1, u ("Bank"), true),
      outCard (2, u ("Sa\u00edda"), true),
      bankChoice (u ("BANK"), {}),
      instrumentChoice (u ("Instrumento"), {}),
      refreshButton (u ("ATUALIZAR")),
      volumeKnob ("VOLUME",
                  u ("trim do master"),
                  64.0f,
                  0.0, 1.0, 0.01, 0.5,
                  [] (double value) { return volumeText (value); },
                  forja::ui::ForjaKnob::Options { 120, true, true, true })
{
    addAndMakeVisible (bankCard);
    bankCard.addAndMakeVisible (bankChoice);
    bankCard.addAndMakeVisible (instrumentChoice);
    bankCard.addAndMakeVisible (refreshButton);
    bankCard.addAndMakeVisible (statusLabel);

    addAndMakeVisible (outCard);
    outCard.addAndMakeVisible (volumeKnob);
    outCard.addAndMakeVisible (outputMeter);
    outCard.addAndMakeVisible (bypassButton);

    rebuildBankChoice();
    rebuildPresetChoice();

    // Instrumento do canal melodic 0 pela lista do bank. O choice nao e um
    // parametro APVTS (a lista muda com cada bank e AudioParameterChoice congela
    // as choices na construcao): dirige direto o engine, como o filtro de
    // categoria do ForjaRack. Program change do host ainda assume o canal.
    instrumentChoice.getComboBox().onChange = [this]
    {
        if (const int id = instrumentChoice.getComboBox().getSelectedId(); id > 0)
            processor.setPresetIndex (id - 1);
    };

    refreshButton.setTooltip (u ("Re-varrer Documents/Forja/Canivete e atualizar a "
                                 "lista de banks sem reabrir o plugin"));
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

    volumeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "volume", volumeKnob.getSlider());

    bypassButton.setClickingTogglesState (true);
    bypassButton.setTooltip (u ("Bypass com rampa de 10 ms"));
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, "bypass", bypassButton);

    outputMeter.onClipReset = [this]
    {
        outputMeter.setClip (false);
    };

    refreshBankViews();

    setSize (windowWidth, windowHeight);
}

CaniveteAudioProcessorEditor::~CaniveteAudioProcessorEditor()
{
    bankAttachment.reset();   // o choice e dono do combo; o attachment primeiro
}

void CaniveteAudioProcessorEditor::rebuildBankChoice()
{
    // O attachment le o indice ao religar; derruba antes de mexer nos itens.
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

void CaniveteAudioProcessorEditor::rebuildPresetChoice()
{
    // Lista o preset do bank atual. O combo so recebe itens com
    // dontSendNotification, entao o onChange (instalado no construtor) nao
    // dispara durante a reconstrucao.
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

void CaniveteAudioProcessorEditor::layoutContent (juce::Rectangle<int> content)
{
    const auto banks = content.withWidth (content.getWidth() * 54 / 100);
    const auto saida = content.withTrimmedLeft (banks.getWidth() + 6);

    bankCard.setBounds (banks);
    outCard.setBounds (saida);

    const auto bankArea = bankCard.getContentBounds();

    bankChoice.setBounds (bankArea.getX() + 2, bankArea.getY() - 2,
                          bankArea.getWidth() - 4, 32);
    refreshButton.setBounds (bankArea.getX() + 2, bankChoice.getBottom() + 3,
                             bankArea.getWidth() * 2 / 5, 22);
    statusLabel.setBounds (refreshButton.getRight() + 6, bankChoice.getBottom() + 3,
                           bankArea.getRight() - refreshButton.getRight() - 8, 22);
    instrumentChoice.setBounds (bankArea.getX() + 2, statusLabel.getBottom() + 3,
                                bankArea.getWidth() - 4, 30);

    const auto outArea = outCard.getContentBounds();

    volumeKnob.setBounds (outArea.getX() - 4, outArea.getY() - 10, 120, 96);
    outputMeter.setBounds (volumeKnob.getRight() + 4, outArea.getY() - 6,
                           outArea.getRight() - volumeKnob.getRight() - 148, 94);
    bypassButton.setBounds (outArea.getRight() - 142, outArea.getBottom() - 26, 142, 26);
}

void CaniveteAudioProcessorEditor::refreshBankViews()
{
    statusLabel.setText (processor.getBankStatus(), juce::dontSendNotification);
}

void CaniveteAudioProcessorEditor::updateTelemetry()
{
    const auto meter = processor.getEngine().readMeter();

    outputMeter.setLevels (meter.peakLeft, meter.peakRight);
    outputMeter.setClip (meter.clip);

    const auto revision = processor.getBankRevision();

    if (revision != lastBankRevision)
    {
        lastBankRevision = revision;
        refreshBankViews();
        rebuildPresetChoice();
    }
    else if (processor.isLoadingBank())
    {
        statusLabel.setText (processor.getBankStatus(), juce::dontSendNotification);
    }
    else if (processor.hasBank())
    {
        // Bank novo sem subida de revisao (lista mudou por fora) ou program
        // change do host trocando o preset do canal 0: realinha o choice.
        if (processor.getNumPresets() != lastPresetCount)
            rebuildPresetChoice();

        auto& combo = instrumentChoice.getComboBox();
        const int selected = processor.getSelectedPresetIndex();

        if (selected >= 0 && selected < processor.getNumPresets()
            && combo.getSelectedId() != selected + 1)
            combo.setSelectedId (selected + 1, juce::dontSendNotification);
    }

    refreshBadge (meter.clip, processor.isBypassed(), processor.hasBank());

    setMetaValues ({ { u ("BANK"), processor.getBankName() },
                     { "VOLUME", volumeText (processor.apvts.getRawParameterValue ("volume")->load()) } });
}

void CaniveteAudioProcessorEditor::refreshBadge (bool clipped, bool bypassed, bool hasBank)
{
    auto kind = forja::ui::BadgeKind::ready;
    juce::String text ("PRONTO");
    juce::String detail;

    if (clipped)
    {
        kind = forja::ui::BadgeKind::clip;
        text = "CLIP";
        detail = u ("Sa\u00edda acima de 0 dBFS \u00b7 baixe o VOLUME.");
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
        text = "SEM BANK";
        detail = u ("Nenhum bank carregado \u00b7 deposite um .sf2 em "
                    "Documentos/Forja/Canivete e toque ATUALIZAR.");
    }
    else
    {
        detail = u ("Tocando \u00b7 16 canais GM; INSTRUMENTO escolhe o preset do "
                    "canal 1.");
    }

    setBadge (kind, text, detail);
}
