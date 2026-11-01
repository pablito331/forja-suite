#include "DrumEngine.h"

namespace forja::drumforge
{

DrumEngine::DrumEngine()
{
    for (int pad = 0; pad < kNumPads; ++pad)
    {
        meterPeak[(size_t) pad].store (0.0f, std::memory_order_relaxed);
        meterReduction[(size_t) pad].store (0.0f, std::memory_order_relaxed);
        meterClip[(size_t) pad].store (false, std::memory_order_relaxed);
        padPlayable[(size_t) pad] = false;
        lastLayer[(size_t) pad] = -1;
        lastRoundRobin[(size_t) pad] = -1;
    }

    noteToPad.fill (-1);
}

void DrumEngine::prepare (double newSampleRate, int maximumBlockSize)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    capacity = juce::jmax (32, maximumBlockSize);

    for (auto& buffer : padBuffers)
        buffer.setSize (2, capacity, false, true, true);

    for (auto& buffer : busBuffers)
        buffer.setSize (2, capacity, false, true, true);

    fx1Buffer.setSize (2, capacity, false, true, true);
    fx2Buffer.setSize (2, capacity, false, true, true);
    bypassEnvelope.setSize (1, capacity, false, true, true);

    voices.prepare (sampleRate);

    for (auto& strip : strips)
        strip.prepare (sampleRate);

    master.prepare (sampleRate);
    bypassSmoothing = mapping::smoothingCoefficient (10.0, sampleRate);

    reset();
}

void DrumEngine::reset()
{
    voices.reset();

    for (auto& strip : strips)
        strip.reset();

    master.reset();

    for (auto& buffer : padBuffers)
        buffer.clear();

    for (auto& buffer : busBuffers)
        buffer.clear();

    fx1Buffer.clear();
    fx2Buffer.clear();

    triggerWrite.store (0, std::memory_order_relaxed);
    triggerRead.store (0, std::memory_order_relaxed);
    bypassGain = 1.0;
}

void DrumEngine::requestKit (const LoadedKit* kit, int token) noexcept
{
    pendingKit.store (kit, std::memory_order_release);
    pendingToken.store (token, std::memory_order_release);
}

void DrumEngine::pushTrigger (int padIndex, float velocity, int offsetSamples) noexcept
{
    if (! juce::isPositiveAndBelow (padIndex, kNumPads))
        return;

    const int write = triggerWrite.load (std::memory_order_relaxed);
    const int read = triggerRead.load (std::memory_order_acquire);

    if (write - read >= triggerCapacity)
        return;

    triggerQueue[(size_t) (write % triggerCapacity)] = { padIndex, velocity, offsetSamples };
    triggerWrite.store (write + 1, std::memory_order_release);
}

bool DrumEngine::popTrigger (TriggerEvent& event) noexcept
{
    const int read = triggerRead.load (std::memory_order_relaxed);
    const int write = triggerWrite.load (std::memory_order_acquire);

    if (read >= write)
        return false;

    event = triggerQueue[(size_t) (read % triggerCapacity)];
    triggerRead.store (read + 1, std::memory_order_release);
    return true;
}

void DrumEngine::adoptPendingKit() noexcept
{
    const int token = pendingToken.load (std::memory_order_acquire);

    if (token == 0)
        return;

    const auto* kit = pendingKit.load (std::memory_order_acquire);

    voices.reset();
    activeKit = kit;

    noteToPad.fill (-1);

    for (int pad = 0; pad < kNumPads; ++pad)
    {
        const bool playable = kit != nullptr && ! kit->layers[(size_t) pad].empty();
        padPlayable[(size_t) pad] = playable;

        for (auto& cursor : roundRobinCursor[(size_t) pad])
            cursor = 0;

        if (! playable)
            continue;

        const int note = kit->kit.pads[(size_t) pad].note;

        if (juce::isPositiveAndBelow (note, 128))
            noteToPad[(size_t) note] = pad;
    }

    acceptedToken.store (token, std::memory_order_release);
    pendingToken.store (0, std::memory_order_release);
}

void DrumEngine::handleMessage (const juce::MidiMessage& message) noexcept
{
    if (! message.isNoteOn())
        return;

    const int note = message.getNoteNumber();

    if (! juce::isPositiveAndBelow (note, 128))
        return;

    const int pad = noteToPad[(size_t) note];

    if (pad >= 0)
        triggerPad (pad, message.getFloatVelocity());
}

void DrumEngine::triggerPad (int padIndex, float velocity) noexcept
{
    if (activeKit == nullptr || ! juce::isPositiveAndBelow (padIndex, kNumPads))
        return;

    if (! padPlayable[(size_t) padIndex])
        return;

    const auto& layers = activeKit->layers[(size_t) padIndex];
    const int layerCount = (int) layers.size();

    if (layerCount == 0)
        return;

    const float strength = juce::jlimit (0.0f, 1.0f, velocity);

    int layer = (int) (strength * (float) layerCount);
    layer = juce::jlimit (0, layerCount - 1, layer);

    const auto& roundRobins = layers[(size_t) layer];

    if (roundRobins.empty())
        return;

    const int roundRobinCount = (int) roundRobins.size();
    auto& cursor = roundRobinCursor[(size_t) padIndex][(size_t) layer];
    cursor %= roundRobinCount;

    const auto sample = roundRobins[(size_t) cursor].get();
    const int slot = cursor;
    cursor = (cursor + 1) % roundRobinCount;

    ++totalTriggers;
    lastLayer[(size_t) padIndex] = layer;
    lastRoundRobin[(size_t) padIndex] = slot;

    if (sample == nullptr || strength <= 0.0f)
        return;

    const int chokeGroup = activeKit->kit.pads[(size_t) padIndex].chokeGroup;

    // Choke curto (5 ms) antes de disparar o novo sample do mesmo grupo.
    voices.choke (chokeGroup, 0.005);

    voices.start (sample, padIndex, chokeGroup, strength * strength,
                  sample->sampleRate / sampleRate);
}

void DrumEngine::process (const juce::MidiBuffer& midi,
                          const std::array<juce::AudioBuffer<float>*, kNumOutputBuses>& outputs,
                          int numSamples,
                          bool bypassed) noexcept
{
    const int frames = juce::jmin (numSamples, capacity);

    if (frames <= 0)
        return;

    adoptPendingKit();

    anySolo = false;
    for (const auto& pad : parameters.pads)
        if (pad.solo)
            anySolo = true;

    for (int pad = 0; pad < kNumPads; ++pad)
    {
        const auto& settings = parameters.pads[(size_t) pad];
        const bool silenced = settings.mute || (anySolo && ! settings.solo);
        strips[(size_t) pad].setParameters (settings, silenced);
    }

    master.setParameters (parameters.master);

    for (auto& buffer : busBuffers)
        buffer.clear (0, frames);

    fx1Buffer.clear (0, frames);
    fx2Buffer.clear (0, frames);

    // Previa da UI/loops: drena a fila e ordena por offset, para os disparos
    // caírem no mesmo ponto do bloco que um evento MIDI equivalente.
    drainedCount = 0;

    TriggerEvent event;

    while (drainedCount < triggerCapacity && popTrigger (event))
    {
        event.offset = juce::jlimit (0, frames - 1, event.offset);
        drainedTriggers[(size_t) drainedCount++] = event;
    }

    for (int index = 1; index < drainedCount; ++index)
    {
        const auto key = drainedTriggers[(size_t) index];
        int scan = index - 1;

        while (scan >= 0 && drainedTriggers[(size_t) scan].offset > key.offset)
        {
            drainedTriggers[(size_t) (scan + 1)] = drainedTriggers[(size_t) scan];
            --scan;
        }

        drainedTriggers[(size_t) (scan + 1)] = key;
    }

    int position = 0;
    int triggerCursor = 0;

    auto midiEvent = midi.begin();
    const auto midiEnd = midi.end();

    while (position < frames)
    {
        int boundary = frames;

        if (triggerCursor < drainedCount)
            boundary = juce::jmin (boundary, drainedTriggers[(size_t) triggerCursor].offset);

        if (midiEvent != midiEnd)
            boundary = juce::jmin (boundary, juce::jlimit (0, frames, (*midiEvent).samplePosition));

        if (boundary > position)
        {
            renderSegment (position, boundary - position);
            position = boundary;
        }

        while (triggerCursor < drainedCount
               && drainedTriggers[(size_t) triggerCursor].offset <= position)
        {
            const auto& pending = drainedTriggers[(size_t) triggerCursor++];
            triggerPad (pending.pad, pending.velocity);
        }

        while (midiEvent != midiEnd
               && juce::jlimit (0, frames, (*midiEvent).samplePosition) <= position)
        {
            handleMessage ((*midiEvent).getMessage());
            ++midiEvent;
        }
    }

    float* mainLeft = busBuffers[0].getWritePointer (0);
    float* mainRight = busBuffers[0].getWritePointer (1);

    master.process (mainLeft, mainRight,
                    fx1Buffer.getWritePointer (0), fx1Buffer.getWritePointer (1),
                    fx2Buffer.getWritePointer (0), fx2Buffer.getWritePointer (1),
                    frames);

    const auto masterMeter = master.readMeters();
    masterPeakLeft.store (masterMeter.peakLeft, std::memory_order_relaxed);
    masterPeakRight.store (masterMeter.peakRight, std::memory_order_relaxed);
    masterReduction.store (masterMeter.reductionDb, std::memory_order_relaxed);
    masterCompReduction.store (masterMeter.compReductionDb, std::memory_order_relaxed);
    masterLimReduction.store (masterMeter.limReductionDb, std::memory_order_relaxed);

    if (masterMeter.clip)
        masterClip.store (true, std::memory_order_relaxed);

    const double bypassTarget = bypassed ? 0.0 : 1.0;

    // Rampa unica de 10 ms aplicada a todos os barramentos: bypass de
    // instrumento silencia a saida sem parar as vozes.
    if (bypassed || bypassGain < 0.999999)
    {
        float* envelope = bypassEnvelope.getWritePointer (0);

        for (int i = 0; i < frames; ++i)
        {
            bypassGain += (bypassTarget - bypassGain) * bypassSmoothing;
            envelope[i] = (float) bypassGain;
        }

        for (auto& buffer : busBuffers)
            for (int channel = 0; channel < 2; ++channel)
            {
                float* data = buffer.getWritePointer (channel);
                for (int i = 0; i < frames; ++i)
                    data[i] *= envelope[i];
            }
    }

    for (int bus = 0; bus < kNumOutputBuses; ++bus)
    {
        auto* destination = outputs[(size_t) bus];

        if (destination == nullptr)
            continue;

        const auto& source = busBuffers[(size_t) bus];
        const int channels = juce::jmin (2, destination->getNumChannels());

        for (int channel = 0; channel < channels; ++channel)
            destination->addFrom (channel, 0, source, channel, 0, frames, 1.0f);
    }

    publishMeters();
}

void DrumEngine::renderSegment (int offset, int numSamples) noexcept
{
    if (numSamples <= 0)
        return;

    for (int pad = 0; pad < kNumPads; ++pad)
    {
        auto& buffer = padBuffers[(size_t) pad];
        buffer.clear (0, numSamples);

        if (! padPlayable[(size_t) pad])
            continue;

        voices.render (buffer, pad, 0, numSamples);

        auto& strip = strips[(size_t) pad];
        strip.process (buffer.getWritePointer (0), buffer.getWritePointer (1), numSamples);

        const int bus = juce::jlimit (0, kNumOutputBuses - 1, strip.outputBus());
        auto& destination = busBuffers[(size_t) bus];

        for (int channel = 0; channel < 2; ++channel)
            destination.addFrom (channel, offset, buffer, channel, 0, numSamples, 1.0f);

        const auto send1 = (float) strip.getSend1();
        if (send1 > 0.0f)
        {
            fx1Buffer.addFrom (0, offset, buffer, 0, 0, numSamples, send1);
            fx1Buffer.addFrom (1, offset, buffer, 1, 0, numSamples, send1);
        }

        const auto send2 = (float) strip.getSend2();
        if (send2 > 0.0f)
        {
            fx2Buffer.addFrom (0, offset, buffer, 0, 0, numSamples, send2);
            fx2Buffer.addFrom (1, offset, buffer, 1, 0, numSamples, send2);
        }
    }
}

void DrumEngine::publishMeters() noexcept
{
    for (int pad = 0; pad < kNumPads; ++pad)
    {
        auto& strip = strips[(size_t) pad];

        const float peak = strip.readPeak();
        const float reduction = strip.readReductionDb();

        if (peak > meterPeak[(size_t) pad].load (std::memory_order_relaxed))
            meterPeak[(size_t) pad].store (peak, std::memory_order_relaxed);

        if (reduction < meterReduction[(size_t) pad].load (std::memory_order_relaxed))
            meterReduction[(size_t) pad].store (reduction, std::memory_order_relaxed);

        if (strip.readClip())
            meterClip[(size_t) pad].store (true, std::memory_order_relaxed);
    }
}

DrumEngine::PadMeter DrumEngine::readPadMeter (int padIndex) noexcept
{
    PadMeter meter;

    if (! juce::isPositiveAndBelow (padIndex, kNumPads))
        return meter;

    meter.peak = meterPeak[(size_t) padIndex].exchange (0.0f, std::memory_order_relaxed);
    meter.reductionDb = meterReduction[(size_t) padIndex].exchange (0.0f, std::memory_order_relaxed);
    meter.clip = meterClip[(size_t) padIndex].exchange (false, std::memory_order_relaxed);
    return meter;
}

DrumEngine::MasterMeter DrumEngine::readMasterMeter() noexcept
{
    MasterMeter meter;
    meter.peakLeft = masterPeakLeft.exchange (0.0f, std::memory_order_relaxed);
    meter.peakRight = masterPeakRight.exchange (0.0f, std::memory_order_relaxed);
    meter.reductionDb = masterReduction.exchange (0.0f, std::memory_order_relaxed);
    meter.compReductionDb = masterCompReduction.exchange (0.0f, std::memory_order_relaxed);
    meter.limReductionDb = masterLimReduction.exchange (0.0f, std::memory_order_relaxed);
    meter.clip = masterClip.exchange (false, std::memory_order_relaxed);
    return meter;
}

int DrumEngine::lastTriggeredLayer (int padIndex) const noexcept
{
    return juce::isPositiveAndBelow (padIndex, kNumPads) ? lastLayer[(size_t) padIndex] : -1;
}

int DrumEngine::lastTriggeredRoundRobin (int padIndex) const noexcept
{
    return juce::isPositiveAndBelow (padIndex, kNumPads) ? lastRoundRobin[(size_t) padIndex] : -1;
}

bool DrumEngine::isPadPlayable (int padIndex) const noexcept
{
    return juce::isPositiveAndBelow (padIndex, kNumPads) && padPlayable[(size_t) padIndex];
}

} // namespace forja::drumforge
