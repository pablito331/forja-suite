#include "PluginProcessor.h"

#include "PluginEditor.h"

namespace
{
juce::AudioParameterFloat* makeUnitParameter (const juce::String& id,
                                              const juce::String& name,
                                              float defaultValue)
{
    return new juce::AudioParameterFloat (
        juce::ParameterID { id, 1 }, name,
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), defaultValue,
        juce::AudioParameterFloatAttributes()
            .withLabel (id)
            .withStringFromValueFunction ([] (float value, int)
            {
                return juce::String (value, 2);
            }));
}
} // namespace

LimaAudioProcessor::LimaAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
                                .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout LimaAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::unique_ptr<juce::AudioParameterFloat> (
        makeUnitParameter ("high", "Banda alta", 0.5f)));
    layout.add (std::unique_ptr<juce::AudioParameterFloat> (
        makeUnitParameter ("highMid", "Banda media alta", 0.5f)));
    layout.add (std::unique_ptr<juce::AudioParameterFloat> (
        makeUnitParameter ("lowMid", "Banda media baixa", 0.5f)));
    layout.add (std::unique_ptr<juce::AudioParameterFloat> (
        makeUnitParameter ("bass", "Banda grave", 0.5f)));
    layout.add (std::unique_ptr<juce::AudioParameterFloat> (
        makeUnitParameter ("highFreq", "Frequencia alta", 0.5f)));
    layout.add (std::unique_ptr<juce::AudioParameterFloat> (
        makeUnitParameter ("highMidFreq", "Frequencia media alta", 0.5f)));
    layout.add (std::unique_ptr<juce::AudioParameterFloat> (
        makeUnitParameter ("lowMidFreq", "Frequencia media baixa", 0.5f)));
    layout.add (std::unique_ptr<juce::AudioParameterFloat> (
        makeUnitParameter ("bassFreq", "Frequencia grave", 0.5f)));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "bypass", 1 }, "Bypass", false));

    return layout;
}

void LimaAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    dsp.prepare (sampleRate);
    analyser.prepare (sampleRate);
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

void LimaAudioProcessor::releaseResources()
{
    monoScratch.setSize (0, 0);
}

bool LimaAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainIn = layouts.getMainInputChannelSet();
    const auto mainOut = layouts.getMainOutputChannelSet();

    if (mainIn.isDisabled() || mainOut.isDisabled() || mainIn != mainOut)
        return false;

    return mainOut == juce::AudioChannelSet::mono()
        || mainOut == juce::AudioChannelSet::stereo();
}

void LimaAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
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

    analyser.pushSamples (buffer.getArrayOfReadPointers(), numInputChannels, numSamples);

    forja::dsp::LimaDSP::Parameters parameters;
    parameters.high = apvts.getRawParameterValue ("high")->load();
    parameters.highMid = apvts.getRawParameterValue ("highMid")->load();
    parameters.lowMid = apvts.getRawParameterValue ("lowMid")->load();
    parameters.bass = apvts.getRawParameterValue ("bass")->load();
    parameters.highFreq = apvts.getRawParameterValue ("highFreq")->load();
    parameters.highMidFreq = apvts.getRawParameterValue ("highMidFreq")->load();
    parameters.lowMidFreq = apvts.getRawParameterValue ("lowMidFreq")->load();
    parameters.bassFreq = apvts.getRawParameterValue ("bassFreq")->load();
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

void LimaAudioProcessor::publishMax (std::atomic<float>& target, float value) noexcept
{
    float previous = target.load (std::memory_order_relaxed);
    while (value > previous
           && ! target.compare_exchange_weak (previous, value, std::memory_order_relaxed))
    {
    }
}

LimaAudioProcessor::MeterSnapshot LimaAudioProcessor::readMeters()
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

void LimaAudioProcessor::resetClip()
{
    clipResetRequested.store (true, std::memory_order_release);
    meterInClip.store (false, std::memory_order_relaxed);
    meterOutClip.store (false, std::memory_order_relaxed);
}

bool LimaAudioProcessor::isBypassed() const
{
    return apvts.getRawParameterValue ("bypass")->load() > 0.5f;
}

juce::AudioProcessorParameter* LimaAudioProcessor::getBypassParameter() const
{
    return apvts.getParameter ("bypass");
}

void LimaAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.state.createXml())
        copyXmlToBinary (*xml, destData);
}

void LimaAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* LimaAudioProcessor::createEditor()
{
    return new LimaAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LimaAudioProcessor();
}
