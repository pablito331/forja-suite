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

CinzelAudioProcessor::CinzelAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
                                .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout CinzelAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::unique_ptr<juce::AudioParameterFloat> (
        makeUnitParameter ("highFreq", "Frequencia alta", 0.5f)));
    layout.add (std::unique_ptr<juce::AudioParameterFloat> (
        makeUnitParameter ("highGain", "Ganho alto", 0.5f)));
    layout.add (std::unique_ptr<juce::AudioParameterFloat> (
        makeUnitParameter ("highReso", "Ressonancia alta", 0.5f)));
    layout.add (std::unique_ptr<juce::AudioParameterFloat> (
        makeUnitParameter ("highMidFreq", "Frequencia media alta", 0.5f)));
    layout.add (std::unique_ptr<juce::AudioParameterFloat> (
        makeUnitParameter ("highMidGain", "Ganho medio alto", 0.5f)));
    layout.add (std::unique_ptr<juce::AudioParameterFloat> (
        makeUnitParameter ("highMidReso", "Ressonancia media alta", 0.5f)));
    layout.add (std::unique_ptr<juce::AudioParameterFloat> (
        makeUnitParameter ("lowMidFreq", "Frequencia media baixa", 0.5f)));
    layout.add (std::unique_ptr<juce::AudioParameterFloat> (
        makeUnitParameter ("lowMidGain", "Ganho medio baixo", 0.5f)));
    layout.add (std::unique_ptr<juce::AudioParameterFloat> (
        makeUnitParameter ("lowMidReso", "Ressonancia media baixa", 0.5f)));
    layout.add (std::unique_ptr<juce::AudioParameterFloat> (
        makeUnitParameter ("mix", "Mistura", 1.0f)));
    layout.add (std::unique_ptr<juce::AudioParameterFloat> (
        makeUnitParameter ("dyn", "Dinamica", 0.0f)));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "bypass", 1 }, "Bypass", false));

    return layout;
}

void CinzelAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    dsp.prepare (sampleRate);
    monoScratch.setSize (1, juce::jmax (1, samplesPerBlock), false, false, true);

    meterInL.store (0.0f);
    meterInR.store (0.0f);
    meterOutL.store (0.0f);
    meterOutR.store (0.0f);
    meterGr.store (0.0f);
    meterInClip.store (false);
    meterOutClip.store (false);
    clipResetRequested.store (false);

    setLatencySamples (dsp.latencySamples());
}

void CinzelAudioProcessor::releaseResources()
{
    monoScratch.setSize (0, 0);
}

bool CinzelAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainIn = layouts.getMainInputChannelSet();
    const auto mainOut = layouts.getMainOutputChannelSet();

    if (mainIn.isDisabled() || mainOut.isDisabled() || mainIn != mainOut)
        return false;

    return mainOut == juce::AudioChannelSet::mono()
        || mainOut == juce::AudioChannelSet::stereo();
}

void CinzelAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
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

    forja::dsp::CinzelDSP::Parameters parameters;
    parameters.highFreq = apvts.getRawParameterValue ("highFreq")->load();
    parameters.highGain = apvts.getRawParameterValue ("highGain")->load();
    parameters.highReso = apvts.getRawParameterValue ("highReso")->load();
    parameters.highMidFreq = apvts.getRawParameterValue ("highMidFreq")->load();
    parameters.highMidGain = apvts.getRawParameterValue ("highMidGain")->load();
    parameters.highMidReso = apvts.getRawParameterValue ("highMidReso")->load();
    parameters.lowMidFreq = apvts.getRawParameterValue ("lowMidFreq")->load();
    parameters.lowMidGain = apvts.getRawParameterValue ("lowMidGain")->load();
    parameters.lowMidReso = apvts.getRawParameterValue ("lowMidReso")->load();
    parameters.mix = apvts.getRawParameterValue ("mix")->load();
    parameters.dyn = apvts.getRawParameterValue ("dyn")->load();
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
    publishMin (meterGr, frame.gainReductionDb);

    if (frame.inClip)
        meterInClip.store (true, std::memory_order_relaxed);
    if (frame.outClip)
        meterOutClip.store (true, std::memory_order_relaxed);
}

void CinzelAudioProcessor::publishMax (std::atomic<float>& target, float value) noexcept
{
    float previous = target.load (std::memory_order_relaxed);
    while (value > previous
           && ! target.compare_exchange_weak (previous, value, std::memory_order_relaxed))
    {
    }
}

void CinzelAudioProcessor::publishMin (std::atomic<float>& target, float value) noexcept
{
    float previous = target.load (std::memory_order_relaxed);
    while (value < previous
           && ! target.compare_exchange_weak (previous, value, std::memory_order_relaxed))
    {
    }
}

CinzelAudioProcessor::MeterSnapshot CinzelAudioProcessor::readMeters()
{
    MeterSnapshot snapshot;
    snapshot.inPeakL = meterInL.exchange (0.0f, std::memory_order_relaxed);
    snapshot.inPeakR = meterInR.exchange (0.0f, std::memory_order_relaxed);
    snapshot.outPeakL = meterOutL.exchange (0.0f, std::memory_order_relaxed);
    snapshot.outPeakR = meterOutR.exchange (0.0f, std::memory_order_relaxed);
    snapshot.gainReductionDb = meterGr.exchange (0.0f, std::memory_order_relaxed);
    snapshot.inClip = meterInClip.load (std::memory_order_relaxed);
    snapshot.outClip = meterOutClip.load (std::memory_order_relaxed);
    return snapshot;
}

void CinzelAudioProcessor::resetClip()
{
    clipResetRequested.store (true, std::memory_order_release);
    meterInClip.store (false, std::memory_order_relaxed);
    meterOutClip.store (false, std::memory_order_relaxed);
}

bool CinzelAudioProcessor::isBypassed() const
{
    return apvts.getRawParameterValue ("bypass")->load() > 0.5f;
}

juce::AudioProcessorParameter* CinzelAudioProcessor::getBypassParameter() const
{
    return apvts.getParameter ("bypass");
}

void CinzelAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.state.createXml())
        copyXmlToBinary (*xml, destData);
}

void CinzelAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* CinzelAudioProcessor::createEditor()
{
    return new CinzelAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CinzelAudioProcessor();
}
