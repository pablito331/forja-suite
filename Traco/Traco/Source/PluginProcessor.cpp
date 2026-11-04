#include "PluginProcessor.h"

#include "PluginEditor.h"

namespace
{
std::unique_ptr<juce::AudioParameterFloat> makeUnitParameter (const juce::String& id,
                                                              const juce::String& name,
                                                              float defaultValue,
                                                              float step = 0.01f)
{
    return std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { id, 1 }, name,
        juce::NormalisableRange<float> (0.0f, 1.0f, step), defaultValue,
        juce::AudioParameterFloatAttributes()
            .withLabel (id)
            .withStringFromValueFunction ([] (float value, int)
            {
                return juce::String (value, 2);
            }));
}

std::unique_ptr<juce::AudioParameterBool> makeBoolParameter (const juce::String& id,
                                                             const juce::String& name,
                                                             bool defaultValue)
{
    return std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { id, 1 }, name, defaultValue);
}
} // namespace

TracoAudioProcessor::TracoAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
                                .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout TracoAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (makeBoolParameter ("bypass", "Bypass", false));

    layout.add (makeBoolParameter ("hpOn", "HP liga", false));
    layout.add (makeUnitParameter ("hpFreq", "Frequencia HP", 0.06f));
    // Duas posicoes (12 / 24 dB por oitava): passo 1,0, mesmo caso unico da
    // division do REPIQUE - o valor so existe como seletor.
    layout.add (makeUnitParameter ("hpSlope", "Inclinacao HP", 0.0f, 1.0f));

    layout.add (makeBoolParameter ("lpOn", "LP liga", false));
    layout.add (makeUnitParameter ("lpFreq", "Frequencia LP", 0.95f));
    layout.add (makeUnitParameter ("lpSlope", "Inclinacao LP", 0.0f, 1.0f));

    for (int i = 1; i <= numBands; ++i)
    {
        const juce::String n = juce::String ("b") + juce::String (i);

        layout.add (makeBoolParameter (n + "On", "Banda liga", i == 1));
        // Seis tipos (campainha, prateleira baixa/alta, passa-alta/baixa,
        // notch): cinco degraus, passo 0,2 no mesmo registro da divisao.
        layout.add (makeUnitParameter (n + "Type", "Tipo", 0.0f, 0.2f));
        layout.add (makeUnitParameter (n + "Freq", "Frequencia", 0.57f));
        layout.add (makeUnitParameter (n + "Gain", "Ganho", 0.5f));
        layout.add (makeUnitParameter (n + "Q", "Fator Q", 0.5f));
        layout.add (makeBoolParameter (n + "Listen", "Ouvir banda", false));
    }

    layout.add (makeBoolParameter ("essOn", "De-esser liga", false));
    layout.add (makeUnitParameter ("essFreq", "Frequencia esser", 0.31f));
    layout.add (makeUnitParameter ("essWidth", "Largura esser", 0.5f));
    layout.add (makeUnitParameter ("essRange", "Alcance esser", 0.38f));
    layout.add (makeBoolParameter ("essListen", "Ouvir esser", false));

    return layout;
}

forja::traco::TracoEngine::Params TracoAudioProcessor::paramsFromValues (
    const std::function<float (const juce::String&)>& load)
{
    using namespace forja::traco;

    TracoEngine::Params params;

    params.hpOn = load ("hpOn") > 0.5f;
    params.hpFreq = scaleFreq (load ("hpFreq"));
    params.hpSlope24 = load ("hpSlope") > 0.5f;

    params.lpOn = load ("lpOn") > 0.5f;
    params.lpFreq = scaleFreq (load ("lpFreq"));
    params.lpSlope24 = load ("lpSlope") > 0.5f;

    for (int i = 0; i < TracoEngine::numBands; ++i)
    {
        const juce::String n = juce::String ("b") + juce::String (i + 1);
        auto& band = params.bands[static_cast<std::size_t> (i)];
        band.on = load (n + "On") > 0.5f;
        band.type = bandTypeFromNorm (load (n + "Type"));
        band.freq = scaleFreq (load (n + "Freq"));
        band.gainDb = scaleGain (load (n + "Gain"));
        band.q = scaleQ (load (n + "Q"));
        band.listen = load (n + "Listen") > 0.5f;
    }

    params.essOn = load ("essOn") > 0.5f;
    params.essFreq = scaleEssFreq (load ("essFreq"));
    params.essWidthQ = scaleEssWidth (load ("essWidth"));
    params.essRangeDb = scaleEssRange (load ("essRange"));
    params.essListen = load ("essListen") > 0.5f;

    return params;
}

void TracoAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    engine.prepare (sampleRate);
    analyserIn.prepare (sampleRate);
    analyserOut.prepare (sampleRate);
    dryScratch.setSize (2, juce::jmax (1, samplesPerBlock), false, false, true);
    monoScratch.setSize (1, juce::jmax (1, samplesPerBlock), false, false, true);

    bypassGain = 1.0f;

    meterInL.store (0.0f);
    meterInR.store (0.0f);
    meterOutL.store (0.0f);
    meterOutR.store (0.0f);
    meterEssGr.store (0.0f);
    meterInClip.store (false);
    meterOutClip.store (false);
    clipResetRequested.store (false);

    // Zero medido por impulso no forja_traco: nada de lookahead na cadeia.
    setLatencySamples (0);
}

void TracoAudioProcessor::releaseResources()
{
    dryScratch.setSize (0, 0);
    monoScratch.setSize (0, 0);
}

bool TracoAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainIn = layouts.getMainInputChannelSet();
    const auto mainOut = layouts.getMainOutputChannelSet();

    if (mainIn.isDisabled() || mainOut.isDisabled() || mainIn != mainOut)
        return false;

    return mainOut == juce::AudioChannelSet::mono()
        || mainOut == juce::AudioChannelSet::stereo();
}

void TracoAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numInputChannels = getTotalNumInputChannels();

    for (int channel = numInputChannels; channel < getTotalNumOutputChannels(); ++channel)
        buffer.clear (channel, 0, numSamples);

    if (clipResetRequested.exchange (false, std::memory_order_acquire))
    {
        meterInClip.store (false, std::memory_order_relaxed);
        meterOutClip.store (false, std::memory_order_relaxed);
    }

    if (numInputChannels <= 0 || numSamples <= 0)
        return;

    analyserIn.pushSamples (buffer.getArrayOfReadPointers(), numInputChannels, numSamples);

    if (dryScratch.getNumSamples() < numSamples)
        dryScratch.setSize (2, numSamples, false, false, true);

    const int numChannels = juce::jmin (numInputChannels, 2);
    for (int channel = 0; channel < numChannels; ++channel)
        dryScratch.copyFrom (channel, 0, buffer, channel, 0, numSamples);

    const auto params = paramsFromValues ([this] (const juce::String& id)
                                          {
                                              return apvts.getRawParameterValue (id)->load();
                                          });
    engine.setParams (params);

    if (numChannels == 1)
    {
        if (monoScratch.getNumSamples() < numSamples)
            monoScratch.setSize (1, numSamples, false, false, true);

        monoScratch.copyFrom (0, 0, buffer, 0, 0, numSamples);
        engine.process (buffer.getWritePointer (0), nullptr, numSamples);
    }
    else
    {
        engine.process (buffer.getWritePointer (0), buffer.getWritePointer (1), numSamples);
    }

    // Bypass com crossfade de 10 ms por amostra. O engine continua rodando
    // atras do dry: estado de filtro nunca e resetado no bypass.
    const float targetGain = isBypassed() ? 0.0f : 1.0f;
    const float step = 1.0f
        / juce::jmax (1.0f, static_cast<float> (0.01 * sr));

    float* const* wet = buffer.getArrayOfWritePointers();
    float* const* dry = dryScratch.getArrayOfWritePointers();

    float inPeak[2] = { 0.0f, 0.0f };
    float outPeak[2] = { 0.0f, 0.0f };

    for (int i = 0; i < numSamples; ++i)
    {
        bypassGain += juce::jlimit (-step, step, targetGain - bypassGain);
        const float g = bypassGain;
        const float ig = 1.0f - g;

        for (int channel = 0; channel < numChannels; ++channel)
        {
            const float d = dry[channel][i];
            const float y = wet[channel][i] * g + d * ig;
            wet[channel][i] = y;

            const float ad = std::abs (d);
            const float ay = std::abs (y);
            if (ad > inPeak[channel])
                inPeak[channel] = ad;
            if (ay > outPeak[channel])
                outPeak[channel] = ay;
        }
    }

    analyserOut.pushSamples (buffer.getArrayOfReadPointers(), numInputChannels, numSamples);

    publishMax (meterInL, inPeak[0]);
    publishMax (meterInR, numChannels > 1 ? inPeak[1] : inPeak[0]);
    publishMax (meterOutL, outPeak[0]);
    publishMax (meterOutR, numChannels > 1 ? outPeak[1] : outPeak[0]);
    publishMax (meterEssGr, static_cast<float> (engine.gainReductionDb()));

    if (inPeak[0] > 0.999f || (numChannels > 1 && inPeak[1] > 0.999f))
        meterInClip.store (true, std::memory_order_relaxed);
    if (outPeak[0] > 0.999f || (numChannels > 1 && outPeak[1] > 0.999f))
        meterOutClip.store (true, std::memory_order_relaxed);
}

void TracoAudioProcessor::publishMax (std::atomic<float>& target, float value) noexcept
{
    float previous = target.load (std::memory_order_relaxed);
    while (value > previous
           && ! target.compare_exchange_weak (previous, value, std::memory_order_relaxed))
    {
    }
}

TracoAudioProcessor::MeterSnapshot TracoAudioProcessor::readMeters()
{
    MeterSnapshot snapshot;
    snapshot.inPeakL = meterInL.exchange (0.0f, std::memory_order_relaxed);
    snapshot.inPeakR = meterInR.exchange (0.0f, std::memory_order_relaxed);
    snapshot.outPeakL = meterOutL.exchange (0.0f, std::memory_order_relaxed);
    snapshot.outPeakR = meterOutR.exchange (0.0f, std::memory_order_relaxed);
    snapshot.essGainReductionDb = meterEssGr.exchange (0.0f, std::memory_order_relaxed);
    snapshot.inClip = meterInClip.load (std::memory_order_relaxed);
    snapshot.outClip = meterOutClip.load (std::memory_order_relaxed);
    return snapshot;
}

void TracoAudioProcessor::resetClip()
{
    clipResetRequested.store (true, std::memory_order_release);
    meterInClip.store (false, std::memory_order_relaxed);
    meterOutClip.store (false, std::memory_order_relaxed);
}

bool TracoAudioProcessor::isBypassed() const
{
    return apvts.getRawParameterValue ("bypass")->load() > 0.5f;
}

juce::AudioProcessorParameter* TracoAudioProcessor::getBypassParameter() const
{
    return apvts.getParameter ("bypass");
}

void TracoAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.state.createXml())
        copyXmlToBinary (*xml, destData);
}

void TracoAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* TracoAudioProcessor::createEditor()
{
    return new TracoAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TracoAudioProcessor();
}
