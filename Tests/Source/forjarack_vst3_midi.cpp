// Costura do WRAPPER VST3 do ForjaRack. Os outros testes do rack chamam
// processBlock direto no objeto C++ (testes de engine, de processor e ate o
// E2E com sonda), e por isso ficaram verdes enquanto o plugin estava mudo na
// DAW: quem decide se o MIDI do teclado entra no plugin e o WRAPPER VST3 gerado
// pelo JUCE, dirigido pela macro JucePlugin_WantsMidiInput (propriedade de
// build NEEDS_MIDI_INPUT), nao pelo acceptsMidi() do C++. Com a macro em 0 o
// wrapper (a) nao anuncia o barramento kEvent de entrada e (b) pula a conversao
// data.inputEvents -> MidiBuffer em process(), entao o processBlock recebe um
// MidiBuffer sempre vazio e os slots-instrumento nunca ouvem a nota.
//
// Este teste carrega o .vst3 REAL do build pelo formato VST3 do JUCE (o mesmo
// caminho de uma DAW), poe o CANIVETE na gaveta 2 e segura notas ate sair som
// pela saida. E a unica rede contra esse tipo de defeito.
//
// Sem bundle (target nao construido) ou sem soundfont na biblioteca, SKIP com
// exit 0: o teste informa, nao acusa o que nao pode verificar aqui.
#include <cmath>
#include <cstdio>
#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace
{

constexpr double testSampleRate = 48000.0;
constexpr int testBlockSize = 512;

// Gaveta 2: o nome exibido do seletor e "Slot 2 Modulo". O ID do APVTS
// (slot2_type) nao sobrevive ao round-trip do VST3 -- o host devolve o ParamID
// numerico --, entao a procura e pelo nome e confirmada pelos rotulos.
constexpr const char* kSlotName = "Slot 2 Modulo";
constexpr const char* kCaniveteLabel = "CANIVETE";

int failures = 0;

void check (bool condition, const char* label)
{
    if (! condition)
    {
        std::printf ("  FAIL: %s\n", label);
        ++failures;
    }
    else
    {
        std::printf ("  ok: %s\n", label);
    }
}

void pumpMessages (int ms)
{
    if (auto* messages = juce::MessageManager::getInstanceWithoutCreating())
        messages->runDispatchLoopUntil (ms);
}

juce::File findRackBundle()
{
    if (const auto override = juce::SystemStats::getEnvironmentVariable ("FORJA_RACK_VST3", {});
        ! override.isEmpty())
        return { override };

    // build/Tests/Release/forja_forjarack_vst3.exe -> build/
    const auto build = juce::File::getSpecialLocation (juce::File::currentExecutableFile)
                           .getParentDirectory()   // Tests/Release
                           .getParentDirectory();  // Tests
    const auto root = build.getParentDirectory();  // build/

    return root.getChildFile ("Forjarack")
               .getChildFile ("Forjarack")
               .getChildFile ("Forjarack_artefacts")
               .getChildFile ("Release")
               .getChildFile ("VST3")
               .getChildFile ("ForjaRack.vst3");
}

// Biblioteca do CANIVETE (Documents/Forja/Canivete): o modulo auto-carrega o
// banco[0] no idle(), entao o teste so pode esperar voz se houver .sf2/.sf3 la.
bool libraryHasBank()
{
    const auto folder = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                            .getChildFile ("Forja")
                            .getChildFile ("Canivete");

    if (! folder.isDirectory())
        return false;

    return ! folder.findChildFiles (juce::File::findFiles, false, "*.sf2;*.sf3").isEmpty();
}

// Normalizado que faz o seletor mostrar `wanted`, varrendo os proprios rotulos
// do parametro. Evita depender do tamanho do catalogo do lado do teste: quem
// sabe as opcoes e o plugin carregado.
float choiceNormForLabel (juce::AudioProcessorParameter* param, const juce::String& wanted)
{
    if (param == nullptr)
        return -1.0f;

    const int steps = param->getNumSteps();

    for (int i = 0; i < steps; ++i)
    {
        const float norm = steps > 1 ? static_cast<float> (i) / static_cast<float> (steps - 1)
                                     : 0.0f;

        if (param->getText (norm, 64).containsIgnoreCase (wanted))
            return norm;
    }

    return -1.0f;
}

float peakOf (const juce::AudioBuffer<float>& buffer)
{
    float peak = 0.0f;

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int frame = 0; frame < buffer.getNumSamples(); ++frame)
            peak = juce::jmax (peak, std::abs (buffer.getReadPointer (channel)[frame]));

    return peak;
}

// Nota rearmada a cada 100 blocos: o modulo da gaveta so existe depois que o
// timer do processador realinha o grafo, e o bank assenta em background. Com a
// nota voltando, o primeiro bloco depois de a voz existir ja produz amostra.
void feedNotes (juce::MidiBuffer& midi, int block)
{
    midi.clear();

    if (block % 100 == 0)
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, static_cast<juce::uint8> (100)), 0);

    if (block % 100 == 60)
        midi.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
}

int run (juce::AudioPluginInstance& instance)
{
    int slotParamIndex = -1;
    const auto& params = instance.getParameters();

    for (int i = 0; i < params.size(); ++i)
    {
        if (params.getReference (i)->getName (64).contains (kSlotName))
        {
            slotParamIndex = i;
            break;
        }
    }

    check (slotParamIndex >= 0, "a gaveta 2 expoe o seletor de modulo ao host");

    if (slotParamIndex < 0)
        return failures;

    auto* slotParam = params.getReference (slotParamIndex);
    const float norm = choiceNormForLabel (slotParam, kCaniveteLabel);

    check (norm >= 0.0f, "o seletor expoe CANIVETE como opcao");

    if (norm < 0.0f)
        return failures;

    slotParam->setValue (norm);

    // Layout de faixa de instrumento (a configuracao exata do usuario): saida
    // estereo sem entrada de audio. O wrapper repassa a decisao ao plugin.
    juce::AudioProcessor::BusesLayout instrumentLayout;
    instrumentLayout.inputBuses.add (juce::AudioChannelSet::disabled());
    instrumentLayout.outputBuses.add (juce::AudioChannelSet::stereo());

    check (instance.checkBusesLayoutSupported (instrumentLayout),
           "o wrapper aceita o layout de instrumento (entrada desabilitada)");

    if (! instance.setBusesLayout (instrumentLayout))
        std::printf ("  (aviso: host nao aplicou o layout; segue com o atual)\n");

    instance.setNonRealtime (true);
    instance.prepareToPlay (testSampleRate, testBlockSize);

    const int numChannels = juce::jmax (1, instance.getTotalNumOutputChannels());
    juce::AudioBuffer<float> buffer (numChannels, testBlockSize);
    juce::MidiBuffer midi;

    // Fase A: 120 blocos sem nenhuma nota. O banco pode assentar aqui; ainda
    // assim a saida tem que ser silencio, porque sem nota nao ha voz.
    float silentPeak = 0.0f;

    for (int block = 0; block < 120; ++block)
    {
        buffer.clear();
        midi.clear();
        instance.processBlock (buffer, midi);
        silentPeak = juce::jmax (silentPeak, peakOf (buffer));
        pumpMessages (4);
    }

    check (silentPeak < 0.001f, "sem nota tocada o plugin devolve silencio");

    // Fase B: notas rearmadas. Se o MIDI atravessa o wrapper, a voz sai.
    // O teto e alto de proposito: o bank da biblioteca do usuario e carregado
    // em background (um .sf3 de 38 MB leva ~8 s nesta maquina) e as notas
    // precisam continuar entrando ate ele assentar. O loop sai no primeiro
    // som, entao so custa caro numa maquina onde a carga nunca assenta.
    constexpr int kMaxPlayedBlocks = 6000;
    float playedPeak = 0.0f;
    int settledAtBlock = kMaxPlayedBlocks;

    for (int block = 0; block < kMaxPlayedBlocks; ++block)
    {
        buffer.clear();
        feedNotes (midi, block);
        instance.processBlock (buffer, midi);
        playedPeak = juce::jmax (playedPeak, peakOf (buffer));
        pumpMessages (4);

        if (playedPeak > 0.02f)
        {
            settledAtBlock = block + 1;
            break;
        }
    }

    std::printf ("  (pico com nota: %.4f | pico sem nota: %.6f | som no bloco %d)\n",
                 playedPeak, silentPeak, settledAtBlock);

    check (playedPeak > 0.02f, "nota do host atravessa o wrapper VST3 e faz o CANIVETE soar");

    midi.clear();
    midi.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
    instance.processBlock (buffer, midi);
    instance.releaseResources();

    return failures;
}

} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;

    const auto bundle = findRackBundle();

    if (! bundle.isDirectory())
    {
        std::printf ("SKIP: sem bundle do ForjaRack em %s (construa Forjarack_VST3)\n",
                     bundle.getFullPathName().toRawUTF8());
        return 0;
    }

    if (! libraryHasBank())
    {
        std::printf ("SKIP: nao ha .sf2/.sf3 em Documents/Forja/Canivete\n");
        return 0;
    }

    std::printf ("[ForjaRack no formato VST3]\n");

    juce::AudioPluginFormatManager manager;
    juce::addDefaultFormatsToManager (manager);

    bool hasVst3Format = false;

    for (auto* format : manager.getFormats())
        hasVst3Format = hasVst3Format || format->getName().containsIgnoreCase ("VST3");

    check (hasVst3Format, "o build do teste traz o formato VST3 (JUCE_PLUGINHOST_VST3)");

    if (! hasVst3Format)
        return 1;

    const juce::FileSearchPath searchPath (bundle.getParentDirectory().getFullPathName());
    juce::OwnedArray<juce::PluginDescription> descriptions;

    for (auto* format : manager.getFormats())
    {
        for (const auto& fileOrId : format->searchPathsForPlugins (searchPath, false, false))
            format->findAllTypesForFile (descriptions, fileOrId);
    }

    const juce::PluginDescription* rack = nullptr;

    for (auto* desc : descriptions)
    {
        if (desc->name == "ForjaRack")
        {
            rack = desc;
            break;
        }
    }

    check (rack != nullptr, "o bundle real do ForjaRack e reconhecido pelo formato VST3");

    if (rack == nullptr)
    {
        std::printf ("  (%d descritores no scan)\n", descriptions.size());
        return 1;
    }

    juce::String error;
    auto instance = manager.createPluginInstance (*rack, testSampleRate, testBlockSize, error);

    check (instance != nullptr, "o ForjaRack carrega como instancia de host");

    if (instance == nullptr)
    {
        std::printf ("  (%s)\n", error.toRawUTF8());
        return 1;
    }

    // A assercao que teria pego o bug: o host so manda notas se o plugin
    // anuncia a fila de eventos. Vem do NEEDS_MIDI_INPUT do CMake, nao do
    // acceptsMidi() do C++.
    check (instance->acceptsMidi(), "o wrapper VST3 anuncia entrada de MIDI ao host");

    run (*instance);

    instance.reset();

    if (failures == 0)
    {
        std::printf ("\nPASS: ForjaRack ponta a ponta no formato VST3\n");
        return 0;
    }

    std::printf ("\nFAIL: %d verificacoes falharam\n", failures);
    return 1;
}
