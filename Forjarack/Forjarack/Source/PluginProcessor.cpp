#include "PluginProcessor.h"

#include "PluginEditor.h"
#include "rack/ModuleCatalog.h"
#include "minidrum/MiniDrumRackModule.h"
#include "minisat/MiniSatRackModule.h"

namespace
{
juce::String dbText (float normalized)
{
    const double db = (static_cast<double> (normalized) - 0.5) * 48.0;
    return (db > 0.0 ? "+" : "") + juce::String (db, 1) + " dB";
}
} // namespace

ForjarackAudioProcessor::ForjarackAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
                                .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    cacheParameterPointers();
    startTimerHz (20);
}

ForjarackAudioProcessor::~ForjarackAudioProcessor()
{
    stopTimer();
}

juce::String ForjarackAudioProcessor::slotId (int slot, const juce::String& suffix)
{
    return "slot" + juce::String (slot + 1) + "_" + suffix;
}

juce::String ForjarackAudioProcessor::knobId (int slot, int knob)
{
    return slotId (slot, "knob" + juce::String (knob));
}

juce::AudioProcessorValueTreeState::ParameterLayout ForjarackAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Gavetas registram antes das choices: o catalogo e append-only e a lista
    // de slots e montada a partir dele.
    forja::minidrum::registerMinidrumModule();
    forja::minisat::registerMinisatModule();

    juce::StringArray choices;

    for (int i = 0; i < ModuleCatalog::count(); ++i)
        choices.add (ModuleCatalog::specByIndex (i).name);

    for (int slot = 0; slot < kNumSlots; ++slot)
    {
        const auto label = "Slot " + juce::String (slot + 1) + " ";

        layout.add (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { slotId (slot, "type"), 1 }, label + "Modulo", choices, 0));

        layout.add (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { slotId (slot, "bypass"), 1 }, label + "Bypass", false));
        layout.add (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { slotId (slot, "solo"), 1 }, label + "Solo", false));

        for (int knob = 0; knob < kMaxModuleParams; ++knob)
        {
            layout.add (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { knobId (slot, knob), 1 },
                label + "Knob " + juce::String (knob + 1),
                juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));
        }
    }

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "input_trim", 1 }, "Input Trim",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [] (float v, int) { return dbText (v); })));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "output_trim", 1 }, "Output Trim",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [] (float v, int) { return dbText (v); })));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "phase_invert", 1 }, "Phase Invert R", false));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "bypass", 1 }, "Bypass", false));

    return layout;
}

void ForjarackAudioProcessor::cacheParameterPointers()
{
    for (int slot = 0; slot < kNumSlots; ++slot)
    {
        typePtr[(size_t) slot] = apvts.getRawParameterValue (slotId (slot, "type"));
        bypassPtr[(size_t) slot] = apvts.getRawParameterValue (slotId (slot, "bypass"));
        soloPtr[(size_t) slot] = apvts.getRawParameterValue (slotId (slot, "solo"));

        for (int knob = 0; knob < kMaxModuleParams; ++knob)
            knobPtr[(size_t) slot][(size_t) knob] =
                apvts.getRawParameterValue (knobId (slot, knob));
    }

    inputTrimPtr = apvts.getRawParameterValue ("input_trim");
    outputTrimPtr = apvts.getRawParameterValue ("output_trim");
    phasePtr = apvts.getRawParameterValue ("phase_invert");
}

void ForjarackAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);

    dryBuffer.setSize (2, juce::jmax (1, samplesPerBlock), false, false, true);

    bypassSmoother.prepare (sampleRate);
    bypassSmoother.reset (isBypassed() ? 0.0 : 1.0);

    // Alinha o grafo com os params ja carregados sem sobrescrever os knobs.
    syncGraphFromParams (false);

    setLatencySamples (engine.latencySamples());
}

void ForjarackAudioProcessor::releaseResources()
{
    engine.release();
    dryBuffer.setSize (2, 0);
}

bool ForjarackAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

RackSnapshot ForjarackAudioProcessor::buildSnapshot() const
{
    RackSnapshot snap;

    for (int slot = 0; slot < kNumSlots; ++slot)
    {
        auto& s = snap.slots[(size_t) slot];

        s.moduleIndex = juce::jmax (0, juce::roundToInt (
            typePtr[(size_t) slot] != nullptr ? typePtr[(size_t) slot]->load() : 0.0f));
        s.bypass = bypassPtr[(size_t) slot]->load() > 0.5f;
        s.solo = soloPtr[(size_t) slot]->load() > 0.5f;

        for (int knob = 0; knob < kMaxModuleParams; ++knob)
            s.params[(size_t) knob] = knobPtr[(size_t) slot][(size_t) knob]->load();
    }

    snap.inputTrim = inputTrimPtr->load();
    snap.outputTrim = outputTrimPtr->load();
    snap.phaseInvert = phasePtr->load() > 0.5f;
    snap.globalBypass = isBypassed();

    return snap;
}

MidiEventSpan ForjarackAudioProcessor::collectMidi (
    juce::MidiBuffer& midiMessages,
    std::array<MidiEvent, kMaxRackMidiEvents>& storage) const
{
    // Copia curta e deterministica de notas para o span POD do nucleo
    // JUCE-free. Outros eventos curtos (CC, pitch bend) ficam fora ate existir
    // modulo que os consuma. Excedentes de kMaxRackMidiEvents sao descartados.
    int count = 0;

    for (const auto metadata : midiMessages)
    {
        const auto& message = metadata.getMessage();

        if (! message.isNoteOnOrOff())
            continue;

        auto& event = storage[(size_t) count];
        event.frame = metadata.samplePosition;
        event.channel = static_cast<std::uint8_t> (message.getChannel());
        event.note = static_cast<std::uint8_t> (message.getNoteNumber());
        // getVelocity() devolve 0..1; o contrato do MidiEvent e 0..127.
        event.velocity = static_cast<std::uint8_t> (
            juce::roundToInt (message.getVelocity() * 127.0));
        event.noteOn = message.isNoteOn();
        ++count;

        if (count >= kMaxRackMidiEvents)
            break;
    }

    midiMessages.clear();

    return MidiEventSpan { storage.data(), count };
}

void ForjarackAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();

    if (numSamples <= 0 || buffer.getNumChannels() < 2)
        return;

    if (dryBuffer.getNumSamples() < numSamples)
        dryBuffer.setSize (2, numSamples, false, false, true);

    float* left = buffer.getWritePointer (0);
    float* right = buffer.getWritePointer (1);

    // Guarda o dry antes de qualquer processamento para o crossfade de bypass.
    dryBuffer.copyFrom (0, 0, left, numSamples);
    dryBuffer.copyFrom (1, 0, right, numSamples);

    std::array<MidiEvent, kMaxRackMidiEvents> midiStorage {};
    const auto midiSpan = collectMidi (midiMessages, midiStorage);

    engine.process (buildSnapshot(), left, right, numSamples, midiSpan);

    // Bypass global com rampa de 10 ms entre wet (saida do rack) e dry (input).
    bypassSmoother.setTarget (isBypassed() ? 0.0 : 1.0);

    const float* dryL = dryBuffer.getReadPointer (0);
    const float* dryR = dryBuffer.getReadPointer (1);

    for (int i = 0; i < numSamples; ++i)
    {
        const double wet = bypassSmoother.next();
        const double dry = 1.0 - wet;

        left[i] = static_cast<float> (static_cast<double> (left[i]) * wet
                                    + static_cast<double> (dryL[i]) * dry);
        right[i] = static_cast<float> (static_cast<double> (right[i]) * wet
                                     + static_cast<double> (dryR[i]) * dry);
    }
}

void ForjarackAudioProcessor::syncGraphFromParams (bool applyDefaults)
{
    for (int slot = 0; slot < kNumSlots; ++slot)
    {
        const int raw = typePtr[(size_t) slot] != nullptr
                          ? juce::roundToInt (typePtr[(size_t) slot]->load()) : 0;

        if (raw == cachedTypes[(size_t) slot])
            continue;

        cachedTypes[(size_t) slot] = raw;

        const int index = juce::jlimit (0, ModuleCatalog::count() - 1, raw);

        suspendProcessing (true);
        engine.setSlotModule (slot, index);
        suspendProcessing (false);

        if (applyDefaults)
        {
            const auto defaults = ModuleCatalog::defaultParams (index);

            for (int knob = 0; knob < kMaxModuleParams; ++knob)
            {
                if (auto* param = apvts.getParameter (knobId (slot, knob)))
                    param->setValueNotifyingHost (defaults[(size_t) knob]);
            }
        }
    }
}

void ForjarackAudioProcessor::timerCallback()
{
    // Troca de modulo detectada na thread de mensagens: reconstroi o grafo e
    // aplica os defaults documentados do novo modulo.
    syncGraphFromParams (true);

    // Faxina dos modulos (mini drum libera kit substituido e adota o pedido).
    engine.idleModules();
}

bool ForjarackAudioProcessor::isBypassed() const
{
    auto* param = apvts.getRawParameterValue ("bypass");
    return param != nullptr && param->load() > 0.5f;
}

juce::AudioProcessorParameter* ForjarackAudioProcessor::getBypassParameter() const
{
    return apvts.getParameter ("bypass");
}

void ForjarackAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.state.createXml())
        copyXmlToBinary (*xml, destData);
}

void ForjarackAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            syncGraphFromParams (false);
        }
    }
}

juce::AudioProcessorEditor* ForjarackAudioProcessor::createEditor()
{
    return new ForjarackAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ForjarackAudioProcessor();
}
