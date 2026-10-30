#include "ForjaEditorBase.h"

#include "ForjaTheme.h"

namespace forja::ui
{

ForjaEditorBase::ForjaEditorBase (juce::AudioProcessor& owner,
                                  juce::AudioProcessorValueTreeState& state,
                                  Identity identityToUse,
                                  bool compactLayout)
    : juce::AudioProcessorEditor (&owner),
      processor (owner),
      apvts (state),
      identity (std::move (identityToUse)),
      compact (compactLayout),
      topBar (compactLayout),
      statusBar (compactLayout)
{
    setLookAndFeel (&lookAndFeel);
    setOpaque (true);
    configureResizableEditor (*this, constrainer, getPreferredWidth(), getPreferredHeight());

    addAndMakeVisible (topBar);
    addAndMakeVisible (statusBar);

    topBar.setIdentity (identity.product, identity.family, identity.subtitle, identity.sourceChip);
    statusBar.setIdentity (identity.product, identity.statusSubtitle);
    statusBar.setFooter (identity.footer);

    infoPanel = std::make_unique<ForjaInfoPanel> (identity.info);
    addChildComponent (*infoPanel);
    infoPanel->onClosed = [this] { topBar.grabKeyboardFocus(); };

    topBar.onFundamentos = [this]
    {
        infoPanel->setBounds (0, 0, getPreferredWidth(), getPreferredHeight());
        infoPanel->showPanel();
        infoPanel->toFront (true);
    };

    lastBypass = apvts.getRawParameterValue ("bypass")->load() > 0.5f;
    topBar.setBypassed (lastBypass);

    lastSampleRate = processor.getSampleRate();
    lastBlockSize = processor.getBlockSize();
    statusBar.setMeta (lastSampleRate, lastBlockSize, metaValues);

    startTimerHz (30);
}

ForjaEditorBase::~ForjaEditorBase()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void ForjaEditorBase::paint (juce::Graphics& g)
{
    juce::ColourGradient background (juce::Colour (0xff1c212a), (float) getWidth() * 0.5f, 0.0f,
                                     colour::iron1, (float) getWidth() * 0.5f, (float) getHeight(),
                                     false);
    g.setGradientFill (background);
    g.fillAll();

    g.setColour (juce::Colour (0xff424954));
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 5.0f, 1.0f);
}

void ForjaEditorBase::resized()
{
    juce::Rectangle<int> content;

    if (compact)
    {
        topBar.setBounds (7, 7, 546, 44);
        statusBar.setBounds (7, 215, 546, 38);
        content = { 7, 59, 546, 148 };
    }
    else
    {
        topBar.setBounds (7, 7, 986, 49);
        statusBar.setBounds (7, 458, 986, 75);
        content = { 7, 63, 986, 388 };
    }

    if (infoPanel != nullptr)
        infoPanel->setBounds (0, 0, getPreferredWidth(), getPreferredHeight());

    layoutContent (content);

    applyCanvasScale (*this, getPreferredWidth(), getPreferredHeight());
}

void ForjaEditorBase::setBadge (BadgeKind kind, juce::String text, juce::String detail)
{
    statusBar.setBadge (kind, std::move (text), std::move (detail));
}

void ForjaEditorBase::setMetaValues (std::vector<std::pair<juce::String, juce::String>> values)
{
    metaValues = std::move (values);
    statusBar.setMeta (processor.getSampleRate(), processor.getBlockSize(), metaValues);
}

void ForjaEditorBase::refreshBypassChip()
{
    const bool bypassed = apvts.getRawParameterValue ("bypass")->load() > 0.5f;

    if (bypassed != lastBypass)
    {
        lastBypass = bypassed;
        topBar.setBypassed (bypassed);
    }
}

void ForjaEditorBase::timerCallback()
{
    refreshBypassChip();

    const double sampleRate = processor.getSampleRate();
    const int blockSize = processor.getBlockSize();

    if (sampleRate != lastSampleRate || blockSize != lastBlockSize)
    {
        lastSampleRate = sampleRate;
        lastBlockSize = blockSize;
        statusBar.setMeta (sampleRate, blockSize, metaValues);
    }

    updateTelemetry();
}

} // namespace forja::ui
