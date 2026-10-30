#include "PluginProcessor.h"

#include "PluginEditor.h"

BrasaAudioProcessor::BrasaAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
                                .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout BrasaAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "input", 1 }, "Entrada",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f,
        juce::AudioParameterFloatAttributes()
            .withLabel ("input")
            .withStringFromValueFunction ([] (float value, int)
            {
                return juce::String (value, 2);
            })));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "tube", 1 }, juce::String::fromUTF8 ("Válvula"),
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f,
        juce::AudioParameterFloatAttributes()
            .withLabel ("drive")
            .withStringFromValueFunction ([] (float value, int)
            {
                return juce::String (value, 2);
            })));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "output", 1 }, juce::String::fromUTF8 ("Saída"),
        juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel ("dB")
            .withStringFromValueFunction ([] (float value, int)
            {
                return juce::String (value > 0.0f ? "+" : "") + juce::String (value, 1) + " dB";
            })));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "bypass", 1 }, "Bypass", false));

    return layout;
}

void BrasaAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
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

    setLatencySamples (0);
}

void BrasaAudioProcessor::releaseResources()
{
    monoScratch.setSize (0, 0);
}

bool BrasaAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainIn = layouts.getMainInputChannelSet();
    const auto mainOut = layouts.getMainOutputChannelSet();

    if (mainIn.isDisabled() || mainOut.isDisabled() || mainIn != mainOut)
        return false;

    return mainOut == juce::AudioChannelSet::mono()
        || mainOut == juce::AudioChannelSet::stereo();
}

void BrasaAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

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

    forja::BrasaDSP::Parameters parameters;
    parameters.input = apvts.getRawParameterValue ("input")->load();
    parameters.tube = apvts.getRawParameterValue ("tube")->load();
    parameters.outputDb = apvts.getRawParameterValue ("output")->load();
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

void BrasaAudioProcessor::publishMax (std::atomic<float>& target, float value) noexcept
{
    float previous = target.load (std::memory_order_relaxed);
    while (value > previous
           && ! target.compare_exchange_weak (previous, value, std::memory_order_relaxed))
    {
    }
}

BrasaAudioProcessor::MeterSnapshot BrasaAudioProcessor::readMeters()
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

void BrasaAudioProcessor::resetClip()
{
    clipResetRequested.store (true, std::memory_order_release);
    meterInClip.store (false, std::memory_order_relaxed);
    meterOutClip.store (false, std::memory_order_relaxed);
}

bool BrasaAudioProcessor::isBypassed() const
{
    return apvts.getRawParameterValue ("bypass")->load() > 0.5f;
}

juce::AudioProcessorParameter* BrasaAudioProcessor::getBypassParameter() const
{
    return apvts.getParameter ("bypass");
}

void BrasaAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.state.createXml())
        copyXmlToBinary (*xml, destData);
}

void BrasaAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* BrasaAudioProcessor::createEditor()
{
    return new BrasaAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BrasaAudioProcessor();
}
