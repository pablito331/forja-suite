#include "PluginProcessor.h"

#include <functional>

#include "PluginEditor.h"
#include "rack/ModuleCatalog.h"
#include "minidrum/MiniDrumRackModule.h"
#include "minisat/MiniSatRackModule.h"
#include "canivete/CaniveteRackModule.h"

namespace
{
juce::String dbText (float normalized)
{
    const double db = (static_cast<double> (normalized) - 0.5) * 48.0;
    return (db > 0.0 ? "+" : "") + juce::String (db, 1) + " dB";
}

// Janela onde a UI nativa de um plugin hospedado (ou a lista generica de
// parametros) e montada quando o usuario aperta EDITAR. E o "abrir em aba
// separada" para quem nao segue o padrao visual do ForjaRack. Ao fechar, roda
// o callback (o processor destrava os params para o rack voltar a comandar).
class HostedEditorWindow : public juce::DocumentWindow
{
public:
    HostedEditorWindow (const juce::String& title, std::function<void()> closed)
        : juce::DocumentWindow (title,
                                juce::Colours::black,
                                juce::DocumentWindow::allButtons,
                                /*useNativeTitleBar=*/ true),
          onClosed (std::move (closed))
    {
        setResizable (true, true);
    }

    void closeButtonPressed() override
    {
        if (onClosed)
            onClosed();

        setVisible (false);
    }

private:
    std::function<void()> onClosed;
};
} // namespace

ForjarackAudioProcessor::ForjarackAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
                                .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    cacheParameterPointers();

    // Gaveta hospedada: resolve a ficha publicada no catalogo e varre a pasta
    // uma vez (scan barato quando a pasta esta vazia; o botao PROCURAR revarre).
    hostedIndex = ModuleCatalog::indexOf ("hospedado");
    hostedLibrary.refresh();

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
    forja::canivete::registerCaniveteModule();

    // Gaveta HOSPEDADO: entrada unica no catalogo para plugins carregados em
    // runtime. Fabrica nula (o processor adota um HostedModule no slot, o
    // catalogo nao cria) e numParams 0 (os knobs sao dinamicos via IHostedSlot).
    // O papel real (efeito/instrumento) vem do plugin, nao desta ficha.
    if (ModuleCatalog::indexOf ("hospedado") < 0)
    {
        ModuleSpec hosted;
        hosted.id = "hospedado";
        hosted.name = "HOSPEDADO";
        hosted.category = ModuleCategory::hosted;
        hosted.accent = 0xff5ac8c0;   // azul aco, distinto das familias Airwindows
        hosted.role = ModuleRole::effect;
        hosted.numParams = 0;
        hosted.factory = nullptr;
        hosted.face = ModuleFace::generic;
        ModuleCatalog::registerModule (hosted);
    }

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
    // JUCE-free (modulos embutidos: mini drum, canivete). Alem disso expoe o
    // MidiBuffer nativo do host em `native` (void* opaco) para o plugin
    // hospedado ler a trilha completa (nota + CC + pitch bend + program). NAO
    // se limpa aqui: o clear acontece em processBlock, depois do engine consumir.
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

    return MidiEventSpan { storage.data(), count, const_cast<juce::MidiBuffer*> (&midiMessages) };
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

    // Rack consumiu o MIDI do bloco (POD para os embutidos, nativo para os
    // hospedados). Marca como tratado so agora, apos o consumo.
    midiMessages.clear();

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

        if (hostedIndex >= 0 && index == hostedIndex)
        {
            // Gaveta hospedada: o modulo vem do arquivo escolhido, nao do
            // catalogo. adoptHosted suspende, carrega, adota e semear knobs.
            adoptHosted (slot);
            continue;
        }

        // Saiu de HOSPEDADO: destroi qualquer editor nativo aberto antes de
        // trocar o modulo (a janela e dona do editor do plugin antigo).
        hostedWindows[(size_t) slot].reset();

        suspendProcessing (true);
        engine.setSlotModule (slot, index);
        suspendProcessing (false);

        // Airwindows reportam 0; ao remover um hospedado a latencia pode cair.
        setLatencySamples (engine.latencySamples());

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

            // A escolha de arquivo hospedado vive em propriedade do estado (o
            // AudioParameterChoice congela as opcoes na construcao, nao serve
            // para uma lista dinamica). Le de volta antes de realinhar o grafo.
            for (int slot = 0; slot < kNumSlots; ++slot)
                hostedFile[(size_t) slot] = apvts.state.getProperty (hostedStateKey (slot)).toString();

            syncGraphFromParams (false);
        }
    }
}

juce::AudioProcessorEditor* ForjarackAudioProcessor::createEditor()
{
    return new ForjarackAudioProcessorEditor (*this);
}

// ---------------------------------------------------------------------------
// Gaveta hospedada (forjarackui::IHostedSlot). Tudo aqui roda na thread de
// mensagens; a carga/recaega de plugin acontece sob suspendProcessing.

juce::String ForjarackAudioProcessor::hostedStateKey (int slot)
{
    return "hosted_slot" + juce::String (slot + 1);
}

HostedModule* ForjarackAudioProcessor::hostedModuleAt (int slot) const noexcept
{
    if (slot < 0 || slot >= kNumSlots)
        return nullptr;

    // moduleAt const devolve ModuleBase* const; o modulo de um slot hospedado e
    // sempre um HostedModule. A pontaria mutavel volta via const_cast pois
    // engine e o unico dono e estas operacoes sao de configuracao (msg thread).
    const auto* base = engine.moduleAt (slot);
    return const_cast<HostedModule*> (dynamic_cast<const HostedModule*> (base));
}

void ForjarackAudioProcessor::seedHostedKnobs (int slot, HostedModule& module)
{
    // Semeia os knobs do rack com os valores atuais do plugin, para a primeira
    // empurrada do engine (knob -> setParam) nao chutar o plugin para 0,5.
    const int n = juce::jmin (module.numHostedParams(), kMaxModuleParams);

    for (int k = 0; k < n; ++k)
        if (auto* param = apvts.getParameter (knobId (slot, k)))
            param->setValueNotifyingHost (module.hostedParamValue (k));
}

void ForjarackAudioProcessor::adoptHosted (int slot)
{
    if (hostedIndex < 0 || slot < 0 || slot >= kNumSlots)
        return;

    // Editor nativo antigo morre antes do modulo antigo (a janela e dona do
    // editor do plugin; depois o engine destroi o HostedModule).
    hostedWindows[(size_t) slot].reset();

    const double rate = getSampleRate() > 0.0 ? getSampleRate() : 44100.0;
    const int block = getBlockSize() > 0 ? getBlockSize() : 512;

    suspendProcessing (true);

    juce::String error;
    std::unique_ptr<ModuleBase> module;

    if (! hostedFile[(size_t) slot].isEmpty())
        module = hostedLibrary.load (hostedFile[(size_t) slot], rate, block, error);

    engine.adoptSlotModule (slot, hostedIndex, std::move (module));
    suspendProcessing (false);

    // Latencia/PDC do plugin hospedado sobe para o host (setLatencySamples ja
    // notifica o host quando o valor muda).
    setLatencySamples (engine.latencySamples());

    if (auto* hosted = hostedModuleAt (slot))
        seedHostedKnobs (slot, *hosted);
}

bool ForjarackAudioProcessor::isHosted (int slot) const
{
    if (hostedIndex < 0 || slot < 0 || slot >= kNumSlots)
        return false;

    // Fonte de verdade imediata: o parametro 'type' (reflete o seletor na hora,
    // sem esperar o proximo tick de syncGraphFromParams).
    const int raw = typePtr[(size_t) slot] != nullptr
                      ? juce::roundToInt (typePtr[(size_t) slot]->load()) : 0;

    return raw == hostedIndex;
}

juce::StringArray ForjarackAudioProcessor::fileList() const
{
    return hostedLibrary.fileNames();
}

juce::String ForjarackAudioProcessor::fileName (int slot) const
{
    return (slot >= 0 && slot < kNumSlots) ? hostedFile[(size_t) slot] : juce::String();
}

void ForjarackAudioProcessor::chooseFile (int slot, const juce::String& name)
{
    if (slot < 0 || slot >= kNumSlots || hostedIndex < 0)
        return;

    hostedFile[(size_t) slot] = name;
    apvts.state.setProperty (hostedStateKey (slot), name, nullptr);

    // Leva a gaveta para HOSPEDADO (se ainda nao estiver) e carrega o arquivo.
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (
            apvts.getParameter (slotId (slot, "type"))))
    {
        const int last = juce::jmax (1, ModuleCatalog::count() - 1);
        choice->setValueNotifyingHost (static_cast<float> (hostedIndex) / static_cast<float> (last));
    }

    cachedTypes[(size_t) slot] = hostedIndex;
    adoptHosted (slot);
}

int ForjarackAudioProcessor::paramCount (int slot) const
{
    auto* module = hostedModuleAt (slot);
    return module != nullptr ? juce::jmin (module->numHostedParams(), kMaxModuleParams) : 0;
}

juce::String ForjarackAudioProcessor::paramLabel (int slot, int index) const
{
    auto* module = hostedModuleAt (slot);
    return module != nullptr ? module->hostedParamLabel (index) : juce::String();
}

bool ForjarackAudioProcessor::canOpenEditor (int slot) const
{
    return hostedModuleAt (slot) != nullptr;
}

void ForjarackAudioProcessor::openEditor (int slot)
{
    auto* module = hostedModuleAt (slot);

    if (module == nullptr)
        return;

    // Ao fechar, destrava os params: o rack volta a manda neles pelo knob.
    // A janela apenas se esconde: o editor e do plugin, nao nosso.
    auto unlock = [this, slot]
    {
        if (auto* m = hostedModuleAt (slot))
            m->setParamsLockedFromHost (false);
    };

    auto* content = module->createNativeEditor();

    if (content == nullptr)
        return;

    auto* window = static_cast<HostedEditorWindow*> (hostedWindows[(size_t) slot].get());

    if (window == nullptr)
    {
        window = new HostedEditorWindow (module->pluginName(), unlock);
        window->setUsingNativeTitleBar (true);
        window->setResizable (true, true);
        hostedWindows[(size_t) slot].reset (window);

        // Anexa primeiro, dimensiona depois: centreWithSize usa o tamanho que
        // a janela assume ao receber o content.
        window->setContentNonOwned (content, /*deleteWhenDismissed=*/ false);
        window->centreWithSize (juce::jmax (content->getWidth(), 320),
                                juce::jmax (content->getHeight(), 240));
    }
    else if (window->getContentComponent() != content)
    {
        // O plugin trocou de editor (recriacao permitida pelo JUCE): reanexa.
        window->setContentNonOwned (content, false);
    }

    module->setParamsLockedFromHost (true);
    window->setVisible (true);
    window->toFront (true);
}

int ForjarackAudioProcessor::rescanFiles()
{
    return hostedLibrary.refresh();
}

int ForjarackAudioProcessor::rescan()
{
    return hostedLibrary.refresh();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ForjarackAudioProcessor();
}
