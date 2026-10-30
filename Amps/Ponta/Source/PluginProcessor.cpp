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

PontaAudioProcessor::PontaAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
                                .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout PontaAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::unique_ptr<juce::AudioParameterFloat> (
        makeUnitParameter ("drive", "Drive", 0.6f)));
    layout.add (std::unique_ptr<juce::AudioParameterFloat> (
        makeUnitParameter ("presence", "Presence", 0.5f)));
    layout.add (std::unique_ptr<juce::AudioParameterFloat> (
        makeUnitParameter ("high", "High", 0.8f)));
    layout.add (std::unique_ptr<juce::AudioParameterFloat> (
        makeUnitParameter ("mid", "Mid", 0.6f)));
    layout.add (std::unique_ptr<juce::AudioParameterFloat> (
        makeUnitParameter ("low", "Low", 0.4f)));
    layout.add (std::unique_ptr<juce::AudioParameterFloat> (
        makeUnitParameter ("sub", "Sub", 0.5f)));
    layout.add (std::unique_ptr<juce::AudioParameterFloat> (
        makeUnitParameter ("hspeaker", "High speaker", 0.7f)));
    layout.add (std::unique_ptr<juce::AudioParameterFloat> (
        makeUnitParameter ("lspeaker", "Low speaker", 0.4f)));
    layout.add (std::unique_ptr<juce::AudioParameterFloat> (
        makeUnitParameter ("gate", "Gate", 0.0f)));
    layout.add (std::unique_ptr<juce::AudioParameterFloat> (
        makeUnitParameter ("output", "Output", 0.7f)));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "bypass", 1 }, "Bypass", false));

    return layout;
}

void PontaAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
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

void PontaAudioProcessor::releaseResources()
{
    monoScratch.setSize (0, 0);
}

bool PontaAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainIn = layouts.getMainInputChannelSet();
    const auto mainOut = layouts.getMainOutputChannelSet();

    if (mainIn.isDisabled() || mainOut.isDisabled() || mainIn != mainOut)
        return false;

    return mainOut == juce::AudioChannelSet::mono()
        || mainOut == juce::AudioChannelSet::stereo();
}

void PontaAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
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

    forja::dsp::PontaDSP::Parameters parameters;
    parameters.drive = apvts.getRawParameterValue ("drive")->load();
    parameters.presence = apvts.getRawParameterValue ("presence")->load();
    parameters.high = apvts.getRawParameterValue ("high")->load();
    parameters.mid = apvts.getRawParameterValue ("mid")->load();
    parameters.low = apvts.getRawParameterValue ("low")->load();
    parameters.sub = apvts.getRawParameterValue ("sub")->load();
    parameters.hspeaker = apvts.getRawParameterValue ("hspeaker")->load();
    parameters.lspeaker = apvts.getRawParameterValue ("lspeaker")->load();
    parameters.gate = apvts.getRawParameterValue ("gate")->load();
    parameters.output = apvts.getRawParameterValue ("output")->load();
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

void PontaAudioProcessor::publishMax (std::atomic<float>& target, float value) noexcept
{
    float previous = target.load (std::memory_order_relaxed);
    while (value > previous
           && ! target.compare_exchange_weak (previous, value, std::memory_order_relaxed))
    {
    }
}

PontaAudioProcessor::MeterSnapshot PontaAudioProcessor::readMeters()
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

void PontaAudioProcessor::resetClip()
{
    clipResetRequested.store (true, std::memory_order_release);
    meterInClip.store (false, std::memory_order_relaxed);
    meterOutClip.store (false, std::memory_order_relaxed);
}

bool PontaAudioProcessor::isBypassed() const
{
    return apvts.getRawParameterValue ("bypass")->load() > 0.5f;
}

juce::AudioProcessorParameter* PontaAudioProcessor::getBypassParameter() const
{
    return apvts.getParameter ("bypass");
}

void PontaAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.state.createXml())
        copyXmlToBinary (*xml, destData);
}

void PontaAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* PontaAudioProcessor::createEditor()
{
    return new PontaAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PontaAudioProcessor();
}
