#include "PluginProcessor.h"

#include "PluginEditor.h"

namespace
{
juce::AudioParameterFloat* makeUnitParameter (const juce::String& id,
                                              const juce::String& name,
                                              float defaultValue,
                                              float step = 0.01f)
{
    return new juce::AudioParameterFloat (
        juce::ParameterID { id, 1 }, name,
        juce::NormalisableRange<float> (0.0f, 1.0f, step), defaultValue,
        juce::AudioParameterFloatAttributes()
            .withLabel (id)
            .withStringFromValueFunction ([] (float value, int)
            {
                return juce::String (value, 2);
            }));
}

juce::AudioParameterFloat* makeDivisionParameter()
{
    return new juce::AudioParameterFloat (
        juce::ParameterID { "division", 1 }, "Divisao",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.2f), 0.4f,
        juce::AudioParameterFloatAttributes()
            .withLabel ("division")
            .withStringFromValueFunction ([] (float value, int)
            {
                return juce::String (forja::dsp::syncDivisionLabels[
                    forja::dsp::syncDivisionIndex (value)]);
            }));
}
} // namespace

EcoAudioProcessor::EcoAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
                                .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout EcoAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // 17 discrete spaces, still a 0-1 parameter: index = int(value * 16.999).
    layout.add (std::unique_ptr<juce::AudioParameterFloat> (
        makeUnitParameter ("select", "Espaco", 0.5f, 1.0f / 16.0f)));
    layout.add (std::unique_ptr<juce::AudioParameterFloat> (
        makeUnitParameter ("regen", "Regeneracao", 0.5f)));
    layout.add (std::unique_ptr<juce::AudioParameterFloat> (
        makeUnitParameter ("speed", "Velocidade", 0.5f)));
    layout.add (std::unique_ptr<juce::AudioParameterFloat> (
        makeUnitParameter ("mix", "Mistura", 0.5f)));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "sync", 1 }, "Sync do host", false));
    layout.add (std::unique_ptr<juce::AudioParameterFloat> (makeDivisionParameter()));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "bypass", 1 }, "Bypass", false));

    return layout;
}

void EcoAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    dsp.prepare (sampleRate);
    monoScratch.setSize (1, juce::jmax (1, samplesPerBlock), false, false, true);

    meterInL.store (0.0f);
    meterInR.store (0.0f);
    meterOutL.store (0.0f);
    meterOutR.store (0.0f);
    meterInClip.store (false);
    meterOutClip.store (false);
    clipResetRequested.store (false);

    setLatencySamples (dsp.latencySamples());
}

void EcoAudioProcessor::releaseResources()
{
    monoScratch.setSize (0, 0);
}

bool EcoAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainIn = layouts.getMainInputChannelSet();
    const auto mainOut = layouts.getMainOutputChannelSet();

    if (mainIn.isDisabled() || mainOut.isDisabled() || mainIn != mainOut)
        return false;

    return mainOut == juce::AudioChannelSet::mono()
        || mainOut == juce::AudioChannelSet::stereo();
}

void EcoAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    double bpm = 0.0;
    if (auto* hostPlayHead = getPlayHead())
        if (const auto position = hostPlayHead->getPosition())
            if (const auto tempo = position->getBpm())
                bpm = *tempo;
    hostBpm.store (bpm, std::memory_order_relaxed);

    const int numSamples = buffer.getNumSamples();
    const int numInputChannels = getTotalNumInputChannels();

    for (int channel = numInputChannels; channel < getTotalNumOutputChannels(); ++channel)
        buffer.clear (channel, 0, numSamples);

    if (clipResetRequested.exchange (false, std::memory_order_acquire))
    {
        dsp.resetClip();
        meterInClip.store (false, std::memory_order_relaxed);
        meterOutClip.store (false, std::memory_order_relaxed);
    }

    if (numInputChannels <= 0 || numSamples <= 0)
        return;

    forja::dsp::EcoDSP::Parameters parameters;
    parameters.select = apvts.getRawParameterValue ("select")->load();
    parameters.regen = apvts.getRawParameterValue ("regen")->load();
    parameters.speed = apvts.getRawParameterValue ("speed")->load();
    parameters.dryWet = apvts.getRawParameterValue ("mix")->load();
    parameters.sync = isSyncEnabled();
    parameters.division = apvts.getRawParameterValue ("division")->load();
    parameters.hostBpm = hostTempo();
    parameters.bypass = isBypassed();
    dsp.setParameters (parameters);

    if (numInputChannels == 1)
    {
        if (monoScratch.getNumSamples() < numSamples)
            monoScratch.setSize (1, numSamples, false, false, true);

        monoScratch.copyFrom (0, 0, buffer, 0, 0, numSamples);
        dsp.process (buffer.getWritePointer (0), monoScratch.getWritePointer (0), numSamples);
    }
    else
    {
        dsp.process (buffer.getWritePointer (0), buffer.getWritePointer (1), numSamples);
    }

    const auto frame = dsp.readMeters (true);
    publishMax (meterInL, frame.inPeakL);
    publishMax (meterInR, frame.inPeakR);
    publishMax (meterOutL, frame.outPeakL);
    publishMax (meterOutR, frame.outPeakR);

    if (frame.inClip)
        meterInClip.store (true, std::memory_order_relaxed);
    if (frame.outClip)
        meterOutClip.store (true, std::memory_order_relaxed);
}

void EcoAudioProcessor::publishMax (std::atomic<float>& target, float value) noexcept
{
    float previous = target.load (std::memory_order_relaxed);
    while (value > previous
           && ! target.compare_exchange_weak (previous, value, std::memory_order_relaxed))
    {
    }
}

EcoAudioProcessor::MeterSnapshot EcoAudioProcessor::readMeters()
{
    MeterSnapshot snapshot;
    snapshot.inPeakL = meterInL.exchange (0.0f, std::memory_order_relaxed);
    snapshot.inPeakR = meterInR.exchange (0.0f, std::memory_order_relaxed);
    snapshot.outPeakL = meterOutL.exchange (0.0f, std::memory_order_relaxed);
    snapshot.outPeakR = meterOutR.exchange (0.0f, std::memory_order_relaxed);
    snapshot.inClip = meterInClip.load (std::memory_order_relaxed);
    snapshot.outClip = meterOutClip.load (std::memory_order_relaxed);
    return snapshot;
}

void EcoAudioProcessor::resetClip()
{
    clipResetRequested.store (true, std::memory_order_release);
    meterInClip.store (false, std::memory_order_relaxed);
    meterOutClip.store (false, std::memory_order_relaxed);
}

bool EcoAudioProcessor::isBypassed() const
{
    return apvts.getRawParameterValue ("bypass")->load() > 0.5f;
}

int EcoAudioProcessor::selectedSpace() const
{
    const float value = apvts.getRawParameterValue ("select")->load();
    return juce::jlimit (0, forja::dsp::EcoDSP::spaceCount() - 1, (int) (value * 16.999f));
}

bool EcoAudioProcessor::isSyncEnabled() const
{
    return apvts.getRawParameterValue ("sync")->load() > 0.5f;
}

double EcoAudioProcessor::syncDelaySeconds() const
{
    const double sampleRate = getSampleRate() > 0.0 ? getSampleRate() : 48000.0;
    return forja::dsp::EcoDSP::resolveDelaySeconds (
        apvts.getRawParameterValue ("speed")->load(),
        isSyncEnabled(),
        apvts.getRawParameterValue ("division")->load(),
        hostTempo(), sampleRate, selectedSpace());
}

juce::AudioProcessorParameter* EcoAudioProcessor::getBypassParameter() const
{
    return apvts.getParameter ("bypass");
}

void EcoAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.state.createXml())
        copyXmlToBinary (*xml, destData);
}

void EcoAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* EcoAudioProcessor::createEditor()
{
    return new EcoAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EcoAudioProcessor();
}
