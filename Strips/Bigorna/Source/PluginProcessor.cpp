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

BigornaAudioProcessor::BigornaAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
                                .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout BigornaAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::unique_ptr<juce::AudioParameterFloat> (
        makeUnitParameter ("treble", "Agudos", 0.5f)));
    layout.add (std::unique_ptr<juce::AudioParameterFloat> (
        makeUnitParameter ("mid", "Medios", 0.5f)));
    layout.add (std::unique_ptr<juce::AudioParameterFloat> (
        makeUnitParameter ("bass", "Graves", 0.5f)));
    layout.add (std::unique_ptr<juce::AudioParameterFloat> (
        makeUnitParameter ("trebFreq", "Freq agudos", 0.5f)));
    layout.add (std::unique_ptr<juce::AudioParameterFloat> (
        makeUnitParameter ("bassFreq", "Freq graves", 0.5f)));
    layout.add (std::unique_ptr<juce::AudioParameterFloat> (
        makeUnitParameter ("lowCap", "Passa-baixos", 1.0f)));
    layout.add (std::unique_ptr<juce::AudioParameterFloat> (
        makeUnitParameter ("hiCap", "Passa-altos", 0.0f)));
    layout.add (std::unique_ptr<juce::AudioParameterFloat> (
        makeUnitParameter ("compres", "Compressao", 0.0f)));
    layout.add (std::unique_ptr<juce::AudioParameterFloat> (
        makeUnitParameter ("compSpd", "Velocidade", 0.0f)));
    layout.add (std::unique_ptr<juce::AudioParameterFloat> (
        makeUnitParameter ("output", "Saida", 0.33f)));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "bypass", 1 }, "Bypass", false));

    return layout;
}

void BigornaAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
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

void BigornaAudioProcessor::releaseResources()
{
    monoScratch.setSize (0, 0);
}

bool BigornaAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainIn = layouts.getMainInputChannelSet();
    const auto mainOut = layouts.getMainOutputChannelSet();

    if (mainIn.isDisabled() || mainOut.isDisabled() || mainIn != mainOut)
        return false;

    return mainOut == juce::AudioChannelSet::mono()
        || mainOut == juce::AudioChannelSet::stereo();
}

void BigornaAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
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

    forja::dsp::BigornaDSP::Parameters parameters;
    parameters.treble = apvts.getRawParameterValue ("treble")->load();
    parameters.mid = apvts.getRawParameterValue ("mid")->load();
    parameters.bass = apvts.getRawParameterValue ("bass")->load();
    parameters.trebFreq = apvts.getRawParameterValue ("trebFreq")->load();
    parameters.bassFreq = apvts.getRawParameterValue ("bassFreq")->load();
    parameters.lowCap = apvts.getRawParameterValue ("lowCap")->load();
    parameters.hiCap = apvts.getRawParameterValue ("hiCap")->load();
    parameters.compres = apvts.getRawParameterValue ("compres")->load();
    parameters.compSpd = apvts.getRawParameterValue ("compSpd")->load();
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
    publishMin (meterGr, frame.gainReductionDb);

    if (frame.inClip)
        meterInClip.store (true, std::memory_order_relaxed);
    if (frame.outClip)
        meterOutClip.store (true, std::memory_order_relaxed);
}

void BigornaAudioProcessor::publishMax (std::atomic<float>& target, float value) noexcept
{
    float previous = target.load (std::memory_order_relaxed);
    while (value > previous
           && ! target.compare_exchange_weak (previous, value, std::memory_order_relaxed))
    {
    }
}

void BigornaAudioProcessor::publishMin (std::atomic<float>& target, float value) noexcept
{
    float previous = target.load (std::memory_order_relaxed);
    while (value < previous
           && ! target.compare_exchange_weak (previous, value, std::memory_order_relaxed))
    {
    }
}

BigornaAudioProcessor::MeterSnapshot BigornaAudioProcessor::readMeters()
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

void BigornaAudioProcessor::resetClip()
{
    clipResetRequested.store (true, std::memory_order_release);
    meterInClip.store (false, std::memory_order_relaxed);
    meterOutClip.store (false, std::memory_order_relaxed);
}

bool BigornaAudioProcessor::isBypassed() const
{
    return apvts.getRawParameterValue ("bypass")->load() > 0.5f;
}

juce::AudioProcessorParameter* BigornaAudioProcessor::getBypassParameter() const
{
    return apvts.getParameter ("bypass");
}

void BigornaAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.state.createXml())
        copyXmlToBinary (*xml, destData);
}

void BigornaAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* BigornaAudioProcessor::createEditor()
{
    return new BigornaAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BigornaAudioProcessor();
}
