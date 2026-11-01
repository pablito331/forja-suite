#include "MiniDrumRackModule.h"

#include <algorithm>

#include "KitLibrary.h"
#include "rack/ModuleCatalog.h"

namespace forja::minidrum
{

MiniDrumModule::MiniDrumModule() = default;

void MiniDrumModule::refreshLibrary()
{
    // Somente thread de mensagens: varre a biblioteca compartilhada com o
    // Drumforge. O audio nao le kits[], so o tamanho publicado em kitCount.
    const auto found = findKits();

    kits.clear();
    kits.reserve (static_cast<size_t> (found.size()));

    for (const auto& file : found)
        kits.push_back (file);

    kitCount.store (static_cast<int> (kits.size()), std::memory_order_release);
}

void MiniDrumModule::reselectAfterRefresh()
{
    // Re-prepare / BUSCAR: a escolha ativa vira indice contra a lista nova.
    // Externo continua externo (o knob fica mudo para o idle ate a proxima
    // escolha pela lista); nome que sumiu da biblioteca recai no default 0.
    if (selectedPath.empty())
    {
        externalActive.store (false, std::memory_order_relaxed);
        desiredKit.store (-1, std::memory_order_relaxed);
        loadedKit = -1;
        return;
    }

    for (size_t i = 0; i < kits.size(); ++i)
    {
        if (kits[i].getFullPathName().toStdString() != selectedPath)
            continue;

        externalActive.store (false, std::memory_order_relaxed);
        loadedKit = static_cast<int> (i);
        desiredKit.store (loadedKit, std::memory_order_relaxed);
        return;
    }

    if (externalActive.load (std::memory_order_relaxed))
    {
        loadedKit = -1;   // externo: idle colhera externalPath / recarrega
        return;
    }

    selectedPath.clear();
    externalActive.store (false, std::memory_order_relaxed);
    desiredKit.store (-1, std::memory_order_relaxed);
    loadedKit = -1;
}

void MiniDrumModule::prepare (double sampleRate)
{
    refreshLibrary();

    // Pre-aloca o MidiBuffer na thread de mensagens: o processo de audio so da
    // clear/addEvent e, depois da primeira carga, nunca mais pede memoria.
    midi.ensureSize (forjarack::kMaxRackMidiEvents * 3 * static_cast<int> (sizeof (juce::int32)));

    drum.prepare (sampleRate, engineCapacity);

    // Re-adota a escolha da gaveta contra a lista recem-escaneada (default: o
    // kit indicado pelo knob no proximo idle()).
    reselectAfterRefresh();
}

void MiniDrumModule::setParam (int index, float value)
{
    switch (index)
    {
        case 0:
            drum.setVolume (juce::jlimit (0.0f, 1.0f, value));
            break;

        case 1:
        {
            const int count = kitCount.load (std::memory_order_acquire);

            if (count > 0)
            {
                const auto normalized = juce::jlimit (0.0f, 1.0f, value);
                desiredKit.store (juce::roundToInt (normalized * float (count - 1)),
                                  std::memory_order_relaxed);
            }

            break;
        }

        default:
            break;
    }
}

void MiniDrumModule::processMidi (const forjarack::MidiEventSpan& span) noexcept
{
    // Materializa o span POD no MidiBuffer que o engine le. Notas ja vem como
    // eventos curtos; o canal e 1-based (convencao JUCE) e a velocidade 0..127.
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

void MiniDrumModule::process (float* left, float* right, int numSamples)
{
    if (numSamples > engineCapacity)
    {
        lastPeak = 0.0f;
        return;   // bloco maior que a reserva: pass-through silencioso no scratch
    }

    float* channels[2] = { left, right };
    juce::AudioBuffer<float> view (channels, 2, numSamples);

    drum.process (midi, view, numSamples, bypassedFlag);

    const auto meter = drum.readMasterMeter();
    lastPeak = std::max (meter.peakLeft, meter.peakRight);
}

void MiniDrumModule::idle()
{
    drum.reapRetiredKits();

    // Carga externa pedida pela gaveta (FileChooser) ou re-colheita depois de
    // um re-prepare: roda no loader em background do MiniDrum, seguro na
    // thread de mensagens com audio ativo.
    if (externalActive.load (std::memory_order_relaxed)
        && (! externalPath.empty() || loadedExternalName.empty()))
    {
        const juce::File file (juce::String (externalPath.empty() ? selectedPath : externalPath));
        externalPath.clear();

        if (file.existsAsFile() || file.isDirectory())
        {
            loadedExternalName = file.getFileName().toStdString();
            loadedKit = -1;
            drum.loadKitFile (file);
        }
        else
        {
            // Arquivo sumiu (escolha persistida de outra maquina): o knob
            // volta a mandar e a gaveta recai no kit indicado pelo indice.
            externalActive.store (false, std::memory_order_relaxed);
            selectedPath.clear();
            loadedKit = -1;
            desiredKit.store (-1, std::memory_order_relaxed);
        }

        return;
    }

    if (externalActive.load (std::memory_order_relaxed))
        return;   // kit de fora do seletor: o knob nao manda ate nova escolha

    if (kits.empty())
        return;

    // Primeiro idle apos prepare: adota o kit indicado pelo knob (default 0).
    int wanted = desiredKit.load (std::memory_order_relaxed);

    if (wanted < 0)
        wanted = 0;

    if (wanted == loadedKit)
        return;

    if (! juce::isPositiveAndBelow (wanted, static_cast<int> (kits.size())))
        return;

    loadedKit = wanted;
    drum.loadKitFile (kits[static_cast<size_t> (wanted)]);
}

void registerMinidrumModule()
{
    if (forjarack::ModuleCatalog::indexOf ("minidrum") >= 0)
        return;

    forjarack::ModuleSpec spec;
    spec.id = "minidrum";
    spec.name = "MINIDRUM";
    spec.category = forjarack::ModuleCategory::instrument;
    spec.accent = 0xffe06fa8;   // rosa dos instrumentos (mesma cor do catalogo)
    spec.role = forjarack::ModuleRole::instrument;
    spec.numParams = 2;
    spec.knobs[0] = { "VOLUME", 0.5f };
    spec.knobs[1] = { "KIT", 0.0f };
    spec.face = forjarack::ModuleFace::drum;

    spec.factory = [] () -> std::unique_ptr<forjarack::ModuleBase>
    {
        return std::make_unique<MiniDrumModule>();
    };

    forjarack::ModuleCatalog::registerModule (spec);
}

// ---- gaveta de arquivos (thread de mensagens) --------------------------------

std::vector<std::string> MiniDrumModule::drawerFileNames() const
{
    // Caminhos completos: sao o identificador da lista e a chave de match de
    // drawerSelectFile. A UI mostra so o nome; o nucleo nao conhece juce::File.
    std::vector<std::string> names;
    names.reserve (kits.size());

    for (const auto& kit : kits)
        names.push_back (kit.getFullPathName().toStdString());

    return names;
}

std::string MiniDrumModule::drawerCurrentName() const
{
    if (externalActive.load (std::memory_order_relaxed))
        return loadedExternalName;

    if (loadedKit >= 0 && static_cast<size_t> (loadedKit) < kits.size())
        return kits[static_cast<size_t> (loadedKit)].getFileName().toStdString();

    return {};
}

bool MiniDrumModule::drawerSelectFile (const std::string& name)
{
    for (size_t i = 0; i < kits.size(); ++i)
    {
        if (kits[i].getFullPathName().toStdString() != name)
            continue;

        selectedPath = name;
        externalActive.store (false, std::memory_order_relaxed);
        loadedExternalName.clear();
        desiredKit.store (static_cast<int> (i), std::memory_order_relaxed);
        return true;
    }

    return false;
}

void MiniDrumModule::drawerLoadExternalFile (const std::string& path)
{
    selectedPath = path;
    externalActive.store (true, std::memory_order_relaxed);
    loadedExternalName.clear();
    externalPath = path;
}

} // namespace forja::minidrum
