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

void CaniveteModule::prepare (double sampleRate)
{
    refreshLibrary();

    // Pre-aloca o MidiBuffer na thread de mensagens: o processo de audio so da
    // clear/addEvent e, depois da primeira carga, nunca mais pede memoria.
    midi.ensureSize (forjarack::kMaxRackMidiEvents * 3 * static_cast<int> (sizeof (juce::int32)));

    engine.prepare (sampleRate, engineCapacity);

    // Recomeca do bank indicado pelo knob no proximo idle().
    loadedBank = -1;
    desiredBank.store (-1, std::memory_order_relaxed);
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
    engine.loadBankFile (banks[static_cast<size_t> (wanted)]);
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

} // namespace forja::canivete
