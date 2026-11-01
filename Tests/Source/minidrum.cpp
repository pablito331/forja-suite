// Testes do wrapper MiniDrum: carga de kit assincrona, adocao pelo audio,
// volume no trim do master, premix gravado no kit, bypass, medidor, troca de
// kit com reap, rejeicao de kit invalido, ausencia de alocacao no caminho de
// audio e a gaveta do ForjaRack (mesmo wrapper pelo contrato ModuleBase).
#include <juce_events/juce_events.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <new>
#include <vector>

#include "minidrum/KitLibrary.h"
#include "minidrum/MiniDrum.h"
#include "minidrum/MiniDrumRackModule.h"
#include "rack/ModuleCatalog.h"

#if defined(_MSC_VER)
 #include <malloc.h>
#endif

using namespace forja::minidrum;

namespace
{

constexpr double testSampleRate = 48000.0;
constexpr int testBlockSize = 256;

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

// WAV de teste: clique com ataque imediato e cauda curta. Serve para medir
// nivel e provar latencia zero pelo primeiro frame nao zero.
bool writeClickWav (const juce::File& file, float amplitude)
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

    juce::AudioBuffer<float> buffer (1, 200);
    buffer.clear();
    buffer.getWritePointer (0)[0] = amplitude;
    buffer.getWritePointer (0)[1] = amplitude * 0.5f;

    return writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
}

// Kit completo de teste (bumbo 36, caixa 38, chimbal 42) em pasta temporaria;
// extraLines permite acrescentar linhas de premix sem repetir o corpo.
juce::File makeFullKit (const juce::String& folderName, const juce::String& extraLines)
{
    const auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory)
                            .getChildFile (folderName);
    folder.deleteRecursively();
    folder.createDirectory();

    writeClickWav (folder.getChildFile ("kick.wav"), 0.20f);
    writeClickWav (folder.getChildFile ("snare.wav"), 0.12f);
    writeClickWav (folder.getChildFile ("hat.wav"), 0.06f);

    folder.getChildFile ("drumkit.txt").replaceWithText (
        "#name Kit Cheio\n"
        + extraLines
        + "[36]KICK=kick.wav\n"
        + "[38]SNARE=snare.wav\n"
        + "[42]HIHAT=hat.wav\n");

    // Espelho em .dfkit: so esse leitor honra #name e #mix@ (o drumkit.txt do
    // Drumlabooh engole comentarios e batiza o kit pelo nome da pasta).
    folder.getChildFile ("kit.dfkit").replaceWithText (
        "#name Kit Cheio\n"
        + extraLines
        + "[36]@0 KICK=kick.wav\n"
        + "[38]@1 SNARE=snare.wav\n"
        + "[42]@2 HIHAT=hat.wav\n");

    return folder;
}

// Kit com so chimbal: depois da troca, o bumbo do kit antigo tem que calar.
juce::File makeHatOnlyKit (const juce::String& folderName)
{
    const auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory)
                            .getChildFile (folderName);
    folder.deleteRecursively();
    folder.createDirectory();

    writeClickWav (folder.getChildFile ("hat.wav"), 0.06f);

    folder.getChildFile ("drumkit.txt").replaceWithText (
        "#name So Chat\n"
        "[42]HIHAT=hat.wav\n");

    return folder;
}

void pumpMessages (int ms)
{
    if (auto* messages = juce::MessageManager::getInstanceWithoutCreating())
        messages->runDispatchLoopUntil (ms);
}

// Carrega e espera a conclusao (flag + bomba de mensagens com timeout).
bool loadAndWait (MiniDrum& drum, const juce::File& file, juce::String& errorOut)
{
    errorOut = "timeout";
    std::atomic<bool> done { false };

    drum.loadKitFile (file, [&] (const juce::String& result)
    {
        errorOut = result;
        done.store (true, std::memory_order_release);
    });

    const auto deadline = juce::Time::getMillisecondCounter() + 8000;

    while (! done.load (std::memory_order_acquire)
             && juce::Time::getMillisecondCounter() < deadline)
        pumpMessages (20);

    return done.load (std::memory_order_acquire);
}

// Dispara a nota no frame |atFrame| do primeiro bloco e renderiza, devolvendo
// { pico, indice do primeiro frame nao zero } sobre todos os blocos.
struct RenderResult
{
    float peak = 0.0f;
    int firstNonZero = -1;
};

RenderResult triggerRender (MiniDrum& drum, int note, float velocity, int blocks,
                            int atFrame = 0, bool bypassed = false)
{
    juce::AudioBuffer<float> out (2, testBlockSize);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, note, velocity), atFrame);

    RenderResult result;

    for (int block = 0; block < blocks; ++block)
    {
        out.clear();
        drum.process (midi, out, testBlockSize, bypassed);
        midi.clear();

        for (int i = 0; i < testBlockSize; ++i)
        {
            const float magnitude = std::max (std::abs (out.getReadPointer (0) [i]),
                                              std::abs (out.getReadPointer (1) [i]));

            if (magnitude > 0.00001f)
            {
                if (result.firstNonZero < 0)
                    result.firstNonZero = block * testBlockSize + i;

                result.peak = std::max (result.peak, magnitude);
            }
        }
    }

    return result;
}

RenderResult silentRender (MiniDrum& drum, int blocks, bool bypassed = false)
{
    return triggerRender (drum, 0, 0.0f, blocks, 0, bypassed);
}

void testSilenceWithoutKit()
{
    std::printf ("\n[sem kit]\n");

    MiniDrum drum;
    drum.prepare (testSampleRate, 512);

    const auto silent = silentRender (drum, 4);
    check (silent.peak == 0.0f, "sem kit o par estereo fica mudo");
    check (drum.getLoadedKit() == nullptr, "nada adotado sem carga");
    check (drum.getStatus().contains ("Nenhum"), "status informa ausencia de kit");
}

void testKitLoadAndAdoption()
{
    std::printf ("\n[carga e adocao]\n");

    const auto folder = makeFullKit ("forja-minidrum-full", {});

    MiniDrum drum;
    drum.prepare (testSampleRate, 512);

    juce::String error;
    check (loadAndWait (drum, folder.getChildFile ("kit.dfkit"), error), "carga concluiu", error);
    check (error.isEmpty(), "carga sem erro", error);

    auto loaded = drum.getLoadedKit();
    check (loaded != nullptr, "kit ficou ativo");

    if (loaded != nullptr)
    {
        check (loaded->totalSamples == 3, "os tres samples do kit foram lidos",
               juce::String (loaded->totalSamples));
        check (loaded->kit.name == "Kit Cheio", "nome do kit vem do #name");
    }

    check (drum.getStatus().contains ("pronto"), "status publica kit pronto", drum.getStatus());

    const auto kick = triggerRender (drum, 36, 0.8f, 8);
    check (kick.peak > 0.0f, "bumbo toca apos a adocao", juce::String (kick.peak));

    folder.deleteRecursively();
}

void testVolumeKnob()
{
    std::printf ("\n[volume no trim]\n");

    const auto folder = makeFullKit ("forja-minidrum-volume", {});

    MiniDrum drum;
    drum.prepare (testSampleRate, 512);

    juce::String error;
    loadAndWait (drum, folder.getChildFile ("drumkit.txt"), error);

    drum.setVolume (0.5f);
    silentRender (drum, 30);            // 160 ms: o trim de 12 ms assenta de verdade
    const auto unity = triggerRender (drum, 36, 0.8f, 8).peak;

    // 0,75 = +12 dB no taper trimDb(): o pico tem que quadruplicar.
    drum.setVolume (0.75f);
    silentRender (drum, 30);
    const auto boosted = triggerRender (drum, 36, 0.8f, 8).peak;

    const auto ratio = unity > 0.0f ? boosted / unity : 0.0f;
    check (ratio > 3.4f && ratio < 4.6f, "volume 0,75 = +12 dB (x4)", juce::String (ratio, 2));

    drum.setVolume (0.25f);
    silentRender (drum, 30);
    const auto soft = triggerRender (drum, 36, 0.8f, 8).peak;
    check (unity > 0.0f && soft / unity < 0.32f, "volume 0,25 atenua de verdade",
           juce::String (soft / unity, 3));

    folder.deleteRecursively();
}

void testKitPremix()
{
    std::printf ("\n[premix gravada no kit]\n");

    const auto plainFolder = makeFullKit ("forja-minidrum-mix-a", {});
    const auto mixFolder = makeFullKit ("forja-minidrum-mix-b", "#mix@0 trim=0.75\n");

    MiniDrum plain;
    MiniDrum mixed;
    plain.prepare (testSampleRate, 512);
    mixed.prepare (testSampleRate, 512);

    juce::String error;
    loadAndWait (plain, plainFolder.getChildFile ("kit.dfkit"), error);
    loadAndWait (mixed, mixFolder.getChildFile ("kit.dfkit"), error);

    check (mixed.getLoadedKit() != nullptr && mixed.getLoadedKit()->kit.hasMix,
           "linha #mix@ acende o premix do kit");

    // Volume do wrapper igual nos dois; so o trim do pad 0 (bumbo) muda.
    plain.setVolume (0.25f);
    mixed.setVolume (0.25f);
    silentRender (plain, 30);            // deixa o trim do pad assentar antes da nota
    silentRender (mixed, 30);

    const auto plainPeak = triggerRender (plain, 36, 0.8f, 8).peak;
    const auto mixedPeak = triggerRender (mixed, 36, 0.8f, 8).peak;

    const auto ratio = plainPeak > 0.0f ? mixedPeak / plainPeak : 0.0f;
    check (ratio > 3.4f && ratio < 4.6f, "trim 0,75 do kit = +12 dB no pad",
           juce::String (ratio, 2));

    plainFolder.deleteRecursively();
    mixFolder.deleteRecursively();
}

void testZeroLatency()
{
    std::printf ("\n[latencia]\n");

    const auto folder = makeFullKit ("forja-minidrum-latencia", {});

    MiniDrum drum;
    drum.prepare (testSampleRate, 512);

    juce::String error;
    loadAndWait (drum, folder.getChildFile ("drumkit.txt"), error);

    const auto hit = triggerRender (drum, 36, 0.8f, 2, 64);
    check (hit.firstNonZero >= 64 && hit.firstNonZero <= 88,
           "primeiro frame de audio casa com o frame do note-on (PDC 0)",
           juce::String (hit.firstNonZero));

    folder.deleteRecursively();
}

void testBypass()
{
    std::printf ("\n[bypass]\n");

    const auto folder = makeFullKit ("forja-minidrum-bypass", {});

    MiniDrum drum;
    drum.prepare (testSampleRate, 512);

    juce::String error;
    loadAndWait (drum, folder.getChildFile ("drumkit.txt"), error);

    silentRender (drum, 30, true);       // a rampa de 10 ms precisa fechar antes da nota

    const auto muted = triggerRender (drum, 36, 0.8f, 40, 0, true);
    check (muted.peak == 0.0f, "bypass continuo entrega silencio", juce::String (muted.peak));

    const auto back = triggerRender (drum, 36, 0.8f, 8);
    check (back.peak > 0.0f, "sair do bypass volta a tocar");

    folder.deleteRecursively();
}

void testMasterMeter()
{
    std::printf ("\n[medidor]\n");

    const auto folder = makeFullKit ("forja-minidrum-meter", {});

    MiniDrum drum;
    drum.prepare (testSampleRate, 512);

    juce::String error;
    loadAndWait (drum, folder.getChildFile ("drumkit.txt"), error);

    drum.setVolume (0.5f);
    triggerRender (drum, 36, 0.8f, 1);

    const auto first = drum.readMasterMeter();
    const auto second = drum.readMasterMeter();

    check (first.peakLeft > 0.0f && first.peakRight > 0.0f, "pico master publicado",
           juce::String (first.peakLeft));
    check (second.peakLeft == 0.0f, "leitura limpa o medidor (self-clearing)",
           juce::String (second.peakLeft));

    folder.deleteRecursively();
}

void testKitSwapAndReap()
{
    std::printf ("\n[troca de kit e reap]\n");

    const auto fullFolder = makeFullKit ("forja-minidrum-swap-a", {});
    const auto hatFolder = makeHatOnlyKit ("forja-minidrum-swap-b");

    MiniDrum drum;
    drum.prepare (testSampleRate, 512);

    juce::String error;
    loadAndWait (drum, fullFolder.getChildFile ("drumkit.txt"), error);

    auto retired = drum.getLoadedKit();
    check (retired != nullptr, "kit cheio ficou ativo antes da troca");

    loadAndWait (drum, hatFolder.getChildFile ("drumkit.txt"), error);

    // O audio adota o token novo no proximo bloco; so depois disso o reap pode
    // liberar o kit substituido sem UAF.
    silentRender (drum, 1);
    drum.reapRetiredKits();

    check (drum.getLoadedKit() != retired, "kit novo substituiu o antigo");

    const auto kick = triggerRender (drum, 36, 0.8f, 4);
    const auto hat = triggerRender (drum, 42, 0.8f, 4);
    check (kick.peak == 0.0f, "bumbo do kit antigo nao toca mais", juce::String (kick.peak));
    check (hat.peak > 0.0f, "chimbal do kit novo toca");

    const auto useCount = retired.use_count();
    check (useCount == 1, "reap liberou o kit substituido", juce::String (static_cast<long long> (useCount)));

    fullFolder.deleteRecursively();
    hatFolder.deleteRecursively();
}

void testInvalidKitKeepsPlaying()
{
    std::printf ("\n[kit invalido]\n");

    const auto folder = makeFullKit ("forja-minidrum-invalido", {});

    MiniDrum drum;
    drum.prepare (testSampleRate, 512);

    juce::String error;
    loadAndWait (drum, folder.getChildFile ("drumkit.txt"), error);

    const auto before = drum.getLoadedKit();

    juce::String secondError;
    const auto completed = loadAndWait (drum, folder.getChildFile ("inexistente.dfkit"), secondError);

    check (completed, "kit inexistente conclui com erro (nao trava)");
    check (secondError.isNotEmpty(), "erro de leitura e descritivo", secondError);
    check (drum.getLoadedKit() == before, "kit invalido nao substitui o tocador atual");

    const auto kick = triggerRender (drum, 36, 0.8f, 4);
    check (kick.peak > 0.0f, "o kit anterior continua tocando");

    folder.deleteRecursively();
}

void testNoAllocations()
{
    std::printf ("\n[sem alocacao no audio]\n");

    const auto folder = makeFullKit ("forja-minidrum-alloc", {});

    MiniDrum drum;
    drum.prepare (testSampleRate, 512);

    juce::String error;
    loadAndWait (drum, folder.getChildFile ("drumkit.txt"), error);

    juce::AudioBuffer<float> out (2, testBlockSize);
    juce::MidiBuffer midi;
    midi.ensureSize (64 * static_cast<int> (sizeof (juce::int32)));

    // Prepara o evento fora da janela monitorada; dentro dela so clear/addEvent
    // com capacidade garantida e o process() do wrapper.
    const auto message = juce::MidiMessage::noteOn (1, 36, 0.8f);

    for (int block = 0; block < 8; ++block)
    {
        out.clear();
        drum.process (midi, out, testBlockSize, false);
    }

    trackAllocations.store (true, std::memory_order_relaxed);

    for (int block = 0; block < 64; ++block)
    {
        midi.clear();
        midi.addEvent (message, 0);

        out.clear();
        drum.process (midi, out, testBlockSize, block % 7 == 0);
    }

    trackAllocations.store (false, std::memory_order_relaxed);

    check (! allocationTripped.load (std::memory_order_relaxed),
           "nenhuma alocacao em 64 blocos de audio");

    folder.deleteRecursively();
}

void testRackModule()
{
    std::printf ("\n[gaveta do ForjaRack]\n");

    MiniDrumModule module;

    // prepare so varre a biblioteca compartilhada (pode estar vazia na CI).
    module.prepare (testSampleRate);

    std::array<forjarack::MidiEvent, 2> events {};
    events[0] = { 10, 1, 36, 100, true };
    events[1] = { 120, 1, 36, 0, false };
    const forjarack::MidiEventSpan span { events.data(), 2 };

    module.processMidi (span);

    std::vector<float> left (static_cast<size_t> (testBlockSize), 0.0f);
    std::vector<float> right (static_cast<size_t> (testBlockSize), 0.0f);

    module.process (left.data(), right.data(), testBlockSize);
    check (true, "span MIDI vira audio sem crash");

    // Bloco maior que a reserva: pass-through silencioso, sem estourar o scratch.
    std::vector<float> bigLeft (static_cast<size_t> (8192), 0.0f);
    std::vector<float> bigRight (static_cast<size_t> (8192), 0.0f);
    module.process (bigLeft.data(), bigRight.data(), 8192);

    bool bigSilent = true;
    for (const float sample : bigLeft)
        if (sample != 0.0f)
            bigSilent = false;
    check (bigSilent, "bloco acima da capacidade e pulado em silencio");

    // Knob de volume por setParam e lido pelo wrapper atomico.
    module.setParam (0, 0.75f);
    check (module.getVolumeForTest() == 0.75f, "setParam 0 mexe no volume do wrapper");
    module.setParam (0, 5.0f);
    check (module.getVolumeForTest() == 1.0f, "knob fora da faixa e limitado");

    // idle() com a biblioteca compartilhada real: se houver kit, ele deve ser
    // adotado sem tocar no caminho de audio; se nao houver, nada acontece.
    const bool hasLibrary = ! findKits().isEmpty();

    for (int pass = 0; pass < 50 && hasLibrary; ++pass)
    {
        module.idle();
        pumpMessages (20);
    }

    if (hasLibrary)
        check (true, "idle adota kit da biblioteca sem crash (biblioteca presente)");
    else
        std::printf ("  SKIP  biblioteca compartilhada vazia nesta maquina\n");
}

void testDrawerFiles()
{
    std::printf ("\n[gaveta de arquivos do rack]\n");

    // Kit avulso fora da biblioteca: .dfkit com os samples ao lado, no tmp.
    const auto box = juce::File::getSpecialLocation (juce::File::tempDirectory)
                         .getChildFile ("forja-minidrum-drawer");
    box.deleteRecursively();
    box.createDirectory();

    writeClickWav (box.getChildFile ("kick.wav"), 0.2f);
    box.getChildFile ("solto.dfkit").replaceWithText (
        "#name Kit Solto\n[36]@0 KICK=kick.wav\n");

    MiniDrumModule module;
    module.prepare (testSampleRate);

    check (module.supportsDrawerFiles(), "gaveta expoe a banda de arquivos");

    // Nome fora da biblioteca nao e escolha de lista.
    check (! module.drawerSelectFile ("C:/nao/existe/kit.dfkit"),
           "nome fora da lista nao e aceito como escolha");

    // Externo de verdade: o idle colhe a carga e passa a exibir o nome; o knob
    // de indice fica mudo ate a proxima escolha pela lista.
    module.drawerLoadExternalFile (box.getChildFile ("solto.dfkit").getFullPathName().toStdString());

    for (int pass = 0; pass < 5; ++pass)
    {
        module.idle();
        pumpMessages (20);
    }

    const auto shown = juce::String (module.drawerCurrentName());
    check (shown.containsIgnoreCase ("solto.dfkit"), "externo colhido pelo idle",
           shown);

    module.setParam (1, 0.5f);   // o knob nao pode recolher o externo

    for (int pass = 0; pass < 5; ++pass)
    {
        module.idle();
        pumpMessages (20);
    }

    check (juce::String (module.drawerCurrentName()).containsIgnoreCase ("solto.dfkit"),
           "knob fica mudo enquanto o externo manda");

    // Caminho inexistente: a gaveta tenta, falha e devolve o comando ao knob.
    module.drawerLoadExternalFile ("C:/forja-que-nao-existe/kit.dfkit");
    module.idle();
    pumpMessages (20);
    module.idle();

    for (int pass = 0; pass < 50; ++pass)
    {
        module.idle();
        pumpMessages (10);
    }

    check (true, "externo inexistente nao derruba a gaveta");

    box.deleteRecursively();
}

void testRegistration()
{
    std::printf ("\n[registro no catalogo]\n");

    const int before = forjarack::ModuleCatalog::count();

    registerMinidrumModule();
    registerMinidrumModule();

    const int index = forjarack::ModuleCatalog::indexOf ("minidrum");
    check (index >= 0, "minidrum esta registrado");
    check (forjarack::ModuleCatalog::count() == before + (index >= before ? 1 : 0),
           "registro duplo e ignorado (append-only)",
           juce::String (forjarack::ModuleCatalog::count()) + " vs " + juce::String (before));

    if (index >= 0)
    {
        const auto& spec = forjarack::ModuleCatalog::specByIndex (index);
        check (juce::String (spec.name) == "MINIDRUM", "rotulo da gaveta");
        check (spec.role == forjarack::ModuleRole::instrument, "papel de instrumento");
        check (spec.face == forjarack::ModuleFace::drum, "fachada drum");
        check (spec.numParams == 2, "dois knobs (VOLUME e KIT)");

        auto created = forjarack::ModuleCatalog::create (index);
        check (created != nullptr && created->isInstrument(), "fabrica entrega um instrument");
    }
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
    juce::ScopedJuceInitialiser_GUI juceSetup;

    std::printf ("MiniDrum tests\n");

    testSilenceWithoutKit();
    testKitLoadAndAdoption();
    testVolumeKnob();
    testKitPremix();
    testZeroLatency();
    testBypass();
    testMasterMeter();
    testKitSwapAndReap();
    testInvalidKitKeepsPlaying();
    testNoAllocations();
    testRackModule();
    testDrawerFiles();
    testRegistration();

    if (failures == 0)
    {
        std::printf ("\nPASS: wrapper MiniDrum\n");
        return 0;
    }

    std::printf ("\nFAIL: %d verificacao(oes)\n", failures);
    return 1;
}
