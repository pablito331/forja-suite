#include "PluginProcessor.h"

#include "PluginEditor.h"

using forja::drumforge::mapping::trimDb;

namespace
{

juce::String volumeText (float value)
{
    const auto db = trimDb (value);
    return (db > 0.0f ? "+" : "") + juce::String (db, 1) + " dB";
}

} // namespace

MinidrumAudioProcessor::MinidrumAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
                                .withOutput ("Main", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    volumeParam = apvts.getRawParameterValue ("volume");

    // Reap dos kits substituidos; 4 Hz igual ao do Drumforge.
    startTimerHz (4);
}

MinidrumAudioProcessor::~MinidrumAudioProcessor()
{
    stopTimer();
}

juce::AudioProcessorValueTreeState::ParameterLayout MinidrumAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "bypass", 1 },
                                                            "Bypass", false));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "volume", 1 }, "Volume",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f,
        juce::AudioParameterFloatAttributes()
            .withLabel ("volume")
            .withStringFromValueFunction ([] (float value, int) { return volumeText (value); })));

    return layout;
}

void MinidrumAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    drum.prepare (sampleRate, samplesPerBlock);

    // Mesma latencia medida no harness do Drumforge: o engine nao atrasa o bloco.
    setLatencySamples (0);
}

void MinidrumAudioProcessor::releaseResources()
{
}

bool MinidrumAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::disabled()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void MinidrumAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                           juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    buffer.clear();

    auto mainBus = getBusBuffer (buffer, false, 0);

    drum.setVolume (volumeParam->load (std::memory_order_relaxed));
    drum.process (midiMessages, mainBus, buffer.getNumSamples(), isBypassed());
}

bool MinidrumAudioProcessor::isBypassed() const
{
    return apvts.getRawParameterValue ("bypass")->load() > 0.5f;
}

juce::AudioProcessorParameter* MinidrumAudioProcessor::getBypassParameter() const
{
    return apvts.getParameter ("bypass");
}

void MinidrumAudioProcessor::loadKitFile (const juce::File& file)
{
    drum.loadKitFile (file, [this] (const juce::String&)
    {
        kitRevision.fetch_add (1, std::memory_order_relaxed);
    });
}

void MinidrumAudioProcessor::timerCallback()
{
    drum.reapRetiredKits();
}

void MinidrumAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();

    if (const auto kit = drum.getLoadedKit())
    {
        state.setProperty ("kitPath", kit->kit.sourceFile, nullptr);
        state.setProperty ("kitName", kit->kit.name, nullptr);
    }

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void MinidrumAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);

    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    apvts.replaceState (juce::ValueTree::fromXml (*xml));

    // O mini nao tem mesa: o kit reabre de onde estava na biblioteca
    // compartilhada com o Drumforge e traz a propia mix.
    const auto path = apvts.state.getProperty ("kitPath").toString();

    if (path.isNotEmpty() && juce::File (path).exists())
        loadKitFile (juce::File (path));
}

juce::AudioProcessorEditor* MinidrumAudioProcessor::createEditor()
{
    return new MinidrumAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MinidrumAudioProcessor();
}
