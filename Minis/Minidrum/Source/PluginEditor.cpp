#include "PluginEditor.h"

#include "ForjaInfoText.h"
#include "PluginProcessor.h"
#include "minidrum/KitLibrary.h"
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
    const auto db = forja::drumforge::mapping::trimDb ((float) value);
    return (db > 0.0f ? "+" : "") + juce::String (db, 1) + " dB";
}
} // namespace

void KitArt::setKit (std::shared_ptr<const forja::drumforge::LoadedKit> kit)
{
    shown = std::move (kit);
    repaint();
}

void KitArt::paint (juce::Graphics& g)
{
    const auto area = getLocalBounds().toFloat();

    g.setColour (juce::Colour (0xff0d1116));
    g.fillRoundedRectangle (area, 6.0f);

    if (shown != nullptr && shown->preview.isValid())
    {
        const auto fit = area.toNearestInt();
        g.drawImageWithin (shown->preview, fit.getX(), fit.getY(),
                           fit.getWidth(), fit.getHeight(),
                           juce::RectanglePlacement::centred);
        return;
    }

    // Sem foto: aro + pele, a marca d'agua de instrumento do rack.
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff18202a), 0.0f, area.getY(),
                                             juce::Colour (0xff10151b), 0.0f, area.getBottom(),
                                             false));
    g.fillRoundedRectangle (area, 6.0f);

    const auto centre = area.getCentre();
    const auto radius = juce::jmin (area.getWidth(), area.getHeight()) * 0.32f;

    g.setColour (juce::Colour (0xff2a333d));
    g.drawRoundedRectangle (area.reduced (4.0f), 5.0f, 1.0f);

    g.setColour (juce::Colour (0xff3c4854));
    g.drawEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre), 1.5f);
    g.setColour (juce::Colour (0xff242c35));
    g.drawEllipse (juce::Rectangle<float> (radius * 1.3f, radius * 1.3f).withCentre (centre), 1.0f);

    const juce::String name = shown != nullptr ? shown->kit.name : u ("Nenhum kit");

    g.setFont (forja::ui::uiFont (9.0f, juce::Font::bold));
    g.setColour (juce::Colour (0xff8a97a5));

    const auto nameArea = area.toNearestInt();
    g.drawText (name.toUpperCase(),
                nameArea.withTrimmedTop (nameArea.getHeight() - 24).withTrimmedBottom (4),
                juce::Justification::centred, true);
}

MinidrumAudioProcessorEditor::MinidrumAudioProcessorEditor (MinidrumAudioProcessor& p)
    : ForjaEditorBase (
          p, p.apvts,
          { "MINIDRUM",
            u ("DE KITS"),
            "TOCADOR DE KIT",
            "DRUMFORGE ENGINE",
            u ("Kit premixado \u00b7 engine Drumforge"),
            u ("engine Drumforge \u00b7 DSP Forja \u00b7 lat\u00eancia 0 amostras"),
            forja::minidrum::makeForjaInfo() },
          true),
      processor (p),
      kitCard (1, u ("Kit"), true),
      outCard (2, u ("Sa\u00edda"), true),
      loadButton (u ("PUXAR KIT")),
      volumeKnob ("VOLUME",
                  u ("trim do master"),
                  64.0f,
                  0.0, 1.0, 0.01, 0.5,
                  [] (double value) { return volumeText (value); },
                  forja::ui::ForjaKnob::Options { 120, true, true, true })
{
    addAndMakeVisible (kitCard);
    kitCard.addAndMakeVisible (kitArt);
    kitCard.addAndMakeVisible (loadButton);
    kitCard.addAndMakeVisible (statusLabel);

    addAndMakeVisible (outCard);
    outCard.addAndMakeVisible (volumeKnob);
    outCard.addAndMakeVisible (outputMeter);
    outCard.addAndMakeVisible (bypassButton);

    loadButton.setTooltip (u ("Abrir kit da biblioteca compartilhada com o Drumforge "
                              "(Documentos/Forja/Drumforge/Kits)"));
    loadButton.onClick = [this] { chooseKitFile(); };

    statusLabel.setJustificationType (juce::Justification::centredLeft);
    statusLabel.setFont (forja::ui::uiFont (8.5f));
    statusLabel.setColour (juce::Label::textColourId, juce::Colour (0xff8a97a5));
    statusLabel.setInterceptsMouseClicks (false, false);

    volumeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "volume", volumeKnob.getSlider());

    bypassButton.setClickingTogglesState (true);
    bypassButton.setTooltip ("Bypass com rampa de 10 ms");
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, "bypass", bypassButton);

    outputMeter.onClipReset = [this]
    {
        outputMeter.setClip (false);
    };

    refreshKitViews();

    setSize (windowWidth, windowHeight);
}

MinidrumAudioProcessorEditor::~MinidrumAudioProcessorEditor()
{
    chooser.reset();
}

void MinidrumAudioProcessorEditor::layoutContent (juce::Rectangle<int> content)
{
    const auto kits = content.withWidth (content.getWidth() * 54 / 100)
                              .reduced (0, 0);
    const auto saida = content.withTrimmedLeft (kits.getWidth() + 6);

    kitCard.setBounds (kits);
    outCard.setBounds (saida);

    const auto kitArea = kitCard.getContentBounds();

    kitArt.setBounds (kitArea.getX() + 2, kitArea.getY() - 4, kitArea.getWidth() - 4, 74);
    loadButton.setBounds (kitArea.getX() + 2, kitArt.getBottom() + 4,
                          kitArea.getWidth() * 5 / 8, 24);
    statusLabel.setBounds (loadButton.getRight() + 6, kitArt.getBottom() + 4,
                           kitArea.getRight() - loadButton.getRight() - 8, 24);

    const auto outArea = outCard.getContentBounds();

    volumeKnob.setBounds (outArea.getX() - 4, outArea.getY() - 10, 120, 96);
    outputMeter.setBounds (volumeKnob.getRight() + 4, outArea.getY() - 6,
                           outArea.getRight() - volumeKnob.getRight() - 148, 94);
    bypassButton.setBounds (outArea.getRight() - 142, outArea.getBottom() - 26, 142, 26);
}

void MinidrumAudioProcessorEditor::chooseKitFile()
{
    auto start = forja::minidrum::libraryDirectory();

    if (! start.isDirectory())
        start = start.getParentDirectory();

    if (! start.isDirectory())
        start = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);

    chooser = std::make_unique<juce::FileChooser> (
        u ("Puxar kit da biblioteca do Drumforge"), start,
        "*.dfkit;*.txt;*.labooh");

    juce::Component::SafePointer<MinidrumAudioProcessorEditor> safe (this);

    chooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectFiles,
                          [safe] (const juce::FileChooser& result)
                          {
                              if (safe == nullptr)
                                  return;

                              const auto file = result.getResult();

                              if (file.existsAsFile() || file.isDirectory())
                                  safe->processor.loadKitFile (file);
                          });
}

void MinidrumAudioProcessorEditor::refreshKitViews()
{
    kitArt.setKit (processor.getLoadedKit());
    statusLabel.setText (processor.getKitStatus(), juce::dontSendNotification);
}

void MinidrumAudioProcessorEditor::updateTelemetry()
{
    const auto meter = processor.getDrum().readMasterMeter();

    outputMeter.setLevels (meter.peakLeft, meter.peakRight);
    outputMeter.setClip (meter.clip);

    const auto revision = processor.getKitRevision();

    if (revision != lastKitRevision)
    {
        lastKitRevision = revision;
        refreshKitViews();
    }
    else if (processor.isLoadingKit())
    {
        statusLabel.setText (processor.getKitStatus() + " "
                             + juce::String (processor.getLoadProgress()) + "%",
                             juce::dontSendNotification);
    }

    refreshBadge (meter.clip, processor.isBypassed(), processor.getLoadedKit() != nullptr);

    setMetaValues ({ { u ("KIT"), processor.getKitName() },
                     { "VOLUME", volumeText (processor.apvts.getRawParameterValue ("volume")->load()) } });
}

void MinidrumAudioProcessorEditor::refreshBadge (bool clipped, bool bypassed, bool hasKit)
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
    else if (! hasKit)
    {
        kind = forja::ui::BadgeKind::silent;
        text = "SEM KIT";
        detail = u ("Nenhum kit carregado \u00b7 toque PUXAR KIT.");
    }
    else
    {
        detail = u ("Tocando \u00b7 a mix gravada no kit dirige os pads.");
    }

    setBadge (kind, text, detail);
}
