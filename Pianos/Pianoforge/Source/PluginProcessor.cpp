#include "PluginProcessor.h"

#include "PluginEditor.h"

namespace
{
using namespace forja::pianoforge;

juce::File libraryDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
        .getChildFile ("Forja")
        .getChildFile ("Pianoforge");
}

// Varredura da biblioteca do piano: .sf2, .sf3 (se o build traz o decodificador)
// e .pforge (bundle do projeto-irmao). Ordem alfabetica; o arquivo e lido direto
// do lugar, em thread de background.
juce::Array<juce::File> findBanks (bool includeSf3)
{
    const auto directory = libraryDirectory();

    if (! directory.isDirectory())
        return {};

    auto banks = directory.findChildFiles (juce::File::findFiles, false, "*.sf2");
    banks.addArray (directory.findChildFiles (juce::File::findFiles, false, "*.pforge"));

    if (includeSf3)
        banks.addArray (directory.findChildFiles (juce::File::findFiles, false, "*.sf3"));

    banks.sort();
    return banks;
}

void ensureLibraryDirectory()
{
    auto directory = libraryDirectory();

    if (! directory.isDirectory())
        directory.createDirectory();
}

const char* reverbRoomName (int type) noexcept
{
    switch (type)
    {
        case 0: return "Sala pequena";
        case 1: return "Studio";
        case 2: return "Prato";
        case 3: return "Catedral";
        default: return "?";
    }
}

} // namespace

PianoforgeAudioProcessor::PianoforgeAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
                                .withOutput ("Main", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    for (int i = 0; i < Param_Count; ++i)
        rawParams[static_cast<std::size_t> (i)] = apvts.getRawParameterValue (kParamDefs[i].id);

    bankParam = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter ("bank"));

    refreshLibrary();

    apvts.addParameterListener ("bank", this);

    if (! bankFiles.isEmpty())
        loadBankByIndex (0);

    // Colhe o resultado da carga em background e faz o reap dos banks; 8 Hz
    // deixa a UI esperta sem custo perceptivel.
    startTimerHz (8);
}

PianoforgeAudioProcessor::~PianoforgeAudioProcessor()
{
    stopTimer();
    apvts.removeParameterListener ("bank", this);
}

juce::String PianoforgeAudioProcessor::paramText (int paramId, double value01)
{
    if (paramId < 0 || paramId >= Param_Count)
        return {};

    const float engineValue = toEngineDomain (paramId, static_cast<float> (value01));
    const auto& def = kParamDefs[paramId];

    if (paramId == Param_ReverbType)
        return reverbRoomName (static_cast<int> (engineValue));

    // Dominios que nao sao 0..1 puro ganham unidade explicita.
    if (paramId == Param_TrimDb)
        return (engineValue >= 0.0f ? "+" : "") + juce::String (engineValue, 1) + " dB";

    if (paramId == Param_EngineTune)
        return (engineValue >= 0.0f ? "+" : "") + juce::String (engineValue, 0) + " cents";

    if (paramId == Param_Width)
        return juce::String (engineValue, 2);

    // 0..1: mostra como percentual (o dominio real dos knobs de modelagem).
    if (def.min == 0.0f && def.max == 1.0f)
        return juce::String (engineValue * 100.0, 0) + "%";

    return juce::String (engineValue, 2);
}

juce::AudioProcessorValueTreeState::ParameterLayout PianoforgeAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "bypass", 1 },
                                                            "Bypass", false));

    for (int i = 0; i < Param_Count; ++i)
    {
        const auto& def = kParamDefs[i];

        // A regra da suite manda o APVTS viver em 0..1 (passo 0,01). O reverbType
        // e a excecao no estilo da `division`: choice de 4 posicoes, entao o passo
        // e 1/3 para as posicoes da grade cair exatamente nos 4 tipos (bit-exato na
        // ida e volta testada).
        const float interval = (i == Param_ReverbType) ? (1.0f / 3.0f) : 0.01f;

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { juce::String (def.id), 1 }, def.id,
            juce::NormalisableRange<float> (0.0f, 1.0f, interval), def.default01,
            juce::AudioParameterFloatAttributes()
                .withLabel (def.id)
                .withStringFromValueFunction ([i] (float value, int)
                {
                    return PianoforgeAudioProcessor::paramText (i, static_cast<double> (value));
                })));
    }

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "bank", 1 }, "Bank",
        []
        {
            juce::StringArray names;
            names.add ("none");

            for (const auto& file : findBanks (PianoEngine::sf3Available()))
                names.add (file.getFileNameWithoutExtension());

            return names;
        }(), 0));

    return layout;
}

void PianoforgeAudioProcessor::refreshLibrary()
{
    ensureLibraryDirectory();
    bankFiles = findBanks (PianoEngine::sf3Available());
}

void PianoforgeAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);

    // Sem PDC: o tsf entrega no mesmo bloco; a cauda depende do banco. A
    // latencia real e medida por impulso no teste da suite.
    setLatencySamples (0);
}

void PianoforgeAudioProcessor::releaseResources()
{
}

bool PianoforgeAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::disabled()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void PianoforgeAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                             juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    buffer.clear();

    // Empurra os 17 parametros (dominio do APVTS em 0..1) para o dominio real do
    // engine. Roda na thread de audio: escreve so nos atomicos lidos pelo render.
    for (int i = 0; i < Param_Count; ++i)
    {
        if (auto* raw = rawParams[static_cast<std::size_t> (i)]; raw != nullptr)
            engine.setParameter (i, toEngineDomain (i, raw->load (std::memory_order_relaxed)));
    }

    auto mainBus = getBusBuffer (buffer, false, 0);

    engine.process (midiMessages, mainBus, buffer.getNumSamples(), isBypassed());
}

bool PianoforgeAudioProcessor::isBypassed() const
{
    return apvts.getRawParameterValue ("bypass")->load() > 0.5f;
}

juce::AudioProcessorParameter* PianoforgeAudioProcessor::getBypassParameter() const
{
    return apvts.getParameter ("bypass");
}

void PianoforgeAudioProcessor::loadBankFile (const juce::File& file)
{
    lastLoadedPath = file.getFullPathName();

    engine.loadBankFile (file, [this] (const juce::String&)
    {
        bankRevision.fetch_add (1, std::memory_order_relaxed);
    });
}

void PianoforgeAudioProcessor::setPresetIndex (int index)
{
    engine.requestPreset (index);   // atomic; o audio aplica no proximo bloco
}

void PianoforgeAudioProcessor::loadBankByIndex (int index)
{
    if (! juce::isPositiveAndBelow (index, bankFiles.size()))
        return;

    if (bankParam != nullptr && bankParam->getIndex() != index + 1)   // 0 = none
    {
        const juce::ScopedValueSetter setter (applyingState, true);
        *bankParam = index + 1;
    }

    loadBankFile (bankFiles[index]);
}

void PianoforgeAudioProcessor::selectBankByName (const juce::String& name)
{
    for (int index = 0; index < bankFiles.size(); ++index)
        if (bankFiles[index].getFileNameWithoutExtension() == name)
        {
            loadBankByIndex (index);
            return;
        }

    refreshLibrary();

    for (int index = 0; index < bankFiles.size(); ++index)
        if (bankFiles[index].getFileNameWithoutExtension() == name)
        {
            loadBankByIndex (index);
            return;
        }
}

void PianoforgeAudioProcessor::parameterChanged (const juce::String& parameterID, float newValue)
{
    if (parameterID != "bank" || applyingState || bankParam == nullptr)
        return;

    const auto last = juce::jmax (0, bankParam->choices.size() - 1);
    const auto chosen = juce::roundToInt (newValue * static_cast<float> (last));
    const auto index = chosen - 1;   // 0 = none

    if (index < 0)
        return;

    // Troca de banco: o preset volta ao primeiro do novo banco (a lista e outra)
    // e descarta qualquer restauracao pendente.
    pendingPresetName.clear();
    engine.requestPreset (0);

    loadBankByIndex (index);
}

void PianoforgeAudioProcessor::timerCallback()
{
    engine.pollLoadResult();
    engine.reapRetiredBanks();

    if (pendingPresetName.isNotEmpty() && engine.hasBank() && ! engine.isLoading())
    {
        const auto wanted = pendingPresetName.trimEnd();

        for (int i = 0; i < engine.getNumPresets(); ++i)
            if (engine.getPresetName (i).trimEnd() == wanted)
            {
                engine.requestPreset (i);
                break;
            }

        pendingPresetName.clear();
    }
}

void PianoforgeAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();

    if (lastLoadedPath.isNotEmpty())
    {
        state.setProperty ("bankPath", lastLoadedPath, nullptr);
        state.setProperty ("bankName", engine.getBankName(), nullptr);
    }

    const auto presetName = engine.getSelectedPresetName();

    if (presetName.isNotEmpty())
        state.setProperty ("presetName", presetName, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void PianoforgeAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);

    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    {
        const juce::ScopedValueSetter setter (applyingState, true);
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
    }

    refreshLibrary();

    pendingPresetName = apvts.state.getProperty ("presetName").toString();

    const auto path = apvts.state.getProperty ("bankPath").toString();

    if (path.isNotEmpty() && juce::File (path).existsAsFile())
    {
        loadBankFile (juce::File (path));
        return;
    }

    selectBankByName (apvts.state.getProperty ("bankName").toString());
}

juce::AudioProcessorEditor* PianoforgeAudioProcessor::createEditor()
{
    return new PianoforgeAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PianoforgeAudioProcessor();
}
