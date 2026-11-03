#include "PianoforgeRackModule.h"

#include <algorithm>

#include "rack/ModuleCatalog.h"

namespace forja::pianoforge
{

namespace
{

// Biblioteca do PIANOFORGE: Documents/Forja/Pianoforge. O usuario deposita ai os
// pianos (.sf2, .sf3 quando o build traz o decodificador e .pforge do projeto-
// irmao); o knob BANK da UI e a gaveta listam esta pasta em ordem alfabetica.
juce::File libraryDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
        .getChildFile ("Forja")
        .getChildFile ("Pianoforge");
}

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

// Chave de comparacao de nome de preset: o phdr do SF2 e um campo fixo de 20
// bytes recheado com espacos, e o preset escolhido passa por XML/host que
// costumam podar espagos de fim. O matching ignora os espagos de fim dos DOIS
// lados; nomes iguais depois de podados resolvem para o primeiro da lista.
std::string presetKey (const std::string& name)
{
    const auto last = name.find_last_not_of (" \t");

    if (last == std::string::npos)
        return {};

    return name.substr (0, last + 1);
}

} // namespace

PianoforgeModule::PianoforgeModule() = default;

void PianoforgeModule::refreshLibrary()
{
    // Somente thread de mensagens: varre Documents/Forja/Pianoforge. O audio nao
    // le banks[], so o tamanho publicado em bankCount.
    const auto found = findBanks (PianoEngine::sf3Available());

    banks.clear();
    banks.reserve (static_cast<size_t> (found.size()));

    for (const auto& file : found)
        banks.push_back (file);

    bankCount.store (static_cast<int> (banks.size()), std::memory_order_release);
}

void PianoforgeModule::reselectAfterRefresh()
{
    if (selectedPath.empty())
    {
        externalActive.store (false, std::memory_order_relaxed);
        desiredBank.store (-1, std::memory_order_relaxed);
        loadedBank = -1;
        return;
    }

    for (size_t i = 0; i < banks.size(); ++i)
    {
        if (banks[i].getFullPathName().toStdString() != selectedPath)
            continue;

        externalActive.store (false, std::memory_order_relaxed);
        loadedBank = static_cast<int> (i);
        desiredBank.store (loadedBank, std::memory_order_relaxed);
        return;
    }

    if (externalActive.load (std::memory_order_relaxed))
    {
        loadedBank = -1;   // externo: idle colhera externalPath / selectedPath
        return;
    }

    selectedPath.clear();
    externalActive.store (false, std::memory_order_relaxed);
    desiredBank.store (-1, std::memory_order_relaxed);
    loadedBank = -1;
}

void PianoforgeModule::prepare (double sampleRate)
{
    refreshLibrary();

    // Pre-aloca o MidiBuffer na thread de mensagens: o processo de audio so da
    // clear/addEvent e, depois da primeira carga, nunca mais pede memoria.
    midi.ensureSize (forjarack::kMaxRackMidiEvents * 3 * static_cast<int> (sizeof (juce::int32)));

    engine.prepare (sampleRate, engineCapacity);

    reselectAfterRefresh();
}

void PianoforgeModule::setParam (int index, float value)
{
    switch (index)
    {
        case 0:
            // Volume vive no dominio do engine (0..1); o knob do rack ja vem 0..1.
            engine.setParameter (Param_Volume, juce::jlimit (0.0f, 1.0f, value));
            break;

        case 1:
        {
            const int count = bankCount.load (std::memory_order_acquire);

            if (count > 0)
            {
                const auto normalized = juce::jlimit (0.0f, 1.0f, value);
                desiredBank.store (juce::roundToInt (normalized * float (count - 1)),
                                   std::memory_order_relaxed);
            }

            break;
        }

        default:
            break;
    }
}

void PianoforgeModule::processMidi (const forjarack::MidiEventSpan& span) noexcept
{
    // Materializa o span POD no MidiBuffer que o engine le. O nucleo do rack so
    // expoe notas on/off; CC (pedal) e program change ficam para o plugin.
    midi.clear();

    for (int i = 0; i < span.numEvents; ++i)
    {
        const auto& event = span.events[static_cast<size_t> (i)];

        const auto message = event.noteOn
            ? juce::MidiMessage::noteOn (event.channel, event.note,
                                         static_cast<juce::uint8> (event.velocity))
            : juce::MidiMessage::noteOff (event.channel, event.note);

        midi.addEvent (message, event.frame);
    }
}

void PianoforgeModule::process (float* left, float* right, int numSamples)
{
    if (numSamples > engineCapacity)
    {
        lastPeak = 0.0f;
        return;   // bloco maior que a reserva: pass-through silencioso no scratch
    }

    float* channels[2] = { left, right };
    juce::AudioBuffer<float> view (channels, 2, numSamples);

    engine.process (midi, view, numSamples, bypassedFlag);

    const auto meter = engine.readMeter();
    lastPeak = std::max (meter.peakLeft, meter.peakRight);
}

void PianoforgeModule::idle()
{
    engine.pollLoadResult();
    engine.reapRetiredBanks();

    applyPendingInstrument();

    // Carga externa pedida pela gaveta (FileChooser) ou re-colheita depois de um
    // re-prepare: a leitura vai ao background do PianoEngine, segura na thread de
    // mensagens com audio ativo.
    if (externalActive.load (std::memory_order_relaxed)
        && (! externalPath.empty() || loadedExternalName.empty()))
    {
        const juce::File file (juce::String (externalPath.empty() ? selectedPath : externalPath));
        externalPath.clear();

        if (file.existsAsFile())
        {
            loadedExternalName = file.getFileName().toStdString();
            loadedBank = -1;
            selectedInstrument.clear();   // bank novo: preset volta ao primeiro
            engine.loadBankFile (file);
        }
        else
        {
            externalActive.store (false, std::memory_order_relaxed);
            selectedPath.clear();
            loadedBank = -1;
            desiredBank.store (-1, std::memory_order_relaxed);
        }

        return;
    }

    if (externalActive.load (std::memory_order_relaxed))
        return;   // bank de fora do seletor: o knob nao manda ate nova escolha

    if (banks.empty())
        return;

    int wanted = desiredBank.load (std::memory_order_relaxed);

    if (wanted < 0)
        wanted = 0;

    if (wanted == loadedBank)
        return;

    if (! juce::isPositiveAndBelow (wanted, static_cast<int> (banks.size())))
        return;

    loadedBank = wanted;
    selectedInstrument.clear();   // bank novo: preset volta ao primeiro
    engine.loadBankFile (banks[static_cast<size_t> (wanted)]);
}

void PianoforgeModule::applyPendingInstrument()
{
    if (selectedInstrument.empty() || ! engine.hasBank() || engine.isLoading())
        return;

    const auto wanted = presetKey (selectedInstrument);

    if (presetKey (engine.getSelectedPresetName().toStdString()) == wanted)
        return;

    const int count = engine.getNumPresets();

    for (int i = 0; i < count; ++i)
    {
        if (presetKey (engine.getPresetName (i).toStdString()) == wanted)
        {
            engine.requestPreset (i);
            return;
        }
    }
}

void registerPianoforgeModule()
{
    if (forjarack::ModuleCatalog::indexOf ("pianoforge") >= 0)
        return;

    forjarack::ModuleSpec spec;
    spec.id = "pianoforge";
    spec.name = "PIANOFORGE";
    spec.category = forjarack::ModuleCategory::instrument;
    spec.accent = 0xffe06fa8;   // rosa dos instrumentos (mesma cor do catalogo)
    spec.role = forjarack::ModuleRole::instrument;
    spec.numParams = 2;
    spec.knobs[0] = { "VOLUME", 0.85f };
    spec.knobs[1] = { "BANK", 0.0f };
    spec.face = forjarack::ModuleFace::drum;   // face de instrumento generico

    spec.factory = [] () -> std::unique_ptr<forjarack::ModuleBase>
    {
        return std::make_unique<PianoforgeModule>();
    };

    forjarack::ModuleCatalog::registerModule (spec);
}

// ---- gaveta de arquivos (thread de mensagens) --------------------------------

std::vector<std::string> PianoforgeModule::drawerFileNames() const
{
    std::vector<std::string> names;
    names.reserve (banks.size());

    for (const auto& bank : banks)
        names.push_back (bank.getFullPathName().toStdString());

    return names;
}

std::string PianoforgeModule::drawerCurrentName() const
{
    if (externalActive.load (std::memory_order_relaxed))
        return loadedExternalName;

    if (loadedBank >= 0 && static_cast<size_t> (loadedBank) < banks.size())
        return banks[static_cast<size_t> (loadedBank)].getFileName().toStdString();

    return {};
}

bool PianoforgeModule::drawerSelectFile (const std::string& name)
{
    for (size_t i = 0; i < banks.size(); ++i)
    {
        if (banks[i].getFullPathName().toStdString() != name)
            continue;

        selectedPath = name;
        externalActive.store (false, std::memory_order_relaxed);
        loadedExternalName.clear();
        desiredBank.store (static_cast<int> (i), std::memory_order_relaxed);
        return true;
    }

    return false;
}

void PianoforgeModule::drawerLoadExternalFile (const std::string& path)
{
    selectedPath = path;
    externalActive.store (true, std::memory_order_relaxed);
    loadedExternalName.clear();
    externalPath = path;
}

// ---- instrumento (preset do bank carregado) --------------------------------

std::vector<std::string> PianoforgeModule::drawerInstrumentNames() const
{
    std::vector<std::string> names;

    if (! engine.hasBank() || engine.isLoading())
        return names;

    const int count = engine.getNumPresets();
    names.reserve (static_cast<size_t> (count));

    for (int i = 0; i < count; ++i)
        names.push_back (engine.getPresetName (i).toStdString());

    return names;
}

std::string PianoforgeModule::drawerCurrentInstrument() const
{
    return engine.getSelectedPresetName().toStdString();
}

void PianoforgeModule::drawerSelectInstrument (const std::string& name)
{
    selectedInstrument = name;
    applyPendingInstrument();   // se o bank ja esta pronto, aplica ja
}

} // namespace forja::pianoforge
