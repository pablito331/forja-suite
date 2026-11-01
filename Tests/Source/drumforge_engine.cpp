// Testes do engine do Drumforge: camadas, round-robin, choke, roubo de voz,
// roteamento dos auxiliares, bypass, mapa MIDI, ausencia de alocacao no audio,
// leitor de drumkit.txt/drumkit.labooh do Drumlabooh, biblioteca de loops MIDI
// e preview.
#include <juce_audio_basics/juce_audio_basics.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <new>
#include <utility>
#include <vector>

#include "engine/DrumEngine.h"
#include "engine/Kit.h"
#include "engine/MasterBus.h"
#include "loops/LoopLibrary.h"
#include "loops/LoopPlayer.h"

#if defined(_MSC_VER)
 #include <malloc.h>
#endif

using namespace forja::drumforge;

namespace
{

constexpr double testSampleRate = 48000.0;
constexpr int testBlockSize = 256;
constexpr int testCapacity = 4096;

std::atomic<bool> trackAllocations { false };
std::atomic<bool> allocationTripped { false };

void noteAllocation() noexcept
{
    if (trackAllocations.load (std::memory_order_relaxed))
        allocationTripped.store (true, std::memory_order_relaxed);
}

void* allocateAligned (std::size_t size, std::size_t alignment) noexcept
{
#if defined(_MSC_VER)
    return _aligned_malloc (size == 0 ? 1 : size, alignment);
#else
    return std::aligned_alloc (alignment, size == 0 ? 1 : size);
#endif
}

void freeAligned (void* pointer) noexcept
{
#if defined(_MSC_VER)
    _aligned_free (pointer);
#else
    std::free (pointer);
#endif
}

int failures = 0;

void check (bool condition, const char* name, const juce::String& info = {})
{
    if (condition)
    {
        std::printf ("  PASS  %s\n", name);
        return;
    }

    std::printf ("  FAIL  %s%s%s\n", name, info.isEmpty() ? "" : " - ", info.toRawUTF8());
    ++failures;
}

std::shared_ptr<const SampleData> makeSine (double frequency, float amplitude, int length)
{
    auto data = std::make_shared<SampleData>();
    data->sampleRate = testSampleRate;
    data->length = length;
    data->channels = 1;
    data->audio.setSize (1, length);

    auto* write = data->audio.getWritePointer (0);

    for (int i = 0; i < length; ++i)
        write[i] = amplitude * (float) std::sin (2.0 * juce::MathConstants<double>::pi
                                                 * frequency * (double) i / testSampleRate);

    return data;
}

// Amostra com ataque em 1.0 no primeiro frame: serve para provar latencia zero.
std::shared_ptr<const SampleData> makeClick (float amplitude, int length)
{
    auto data = std::make_shared<SampleData>();
    data->sampleRate = testSampleRate;
    data->length = length;
    data->channels = 1;
    data->audio.setSize (1, length);
    data->audio.clear();

    auto* write = data->audio.getWritePointer (0);
    write[0] = amplitude;
    write[1] = amplitude * 0.5f;

    return data;
}

struct Harness
{
    DrumEngine engine;
    std::unique_ptr<LoadedKit> kit;
    EngineParameters parameters;
    std::array<juce::AudioBuffer<float>, kNumOutputBuses> buses;
    int lastFrames = 0;

    Harness()
    {
        kit = std::make_unique<LoadedKit>();
        kit->kit = Kit::createDefault();
        kit->kit.name = "Kit de teste";

        // Bumbo em tres camadas com amplitudes distintas para provar a escolha.
        addTake (0, 0, makeSine (60.0, 0.06f, 9600));
        addTake (0, 1, makeSine (60.0, 0.15f, 9600));
        addTake (0, 2, makeSine (60.0, 0.30f, 9600));

        // Caixa com tres tomadas em round-robin.
        addTake (1, 0, makeSine (220.0, 0.30f, 4800));
        addTake (1, 0, makeSine (220.0, 0.40f, 4800));
        addTake (1, 0, makeSine (220.0, 0.50f, 4800));

        addTake (4, 0, makeSine (120.0, 0.30f, 4800));
        addTake (5, 0, makeSine (8000.0, 0.40f, 4800));
        addTake (6, 0, makeSine (4000.0, 0.40f, 4800));
        addTake (7, 0, makeClick (0.90f, 4800));

        for (const auto& layers : kit->layers)
            if (! layers.empty())
                ++kit->padsWithAudio;

        for (auto& bus : buses)
            bus.setSize (2, testCapacity, false, true, false);

        // Compressor de canal neutro para as medidas de amplitude serem lineares.
        for (auto& pad : parameters.pads)
        {
            pad.compThreshold = 1.0f;
            pad.compRatio = 0.0f;
        }

        engine.prepare (testSampleRate, testBlockSize);
        engine.requestKit (kit.get(), 7);
        render (testBlockSize);
    }

    void addTake (int pad, int layerIndex, std::shared_ptr<const SampleData> sample)
    {
        auto& layers = kit->layers[(size_t) pad];

        if ((int) layers.size() <= layerIndex)
            layers.resize ((size_t) layerIndex + 1);

        layers[(size_t) layerIndex].push_back (std::move (sample));
        ++kit->totalSamples;
    }

    void render (int frames, const juce::MidiBuffer& midi = juce::MidiBuffer(), bool bypassed = false)
    {
        frames = juce::jlimit (1, testCapacity, frames);
        lastFrames = frames;

        std::array<juce::AudioBuffer<float>*, kNumOutputBuses> outputs {};

        for (int bus = 0; bus < kNumOutputBuses; ++bus)
        {
            buses[(size_t) bus].clear (0, frames);
            outputs[(size_t) bus] = &buses[(size_t) bus];
        }

        engine.setParameters (parameters);
        engine.process (midi, outputs, frames, bypassed);
    }

    std::array<float, kNumPads> readPadPeaks()
    {
        std::array<float, kNumPads> peaks {};

        for (int pad = 0; pad < kNumPads; ++pad)
            peaks[(size_t) pad] = engine.readPadMeter (pad).peak;

        return peaks;
    }

    float busPeak (int bus) const
    {
        const auto& buffer = buses[(size_t) bus];
        float peak = 0.0f;

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            peak = std::max (peak, buffer.getMagnitude (channel, 0, lastFrames));

        return peak;
    }
};

void testKitAdoption()
{
    std::printf ("\n[kit e isolamento dos canais]\n");

    Harness h;

    check (h.engine.acceptedKitToken() == 7, "kit adotado por token");
    check (h.engine.isPadPlayable (0), "pad com sample pode tocar");
    check (! h.engine.isPadPlayable (2), "pad vazio nao toca");

    h.engine.pushTrigger (0, 1.0f);
    h.render (testBlockSize);

    const auto peaks = h.readPadPeaks();

    check (peaks[0] > 0.01f, "pad disparado gera audio");

    bool leaked = false;

    for (int pad = 1; pad < kNumPads; ++pad)
        if (peaks[(size_t) pad] > 0.0f)
            leaked = true;

    check (! leaked, "audio nao vaza para os outros strips");
    check (h.busPeak (0) > 0.01f, "master recebe o pad disparado");
}

void testVelocityLayers()
{
    std::printf ("\n[camadas de velocity]\n");

    Harness h;

    const float velocities[] = { 0.10f, 0.50f, 0.95f };
    const int expectedLayers[] = { 0, 1, 2 };
    float previousPeak = 0.0f;

    for (int i = 0; i < 3; ++i)
    {
        h.engine.pushTrigger (0, velocities[i]);
        h.render (testBlockSize);

        check (h.engine.lastTriggeredLayer (0) == expectedLayers[i],
               "velocity escolhe a camada certa");

        const float peak = h.readPadPeaks()[0];
        check (peak > previousPeak, "camada mais forte toca mais alto");
        previousPeak = peak;

        // Deixa o sample de 200 ms terminar antes da proxima medida.
        for (int block = 0; block < 40; ++block)
            h.render (testBlockSize);

        h.readPadPeaks();
    }
}

void testRoundRobin()
{
    std::printf ("\n[round-robin]\n");

    Harness h;

    for (int trigger = 0; trigger < 4; ++trigger)
    {
        h.engine.pushTrigger (1, 0.5f);
        h.render (testBlockSize);

        check (h.engine.lastTriggeredRoundRobin (1) == trigger % 3,
               "tomadas alternam em round-robin");
    }
}

void testChokeGroup()
{
    std::printf ("\n[choke de pratos]\n");

    Harness h;

    h.engine.pushTrigger (6, 1.0f);
    h.render (testBlockSize);
    h.readPadPeaks();

    check (h.engine.activeVoices() > 0, "open hat esta tocando");

    h.engine.pushTrigger (5, 1.0f);
    h.render (testBlockSize);
    h.readPadPeaks();

    h.render (testBlockSize);
    h.render (testBlockSize);

    const auto peaks = h.readPadPeaks();

    check (peaks[6] < 1.0e-3f, "choke em 5 ms corta o open hat",
           juce::String (peaks[6], 9));
    check (peaks[5] > 0.01f, "closed hat segue tocando");
}

void testVoiceStealing()
{
    std::printf ("\n[roubo de voz]\n");

    Harness h;

    for (int trigger = 0; trigger < 100; ++trigger)
        h.engine.pushTrigger (1, 0.5f);

    h.render (testBlockSize);

    check (h.engine.activeVoices() <= kMaxVoices, "pool respeita o limite de vozes");
    check (h.engine.activeVoices() > 0, "pool continua tocando depois do roubo");
    check (h.engine.triggerCount() >= 100, "todos os disparos foram contados");
}

void testMultiOut()
{
    std::printf ("\n[barramentos auxiliares]\n");

    Harness h;

    h.parameters.pads[4].output = 2;
    h.render (testBlockSize);

    h.engine.pushTrigger (4, 1.0f);
    h.render (testBlockSize);

    check (h.busPeak (2) > 0.01f, "pad roteado chega no auxiliar 2");
    check (h.busPeak (0) < 1.0e-4f, "master nao recebe o pad roteado",
           juce::String (h.busPeak (0), 12));
    check (h.busPeak (1) < 1.0e-4f, "auxiliar 1 permanece em silencio");
}

void testMuteAndSolo()
{
    std::printf ("\n[mute e solo]\n");

    Harness h;

    h.parameters.pads[1].solo = true;

    // Solo e mute tem rampa de 12 ms: deixa o ganho assentar antes de medir.
    for (int block = 0; block < 40; ++block)
        h.render (testBlockSize);

    h.readPadPeaks();

    h.engine.pushTrigger (0, 1.0f);
    h.render (testBlockSize);
    check (h.readPadPeaks()[0] < 1.0e-4f, "solo silencia os outros canais");

    h.engine.pushTrigger (1, 1.0f);
    h.render (testBlockSize);
    check (h.readPadPeaks()[1] > 0.01f, "canal em solo segue audivel");

    h.parameters.pads[1].solo = false;
    h.parameters.pads[0].mute = true;

    for (int block = 0; block < 40; ++block)
        h.render (testBlockSize);

    h.readPadPeaks();
    h.engine.pushTrigger (0, 1.0f);
    h.render (testBlockSize);
    check (h.readPadPeaks()[0] < 1.0e-4f, "mute corta o canal");
}

void testMidiAndLatency()
{
    std::printf ("\n[mapa MIDI e latencia]\n");

    Harness h;

    juce::MidiBuffer kick;
    kick.addEvent (juce::MidiMessage::noteOn (1, 36, (juce::uint8) 100), 0);
    h.render (testBlockSize, kick);

    check (h.engine.lastTriggeredLayer (0) >= 0, "note-on GM mapeia para o pad");

    // O click tem ataque no frame 0: um evento no meio do bloco deve soar
    // exatamente nele, sem atraso. Harness novo garante bloco sem residuo.
    Harness clickH;
    juce::MidiBuffer click;
    click.addEvent (juce::MidiMessage::noteOn (1, 49, (juce::uint8) 127), 128);
    clickH.render (testBlockSize, click);

    const auto& main = clickH.buses[0];

    check (std::abs (main.getSample (0, 127)) < 1.0e-6f, "nenhum audio antes do evento",
           juce::String (std::abs (main.getSample (0, 127)), 12));
    check (std::abs (main.getSample (0, 128)) > 0.05f, "evento soa no frame exato");
}

void testBypass()
{
    std::printf ("\n[bypass]\n");

    Harness h;

    h.engine.pushTrigger (0, 1.0f);
    h.render (testBlockSize);
    check (h.busPeak (0) > 0.01f, "audio presente antes do bypass");

    for (int block = 0; block < 20; ++block)
        h.render (testBlockSize, juce::MidiBuffer(), true);

    check (h.busPeak (0) < 1.0e-4f, "bypass silencia a saida");
    check (h.engine.activeVoices() > 0, "bypass nao mata as vozes");

    for (int block = 0; block < 10; ++block)
        h.render (testBlockSize);

    check (h.busPeak (0) > 1.0e-3f, "bypass desligado retoma o audio");
}

// O medidor do master separa compressor e limitador: cada rail da UI so
// recebe a reducao da propria etapa.
void testMasterMeterSplit()
{
    std::printf ("\n[medicao do master: comp e limiter separados]\n");

    constexpr int frames = 512;
    std::array<float, frames> left {};
    std::array<float, frames> right {};
    std::array<float, frames> fx1L {}, fx1R {}, fx2L {}, fx2R {};

    auto fillSine = [&] (float amplitude)
    {
        for (int i = 0; i < frames; ++i)
        {
            const auto value = amplitude
                * std::sin (2.0 * juce::MathConstants<double>::pi * (double) i / 24.0);
            left[(size_t) i] = (float) value;
            right[(size_t) i] = (float) value;
        }
    };

    auto run = [&] (const MasterParameters& parameters, float amplitude)
    {
        MasterBus bus;
        bus.prepare (testSampleRate);
        bus.setParameters (parameters);

        MasterBus::Meter worst;
        worst.compReductionDb = worst.limReductionDb = 0.0f;

        for (int block = 0; block < 8; ++block)
        {
            fillSine (amplitude);
            bus.process (left.data(), right.data(), fx1L.data(), fx1R.data(),
                         fx2L.data(), fx2R.data(), frames);
            const auto meter = bus.readMeters();
            worst.compReductionDb = std::min (worst.compReductionDb, meter.compReductionDb);
            worst.limReductionDb = std::min (worst.limReductionDb, meter.limReductionDb);
        }

        return worst;
    };

    MasterParameters base;
    base.trim = 0.5f;   // 0 dB
    base.fx1Regen = 0.0f;
    base.fx2Drive = 0.0f;

    MasterParameters compOnly = base;
    compOnly.comp = 1.0f;
    compOnly.limiter = false;

    const auto comp = run (compOnly, 0.9f);
    check (comp.compReductionDb < -0.1f, "compressor mede reducao proprio",
           juce::String (comp.compReductionDb, 2));
    check (comp.limReductionDb == 0.0f, "limiter desligado nao mede nada");

    MasterParameters limOnly = base;
    limOnly.comp = 0.0f;
    limOnly.limiter = true;

    const auto lim = run (limOnly, 1.0f);
    check (lim.compReductionDb == 0.0f, "compressor desligado nao mede nada");
    check (lim.limReductionDb < -0.05f, "limitador mede reducao proprio",
           juce::String (lim.limReductionDb, 3));

    const auto off = run (base, 0.8f);
    check (off.compReductionDb == 0.0f && off.limReductionDb == 0.0f,
           "sem cadeia dinamica os dois rails ficam zerados");
}

// O envio de ambiente (S1) volta 100% wet pelo SALAO, entao o knob entra com
// atenuacao quadrada: um dedo nao injeta sala mais alta que a fonte.
void testSendTaper()
{
    std::printf ("\n[taper do envio de ambiente]\n");

    check (mapping::wetSendGain (0.0f) == 0.0f, "zero nao injeta nada");
    check (mapping::wetSendGain (0.10f) < 0.005f, "dedo no knob injeta menos de -46 dB",
           juce::String (mapping::gainToDb (mapping::wetSendGain (0.10f)), 1) + " dB");
    check (mapping::wetSendGain (1.0f) <= 0.25f, "topo do envio limita em -12 dB",
           juce::String (mapping::gainToDb (mapping::wetSendGain (1.0f)), 1) + " dB");
    check (mapping::wetSendGain (0.5f) > mapping::wetSendGain (0.4f)
               && mapping::wetSendGain (0.5f) < mapping::wetSendGain (0.6f),
           "a curva e monotona");
    check (mapping::wetSendGain (0.5f) < mapping::unit (0.5f),
           "a metade do knob injeta menos que a metade linear");
}

void testNoAllocations()
{
    std::printf ("\n[RT-safety]\n");

    Harness h;

    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 38, (juce::uint8) 90), 64);

    std::array<juce::AudioBuffer<float>*, kNumOutputBuses> outputs {};

    for (int bus = 0; bus < kNumOutputBuses; ++bus)
        outputs[(size_t) bus] = &h.buses[(size_t) bus];

    h.engine.setParameters (h.parameters);

    allocationTripped.store (false, std::memory_order_relaxed);
    trackAllocations.store (true, std::memory_order_relaxed);

    for (int block = 0; block < 64; ++block)
    {
        h.engine.pushTrigger (block % kNumPads, 0.8f);
        h.engine.process (midi, outputs, testBlockSize, false);
    }

    trackAllocations.store (false, std::memory_order_relaxed);

    check (! allocationTripped.load (std::memory_order_relaxed),
           "process nao aloca memoria");
}

bool writeTestWav (const juce::File& file, float level)
{
    file.deleteFile();

    auto stream = std::unique_ptr<juce::FileOutputStream> (file.createOutputStream());

    if (stream == nullptr)
        return false;

    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> writer (
        wav.createWriterFor (stream.get(), 44100.0, 1, 16, {}, 0));

    if (writer == nullptr)
        return false;

    stream.release();

    juce::AudioBuffer<float> buffer (1, 100);
    buffer.clear();
    buffer.getWritePointer (0)[0] = level;

    return writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
}

// Importacao de pasta com marcacao vl/rr, leitura de WAV e ida e volta do .dfkit.
void testSampleFolderImport()
{
    std::printf ("\n[importacao de pasta]\n");

    const auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory)
                            .getChildFile ("forja-drumforge-test");
    folder.deleteRecursively();
    folder.createDirectory();

    check (writeTestWav (folder.getChildFile ("Kick_vl1_rr1.wav"), 0.20f), "gravei kick vl1");
    check (writeTestWav (folder.getChildFile ("Kick_vl2_rr1.wav"), 0.40f), "gravei kick vl2");
    check (writeTestWav (folder.getChildFile ("Snare_rr1.wav"), 0.30f), "gravei snare rr1");
    check (writeTestWav (folder.getChildFile ("Snare_rr2.wav"), 0.35f), "gravei snare rr2");
    check (writeTestWav (folder.getChildFile ("Hats.wav"), 0.15f), "gravei hats");

    juce::String error;
    const auto kit = Kit::createFromSampleFolder (folder, error);

    check (error.isEmpty(), "importacao sem erro", error);
    check (kit.name == folder.getFileName(), "kit herda o nome da pasta");
    check (kit.padsWithAudio() == 3, "tres pads classificados");

    const auto& kick = kit.pads[0];
    check (kick.layerCount() == 2, "kick importado em duas camadas");
    check (kick.sampleCount() == 2, "kick com dois samples");

    const auto& snare = kit.pads[1];
    check (snare.layerCount() == 1, "snare em uma camada");
    check (snare.sampleCount() == 2, "snare com dois round-robins");
    check (kit.pads[5].chokeGroup == 1, "hat entra no grupo de choke");

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    const auto sample = SampleStore::readSample (formats, folder.getChildFile ("Kick_vl2_rr1.wav"));
    check (sample != nullptr, "WAV decodificado");
    check (sample != nullptr && sample->length == 100, "comprimento do WAV");
    check (sample != nullptr && std::abs (sample->sampleRate - 44100.0) < 0.5, "sample rate do WAV");
    check (sample != nullptr && std::abs (sample->audio.getSample (0, 0) - 0.40f) < 0.002f,
           "audio do WAV decodificado");

    const auto kitFile = folder.getChildFile ("teste.dfkit");
    check (kit.saveToFile (kitFile, error), "kit salvo em .dfkit", error);

    Kit reloaded;

    if (kitFile.existsAsFile())
    {
        check (Kit::loadFromFile (kitFile, reloaded, error), "kit recarregado do .dfkit", error);
        check (reloaded.pads[0].layerCount() == 2, "camadas preservadas no .dfkit");
        check (reloaded.pads[1].layerCount() == 1 && reloaded.pads[1].sampleCount() == 2,
               "round-robins preservados no .dfkit");
        check (reloaded.pads[0].layers[0][0].contains ("Kick_vl1_rr1.wav"),
               "caminho do sample preservado");
    }

    folder.deleteRecursively();
}

// Leitor do formato de texto do Drumlabooh (drumkit.txt, especificacao 1.1):
// nota embutida, prefixos > e *, camadas separadas por virgula, lista .txt,
// subpastas, info.txt e o teto de 16 pads.
void testDrumlaboohTxtKit()
{
    std::printf ("\n[drumkit.txt do Drumlabooh]\n");

    const auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory)
                            .getChildFile ("forja-drumforge-txtkit");
    folder.deleteRecursively();
    folder.createDirectory();

    const auto bass = folder.getChildFile ("bass");
    const auto hats = folder.getChildFile ("hats");
    bass.createDirectory();
    hats.createDirectory();

    check (writeTestWav (bass.getChildFile ("k01.wav"), 0.10f), "gravei bass k01");
    check (writeTestWav (bass.getChildFile ("k02.wav"), 0.40f), "gravei bass k02");
    check (writeTestWav (hats.getChildFile ("closed1.wav"), 0.20f), "gravei hat fechado 1");
    check (writeTestWav (hats.getChildFile ("closed2.wav"), 0.25f), "gravei hat fechado 2");
    check (writeTestWav (hats.getChildFile ("open.wav"), 0.30f), "gravei hat aberto");
    check (writeTestWav (folder.getChildFile ("sn01.wav"), 0.15f), "gravei snare 1");
    check (writeTestWav (folder.getChildFile ("sn02.wav"), 0.18f), "gravei snare 2");
    check (writeTestWav (folder.getChildFile ("sn03.wav"), 0.21f), "gravei snare 3");
    check (writeTestWav (folder.getChildFile ("crash16.wav"), 0.35f), "gravei crash");
    check (writeTestWav (folder.getChildFile ("cowbell.wav"), 0.12f), "gravei cowbell");

    hats.getChildFile ("closed.txt").replaceWithText ("closed1.wav\r\nclosed2.wav\r\n");
    folder.getChildFile ("info.txt").replaceWithText ("Kit de teste do leitor.\nSegunda linha.\n");

    // Convencao drum_sklad: image.png e a foto de preview do kit. So precisa
    // existir; a deteccao nao decodifica (o decodificador roda no store).
    folder.getChildFile ("image.png").replaceWithText ("png-falso");

    folder.getChildFile ("drumkit.txt").replaceWithText (
        "# comentario nao e instrumento\n"
        "[36]Bass=bass/k01.wav,bass/k02.wav\n"
        ">Snare=sn01.wav,sn02.wav,sn03.wav\n"
        "*Crash 16=crash16.wav\n"
        "hihat close=hats/closed.txt\n"
        "[46]Hihat Open=hats/open.wav\n"
        "^Cowbell=cowbell.wav\n");

    const auto kitFile = folder.getChildFile ("drumkit.txt");

    juce::String error;
    check (Kit::folderHasTxtKit (folder), "pasta com drumkit.txt e reconhecida");
    check (Kit::isTxtKitFile (kitFile), "drumkit.txt abre como kit");
    check (! Kit::isTxtKitFile (hats.getChildFile ("closed.txt")), "lista .txt nao e um kit");

    const auto kit = Kit::createFromDrumlaboohTxt (kitFile, error);

    check (error.isEmpty(), "leitura sem erro", error);
    check (kit.name == folder.getFileName(), "kit herda o nome da pasta");
    check (kit.description.contains ("Kit de teste do leitor"), "info.txt vira descricao");
    check (kit.padsWithAudio() == 6, "seis instrumentos viram seis pads",
           juce::String (kit.padsWithAudio()));

    const auto& bassPad = kit.pads[0];
    check (bassPad.note == 36 && bassPad.name == "Bass", "nota embutida escolhe o pad");
    check (bassPad.layerCount() == 2, "lista em virgula sao camadas de velocity");
    check (bassPad.layers[0][0].contains ("k01.wav") && bassPad.layers[1][0].contains ("k02.wav"),
           "camadas vao da mais suave para a mais forte");

    const auto& snarePad = kit.pads[1];
    check (snarePad.type == "snare" && snarePad.note == 38, "nome sem nota classifica o pad");
    check (snarePad.layerCount() == 1 && snarePad.sampleCount() == 3, "prefixo > vira round-robin");
    check (kit.pads[7].layerCount() == 1 && kit.pads[7].sampleCount() == 1, "prefixo * toca uma tomada");
    check (kit.pads[7].name == "Crash 16", "asterisco nao sobra no nome");
    check (kit.pads[11].name == "Cowbell" && kit.pads[11].sampleCount() == 1, "prefixo ^ e multicamada");

    const auto& hatPad = kit.pads[5];
    check (hatPad.type == "hat" && hatPad.chokeGroup == 1, "chimbal fechado entra no choke");
    check (hatPad.sampleCount() == 2, "lista .txt expande os arquivos");
    check (hatPad.layers[0][0].contains ("closed1.wav"), "caminho da lista e relativo a ela");
    check (kit.pads[6].note == 46 && kit.pads[6].chokeGroup == 1, "nota 46 acha o chimbal aberto");

    // IMPORTAR PASTA e o arrastar e soltar passam pelo mesmo leitor.
    const auto imported = Kit::createFromSampleFolder (folder, error);
    check (error.isEmpty() && imported.padsWithAudio() == 6, "pasta delega ao drumkit.txt");
    check (imported.pads[1].sampleCount() == 3, "round-robin sobrevem na delegacao");

    Kit loaded;
    check (Kit::loadFromFile (kitFile, loaded, error), "loadFromFile aceita o drumkit.txt", error);
    check (loaded.sourceFile == kitFile.getFullPathName(), "kit aponta para o proprio .txt");

    check (kit.imageFile == folder.getChildFile ("image.png").getFullPathName(),
           "image.png da pasta vira preview do kit");
    check (imported.imageFile == kit.imageFile, "importar a pasta herda o preview");

    // O .dfkit grava o preview em linha #image (comentario, portavel) com o
    // caminho relativo quando a foto esta na pasta do kit.
    const auto withImage = kit.createText (folder, false);
    check (withImage.contains ("#image image.png"), "dfkit grava #image relativo");

    const auto imageBack = Kit::createFromText (withImage, folder, "Ignorado");
    check (imageBack.imageFile == folder.getChildFile ("image.png").getFullPathName(),
           "#image volta absoluto na recarga");

    check (! Kit::loadFromFile (folder.getChildFile ("sn01.wav"), loaded, error),
           "WAV solto nao e lido como kit");
    check (error.isNotEmpty(), "erro de kit invalido e descritivo");

    folder.deleteRecursively();

    // Camadas demais para o motor: agrupam em blocos contiguos e nada se perde.
    const auto manyFolder = juce::File::getSpecialLocation (juce::File::tempDirectory)
                               .getChildFile ("forja-drumforge-manylayers");
    manyFolder.deleteRecursively();
    manyFolder.createDirectory();

    juce::String many = "kick=";

    for (int i = 1; i <= 11; ++i)
    {
        const auto name = "k" + juce::String (i).paddedLeft ('0', 2);
        writeTestWav (manyFolder.getChildFile (name + ".wav"), 0.10f);
        many += (i > 1 ? "," : "") + name + ".wav";
    }

    manyFolder.getChildFile ("drumkit.txt").replaceWithText (many + "\n");

    const auto manyKit = Kit::createFromDrumlaboohTxt (manyFolder.getChildFile ("drumkit.txt"), error);
    check (manyKit.pads[0].layerCount() == kMaxLayers, "mais de oito camadas sao agrupadas",
           juce::String (manyKit.pads[0].layerCount()));
    check (manyKit.pads[0].sampleCount() == 11, "nenhum sample se perde no agrupamento");

    manyFolder.deleteRecursively();

    // Instruments alem dos 16 pads: os que cabem viram pads tocaveis e sobra um aviso.
    const auto wideFolder = juce::File::getSpecialLocation (juce::File::tempDirectory)
                               .getChildFile ("forja-drumforge-widekit");
    wideFolder.deleteRecursively();
    wideFolder.createDirectory();
    writeTestWav (wideFolder.getChildFile ("hit.wav"), 0.20f);

    juce::String wide;

    for (int i = 1; i <= 18; ++i)
        wide += "instr" + juce::String (i).paddedLeft ('0', 2) + "=hit.wav\n";

    wideFolder.getChildFile ("drumkit.txt").replaceWithText (wide);

    const auto wideKit = Kit::createFromDrumlaboohTxt (wideFolder.getChildFile ("drumkit.txt"), error);
    check (wideKit.padsWithAudio() == kNumPads, "kit gigante preenche os 16 pads",
           juce::String (wideKit.padsWithAudio()));

    int padsComNota = 0;
    for (const auto& pad : wideKit.pads)
        padsComNota += pad.note >= 0 ? 1 : 0;

    check (padsComNota == kNumPads, "todo pad usado ganha nota MIDI", juce::String (padsComNota));

    wideFolder.deleteRecursively();

    // Layout do Jazzkit do drum_sklad: linha repetida, aspas no tamanho do tom,
    // pecas sem nota ao lado do mapa embutido. Os WAV nao precisam existir aqui;
    // o leitor so monta o mapa, e o store e quem reclama de arquivo ausente.
    const auto realFolder = juce::File::getSpecialLocation (juce::File::tempDirectory)
                               .getChildFile ("forja-drumforge-jazzkit");
    realFolder.deleteRecursively();
    realFolder.createDirectory();

    realFolder.getChildFile ("drumkit.txt").replaceWithText (
        "[36]*Bass Drum=bass/b01.wav,bass/b02.wav\n"
        "Snare=snare/sn-tight-1.wav\n"
        "Snare=snare/sn-tight-1.wav\n"
        "[38]*Snare=snare/sn-tight-1.wav,snare/sn-tight-2.wav,snare/sn-loose.wav\n"
        "[41]Floor Tom=toms/floor.wav\n"
        "[45]Tom \"12=toms/t12.wav\n"
        "[47]Tom \"10=toms/t10.wav\n"
        "[50]Tom \"8=toms/t8.wav\n"
        "[42]Hihat Close=cymbals/hat1.wav,cymbals/hat2.wav,cymbals/hat3.wav\n"
        "Hihat Close Hit=cymbals/hit1.wav\n"
        "Hihat Close Half=cymbals/half1.wav\n"
        "[46]*Hihat Open=cymbals/open1.wav,cymbals/open2.wav\n"
        "[49]Cymbal Low=cymbals/low1.wav,cymbals/low2.wav\n"
        "[55]Cymbal Big=cymbals/big1.wav\n"
        "[51]*Cymbal Small=cymbals/small1.wav\n");

    const auto jazz = Kit::createFromDrumlaboohTxt (realFolder.getChildFile ("drumkit.txt"), error);

    check (error.isEmpty(), "jazzkit lido sem erro", error);
    check (jazz.padsWithAudio() == 13, "treze pecas viram treze pads",
           juce::String (jazz.padsWithAudio()));

    bool notasUnicas = true;
    bool todosNotaveis = true;

    for (int i = 0; i < kNumPads; ++i)
    {
        const auto& pad = jazz.pads[(size_t) i];

        if (! pad.hasSamples())
            continue;

        if (pad.note < 0)
            todosNotaveis = false;

        for (int j = i + 1; j < kNumPads; ++j)
            if (jazz.pads[(size_t) j].hasSamples() && jazz.pads[(size_t) j].note == pad.note)
                notasUnicas = false;
    }

    check (todosNotaveis, "toda peca do jazzkit ganha nota MIDI");
    check (notasUnicas, "nenhuma nota repetida entre as pecas");

    int hatPads = 0;

    for (const auto& pad : jazz.pads)
        hatPads += (pad.hasSamples() && pad.type == "hat") ? 1 : 0;

    check (hatPads == 3, "os tres chimbais fechados ficam em pads proprios",
           juce::String (hatPads));
    check (jazz.pads[0].sampleCount() == 2 && jazz.pads[0].layerCount() == 1,
           "bumbo em round-robin numa camada");
    check (jazz.pads[5].layerCount() == 3, "chimbal fechado com tres camadas de velocity",
           juce::String (jazz.pads[5].layerCount()));
    check (jazz.pads[6].sampleCount() == 2 && jazz.pads[6].chokeGroup == 1,
           "chimbal aberto continua no grupo de choke");

    int tomsComNota = 0;

    for (const auto& pad : jazz.pads)
        tomsComNota += (pad.hasSamples() && (pad.type == "tom" || pad.type == "floor")) ? 1 : 0;

    check (tomsComNota == 4, "os quatro toms do kit cabem nos pads",
           juce::String (tomsComNota));

    realFolder.deleteRecursively();
}

// Leitor do drumkit.labooh (XML do Drumlabooh/drum_sklad): <sample name note
// layer_index_mode> com lista de arquivos. Mapeia como o drumkit.txt e roda pelo
// mesmo alocador em estagios. root type="alt" e layer_index_mode viram round-robin.
void testDrumlaboohXmlKit()
{
    std::printf ("\n[drumkit.labooh do Drumlabooh]\n");

    const auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory)
                            .getChildFile ("forja-drumforge-labooh");
    folder.deleteRecursively();
    folder.createDirectory();

    const auto hats = folder.getChildFile ("cymbals");
    hats.createDirectory();

    check (writeTestWav (folder.getChildFile ("k01.wav"), 0.10f), "gravei kick 1");
    check (writeTestWav (folder.getChildFile ("k02.wav"), 0.40f), "gravei kick 2");
    check (writeTestWav (folder.getChildFile ("b01.wav"), 0.30f), "gravei bass 1");
    check (writeTestWav (folder.getChildFile ("snare.wav"), 0.20f), "gravei snare");
    check (writeTestWav (hats.getChildFile ("hat-c.wav"), 0.15f), "gravei hat fechado");
    check (writeTestWav (hats.getChildFile ("hat-2.wav"), 0.18f), "gravei hat 2");
    check (writeTestWav (folder.getChildFile ("perc.wav"), 0.12f), "gravei perc");

    folder.getChildFile ("kick.txt").replaceWithText ("k01.wav\r\nk02.wav\r\n");
    folder.getChildFile ("bass.txt").replaceWithText ("b01.wav\r\n");
    hats.getChildFile ("closed.txt").replaceWithText ("hat-c.wav\r\nhat-2.wav\r\n");
    folder.getChildFile ("info.txt").replaceWithText ("Kit labooh de teste.\n");

    folder.getChildFile ("drumkit.labooh").replaceWithText (
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<root type=\"alt\">\n"
        "  <sample name=\"Kick\" note=\"36\">kick.txt</sample>\n"
        "  <sample name=\"Hihat Close\" note=\"42\">cymbals/closed.txt</sample>\n"
        "  <sample name=\"Snare 1\" note=\"38\">snare.wav</sample>\n"
        "  <sample name=\"Snare 2\" note=\"40\">snare.wav</sample>\n"
        "  <sample name=\"Perc A\">perc.wav</sample>\n"
        "  <sample name=\"Bass Drum\" note=\"36\" layer_index_mode=\"alt\">bass.txt</sample>\n"
        "</root>\n");

    const auto laboohFile = folder.getChildFile ("drumkit.labooh");

    juce::String error;
    check (Kit::folderHasTxtKit (folder), "pasta com drumkit.labooh e reconhecida");
    check (Kit::isTxtKitFile (laboohFile), "drumkit.labooh abre como kit");
    check (! Kit::isTxtKitFile (folder.getChildFile ("kick.txt")), "lista .txt nao e um kit");

    const auto kit = Kit::createFromDrumlaboohTxt (folder, error);

    check (error.isEmpty(), "leitura labooh sem erro", error);
    check (kit.sourceFile == laboohFile.getFullPathName(), "folder sem .txt escolhe o .labooh");
    check (kit.description.contains ("Kit labooh de teste"), "info.txt vira descricao");
    check (kit.padsWithAudio() == 5, "cinco pads (Bass Drum mescla no Kick)",
           juce::String (kit.padsWithAudio()));

    const auto& kickPad = kit.pads[0];
    check (kickPad.note == 36 && kickPad.type == "kick", "pad do bumbo pela nota 36");
    check (kickPad.sampleCount() == 3,
           "Kick e Bass Drum (mesma nota 36) mesclam num so pad",
           juce::String (kickPad.sampleCount()));
    check (kickPad.layerCount() == 2, "cada instrumento entra como uma camada",
           juce::String (kickPad.layerCount()));

    const auto& hatPad = kit.pads[5];
    check (hatPad.type == "hat" && hatPad.chokeGroup == 1, "chimbal entra no choke");
    check (hatPad.sampleCount() == 2, "subpasta e lista .txt expandem");

    check (kit.pads[1].note == 38 && kit.pads[1].name == "Snare 1", "Snare 1 acha a nota 38");

    bool snare2 = false;
    for (const auto& pad : kit.pads)
        snare2 = snare2 || (pad.hasSamples() && pad.note == 40 && pad.name == "Snare 2");
    check (snare2, "Snare 2 com nota 40 ganha pad proprio");

    bool perc = false;
    for (const auto& pad : kit.pads)
        perc = perc || (pad.hasSamples() && pad.type == "perc" && pad.name == "Perc A");
    check (perc, "Perc A sem nota cai num pad de percussao");

    bool notasUnicas = true;
    for (int i = 0; i < kNumPads; ++i)
    {
        const auto& a = kit.pads[(size_t) i];
        if (! a.hasSamples() || a.note < 0)
            continue;
        for (int j = i + 1; j < kNumPads; ++j)
        {
            const auto& b = kit.pads[(size_t) j];
            if (b.hasSamples() && b.note == a.note)
                notasUnicas = false;
        }
    }
    check (notasUnicas, "mesma nota em duas pecas nao duplica pad");

    // IMPORTAR PASTA e o arrastar e soltar passam pelo mesmo leitor.
    const auto imported = Kit::createFromSampleFolder (folder, error);
    check (error.isEmpty() && imported.padsWithAudio() == 5, "pasta delega ao drumkit.labooh");

    Kit loaded;
    check (Kit::loadFromFile (laboohFile, loaded, error), "loadFromFile aceita o .labooh", error);

    folder.deleteRecursively();
}

bool writeTestMidi (const juce::File& file, double bpm,
                    const std::vector<std::pair<double, int>>& hits)
{
    file.deleteFile();
    file.getParentDirectory().createDirectory();

    juce::MidiFile midi;
    midi.setTicksPerQuarterNote (96);

    juce::MidiMessageSequence track;
    track.addEvent (juce::MidiMessage::tempoMetaEvent ((int) std::llround (60000000.0 / bpm)));

    for (const auto& hit : hits)
        track.addEvent (juce::MidiMessage::noteOn (1, hit.second, (juce::uint8) 100),
                        hit.first * 96.0);

    midi.addTrack (track);

    auto stream = std::unique_ptr<juce::FileOutputStream> (file.createOutputStream());

    return stream != nullptr && midi.writeTo (*stream);
}

// Biblioteca: leitura do .mid, metadados, varredura de pacotes e importacao.
void testLoopLibrary()
{
    std::printf ("\n[biblioteca de loops]\n");

    const auto root = juce::File::getSpecialLocation (juce::File::tempDirectory)
                          .getChildFile ("forja-drumforge-loops");
    root.deleteRecursively();
    root.createDirectory();

    const auto pack = root.getChildFile ("Pacote A");
    const auto groove = pack.getChildFile ("Groove 01.mid");

    check (writeTestMidi (groove, 120.0, { { 0.0, 36 }, { 1.0, 38 }, { 1.5, 36 }, { 2.0, 38 } }),
           "gravei um .mid de teste");

    juce::String error;
    loops::Sequence sequence;

    check (loops::readSequence (groove, sequence, error), "loop MIDI lido", error);
    check (std::abs (sequence.bpm - 120.0) < 0.01, "BPM vem do proprio arquivo");
    check (sequence.beats == 4.0, "loop fecha em compasso inteiro");
    check (sequence.name == "Groove 01", "nome vem do arquivo");
    check (sequence.hits.size() == 4, "todos os golpes entram",
           juce::String ((int) sequence.hits.size()));

    if (sequence.hits.size() == 4)
    {
        check (sequence.hits[1].beat == 1.0, "golpes guardados em beats");
        check (sequence.hits[1].note == 38, "nota preservada");
        check (std::abs (sequence.hits[1].velocity - 100.0f / 127.0f) < 0.01f,
               "velocity normalizada");
        check (sequence.hits[0].beat <= sequence.hits[1].beat, "golpes em ordem de tempo");
    }

    const auto longGroove = pack.getChildFile ("Virada 02.mid");
    check (writeTestMidi (longGroove, 140.0, { { 0.0, 36 }, { 5.0, 38 } }),
           "gravei um loop de dois compassos");

    loops::Sequence longSequence;
    check (loops::readSequence (longGroove, longSequence, error), "loop longo lido", error);
    check (longSequence.beats == 8.0, "loop longo fecha em dois compassos");
    check (std::abs (longSequence.bpm - 140.0) < 0.01, "BPM do loop longo");

    const auto entries = loops::scan (root);
    check (entries.size() == 2, "varredura acha os dois loops",
           juce::String ((int) entries.size()));
    check (! entries.empty() && entries[0].folder == "Pacote A", "pacote vem da subpasta");
    check (! entries.empty() && entries[0].hits == 4, "varredura conta os golpes");
    check (! entries.empty() && entries[0].bars() == 1, "varredura mede os compassos");

    const auto broken = root.getChildFile ("quebrado.mid");
    broken.replaceWithText ("nao sou midi");

    loops::Sequence brokenSequence;
    check (! loops::readSequence (broken, brokenSequence, error), "arquivo invalido e recusado");
    check (error.isNotEmpty(), "erro do arquivo invalido e descritivo");

    const auto noNotes = root.getChildFile ("semnotas.mid");
    juce::MidiFile emptyMidi;
    emptyMidi.setTicksPerQuarterNote (96);

    juce::MidiMessageSequence emptyTrack;
    emptyTrack.addEvent (juce::MidiMessage::textMetaEvent (1, "sem notas"), 0.0);
    emptyMidi.addTrack (emptyTrack);

    {
        auto stream = std::unique_ptr<juce::FileOutputStream> (noNotes.createOutputStream());

        if (stream != nullptr)
            emptyMidi.writeTo (*stream);
    }

    loops::Sequence noNoteSequence;
    check (! loops::readSequence (noNotes, noNoteSequence, error), "MIDI sem notas e recusado");

    const auto incoming = juce::File::getSpecialLocation (juce::File::tempDirectory)
                              .getChildFile ("forja-drumforge-loops-in")
                              .getChildFile ("Groove Novo.mid");
    check (writeTestMidi (incoming, 100.0, { { 0.0, 36 }, { 2.0, 38 } }), "gravei um .mid de fora");

    const auto imported = loops::importFile (incoming, root);
    check (imported.existsAsFile(), "importacao copia para a biblioteca");
    check (imported.getFileName() == "Groove Novo.mid", "importacao mantem o nome");

    loops::Sequence importedSequence;
    check (loops::readSequence (imported, importedSequence, error), "loop importado abre", error);

    // Folder e o caminho relativo completo: categorias aninhadas nao colapsam.
    const auto viradas = pack.getChildFile ("Viradas");
    viradas.createDirectory();
    check (writeTestMidi (viradas.getChildFile ("Virada Curta.mid"), 120.0, { { 0.0, 38 } }),
           "gravei loop aninhado");

    bool foundNested = false;

    for (const auto& entry : loops::scan (root))
        if (entry.name == "Virada Curta")
            foundNested = entry.folder == "Pacote A/Viradas";

    check (foundNested, "folder guarda o caminho relativo ate a subpasta");

    // Importar uma pasta inteira preserva a arvore de categorias e so copia .mid.
    const auto incomingDir = incoming.getParentDirectory();
    const auto externa = incomingDir.getChildFile ("Pacote Externo");
    externa.getChildFile ("Fills").createDirectory();
    externa.getChildFile ("Chorus").createDirectory();
    check (writeTestMidi (externa.getChildFile ("Fills").getChildFile ("Fill 01.mid"),
                          120.0, { { 0.0, 38 } }), "gravei fill de fora");
    check (writeTestMidi (externa.getChildFile ("Chorus").getChildFile ("Chorus 01.mid"),
                          120.0, { { 0.0, 36 } }), "gravei chorus de fora");
    externa.getChildFile ("Fills").getChildFile ("leia-me.txt").replaceWithText ("nao e midi");

    const auto copiedLoops = loops::importFolder (externa, root);
    check (copiedLoops == 2, "importacao de pasta copia so os .mid",
           juce::String (copiedLoops));
    check (root.getChildFile ("Pacote Externo/Fills/Fill 01.mid").existsAsFile(),
           "pasta de categoria vai junto na importacao");
    check (! root.getChildFile ("Pacote Externo/Fills/leia-me.txt").existsAsFile(),
           "lixo da pasta nao entra na biblioteca");

    bool foundImportedCategory = false;

    for (const auto& entry : loops::scan (root))
        if (entry.name == "Fill 01")
            foundImportedCategory = entry.folder == "Pacote Externo/Fills";

    check (foundImportedCategory, "categoria importada aparece na varredura");

    incoming.getParentDirectory().deleteRecursively();
    root.deleteRecursively();
}

// Renderiza um bloco na ordem do processor: preview primeiro, engine depois.
void renderWithPreview (Harness& h, loops::LoopPlayer* player, double bpm, int frames)
{
    static const juce::MidiBuffer emptyMidi;

    if (player != nullptr)
        player->process (frames, bpm, h.engine);

    std::array<juce::AudioBuffer<float>*, kNumOutputBuses> outputs {};

    for (int bus = 0; bus < kNumOutputBuses; ++bus)
    {
        h.buses[(size_t) bus].clear (0, frames);
        outputs[(size_t) bus] = &h.buses[(size_t) bus];
    }

    h.engine.setParameters (h.parameters);
    h.engine.process (emptyMidi, outputs, frames, false);
}

// Preview: os golpes do arquivo caem no frame certo e o loop volta ao compasso.
void testLoopPreview()
{
    std::printf ("\n[preview de loop]\n");

    Harness h;

    loops::Sequence sequence;
    sequence.name = "Teste";
    sequence.bpm = 120.0;
    sequence.beats = 1.0;
    sequence.hits = { { 0.0, 49, 1.0f }, { 0.5, 49, 1.0f } };

    loops::LoopPlayer player;
    player.prepare (testSampleRate);
    player.request (&sequence, 1);
    player.start();

    check (player.isPlaying(), "preview entra em reproducao");

    std::vector<int> onsets;
    onsets.reserve (256);

    int absolute = 0;

    allocationTripped.store (false, std::memory_order_relaxed);
    trackAllocations.store (true, std::memory_order_relaxed);

    for (int block = 0; block < 250; ++block)
    {
        renderWithPreview (h, &player, 120.0, testBlockSize);

        const float* data = h.buses[0].getReadPointer (0);

        for (int i = 0; i < testBlockSize; ++i)
        {
            if (std::abs (data[i]) <= 0.05f)
                continue;

            if (onsets.empty() || absolute + i - onsets.back() > 100)
                onsets.push_back (absolute + i);
        }

        absolute += testBlockSize;
    }

    trackAllocations.store (false, std::memory_order_relaxed);

    check (! allocationTripped.load (std::memory_order_relaxed), "preview nao aloca memoria");
    check (player.activeToken() == 1, "player adota a sequencia por token");
    check (onsets.size() >= 5, "preview dispara os golpes",
           juce::String ((int) onsets.size()));

    if (onsets.size() >= 5)
    {
        check (onsets[0] == 0, "primeiro golpe no frame 0", juce::String (onsets[0]));
        check (std::abs (onsets[1] - 12000) <= 1, "golpe em 1/2 beat sem atraso de bloco",
               juce::String (onsets[1]));
        check (std::abs (onsets[2] - 24000) <= 1, "loop volta ao compasso no frame certo",
               juce::String (onsets[2]));
        check (std::abs (onsets[3] - 36000) <= 1, "segundo compasso mantem a precisao",
               juce::String (onsets[3]));
    }

    const double beats = player.positionBeats();
    check (beats >= 0.0 && beats < sequence.beats, "cursor fica dentro do loop",
           juce::String (beats, 3));

    player.stop();
    check (! player.isPlaying(), "stop desliga o preview");

    const int before = (int) onsets.size();
    for (int block = 0; block < 20; ++block)
        renderWithPreview (h, &player, 120.0, testBlockSize);

    int after = 0;
    {
        const float* data = h.buses[0].getReadPointer (0);

        for (int i = 0; i < testBlockSize; ++i)
            if (std::abs (data[i]) > 0.05f)
                ++after;
    }

    check (after == 0, "preview parado nao dispara mais", juce::String (after));
    check (before == (int) onsets.size(), "nenhum golpe novo fora do loop");
}

void testIdleSilence()
{
    std::printf ("\n[silencio com a mesa parada]\n");

    Harness h;
    float peak = 0.0f;

    // Duas segundos de bloco vazio: o master roda com SALAO e LIXA ligados
    // por padrao. O room tone do kRockstar2 nao pode aparecer aqui.
    for (int block = 0; block < 400; ++block)
    {
        h.render (testBlockSize);
        peak = std::max (peak, h.busPeak (0));
    }

    check (peak <= 1.0e-6f, "sem ruido no master em repouso", juce::String (peak));
}

bool approximatelyEqual (float a, float b)
{
    return std::abs (a - b) < 1.0e-3f;
}

// A mix so pode morar em linha de comentario '#'. Se algum marcador #mix@ ou
// #master cair numa linha de corpo, um leitor do drumlabooh estranharia o kit.
bool dfkitBodyStaysClean (const juce::String& text)
{
    juce::StringArray lines;
    lines.addLines (text);

    for (const auto& rawLine : lines)
    {
        const auto line = rawLine.trim();

        if (line.isEmpty() || line.startsWith ("#"))
            continue;

        if (line.contains ("#mix@") || line.contains ("#master"))
            return false;
    }

    return true;
}

void testKitModel()
{
    std::printf ("\n[modelo de kit]\n");

    const auto tagged = kitdetail::parseTags ("Kick_vl2_rr3");
    check (tagged.velocityLayer == 2 && tagged.roundRobin == 3, "tags vl/rr sao lidas");

    check (kitdetail::parseTags ("Snare_rr2").roundRobin == 2, "tag rr isolada");
    check (kitdetail::parseTags ("Tom1_v3").velocityLayer == 3, "tag v isolada");
    check (kitdetail::parseTags ("Hat").velocityLayer == 0, "arquivo sem tag fica na camada 0");

    Kit kit = Kit::createDefault();
    kit.name = "Roundtrip";

    auto& pad = kit.pads[0];
    pad.layers = { { "D:/Kits/Samples/kick_vl1_rr1.wav" },
                   { "D:/Kits/Samples/kick_vl2_rr1.wav",
                     "D:/Kits/Samples/kick_vl2_rr2.wav" } };

    const juce::File base ("D:/Kits");
    auto xml = kit.createXml (base);

    check (xml != nullptr, "kit gera XML");

    if (xml != nullptr)
    {
        const auto restored = Kit::createFromXml (*xml, base);

        check (restored.pads[0].layers.size() == 2, "camadas sobrevivem ao XML");
        check (restored.pads[0].layers[1].size() == 2, "round-robins sobrevivem ao XML");
        check (restored.pads[0].layers[1][0]
                   == juce::File ("D:/Kits/Samples/kick_vl2_rr1.wav").getFullPathName(),
               "caminho relativo volta absoluto");
    }

    // .dfkit agora e texto no estilo drumlabooh: round-trip sem perdas e pad
    // exato, choke e camadas x tomadas preservados.
    juce::String error;
    kit.pads[5].name = "Hi-Hat";
    kit.pads[5].type = "hat";
    kit.pads[5].note = 42;
    kit.pads[5].chokeGroup = 1;
    kit.pads[5].layers = { { "D:/Kits/Samples/hat-closed-1.wav",
                             "D:/Kits/Samples/hat-closed-2.wav" } };

    const auto text = kit.createText (base);
    check (text.startsWith ("#forja-dfkit"), "kit gera texto .dfkit");
    check (! text.contains ("<"), "formato nao e XML");
    check (text.contains ("[42]@5~1 Hi-Hat="),
           "linha traz nota, pad, choke e nome",
           text.contains ("Hi-Hat=") ? "faltou flag" : "sem linha");
    check (text.contains ("hat-closed-1.wav+") && text.contains ("hat-closed-2.wav"),
           "tomadas de round-robin separam por +", "");

    const auto fromText = Kit::createFromText (text, base, "Ignorado");
    check (fromText.name == "Roundtrip", "nome volta do header do texto");
    check (fromText.pads[0].layers.size() == 2 && fromText.pads[0].layers[1].size() == 2,
           "camadas e tomadas sobrevivem ao texto");
    check (fromText.pads[5].note == 42 && fromText.pads[5].chokeGroup == 1
               && fromText.pads[5].sampleCount() == 2,
           "nota, choke e round-robin voltam pelo texto");
    check (fromText.pads[5].layers[0][0]
               == juce::File ("D:/Kits/Samples/hat-closed-1.wav").getFullPathName(),
           "caminho do texto volta absoluto");

    // Um .dfkit antigo em XML ainda abre pela deteccao de conteudo.
    const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                         .getChildFile ("forja-drumforge-dfkit");
    dir.deleteRecursively();
    dir.createDirectory();

    const auto saved = dir.getChildFile ("roundtrip.dfkit");
    check (kit.saveToFile (saved, error), "salvo .dfkit em texto", error);
    check (saved.loadFileAsString().startsWith ("#forja-dfkit"), "arquivo salvo e texto");

    Kit backFromDisk;
    check (Kit::loadFromFile (saved, backFromDisk, error), "recarrego o .dfkit de disco", error);
    check (backFromDisk.pads[5].chokeGroup == 1 && backFromDisk.pads[0].sampleCount() == 3,
           "kit reconstruido fielmente do texto");

    const auto legacy = dir.getChildFile ("antigo.dfkit");
    legacy.replaceWithText ("<DrumforgeKit name=\"Antigo\">"
                            "<Pad index=\"0\" name=\"KICK\" type=\"kick\" note=\"36\" choke=\"0\">"
                            "<Layer><Sample path=\"Samples/kick_vl1_rr1.wav\"/></Layer>"
                            "</Pad></DrumforgeKit>");

    Kit fromLegacy;
    check (Kit::loadFromFile (legacy, fromLegacy, error), ".dfkit XML antigo ainda abre", error);
    check (fromLegacy.pads[0].note == 36 && fromLegacy.pads[0].sampleCount() == 1,
           "XML legado vira kit");

    dir.deleteRecursively();

    Kit missing;
    error.clear();
    check (! Kit::loadFromFile (juce::File ("D:/Kits/nao_existe.dfkit"), missing, error),
           "kit ausente retorna erro");
    check (error.isNotEmpty(), "erro de kit ausente e descritivo");

    // Kit premixado: a mix vai como comentario (#mix@N / #master), round-tripa
    // sem perdas e nao contamina o corpo legivel por outro leitor drumlabooh.
    kit.hasMix = true;
    kit.mixers[0].trim = 0.80f;
    kit.mixers[0].pan = 0.30f;
    kit.mixers[0].send1 = 0.45f;
    kit.mixers[5].eqLow = 0.62f;
    kit.master.comp = 0.55f;
    kit.master.fx2Drive = 0.35f;

    const auto mixedText = kit.createText (base, true);
    check (mixedText.contains ("#mix@0 ") && mixedText.contains ("#mix@5 "),
           "linhas de mix vao como comentario");
    check (mixedText.contains ("#master "), "master gravado na mix");
    check (mixedText.contains ("trim=0.80") && mixedText.contains ("pan=0.30")
               && mixedText.contains ("send1=0.45") && mixedText.contains ("comp=0.55"),
           "valores normalizados saem com duas casas");
    check (dfkitBodyStaysClean (mixedText), "corpo do kit permanece limpo");

    const auto mixedBack = Kit::createFromText (mixedText, base, "Ignorado");
    check (mixedBack.hasMix, "bandeira de mix volta");
    check (approximatelyEqual (mixedBack.mixers[0].trim, 0.80f)
               && approximatelyEqual (mixedBack.mixers[0].pan, 0.30f)
               && approximatelyEqual (mixedBack.mixers[0].send1, 0.45f),
           "mix do pad volta fiel");
    check (approximatelyEqual (mixedBack.mixers[5].eqLow, 0.62f), "mix do chimbal volta fiel");
    check (approximatelyEqual (mixedBack.master.comp, 0.55f)
               && approximatelyEqual (mixedBack.master.fx2Drive, 0.35f),
           "mix do master volta fiel");
    check (approximatelyEqual (mixedBack.mixers[0].fader, 0.91f),
           "o que nao mudou fica no neutro");

    // Sem withMix o texto nao carrega mix e o kit recriado chega com hasMix
    // falso e valores neutros (createText direto; saveToFile obedece a bandeira).
    const auto plainText = kit.createText (base, false);
    check (! plainText.contains ("#mix@") && ! plainText.contains ("#master "),
           "sem a bandeira nao sai linha de mix");

    const auto plainBack = Kit::createFromText (plainText, base, "Ignorado");
    check (! plainBack.hasMix, "kit sem mix chega com hasMix falso");
    check (approximatelyEqual (plainBack.mixers[0].trim, 0.50f)
               && approximatelyEqual (plainBack.master.trim, 0.50f),
           "kit sem mix chega neutro");

    // Fluxo SALVAR KIT: com hasMix ligado no kit, saveToFile grava a mix em
    // disco e a recarga traz os valores de volta.
    const auto mixDir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                .getChildFile ("forja-drumforge-mixsave");
    mixDir.deleteRecursively();
    mixDir.createDirectory();

    const auto mixFile = mixDir.getChildFile ("premixado.dfkit");
    check (kit.saveToFile (mixFile, error), "saveToFile com hasMix grava a mix", error);
    check (mixFile.loadFileAsString().contains ("#mix@0 "), "arquivo em disco traz as linhas de mix");

    Kit savedBack;
    check (Kit::loadFromFile (mixFile, savedBack, error), "recarrego o .dfkit premixado", error);
    check (savedBack.hasMix
               && approximatelyEqual (savedBack.mixers[0].trim, 0.80f)
               && approximatelyEqual (savedBack.mixers[5].eqLow, 0.62f)
               && approximatelyEqual (savedBack.master.comp, 0.55f),
           "mix volta fiel do disco");

    mixDir.deleteRecursively();
}

// Passa o leitor nos kits de verdade do drum_sklad. O diretorio so existe quando
// quem roda exporta FORJA_TEST_KITS_DIR apontando para o checkout do repositorio
// (a pasta que contem um drumkit.txt ou drumkit.labooh por kit). Nao falha quando
// nao ha nada: o teste padrao em CI nao tem samples nem repositorio baixado.
void testRealTxtKits()
{
    const juce::String dir = juce::SystemStats::getEnvironmentVariable ("FORJA_TEST_KITS_DIR", {});
    if (dir.isEmpty())
    {
        std::printf ("\n[kits reais do Drumlabooh] FORJA_TEST_KITS_DIR nao definido - pulado\n");
        return;
    }

    const auto root = juce::File (dir);
    if (! root.isDirectory())
    {
        std::printf ("\n[kits reais do Drumlabooh] pasta inexistente: %s\n", dir.toRawUTF8());
        ++failures;
        return;
    }

    std::printf ("\n[kits reais do Drumlabooh] %s\n", dir.toRawUTF8());

    std::vector<juce::File> kitFolders;
    const auto gather = [&kitFolders] (const juce::File& folder)
    {
        if (Kit::folderHasTxtKit (folder))
            kitFolders.push_back (folder);
    };

    for (const auto& child : root.findChildFiles (juce::File::findDirectories, false))
    {
        gather (child);
        // ZIP do GitHub coloca <repo>-main no meio: aceita um nivel extra.
        for (const auto& grandChild : child.findChildFiles (juce::File::findDirectories, false))
            gather (grandChild);
    }

    check (! kitFolders.empty(), "pasta tem kits de texto ou labooh");

    int loaded = 0;
    int laboohCount = 0;
    int missingItems = 0;

    for (const auto& folder : kitFolders)
    {
        const bool labooh = folder.getChildFile ("drumkit.txt").existsAsFile() == false;
        juce::String error;
        const auto kit = Kit::createFromDrumlaboohTxt (folder, error);

        if (error.isNotEmpty() || kit.padsWithAudio() == 0)
        {
            std::printf ("  FALHA %-28s %s\n", folder.getFileName().toRawUTF8(), error.toRawUTF8());
            ++failures;
            continue;
        }

        ++loaded;
        laboohCount += labooh ? 1 : 0;

        std::array<bool, 128> notes {};
        bool repeatedNote = false;
        int declaredFiles = 0;
        int foundFiles = 0;

        for (const auto& pad : kit.pads)
        {
            if (! pad.hasSamples())
                continue;

            if (pad.note >= 0 && pad.note < 128)
            {
                if (notes[(size_t) pad.note])
                    repeatedNote = true;
                notes[(size_t) pad.note] = true;
            }

            for (const auto& layer : pad.layers)
                for (const auto& path : layer)
                {
                    ++declaredFiles;
                    if (juce::File (path).existsAsFile())
                        ++foundFiles;
                }
        }

        check (! repeatedNote, "mapa sem nota repetida", kit.name);
        check (kit.padsWithAudio() <= kNumPads, "pads dentro do limite", kit.name);

        if (foundFiles < declaredFiles)
            ++missingItems;

        std::printf ("  ok  %-28s %s  pads %2d  camadas %d  tomadas %2d  arquivos %d/%d\n",
                     kit.name.toRawUTF8(), labooh ? "labooh" : "txt   ",
                     kit.padsWithAudio(), kit.layerCount(),
                     kit.roundRobinCount(), foundFiles, declaredFiles);
    }

    check (loaded == (int) kitFolders.size(), "todos os kits da pasta carregam");
    check (laboohCount >= 8, "os kits so-drumbkit.labooh entram na conta",
           juce::String (laboohCount));

    if (missingItems > 0)
        std::printf ("  nota: %d kit(s) com arquivos de sample ausentes"
                     " (checkout com git-lfs nao baixado)\n", missingItems);
}

// SALVAR KIT em pacote: cria uma pasta com o nome do kit, copia os audios e a
// imagem para dentro e grava o .dfkit com caminhos relativos. Apagar a pasta de
// origem nao pode quebrar o kit salvo.
void testKitBundle()
{
    std::printf ("\n[pacote de kit portavel]\n");

    const auto root = juce::File::getSpecialLocation (juce::File::tempDirectory)
                            .getChildFile ("forja-drumforge-bundle");
    root.deleteRecursively();

    const auto srcSamples = root.getChildFile ("orig").getChildFile ("samples");
    const auto srcAlt = root.getChildFile ("orig").getChildFile ("alt");
    srcSamples.createDirectory();
    srcAlt.createDirectory();

    const auto kick = srcSamples.getChildFile ("kick.wav");
    const auto altKick = srcAlt.getChildFile ("kick.wav");   // mesmo nome, outra pasta
    const auto snare = srcSamples.getChildFile ("snare.wav");
    writeTestWav (kick, 0.5f);
    writeTestWav (altKick, 0.4f);
    writeTestWav (snare, 0.3f);

    const auto srcImage = root.getChildFile ("orig").getChildFile ("photo.png");
    srcImage.replaceWithText ("fake-png-bytes");

    Kit kit = Kit::createDefault();
    kit.name = "Meu Kit";
    kit.pads[0].name = "Kick";
    kit.pads[0].layers = { { kick.getFullPathName() },
                           { kick.getFullPathName(), altKick.getFullPathName() } };
    kit.pads[1].name = "Snare";
    kit.pads[1].layers = { { snare.getFullPathName() } };
    kit.imageFile = srcImage.getFullPathName();
    kit.hasMix = true;
    kit.mixers[0].trim = 0.66f;

    const auto bundleDir = root.getChildFile ("Meu Kit");
    juce::String error;
    check (kit.saveAsBundle (bundleDir, error), "salva o kit em pacote", error);

    const auto savedFile = bundleDir.getChildFile ("Meu Kit.dfkit");
    check (savedFile.existsAsFile(), ".dfkit fica dentro da pasta do kit");
    check (bundleDir.getChildFile ("samples").isDirectory(), "amostras vao para samples/");
    check (bundleDir.getChildFile ("image.png").existsAsFile(), "imagem copiada e normalizada");

    const auto copied = bundleDir.getChildFile ("samples")
                            .findChildFiles (juce::File::findFiles, false, "*.wav");
    check (copied.size() == 3, "os tres WAVs unicos sao copiados (kick reusado nao duplica)",
           juce::String (copied.size()));

    // Originais apagados: o pacote tem de se bastar.
    root.getChildFile ("orig").deleteRecursively();

    Kit loaded;
    error.clear();
    check (Kit::loadFromFile (savedFile, loaded, error), "pacote reabre sem a origem", error);
    check (loaded.padsWithAudio() == 2, "os dois pads voltam com audio");

    bool allExist = true;
    for (const auto& pad : loaded.pads)
        for (const auto& layer : pad.layers)
            for (const auto& path : layer)
                if (! juce::File (path).existsAsFile())
                    allExist = false;

    check (allExist, "todo sample resolve dentro do pacote");
    check (loaded.imageFile.isNotEmpty() && juce::File (loaded.imageFile).existsAsFile(),
           "a imagem volta apontando para dentro do pacote");
    check (loaded.hasMix && approximatelyEqual (loaded.mixers[0].trim, 0.66f),
           "a mix premixada sobrevive ao pacote");

    root.deleteRecursively();
}

} // namespace

void* operator new (std::size_t size)
{
    noteAllocation();
    if (void* pointer = std::malloc (size == 0 ? 1 : size))
        return pointer;

    throw std::bad_alloc();
}

void* operator new[] (std::size_t size)
{
    return ::operator new (size);
}

void operator delete (void* pointer) noexcept
{
    std::free (pointer);
}

void operator delete[] (void* pointer) noexcept
{
    std::free (pointer);
}

void operator delete (void* pointer, std::size_t) noexcept
{
    std::free (pointer);
}

void operator delete[] (void* pointer, std::size_t) noexcept
{
    std::free (pointer);
}

void* operator new (std::size_t size, std::align_val_t alignment)
{
    noteAllocation();
    if (void* pointer = allocateAligned (size, (std::size_t) alignment))
        return pointer;

    throw std::bad_alloc();
}

void* operator new[] (std::size_t size, std::align_val_t alignment)
{
    return ::operator new (size, alignment);
}

void operator delete (void* pointer, std::align_val_t) noexcept
{
    freeAligned (pointer);
}

void operator delete[] (void* pointer, std::align_val_t) noexcept
{
    freeAligned (pointer);
}

int main()
{
    std::printf ("Drumforge engine tests\n");

    testKitModel();
    testSampleFolderImport();
    testDrumlaboohTxtKit();
    testDrumlaboohXmlKit();
    testRealTxtKits();
    testLoopLibrary();
    testKitAdoption();
    testKitBundle();
    testVelocityLayers();
    testRoundRobin();
    testChokeGroup();
    testVoiceStealing();
    testMultiOut();
    testMuteAndSolo();
    testMidiAndLatency();
    testLoopPreview();
    testIdleSilence();
    testBypass();
    testMasterMeterSplit();
    testSendTaper();
    testNoAllocations();

    if (failures == 0)
    {
        std::printf ("\nPASS: engine do Drumforge\n");
        return 0;
    }

    std::printf ("\nFAIL: %d verificacao(oes)\n", failures);
    return 1;
}
