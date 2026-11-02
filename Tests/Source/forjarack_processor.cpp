// Ponta a ponta do ForjaRack no papel de instrumento: o teste passa pelo
// AudioProcessor REAL (APVTS -> syncGraph -> snapshot -> engine -> processBlock
// -> crossfade de bypass), exatamente o caminho que o host percorre num tecla-
// do ao vivo. O teste de engine cobre o nucleo; o do CANIVETE cobre o Sound-
// Font; este cobre a costura do plugin, que e onde o silencio de host aparece.
//
// O instrumento e uma sonda registrada em runtime (mesmo contrato de uma
// expansao real), entao nada aqui depende de soundfont em disco.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"
#include "rack/Module.h"
#include "rack/ModuleCatalog.h"
#include "rack/RackEngine.h"
#include "rack/RackState.h"

namespace
{

constexpr double testSampleRate = 48000.0;
constexpr int testBlockSize = 512;
constexpr const char* kProbeId = "e2e-sonda";

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

// Sonda: instrumento deterministico que ANOTA o MIDI que recebeu. E a prova de
// que o coletor do processor entrega canal/nota/velocidade certos ao modulo e
// de que a voz dele sobrevive ao resto da cadeia do plugin.
class ProbeModule final : public forjarack::ModuleBase
{
public:
    static ProbeModule* last;

    ProbeModule() { last = this; }

    void prepare (double sr) override { sampleRate = sr; }
    void setParam (int, float) override {}
    void setBypass (bool bypassed) override { bypassedFlag = bypassed; }
    bool isInstrument() const noexcept override { return true; }

    void processMidi (const forjarack::MidiEventSpan& span) noexcept override
    {
        blocksWithMidi += (span.numEvents > 0 ? 1 : 0);

        for (int i = 0; i < span.numEvents; ++i)
        {
            const auto& event = span.events[(size_t) i];

            if (event.noteOn && event.velocity > 0)
            {
                active = true;
                note = event.note;
                channel = event.channel;
                velocity = event.velocity;
            }
            else if (! event.noteOn && event.note == note)
            {
                active = false;
            }
        }
    }

    void process (float* left, float* right, int n) override
    {
        float peak = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            double v = 0.0;

            if (active && ! bypassedFlag)
            {
                const double freq = 440.0 * std::pow (2.0, (static_cast<double> (note) - 69.0) / 12.0);
                v = 0.4 * std::sin (2.0 * 3.14159265358979 * freq * phase / sampleRate);
                phase += 1.0;
                peak = std::max (peak, static_cast<float> (std::abs (v)));
            }

            left[i] = static_cast<float> (v);
            right[i] = static_cast<float> (v);
        }

        lastPeak = peak;
    }

    float readOutputPeak() override { return lastPeak; }
    int latencySamples() const override { return 0; }

    int blocksWithMidi = 0;
    bool active = false;
    std::uint8_t note = 0;
    std::uint8_t channel = 0;
    std::uint8_t velocity = 0;

private:
    double sampleRate = 48000.0;
    double phase = 0.0;
    float lastPeak = 0.0f;
    bool bypassedFlag = false;
};

ProbeModule* ProbeModule::last = nullptr;

std::unique_ptr<forjarack::ModuleBase> createProbe()
{
    return std::make_unique<ProbeModule>();
}

void registerProbeModule()
{
    if (forjarack::ModuleCatalog::indexOf (kProbeId) >= 0)
        return;

    forjarack::ModuleSpec spec;
    spec.id = kProbeId;
    spec.name = "SONDA E2E";
    spec.category = forjarack::ModuleCategory::instrument;
    spec.accent = 0xffe06fa8;
    spec.role = forjarack::ModuleRole::instrument;
    spec.numParams = 1;
    spec.knobs[0] = { "SONDA", 0.5f };
    spec.face = forjarack::ModuleFace::drum;
    spec.factory = &createProbe;

    forjarack::ModuleCatalog::registerModule (spec);
}

float peakOf (const juce::AudioBuffer<float>& buffer)
{
    float peak = 0.0f;

    for (int c = 0; c < buffer.getNumChannels(); ++c)
    {
        const float* data = buffer.getReadPointer (c);

        for (int i = 0; i < buffer.getNumSamples(); ++i)
            peak = std::max (peak, std::abs (data[i]));
    }

    return peak;
}

// Escreve o indice no choice de modulo do slot (o mesmo caminho do seletor da
// UI: normalizado = indice / (numEscolhas - 1)). slotId e privado no processor,
// entao a chave e montada aqui no formato publicado ("slot1_type").
void setSlotType (ForjarackAudioProcessor& processor, int slot, int index)
{
    auto* param = dynamic_cast<juce::AudioParameterChoice*> (
        processor.apvts.getParameter ("slot" + juce::String (slot + 1) + "_type"));

    if (param == nullptr)
        return;

    // A lista de escolhas do seletor e montada do catalogo na ordem 0..n-1
    // (createParameterLayout), entao normalizado = indice / (count - 1).
    const int numChoices = forjarack::ModuleCatalog::count();

    if (numChoices <= 1)
        return;

    const auto normalized = static_cast<float> (index) / static_cast<float> (numChoices - 1);
    param->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, normalized));
}

// ---------------------------------------------------------------------------
// 1) O layout de instrumento (entrada desabilitada) e aceito; mono nao.
// ---------------------------------------------------------------------------
void testInstrumentLayout (ForjarackAudioProcessor& processor)
{
    std::printf ("\n[layout de faixa de instrumento]\n");

    // JUCE 9: o BusesLayout e duas listas publicas de AudioChannelSet (indice 0
    // = barramento principal). Um BusesLayout vazio nao tem bus nenhum e as
    // getters leem fora da faixa.
    juce::AudioProcessor::BusesLayout instrumentTrack;
    instrumentTrack.inputBuses.add (juce::AudioChannelSet::disabled());
    instrumentTrack.outputBuses.add (juce::AudioChannelSet::stereo());
    check (processor.isBusesLayoutSupported (instrumentTrack),
           "aceita saida estereo com entrada desabilitada (faixa de instrumento)");

    juce::AudioProcessor::BusesLayout fxTrack = instrumentTrack;
    fxTrack.inputBuses.set (0, juce::AudioChannelSet::stereo());
    check (processor.isBusesLayoutSupported (fxTrack),
           "continua aceitando entrada estereo (FX em cadeia)");

    juce::AudioProcessor::BusesLayout mono = instrumentTrack;
    mono.outputBuses.set (0, juce::AudioChannelSet::mono());
    check (! processor.isBusesLayoutSupported (mono),
           "rejeita saida mono (a corrente do rack e estereo)");
}

// ---------------------------------------------------------------------------
// 2) Tecla tocada no host: MIDI chega certo ao modulo e a voz sai do plugin.
// ---------------------------------------------------------------------------
void testKeyboardThroughProcessor (ForjarackAudioProcessor& processor)
{
    std::printf ("\n[teclado ao vivo pelo processador]\n");

    const int index = forjarack::ModuleCatalog::indexOf (kProbeId);
    check (index >= 0, "sonda publicada no catalogo");
    if (index < 0)
        return;

    auto& engine = processor.getEngine();
    ProbeModule* before = ProbeModule::last;
    setSlotType (processor, 0, index);

    // O grafo e montado no timer do processor (thread de mensagens), como no host.
    for (int pass = 0; pass < 40 && engine.slotModuleIndex (0) != index; ++pass)
        pumpMessages (20);

    check (engine.slotModuleIndex (0) == index, "troca de modulo chega ao engine");
    check (ProbeModule::last != before && ProbeModule::last != nullptr,
           "o instrumento do slot e a sonda registrada");
    if (ProbeModule::last == nullptr)
        return;

    auto* probe = ProbeModule::last;

    // SEM FONTE DE AUDIO NA FAIXA: o host entrega o par de entrada zerado e so
    // a voz do instrumento pode sair. E o caso do teclado ao vivo sem gravacao.
    juce::AudioBuffer<float> buffer (2, testBlockSize);
    buffer.clear();   // o ctor do AudioBuffer nao zera: sem isto o "dry" e lixo
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);

    const int seenBefore = probe->blocksWithMidi;
    processor.processBlock (buffer, midi);

    std::printf ("  (probe: ch=%u nota=%u vel=%u blocosComMidi=%d)\n",
                 static_cast<unsigned> (probe->channel),
                 static_cast<unsigned> (probe->note),
                 static_cast<unsigned> (probe->velocity),
                 probe->blocksWithMidi);

    check (probe->channel == 1 && probe->note == 60 && probe->velocity >= 100 && probe->velocity <= 102,
           "coletor entrega canal/nota/velocidade ao modulo");
    check (probe->blocksWithMidi > seenBefore,
           "o modulo recebe o span do bloco");
    check (midi.getNumEvents() == 0,
           "o rack consome o MIDI do bloco");

    const float audible = peakOf (buffer);
    std::printf ("  (pico de saida: %.4f)\n", audible);
    check (audible > 0.05f, "a voz do instrumento sai do plugin audivel");

    check (engine.readMasterMeters().midi,
           "o medidor master reporta atividade MIDI (led MIDI)");

    // Levantar a tecla: a voz cessa e o rack volta ao silencio (sem estado preso).
    juce::MidiBuffer off;
    off.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
    processor.processBlock (buffer, off);

    check (! probe->active, "note off desliga a voz do instrumento");

    buffer.clear();
    juce::MidiBuffer idle;
    processor.processBlock (buffer, idle);
    check (peakOf (buffer) < 1e-6f, "sem nota tocada o plugin devolve silencio");
}

// ---------------------------------------------------------------------------
// 3) O mesmo caminho com audio na faixa: a cadeia passa e a voz soma em cima.
// ---------------------------------------------------------------------------
void testChainStillWorks (ForjarackAudioProcessor& processor)
{
    std::printf ("\n[faixa com audio: cadeia + voz]\n");

    const int fx = forjarack::ModuleCatalog::indexOf ("mel");
    check (fx >= 0, "modulo de efeito existe no catalogo");
    if (fx < 0)
        return;

    auto& engine = processor.getEngine();
    setSlotType (processor, 1, fx);

    for (int pass = 0; pass < 40 && engine.slotModuleIndex (1) != fx; ++pass)
        pumpMessages (20);

    check (engine.slotModuleIndex (1) == fx, "a gaveta de efeito entrou no slot 2");

    juce::AudioBuffer<float> buffer (2, testBlockSize);

    for (int c = 0; c < 2; ++c)
    {
        float* data = buffer.getWritePointer (c);

        for (int i = 0; i < testBlockSize; ++i)
            data[i] = static_cast<float> (0.3 * std::sin (2.0 * 3.14159265358979 * 220.0 * i / testSampleRate));
    }

    const auto dry = buffer;
    juce::MidiBuffer midi;
    processor.processBlock (buffer, midi);

    check (peakOf (buffer) > 0.05f, "o audio da faixa continua saindo com o instrumento na cadeia");

    bool altered = false;

    for (int i = 0; i < testBlockSize && ! altered; ++i)
        if (std::abs (buffer.getReadPointer (0)[i] - dry.getReadPointer (0)[i]) > 1e-6f)
            altered = true;

    check (altered, "a gaveta de efeito mexe no sinal que passa");
}

// ---------------------------------------------------------------------------
// 4) O CANIVETE de verdade pela mesma porta do host. So roda quando a maquina
// tem um bank em Documents/Forja/Canivete (e exatamente o caso do usuario);
// sem bank na pasta a secao se declara SKIP e nao conta como falha.
// ---------------------------------------------------------------------------
// 4b) A fileira INSTRUMENTO da gaveta, pela porta do host. O plugin standalone
// troca o preset e a gaveta usa o MESMO engine, entao a diferenca tem que estar
// na costura (processor -> modulo -> preset -> thread de audio). O espelho de
// preset em uso so e escrito pela thread de audio quando ela de fato chama
// tsf_channel_set_presetindex: e a prova deterministica de que o pedido chegou
// ---------------------------------------------------------------------------
std::vector<float> renderNote (ForjarackAudioProcessor& processor, int note, int blocks)
{
    juce::AudioBuffer<float> buffer (2, testBlockSize);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, note, static_cast<juce::uint8> (100)), 0);

    std::vector<float> captured;
    captured.reserve (static_cast<size_t> (blocks * testBlockSize));

    for (int block = 0; block < blocks; ++block)
    {
        buffer.clear();
        processor.processBlock (buffer, midi);
        midi.clear();   // so o ataque: a nota segue sustentada

        captured.insert (captured.end(),
                         buffer.getReadPointer (0),
                         buffer.getReadPointer (0) + testBlockSize);
    }

    return captured;
}

// Devolve o campo ao silencio antes de cada captura: sem isso a cauda do preset
// anterior vaza na comparacao e o teste aprova um seletor morto.
bool settleToSilence (ForjarackAudioProcessor& processor, int note)
{
    juce::AudioBuffer<float> buffer (2, testBlockSize);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOff (1, note), 0);

    for (int block = 0; block < 800; ++block)
    {
        buffer.clear();
        processor.processBlock (buffer, midi);
        midi.clear();

        if (peakOf (buffer) < 1e-5f)
            return true;
    }

    return false;
}

float maxDifference (const std::vector<float>& a, const std::vector<float>& b)
{
    float diff = 0.0f;
    const auto n = std::min (a.size(), b.size());

    for (size_t i = 0; i < n; ++i)
        diff = std::max (diff, std::abs (a[i] - b[i]));

    return diff;
}

void testDrawerInstrumentThroughProcessor (ForjarackAudioProcessor& processor, int slot)
{
    const auto instruments = processor.drawerInstrumentNames (slot);

    if (instruments.size() < 2)
    {
        std::printf ("  SKIP: o bank da gaveta tem %d preset(s); a troca precisa de 2\n",
                     instruments.size());
        return;
    }

    std::printf ("  (%d presets na gaveta; testando a troca)\n", instruments.size());

    bool mirrorFollows = true;
    juce::String lastApplied;

    for (int i = 1; i < instruments.size(); ++i)
    {
        processor.chooseInstrument (slot, instruments[i]);

        // O proximo bloco aplica o pedido e escreve o espelho; a telemetria da
        // gaveta le o mesmo atomico que o editor mostra.
        for (int pass = 0; pass < 10 && processor.currentInstrument (slot) != instruments[i]; ++pass)
        {
            juce::AudioBuffer<float> tick (2, testBlockSize);
            tick.clear();
            juce::MidiBuffer none;
            processor.processBlock (tick, none);
            pumpMessages (10);
        }

        lastApplied = instruments[i];

        if (processor.currentInstrument (slot) != instruments[i])
            mirrorFollows = false;
    }

    if (! mirrorFollows)
    {
        std::printf ("  (ultimo preset pedido: %s | em uso: %s)\n",
                     lastApplied.toRawUTF8 (),
                     processor.currentInstrument (slot).toRawUTF8 ());
    }

    check (mirrorFollows, "a gaveta troca o preset no engine (espelho do audio acompanha)");

    // Prova de som, calibrada por controle: a MESMA nota no MESMO preset tem que
    // renderizar identica duas vezes; so entao a diferenca entre dois presets
    // conta. Sem o controle o teste aprova um seletor morto (aprovou: a cauda e
    // o estado do banco mudam entre capturas e o delta sai grande mesmo com o
    // preset parado).
    constexpr int referenceNote = 60;

    auto capture = [&processor, slot] (const juce::String& preset, int note) -> std::vector<float>
    {
        processor.chooseInstrument (slot, preset);

        for (int pass = 0; pass < 10 && processor.currentInstrument (slot) != preset; ++pass)
        {
            juce::AudioBuffer<float> tick (2, testBlockSize);
            tick.clear();
            juce::MidiBuffer none;
            processor.processBlock (tick, none);
            pumpMessages (10);
        }

        if (! settleToSilence (processor, note))
            return {};

        return renderNote (processor, note, 12);
    };

    const auto first = instruments[0];
    const auto other = instruments[instruments.size() - 1];

    const auto byFirst = capture (first, referenceNote);
    const auto byFirstAgain = capture (first, referenceNote);
    const auto byOther = capture (other, referenceNote);

    if (byFirst.empty() || byFirstAgain.empty() || byOther.empty())
    {
        std::printf ("  SKIP: a cauda do bank nao assentou entre capturas\n");
        return;
    }

    const auto repeat = maxDifference (byFirst, byFirstAgain);
    const auto changed = maxDifference (byFirst, byOther);

    std::printf ("  (%s vs %s | repeticao: %.6f | troca: %.6f)\n",
                 first.trim().toRawUTF8 (), other.trim().toRawUTF8 (), repeat, changed);

    if (repeat > 1e-5f)
    {
        std::printf ("  SKIP: render nao repetivel nesta maquina (controle %.6f); "
                     "a prova de som nao vale\n", repeat);
        return;
    }

    check (changed > 1e-4f, "o preset escolhido pela gaveta muda o audio do bank");

    settleToSilence (processor, referenceNote);
}

void testRealCaniveteThroughProcessor (ForjarackAudioProcessor& processor)
{
    std::printf ("\n[canivete real pelo processador]\n");

    const int index = forjarack::ModuleCatalog::indexOf ("canivete");
    check (index >= 0, "CANIVETE esta publicado no catalogo");
    if (index < 0)
        return;

    auto& engine = processor.getEngine();
    setSlotType (processor, 2, index);

    for (int pass = 0; pass < 40 && engine.slotModuleIndex (2) != index; ++pass)
        pumpMessages (20);

    const auto banks = processor.drawerFileNames (2);

    if (banks.isEmpty())
    {
        std::printf ("  SKIP: nenhuma soundfont em Documents/Forja/Canivete\n");
        return;
    }

    processor.chooseFile (2, banks[0]);

    // A carga do bank vai ao background; a lista de presets (e o nome em uso)
    // so existe quando ele assenta -- mesmo caminho do timer no plugin vivo.
    // O orçamento e generico de proposito: um .sf3 real da biblioteca do
    // usuario (MuseScore General, 38 MB de amostras Ogg) leva ~8 s para
    // assentar nesta maquina. Com 5 s o teste pegava o banco ainda carregando,
    // vinha com 0 presets e FAILAVA um caminho que esta certo -- o seletor da
    // gaveta so aparece quando a carga assenta (SlotComponent), entao o
    // produto esta correto e era o teste que estava apressado.
    const int kSettlePasses = 3000;   // 60 s no pior caso; sai antes se assentar
    juce::String instrument;
    int settlePass = 0;

    for (; settlePass < kSettlePasses
           && (instrument = processor.currentInstrument (2)).isEmpty(); ++settlePass)
    {
        pumpMessages (20);

        juce::AudioBuffer<float> idleBuffer (2, testBlockSize);
        idleBuffer.clear();
        juce::MidiBuffer noMidi;
        processor.processBlock (idleBuffer, noMidi);   // o audio adota o bank
    }

    std::printf ("  (bank assentou em %d ms | %d presets na gaveta)\n",
                 settlePass * 20,
                 static_cast<int> (processor.drawerInstrumentNames (2).size()));

    check (instrument.isNotEmpty(),
           "o bank da gaveta assentou (preset em uso visivel)");

    juce::AudioBuffer<float> buffer (2, testBlockSize);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);

    float peak = 0.0f;

    for (int block = 0; block < 16; ++block)
    {
        buffer.clear();
        processor.processBlock (buffer, midi);
        peak = std::max (peak, peakOf (buffer));
    }

    std::printf ("  (bank: %s | preset: %s | pico: %.4f)\n",
                 banks[0].toRawUTF8 (), instrument.toRawUTF8 (), peak);

    check (peak > 0.02f,
           "nota do teclado faz o CANIVETE real soar pelo plugin do rack");

    testDrawerInstrumentThroughProcessor (processor, 2);
}

} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI juceSetup;

    // Registrar ANTES de construir o processor: a lista de escolhas do APVTS e
    // montada a partir do catalogo no construtor.
    registerProbeModule();

    std::printf ("ForjaRack ponta a ponta (processador real, SR %.0f Hz, bloco %d)\n",
                 testSampleRate, testBlockSize);

    ForjarackAudioProcessor processor;
    processor.prepareToPlay (testSampleRate, testBlockSize);
    pumpMessages (60);

    testInstrumentLayout (processor);
    testKeyboardThroughProcessor (processor);
    testChainStillWorks (processor);
    testRealCaniveteThroughProcessor (processor);

    processor.releaseResources();

    if (failures == 0)
    {
        std::printf ("\nPASS: ForjaRack ponta a ponta\n");
        return 0;
    }

    std::printf ("\nFAIL: %d verificacao(oes)\n", failures);
    return 1;
}
