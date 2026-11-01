#include "CaniveteRackModule.h"

#include <algorithm>

#include "BankLibrary.h"
#include "rack/ModuleCatalog.h"

namespace forja::canivete
{

CaniveteModule::CaniveteModule() = default;

void CaniveteModule::refreshLibrary()
{
    // Somente thread de mensagens: varre Documents/Forja/Canivete. O audio nao
    // le banks[], so o tamanho publicado em bankCount.
    const auto found = findBanks (SoundFontEngine::sf3Available());

    banks.clear();
    banks.reserve (static_cast<size_t> (found.size()));

    for (const auto& file : found)
        banks.push_back (file);

    bankCount.store (static_cast<int> (banks.size()), std::memory_order_release);
}

void CaniveteModule::reselectAfterRefresh()
{
    // Re-prepare / BUSCAR: a escolha ativa vira indice contra a lista nova.
    // Externo continua externo (o knob fica mudo para o idle ate a proxima
    // escolha pela lista); nome que sumiu da biblioteca recai no default 0.
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

void CaniveteModule::prepare (double sampleRate)
{
    refreshLibrary();

    // Pre-aloca o MidiBuffer na thread de mensagens: o processo de audio so da
    // clear/addEvent e, depois da primeira carga, nunca mais pede memoria.
    midi.ensureSize (forjarack::kMaxRackMidiEvents * 3 * static_cast<int> (sizeof (juce::int32)));

    engine.prepare (sampleRate, engineCapacity);

    // Re-adota a escolha da gaveta contra a lista recem-escaneada (default: o
    // bank indicado pelo knob no proximo idle()).
    reselectAfterRefresh();
}

void CaniveteModule::setParam (int index, float value)
{
    switch (index)
    {
        case 0:
            engine.setVolume (juce::jlimit (0.0f, 1.0f, value));
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

void CaniveteModule::processMidi (const forjarack::MidiEventSpan& span) noexcept
{
    // Materializa o span POD no MidiBuffer que o engine le. O nucleo do rack so
    // expoe notas on/off; program change/CC ficam para o caminho do plugin.
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

void CaniveteModule::process (float* left, float* right, int numSamples)
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

void CaniveteModule::idle()
{
    engine.pollLoadResult();
    engine.reapRetiredBanks();

    // Assentou o preset pedido pela gaveta? Roda no topo para valer em todos
    // os estados ociosos (bank pronto, sem nova carga): no-op enquanto o bank
    // ainda esta carregando ou o nome ja esta em uso.
    applyPendingInstrument();

    // Carga externa pedida pela gaveta (FileChooser) ou re-colheita depois de
    // um re-prepare: a leitura vai ao background do SoundFontEngine, segura na
    // thread de mensagens com audio ativo.
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
            // Arquivo sumiu (escolha persistida de outra maquina): o knob
            // volta a mandar e a gaveta recai no bank indicado pelo indice.
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

    // Primeiro idle apos prepare: adota o bank indicado pelo knob (default 0).
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

void CaniveteModule::applyPendingInstrument()
{
    // So faz algo com um bank tocavel e um nome pedido. Idempotente: se o
    // preset em uso ja e o pedido, nao re-toca no audio.
    if (selectedInstrument.empty() || ! engine.hasBank() || engine.isLoading())
        return;

    if (engine.getSelectedPresetName().toStdString() == selectedInstrument)
        return;

    const int count = engine.getNumPresets();

    for (int i = 0; i < count; ++i)
    {
        if (engine.getPresetName (i).toStdString() == selectedInstrument)
        {
            engine.requestPreset (i);
            return;
        }
    }
}

void registerCaniveteModule()
{
    if (forjarack::ModuleCatalog::indexOf ("canivete") >= 0)
        return;

    forjarack::ModuleSpec spec;
    spec.id = "canivete";
    spec.name = "CANIVETE";
    spec.category = forjarack::ModuleCategory::instrument;
    spec.accent = 0xffe06fa8;   // rosa dos instrumentos (mesma cor do catalogo)
    spec.role = forjarack::ModuleRole::instrument;
    spec.numParams = 2;
    spec.knobs[0] = { "VOLUME", 0.5f };
    spec.knobs[1] = { "BANK", 0.0f };
    spec.face = forjarack::ModuleFace::drum;   // face de instrumento generico

    spec.factory = [] () -> std::unique_ptr<forjarack::ModuleBase>
    {
        return std::make_unique<CaniveteModule>();
    };

    forjarack::ModuleCatalog::registerModule (spec);
}

// ---- gaveta de arquivos (thread de mensagens) --------------------------------

std::vector<std::string> CaniveteModule::drawerFileNames() const
{
    // Caminhos completos: sao o identificador da lista e a chave de match de
    // drawerSelectFile. A UI mostra so o nome; o nucleo nao conhece juce::File.
    std::vector<std::string> names;
    names.reserve (banks.size());

    for (const auto& bank : banks)
        names.push_back (bank.getFullPathName().toStdString());

    return names;
}

std::string CaniveteModule::drawerCurrentName() const
{
    if (externalActive.load (std::memory_order_relaxed))
        return loadedExternalName;

    if (loadedBank >= 0 && static_cast<size_t> (loadedBank) < banks.size())
        return banks[static_cast<size_t> (loadedBank)].getFileName().toStdString();

    return {};
}

bool CaniveteModule::drawerSelectFile (const std::string& name)
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

void CaniveteModule::drawerLoadExternalFile (const std::string& path)
{
    selectedPath = path;
    externalActive.store (true, std::memory_order_relaxed);
    loadedExternalName.clear();
    externalPath = path;
}

// ---- instrumento (preset do bank carregado) --------------------------------

std::vector<std::string> CaniveteModule::drawerInstrumentNames() const
{
    // A lista so faz sentido com um bank tocavel; durante a carga em background
    // (ou sem bank) devolve vazio e a gaveta esconde o seletor.
    std::vector<std::string> names;

    if (! engine.hasBank() || engine.isLoading())
        return names;

    const int count = engine.getNumPresets();
    names.reserve (static_cast<size_t> (count));

    for (int i = 0; i < count; ++i)
        names.push_back (engine.getPresetName (i).toStdString());

    return names;
}

std::string CaniveteModule::drawerCurrentInstrument() const
{
    return engine.getSelectedPresetName().toStdString();
}

void CaniveteModule::drawerSelectInstrument (const std::string& name)
{
    selectedInstrument = name;
    applyPendingInstrument();   // se o bank ja esta pronto, aplica ja
}

} // namespace forja::canivete
