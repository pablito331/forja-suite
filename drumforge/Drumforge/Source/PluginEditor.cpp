#include "PluginEditor.h"

#include "ForjaInfoText.h"
#include "PluginProcessor.h"
#include "forja/ui/ForjaTheme.h"
#include "ui/KitPanel.h"
#include "ui/LoopsPanel.h"
#include "ui/MixerPanel.h"

using forja::drumforge::kNumPads;

namespace
{
constexpr int windowWidth = 1000;
constexpr int windowHeight = 540;
constexpr int tabRowHeight = 24;

juce::String u (const char* text)
{
    return forja::ui::utf8 (text);
}
} // namespace

DrumforgeAudioProcessorEditor::DrumforgeAudioProcessorEditor (DrumforgeAudioProcessor& owner)
    : ForjaEditorBase (
          owner, owner.apvts,
          { "DRUMFORGE",
            u ("SAMPLER DE BATERIA"),
            "16 PADS / MIXER / MULTI-OUT",
            "FORJA DSP",
            u ("Sampler de bateria \u00b7 engine Forja"),
            u ("Engine Forja \u00b7 FX Airwindows MIT \u00b7 kits drumlabooh \u00b7 "
               "lat\u00eancia 0 amostras"),
            forja::drumforge::makeForjaInfo() }),
      processor (owner)
{
    kitPanel = std::make_unique<drumforgeui::KitPanel> (processor);
    mixerPanel = std::make_unique<drumforgeui::MixerPanel> (processor);
    loopsPanel = std::make_unique<drumforgeui::LoopsPanel> (processor);

    kitPanel->onOpenMixer = [this] { setActiveTab (1); };

    addAndMakeVisible (*kitPanel);
    addChildComponent (*mixerPanel);
    addChildComponent (*loopsPanel);

    for (auto* tab : { &kitsTab, &mixerTab, &loopsTab })
    {
        tab->setClickingTogglesState (true);
        tab->setRadioGroupId (0x4d46);
        tab->setWantsKeyboardFocus (false);
        addAndMakeVisible (*tab);
    }

    kitsTab.onClick = [this] { setActiveTab (0); };
    mixerTab.onClick = [this] { setActiveTab (1); };
    loopsTab.onClick = [this] { setActiveTab (2); };

    setActiveTab (0);
    refreshKit();

    setSize (windowWidth, windowHeight);
}

DrumforgeAudioProcessorEditor::~DrumforgeAudioProcessorEditor() = default;

void DrumforgeAudioProcessorEditor::layoutContent (juce::Rectangle<int> content)
{
    auto tabs = content.removeFromTop (tabRowHeight);

    kitsTab.setBounds (tabs.removeFromLeft (92));
    tabs.removeFromLeft (6);
    mixerTab.setBounds (tabs.removeFromLeft (92));
    tabs.removeFromLeft (6);
    loopsTab.setBounds (tabs.removeFromLeft (104));

    content.removeFromTop (6);

    kitPanel->setBounds (content);
    mixerPanel->setBounds (content);
    loopsPanel->setBounds (content);
}

void DrumforgeAudioProcessorEditor::setActiveTab (int tabIndex)
{
    activeTab = juce::jlimit (0, 2, tabIndex);

    kitsTab.setToggleState (activeTab == 0, juce::dontSendNotification);
    mixerTab.setToggleState (activeTab == 1, juce::dontSendNotification);
    loopsTab.setToggleState (activeTab == 2, juce::dontSendNotification);

    kitPanel->setVisible (activeTab == 0);
    mixerPanel->setVisible (activeTab == 1);
    loopsPanel->setVisible (activeTab == 2);

    // A biblioteca e relida ao abrir a aba: cobre .mid copiado por fora do plugin.
    if (activeTab == 2 && loopsPanel != nullptr)
        loopsPanel->refresh();
}

void DrumforgeAudioProcessorEditor::refreshKit()
{
    lastKitRevision = processor.getKitRevision();

    kitPanel->refresh();
    mixerPanel->refreshNames();
    loopsPanel->refresh();

    auto loaded = processor.getLoadedKit();
    const auto name = loaded != nullptr ? loaded->kit.name : juce::String ("Nenhum kit");
    const int pads = loaded != nullptr ? loaded->padsWithAudio : 0;
    const int samples = loaded != nullptr ? loaded->totalSamples : 0;

    setMetaValues ({
        { "kit", name },
        { "pads", juce::String (pads) + "/" + juce::String (kNumPads) },
        { "amostras", juce::String (samples) }
    });
}

void DrumforgeAudioProcessorEditor::updateTelemetry()
{
    if (processor.getKitRevision() != lastKitRevision)
        refreshKit();

    std::array<forja::drumforge::DrumEngine::PadMeter, kNumPads> padMeters;
    bool padClipped = false;

    for (int pad = 0; pad < kNumPads; ++pad)
    {
        padMeters[(size_t) pad] = processor.getEngine().readPadMeter (pad);

        if (padMeters[(size_t) pad].clip)
            padClipped = true;
    }

    const auto masterMeter = processor.getEngine().readMasterMeter();

    if (activeTab == 0)
        kitPanel->setLevels (padMeters);
    else if (activeTab == 1)
        mixerPanel->setMeters (padMeters, masterMeter);
    else
        loopsPanel->refreshStatus();

    kitPanel->refreshStatus();

    const double now = juce::Time::getMillisecondCounterHiRes();

    if (padClipped || masterMeter.clip)
        clipHoldUntilMs = now + 1200.0;

    const bool clipped = now < clipHoldUntilMs;
    const bool bypassed = processor.apvts.getRawParameterValue ("bypass")->load() > 0.5f;
    const bool loading = processor.isLoadingKit();
    const int voices = processor.getEngine().activeVoices();
    auto loaded = processor.getLoadedKit();
    const bool hasKit = loaded != nullptr && loaded->padsWithAudio > 0;

    auto kind = forja::ui::BadgeKind::ready;
    juce::String text ("PRONTO");
    juce::String detail;

    if (bypassed)
    {
        kind = forja::ui::BadgeKind::bypass;
        text = "BYPASS";
        detail = u ("Bypass do host \u00b7 sa\u00edda com rampa de 10 ms.");
    }
    else if (clipped)
    {
        kind = forja::ui::BadgeKind::clip;
        text = "CLIP";
        detail = u ("Sa\u00edda acima de 0 dBFS \u00b7 reduza o fader ou o trim.");
    }
    else if (loading)
    {
        kind = forja::ui::BadgeKind::ready;
        text = "CARREGANDO";
        detail = u ("Lendo samples \u00b7 ") + juce::String (processor.getLoadProgress()) + "%";
    }
    else if (! hasKit)
    {
        kind = forja::ui::BadgeKind::silent;
        text = "SEM KIT";
        detail = u ("Importe uma pasta de samples ou abra um drumkit.txt.");
    }
    else if (voices > 0)
    {
        text = "TOCANDO";
        detail = juce::String (voices) + u (" voz(es) ativa(s) \u00b7 ")
               + juce::String (loaded->padsWithAudio) + u (" pads com \u00e1udio.");
    }
    else
    {
        detail = juce::String (loaded->padsWithAudio) + u (" pads com \u00e1udio \u00b7 ")
               + juce::String (loaded->totalSamples) + u (" samples carregados.");
    }

    setBadge (kind, text, detail);
}
