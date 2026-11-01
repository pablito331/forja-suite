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

void MiniDrumModule::prepare (double sampleRate)
{
    refreshLibrary();

    // Pre-aloca o MidiBuffer na thread de mensagens: o processo de audio so da
    // clear/addEvent e, depois da primeira carga, nunca mais pede memoria.
    midi.ensureSize (forjarack::kMaxRackMidiEvents * 3 * static_cast<int> (sizeof (juce::int32)));

    drum.prepare (sampleRate, engineCapacity);

    // Recomeca do kit indicado pelo knob no proximo idle().
    loadedKit = -1;
    desiredKit.store (-1, std::memory_order_relaxed);
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

} // namespace forja::minidrum
