#include "HostedModule.h"

#include <algorithm>

namespace forjarack
{

HostedModule::~HostedModule() = default;

bool HostedModule::adoptInstance (std::unique_ptr<juce::AudioPluginInstance> newInstance,
                                  juce::String& error)
{
    if (newInstance == nullptr)
    {
        error = "instancia nula";
        return false;
    }

    // Instrumento = sem bus de entrada (so gera audio a partir de MIDI). Efeito
    // precisa de pelo menos um bus de saida para caber na corrente estereo.
    const bool noInputBus = newInstance->getBusCount (true) == 0;
    const bool hasOutput = newInstance->getBusCount (false) > 0;

    if (! hasOutput)
    {
        error = "plugin sem saida de audio";
        return false;
    }

    instance = std::move (newInstance);
    instrument = noInputBus;
    name = instance->getName();
    nativeMidi = nullptr;
    lastPeak = 0.0f;
    return true;
}

void HostedModule::setBlockSize (int maxSamples) noexcept
{
    block = std::max (1, maxSamples);
}

void HostedModule::prepare (double sampleRate)
{
    if (instance == nullptr)
        return;

    sr = sampleRate;

    // Configura o grafo interno do plugin ANTES de prepareToPlay. Stereo: 2 outs
    // sempre; 2 ins em efeito, 0 em instrumento (o scratch ja vem zerado).
    instance->setPlayConfigDetails (instrument ? 0 : 2, 2, sr, block);

    // Capacidade fixa do buffer-ponte: decide o teto aqui, nunca no audio. Os
    // setSize menores no process usam avoidReallocating=true e assim nao alocam.
    buffer.setSize (2, block);

    instance->prepareToPlay (sr, block);
}

void HostedModule::setParam (int index, float value)
{
    if (instance == nullptr || paramsLocked)
        return;

    auto& params = instance->getParameters();

    if (index < 0 || index >= params.size())
        return;

    params[index]->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, value));
}

void HostedModule::setBypass (bool bypassedFlag)
{
    // O bypass da gaveta e feito pelo engine (pass-through com crossfade no
    // processador); o plugin hospedado segue vivo com seu proprio estado. Nada
    // a propagar para a instancia aqui.
    bypassed = bypassedFlag;
}

void HostedModule::processMidi (const MidiEventSpan& midi) noexcept
{
    // So o caminho de instrumento usa: aponta o MidiBuffer nativo do host (que
    // o core livre de JUCE transporta como void* opaco). Nulo = sem MIDI nativo.
    nativeMidi = static_cast<const juce::MidiBuffer*> (midi.native);
}

void HostedModule::process (float* left, float* right, int numSamples)
{
    if (instance == nullptr || numSamples <= 0)
        return;

    // Nunca processa mais do que a capacidade reservada em prepare.
    const int n = std::min (numSamples, block);

    buffer.setSize (2, n, false, false, true);   // avoidReallocating: cabe em block

    buffer.copyFrom (0, 0, left, n);
    buffer.copyFrom (1, 0, right, n);

    // Efeito ignora MIDI; instrumento consome o buffer nativo do host (o cast
    // fora o const do ponteiro opaco do core: o plugin le os eventos do bloco).
    juce::MidiBuffer& midiInOut = (instrument && nativeMidi != nullptr)
        ? *const_cast<juce::MidiBuffer*> (nativeMidi)
        : emptyMidi;

    instance->processBlock (buffer, midiInOut);

    float peak = 0.0f;

    if (auto* l = buffer.getReadPointer (0))
    {
        if (buffer.getNumChannels() > 1)
        {
            const auto* r = buffer.getReadPointer (1);

            for (int i = 0; i < n; ++i)
                peak = std::max ({ peak, std::abs (l[i]), std::abs (r[i]) });
        }
        else
        {
            for (int i = 0; i < n; ++i)
                peak = std::max (peak, std::abs (l[i]));
        }
    }

    // Devolve o processado. Instrumento: o engine somou o scratch, mas aqui
    // escrevemos o gerado (o scratch veio zerado, entao copiar equivale a somar).
    std::copy (buffer.getReadPointer (0), buffer.getReadPointer (0) + n, left);

    if (buffer.getNumChannels() > 1)
        std::copy (buffer.getReadPointer (1), buffer.getReadPointer (1) + n, right);
    else
        std::copy (buffer.getReadPointer (0), buffer.getReadPointer (0) + n, right);

    lastPeak = peak;
}

float HostedModule::readOutputPeak()
{
    return lastPeak;
}

int HostedModule::latencySamples() const
{
    return instance != nullptr ? instance->getLatencySamples() : 0;
}

int HostedModule::numHostedParams() const noexcept
{
    return instance != nullptr ? instance->getParameters().size() : 0;
}

juce::String HostedModule::hostedParamLabel (int index) const
{
    if (instance == nullptr)
        return {};

    auto& params = instance->getParameters();

    if (index < 0 || index >= params.size())
        return {};

    return params[index]->getName (64);
}

float HostedModule::hostedParamValue (int index) const
{
    if (instance == nullptr)
        return 0.5f;

    auto& params = instance->getParameters();

    if (index < 0 || index >= params.size())
        return 0.5f;

    return params[index]->getValue();
}

juce::Component* HostedModule::createNativeEditor()
{
    if (instance == nullptr)
        return nullptr;

    // Se o plugin traz UI propria, usa ela. O processor e o dono do editor no
    // JUCE 9: createEditorAndMakeActive devolve nullptr quando ja existe um
    // ativo, e ai o getActiveEditor entrega o component vivo para reaproveitar.
    if (instance->hasEditor())
    {
        if (auto* editor = instance->createEditorAndMakeActive())
            return editor;

        return instance->getActiveEditor();
    }

    // Sem UI propria: monta a lista generica de parametros (a "UI generica"
    // pedida para quem nao segue o padrao Forja). Vive enquanto o modulo viver.
    if (genericEditor == nullptr)
        genericEditor = std::make_unique<juce::GenericAudioProcessorEditor> (*instance);

    return genericEditor.get();
}

} // namespace forjarack
