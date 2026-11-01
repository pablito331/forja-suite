#include "RackEngine.h"

#include <algorithm>
#include <cmath>

#include "ModuleCatalog.h"

namespace forjarack
{

float RackEngine::trimGain (float normalized) noexcept
{
    // 0..1 centrado em 0.5 -> -24..+24 dB.
    const double db = (static_cast<double> (normalized) - 0.5) * 48.0;
    return static_cast<float> (std::pow (10.0, db * 0.05));
}

void RackEngine::prepare (double newSampleRate, int maxBlockSamples)
{
    sampleRate = newSampleRate;

    // Capacidade fixa do scratch de instrumentos, decidida aqui (nunca no audio).
    const int capacity = std::max (1, maxBlockSamples);
    this->maxBlockSamples = capacity;   // o parametro esconde o membro
    scratchL.resize (static_cast<size_t> (capacity));
    scratchR.resize (static_cast<size_t> (capacity));

    for (auto& module : modules)
        if (module != nullptr)
        {
            module->setBlockSize (capacity);
            module->prepare (sampleRate);
        }

    inPeakL.store (0.0f);
    inPeakR.store (0.0f);
    outPeakL.store (0.0f);
    outPeakR.store (0.0f);
    clipFlag.store (false);

    for (auto& peak : slotPeaks)
        peak.store (0.0f);

    for (auto& gr : slotGrDb)
        gr.store (0.0f);
}

void RackEngine::release()
{
    for (auto& peak : slotPeaks)
        peak.store (0.0f);

    for (auto& gr : slotGrDb)
        gr.store (0.0f);

    inPeakL.store (0.0f);
    inPeakR.store (0.0f);
    outPeakL.store (0.0f);
    outPeakR.store (0.0f);
}

void RackEngine::idleModules()
{
    // Thread de mensagens: cada modulo vivo faz sua faxina interna (libertar
    // kits substituidos). Nada aqui toca no caminho de audio.
    for (auto& module : modules)
        if (module != nullptr)
            module->idle();
}

void RackEngine::setSlotModule (int slot, int moduleIndex)
{
    if (slot < 0 || slot >= kNumSlots)
        return;

    slotIndices[(size_t) slot] = moduleIndex;
    auto& target = modules[(size_t) slot];

    target = ModuleCatalog::create (moduleIndex);

    if (target == nullptr)
    {
        slotIndices[(size_t) slot] = 0;
        slotPeaks[(size_t) slot].store (0.0f);
        slotGrDb[(size_t) slot].store (0.0f);
        return;
    }

    target->setBlockSize (maxBlockSamples);
    target->prepare (sampleRate);
}

void RackEngine::adoptSlotModule (int slot, int moduleIndex, std::unique_ptr<ModuleBase> module)
{
    if (slot < 0 || slot >= kNumSlots)
        return;

    slotIndices[(size_t) slot] = moduleIndex;
    auto& target = modules[(size_t) slot];

    target = std::move (module);

    if (target == nullptr)
    {
        slotIndices[(size_t) slot] = 0;
        slotPeaks[(size_t) slot].store (0.0f);
        slotGrDb[(size_t) slot].store (0.0f);
        return;
    }

    target->setBlockSize (maxBlockSamples);
    target->prepare (sampleRate);
}

int RackEngine::latencySamples() const
{
    // A corrente e em serie: a latencia percebida do rack e o maior atraso
    // reportado por um modulo vivo. Os adaptadores Airwindows devolvem 0 (o
    // atraso de delay/reverb e musical, PDC fica em 0); um plugin hospedado pode
    // reportar latencia real, que entao subi para o host compensar.
    int maxLatency = 0;

    for (const auto& module : modules)
        if (module != nullptr)
            maxLatency = std::max (maxLatency, module->latencySamples ());

    return maxLatency;
}

void RackEngine::process (const RackSnapshot& snapshot, float* left, float* right,
                          int numSamples, const MidiEventSpan& midi)
{
    if (numSamples <= 0 || left == nullptr || right == nullptr)
        return;

    if (clipResetRequested.exchange (false, std::memory_order_acquire))
        clipFlag.store (false, std::memory_order_relaxed);

    // Nota no bloco: o led MIDI do master precisa acender mesmo em faixa de
    // instrumento, onde o medidor de entrada ve apenas silencio.
    if (midi.numEvents > 0)
        midiArrived.store (true, std::memory_order_relaxed);

    // Trim de entrada + pico medido depois do trim (referencia do led IN).
    const float gainIn = trimGain (snapshot.inputTrim);
    float peakInL = 0.0f;
    float peakInR = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        left[i] *= gainIn;
        right[i] *= gainIn;

        peakInL = std::max (peakInL, std::abs (left[i]));
        peakInR = std::max (peakInR, std::abs (right[i]));
    }

    inPeakL.store (peakInL, std::memory_order_relaxed);
    inPeakR.store (peakInR, std::memory_order_relaxed);

    // Alguem esta em solo? Gaveta em bypass nao conta como solo: ligar o solo
    // numa gaveta ja desligada nao pode silenciar o rack inteiro.
    bool anySolo = false;

    for (const auto& slot : snapshot.slots)
        if (slot.solo && ! slot.bypass)
            anySolo = true;

    for (int slot = 0; slot < kNumSlots; ++slot)
    {
        const auto& s = snapshot.slots[(size_t) slot];
        auto* module = modules[(size_t) slot].get();

        const bool mutedBySolo = anySolo && ! s.solo;
        const bool run = module != nullptr && ! s.bypass && ! mutedBySolo;

        if (! run)
        {
            slotPeaks[(size_t) slot].store (0.0f, std::memory_order_relaxed);
            slotGrDb[(size_t) slot].store (0.0f, std::memory_order_relaxed);
            continue;
        }

        for (int k = 0; k < kMaxModuleParams; ++k)
            module->setParam (k, s.params[(size_t) k]);

        if (module->isInstrument())
        {
            // Instrumento soma sua voz na cadeia; o sinal recebido passa
            // intacto. Bloco maior que o scratch: pulado (pass-through).
            if (numSamples > static_cast<int> (scratchL.size()))
            {
                slotPeaks[(size_t) slot].store (0.0f, std::memory_order_relaxed);
                slotGrDb[(size_t) slot].store (0.0f, std::memory_order_relaxed);
                continue;
            }

            module->processMidi (midi);

            std::fill_n (scratchL.data(), static_cast<size_t> (numSamples), 0.0f);
            std::fill_n (scratchR.data(), static_cast<size_t> (numSamples), 0.0f);

            module->process (scratchL.data(), scratchR.data(), numSamples);

            for (int i = 0; i < numSamples; ++i)
            {
                left[i] += scratchL[(size_t) i];
                right[i] += scratchR[(size_t) i];
            }
        }
        else
        {
            module->process (left, right, numSamples);
        }

        slotPeaks[(size_t) slot].store (module->readOutputPeak(), std::memory_order_relaxed);
        slotGrDb[(size_t) slot].store (module->readGainReductionDb(), std::memory_order_relaxed);
    }

    if (snapshot.phaseInvert)
        for (int i = 0; i < numSamples; ++i)
            right[i] = -right[i];

    // Trim de saida + medidor master e clip.
    const float gainOut = trimGain (snapshot.outputTrim);
    float peakOutL = 0.0f;
    float peakOutR = 0.0f;
    bool clipped = false;

    for (int i = 0; i < numSamples; ++i)
    {
        left[i] *= gainOut;
        right[i] *= gainOut;

        const float a = std::abs (left[i]);
        const float b = std::abs (right[i]);

        peakOutL = std::max (peakOutL, a);
        peakOutR = std::max (peakOutR, b);

        if (a > 0.999f || b > 0.999f)
            clipped = true;
    }

    outPeakL.store (peakOutL, std::memory_order_relaxed);
    outPeakR.store (peakOutR, std::memory_order_relaxed);

    if (clipped)
        clipFlag.store (true, std::memory_order_relaxed);
}

float RackEngine::readSlotPeak (int slot)
{
    if (slot < 0 || slot >= kNumSlots)
        return 0.0f;

    return slotPeaks[(size_t) slot].exchange (0.0f, std::memory_order_relaxed);
}

float RackEngine::readSlotGainReductionDb (int slot) const
{
    if (slot < 0 || slot >= kNumSlots)
        return 0.0f;

    return slotGrDb[(size_t) slot].load (std::memory_order_relaxed);
}

RackEngine::MasterMeters RackEngine::readMasterMeters()
{
    MasterMeters meters;
    meters.inPeakL = inPeakL.exchange (0.0f, std::memory_order_relaxed);
    meters.inPeakR = inPeakR.exchange (0.0f, std::memory_order_relaxed);
    meters.outPeakL = outPeakL.exchange (0.0f, std::memory_order_relaxed);
    meters.outPeakR = outPeakR.exchange (0.0f, std::memory_order_relaxed);
    meters.clip = clipFlag.load (std::memory_order_relaxed);
    // Consumo: o led fica aceso por um tempo na UI, mas o sinal e de evento.
    meters.midi = midiArrived.exchange (false, std::memory_order_relaxed);
    return meters;
}

} // namespace forjarack
