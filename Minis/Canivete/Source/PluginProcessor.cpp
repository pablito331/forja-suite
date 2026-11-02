#include "PluginProcessor.h"

#include "PluginEditor.h"

#include "canivete/BankLibrary.h"

namespace
{

juce::String volumeText (float value)
{
    const auto db = (value - 0.5f) * 48.0f;
    return (db > 0.0f ? "+" : "") + juce::String (db, 1) + " dB";
}

// Varredura feita antes do APVTS nascer: AudioParameterChoice guarda as
// choices num vetor const, entao a lista do knob BANK so pode ser montada na
// construcao do layout. O processor re-varre bankFiles ao carregar; o botao
// ATUALIZAR da UI reabre o plugin quando a pasta mudar de verdade.
juce::StringArray scannedBankNames()
{
    juce::StringArray names;
    names.add ("none");

    for (const auto& file : forja::canivete::findBanks (
             forja::canivete::SoundFontEngine::sf3Available()))
        names.add (file.getFileNameWithoutExtension());

    return names;
}

} // namespace

CaniveteAudioProcessor::CaniveteAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
                                .withOutput ("Main", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    volumeParam = apvts.getRawParameterValue ("volume");
    bankParam = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter ("bank"));

    refreshLibrary();

    apvts.addParameterListener ("bank", this);

    // Comeca tocando o bank 1 da lista (o "0" e o marcador none); sem bank na
    // pasta, fica mudo ate o usuario depositar um .sf2/.sf3 la.
    if (! bankFiles.isEmpty())
        loadBankByIndex (0);

    // Colhe o resultado da carga em background e faz o reap dos banks
    // substituidos; 8 Hz deixa a UI esperta sem custo perceptivel.
    startTimerHz (8);
}

CaniveteAudioProcessor::~CaniveteAudioProcessor()
{
    stopTimer();
    apvts.removeParameterListener ("bank", this);
}

juce::AudioProcessorValueTreeState::ParameterLayout CaniveteAudioProcessor::createParameterLayout()
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

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "bank", 1 }, "Bank", scannedBankNames(), 0));

    return layout;
}

void CaniveteAudioProcessor::refreshLibrary()
{
    forja::canivete::ensureLibraryDirectory();
    bankFiles = forja::canivete::findBanks (forja::canivete::SoundFontEngine::sf3Available());
}

void CaniveteAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);

    // Sem PDC: o tsf entrega no mesmo bloco; a cauda depende do bank.
    setLatencySamples (0);
}

void CaniveteAudioProcessor::releaseResources()
{
}

bool CaniveteAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::disabled()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void CaniveteAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                           juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    buffer.clear();

    auto mainBus = getBusBuffer (buffer, false, 0);

    engine.setVolume (volumeParam->load (std::memory_order_relaxed));
    engine.process (midiMessages, mainBus, buffer.getNumSamples(), isBypassed());
}

bool CaniveteAudioProcessor::isBypassed() const
{
    return apvts.getRawParameterValue ("bypass")->load() > 0.5f;
}

juce::AudioProcessorParameter* CaniveteAudioProcessor::getBypassParameter() const
{
    return apvts.getParameter ("bypass");
}

void CaniveteAudioProcessor::loadBankFile (const juce::File& file)
{
    lastLoadedPath = file.getFullPathName();

    engine.loadBankFile (file, [this] (const juce::String&)
    {
        bankRevision.fetch_add (1, std::memory_order_relaxed);
    });
}

void CaniveteAudioProcessor::setPresetIndex (int index)
{
    engine.requestPreset (index);   // atomic; o audio aplica no proximo bloco
}

void CaniveteAudioProcessor::loadBankByIndex (int index)
{
    if (! juce::isPositiveAndBelow (index, bankFiles.size()))
        return;

    // Reescreve o choice sem disparar a carga pelo listener (indice ja veio
    // dele ou do estado; o valor so sincroniza a maquina).
    if (bankParam != nullptr && bankParam->getIndex() != index + 1)   // 0 = none
    {
        const juce::ScopedValueSetter setter (applyingState, true);
        *bankParam = index + 1;   // operator= recebe o indice da escolha
    }

    loadBankFile (bankFiles[index]);
}

void CaniveteAudioProcessor::selectBankByName (const juce::String& name)
{
    for (int index = 0; index < bankFiles.size(); ++index)
        if (bankFiles[index].getFileNameWithoutExtension() == name)
        {
            loadBankByIndex (index);
            return;
        }

    // Nome fora da lista congelada: re-varre a pasta antes de desistir.
    refreshLibrary();

    for (int index = 0; index < bankFiles.size(); ++index)
        if (bankFiles[index].getFileNameWithoutExtension() == name)
        {
            loadBankByIndex (index);
            return;
        }
}

void CaniveteAudioProcessor::parameterChanged (const juce::String& parameterID, float newValue)
{
    if (parameterID != "bank" || applyingState || bankParam == nullptr)
        return;

    // newValue chega normalizado (0..1): converte para o indice da escolha antes
    // de descontar o marcador "none" na posicao 0.
    const auto last = juce::jmax (0, bankParam->choices.size() - 1);
    const auto chosen = juce::roundToInt (newValue * static_cast<float> (last));
    const auto index = chosen - 1;   // 0 = none

    if (index < 0)
        return;

    // Troca de bank feito pelo usuario: o instrumento volta ao primeiro do novo
    // banco (a lista de presets e outra) e se descarta qualquer restauracao de
    // estado pendente, para nao reescrever um preset de um banco anterior.
    pendingPresetName.clear();
    engine.requestPreset (0);

    loadBankByIndex (index);
}

void CaniveteAudioProcessor::timerCallback()
{
    engine.pollLoadResult();
    engine.reapRetiredBanks();

    // Instrumento pedido pelo estado: resolvido pelo nome quando o bank ja esta
    // tocavel (a carga e assincrona) e entregue ao audio pelo indice na lista.
    if (pendingPresetName.isNotEmpty() && engine.hasBank() && ! engine.isLoading())
    {
        // Os nomes de preset vem do campo phdr do SF2: 20 bytes fixos recheados
        // com espacos ("Monster Hit         "). A comparacao exata funciona
        // enquanto o texto cru sobrevive intacto, mas o preset escolhido passa
        // por XML e pelo host ao ser restaurado, e camadas pelo caminho podam os
        // espacos de fim. Se os DOIS lados forem podados a escolha continua a
        // mesma (o primeiro da lista que casa), entao isto so resgata o caso em
        // que o recheio se perdeu -- e a gaveta do rack morreu exatamente assim.
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

void CaniveteAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();

    if (lastLoadedPath.isNotEmpty())
    {
        state.setProperty ("bankPath", lastLoadedPath, nullptr);
        state.setProperty ("bankName", engine.getBankName(), nullptr);
    }

    // Instrumento em uso no canal melodic 0: guarda pelo NOME (o indice nao e
    // estavel entre bancos diferentes).
    const auto presetName = engine.getSelectedPresetName();

    if (presetName.isNotEmpty())
        state.setProperty ("presetName", presetName, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void CaniveteAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);

    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    {
        const juce::ScopedValueSetter setter (applyingState, true);
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
    }

    refreshLibrary();

    // Instrumento a restaurar: resolvido pelo timer quando o bank ficar pronto.
    pendingPresetName = apvts.state.getProperty ("presetName").toString();

    // Reabre de onde estava: primeiro pelo caminho gravado; se o arquivo saiu
    // da pasta, tenta pelo nome na biblioteca atual.
    const auto path = apvts.state.getProperty ("bankPath").toString();

    if (path.isNotEmpty() && juce::File (path).existsAsFile())
    {
        loadBankFile (juce::File (path));
        return;
    }

    selectBankByName (apvts.state.getProperty ("bankName").toString());
}

juce::AudioProcessorEditor* CaniveteAudioProcessor::createEditor()
{
    return new CaniveteAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CaniveteAudioProcessor();
}
