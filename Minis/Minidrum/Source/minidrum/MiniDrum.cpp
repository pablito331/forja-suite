#include "MiniDrum.h"

#include <algorithm>

namespace forja::minidrum
{

using forja::drumforge::EngineParameters;
using forja::drumforge::kNumOutputBuses;
using forja::drumforge::kNumPads;
using forja::drumforge::LoadedKit;

MiniDrum::MiniDrum()
{
    alive = std::make_shared<std::atomic<bool>> (true);
}

MiniDrum::~MiniDrum()
{
    alive->store (false, std::memory_order_release);
    store.cancel();   // espera o job de fundo; o callback ainda checa a guarda
}

void MiniDrum::prepare (double newSampleRate, int maximumBlockSize)
{
    sampleRate = newSampleRate;
    engine.prepare (newSampleRate, maximumBlockSize);
}

void MiniDrum::process (juce::MidiBuffer& midi,
                        juce::AudioBuffer<float>& stereo,
                        int numSamples,
                        bool bypassed) noexcept
{
    if (auto* kit = pendingKitRaw.exchange (nullptr, std::memory_order_acq_rel))
        currentKit = kit;

    engine.setParameters (buildParameters());

    std::array<juce::AudioBuffer<float>*, kNumOutputBuses> buses {};
    buses[0] = &stereo;   // sem mesa no mini: os quatro auxiliares sao descartados

    engine.process (midi, buses, numSamples, bypassed);
}

EngineParameters MiniDrum::buildParameters() const noexcept
{
    EngineParameters params {};

    // No MiniDrum o knob VOLUME manda no trim do master (0,5 = 0 dB); a mix
    // gravada no kit preenche os pads e os demais controles do master.
    params.master.trim = volume.load (std::memory_order_relaxed);

    if (currentKit != nullptr && currentKit->hasMix)
    {
        for (int pad = 0; pad < kNumPads; ++pad)
        {
            const auto& mix = currentKit->mixers[static_cast<size_t> (pad)];
            auto& out = params.pads[static_cast<size_t> (pad)];

            out.trim = mix.trim;
            out.fader = mix.fader;
            out.pan = mix.pan;
            out.hpf = mix.hpf;
            out.eqLow = mix.eqLow;
            out.eqMid = mix.eqMid;
            out.eqHigh = mix.eqHigh;
            out.compThreshold = mix.compThr;
            out.compRatio = mix.compRatio;
            out.send1 = mix.send1;
            out.send2 = mix.send2;
        }

        params.master.comp = currentKit->master.comp;
        params.master.fx1Regen = currentKit->master.fx1Regen;
        params.master.fx1Position = currentKit->master.fx1Position;
        params.master.fx2Drive = currentKit->master.fx2Drive;
    }

    return params;
}

void MiniDrum::loadKitFile (const juce::File& file, Completion onDone)
{
    forja::drumforge::Kit kit;
    juce::String error;

    if (! forja::drumforge::Kit::loadFromFile (file, kit, error))
    {
        status = "Falha ao ler o kit: " + error;

        if (onDone != nullptr)
            onDone (status);

        return;
    }

    status = "Lendo samples de " + kit.name + "...";

    store.loadAsync (std::move (kit),
                     [this, guard = alive, onDone] (std::unique_ptr<const LoadedKit> loaded,
                                                    juce::String loadError) mutable
    {
        if (! guard->load (std::memory_order_acquire))
            return;

        // Kit sem nenhum sample legivel nao substitui o tocador atual.
        if (loaded == nullptr || loaded->totalSamples == 0)
        {
            status = loaded == nullptr ? "Carga interrompida"
                                       : "Kit sem audio utilizavel: " + loadError;

            if (onDone != nullptr)
                onDone (status);

            return;
        }

        adopt (std::move (loaded));

        status = loadError.isEmpty() ? "Kit " + getKitName() + " pronto"
                                     : "Kit " + getKitName() + ": " + loadError;

        if (onDone != nullptr)
            onDone (loadError);
    });
}

void MiniDrum::cancelLoad()
{
    store.cancel();
    status = "Carga cancelada";
}

bool MiniDrum::isLoading() const
{
    return store.isLoading();
}

int MiniDrum::getProgress() const
{
    return store.getProgress();
}

juce::String MiniDrum::getKitName() const
{
    return activeKit != nullptr ? activeKit->kit.name : juce::String ("Nenhum kit");
}

void MiniDrum::adopt (std::unique_ptr<const LoadedKit> loaded)
{
    std::shared_ptr<const LoadedKit> shared (std::move (loaded));
    const int newToken = kitToken + 1;

    // O kit substituido entra na fila de reap; so sai quando o engine publicar
    // o token novo (currentKit tambem ja tera trocado nesse mesmo bloco).
    if (activeKit != nullptr)
        retiredKits.push_back ({ std::move (activeKit), newToken });

    activeKit = shared;
    kitToken = newToken;

    pendingKitRaw.store (&shared->kit, std::memory_order_release);
    engine.requestKit (shared.get(), newToken);
}

void MiniDrum::reapRetiredKits()
{
    const int accepted = engine.acceptedKitToken();

    retiredKits.erase (std::remove_if (retiredKits.begin(), retiredKits.end(),
                                       [accepted] (const RetiredKit& r)
                                       {
                                           return accepted >= r.token;
                                       }),
                       retiredKits.end());
}

} // namespace forja::minidrum
