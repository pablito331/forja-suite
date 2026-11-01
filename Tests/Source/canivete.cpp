// Testes do CANIVETE SUICO: player SoundFont (TinySoundFont MIT) multi-timbral.
// Sem paridade (DSP nao e nosso; o wrapper so envolve a API publica do tsf).
// O arquivo gera um SF2 minimo valido em disco (RIFF sfbk com pdta completo e
// um smpl senoidal) para a CI nao depender de bank nenhum. Cobre: silencio sem
// bank, carga/adocao, volume no trim, latencia zero, bypass, medidor por bloco
// (o do canivete NAO e self-clearing: e republicado a cada bloco), troca com
// reap, bank invalido mantendo o anterior, CC/program change multi-timbral,
// ausencia de alocacao no caminho de audio e a gaveta do ForjaRack (arquivos e
// instrumento).
#include <juce_events/juce_events.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <memory>
#include <new>
#include <vector>

#include "canivete/BankLibrary.h"
#include "canivete/CaniveteRackModule.h"
#include "canivete/SoundFontEngine.h"
#include "rack/ModuleCatalog.h"

#if defined(_MSC_VER)
 #include <malloc.h>
#endif

using namespace forja::canivete;

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

// ---------------------------------------------------------------------------
// SF2 minimo valido. O tsf exige os 9 chunks pdta (mesmo pmod/imod so com o
// registro terminador) e um smpl com dados. Um preset "Test Piano" (bank 0,
// prog 0) com uma regiao senoidal sem loop e um segundo preset "Test Strings"
// (bank 0, prog 1) apontando para o mesmo instrumento, o bastante para
// exercitar program change e multi-timbralidade.
// ---------------------------------------------------------------------------

void append16 (juce::MemoryBlock& out, int value)
{
    const auto u = static_cast<juce::uint16> (value);
    out.append (&u, sizeof (u));
}

void append32 (juce::MemoryBlock& out, juce::uint32 value)
{
    out.append (&value, sizeof (value));
}

void appendText (juce::MemoryBlock& out, const char* text, std::size_t bytes)
{
    std::vector<char> name (bytes, '\0');

    for (std::size_t i = 0; i < bytes && text[i] != '\0'; ++i)
        name[i] = text[i];

    out.append (name.data(), static_cast<int> (bytes));
}

void appendU16s (juce::MemoryBlock& out, std::initializer_list<int> values)
{
    for (const auto value : values)
        append16 (out, value);
}

juce::uint32 riffSizePlaceholder() { return 0; }

void appendChunk (juce::MemoryBlock& out, const char* fourcc, const juce::MemoryBlock& data)
{
    out.append (fourcc, 4);
    append32 (out, static_cast<juce::uint32> (data.getSize()));
    out.append (data.getData(), static_cast<int> (data.getSize()));

    if (data.getSize() & 1)   // RIFF pede padding par
    {
        const char pad = 0;
        out.append (&pad, 1);
    }
}

void appendListChunk (juce::MemoryBlock& out, const char* fourcc,
                      const char* listType, const juce::MemoryBlock& data)
{
    out.append (fourcc, 4);
    append32 (out, static_cast<juce::uint32> (data.getSize() + 4));
    out.append (listType, 4);
    out.append (data.getData(), static_cast<int> (data.getSize()));
}

juce::File writeTestSf2 (const juce::String& fileName, const char* presetName)
{
    // ---- hdts: versao 2.1 e buffer de destino em seconds (14 bytes) ----
    juce::MemoryBlock hdts;
    append16 (hdts, 2);
    append16 (hdts, 1);
    append32 (hdts, 0x02020202u);   // wordram / rom sem interesse nenhum
    append16 (hdts, 0);

    // ---- pdta ----
    // phdr: 2 presets + terminador (38 bytes por registro)
    juce::MemoryBlock phdr;
    appendText (phdr, presetName, 20);
    appendU16s (phdr, { 0, 0, 0 });           // preset 0, bank 0, pbag 0
    append32 (phdr, 0); append32 (phdr, 0); append32 (phdr, 0);
    appendText (phdr, "Test Strings", 20);
    appendU16s (phdr, { 1, 0, 1 });           // preset 1 (program 1), pbag 1
    append32 (phdr, 0); append32 (phdr, 0); append32 (phdr, 0);
    appendText (phdr, "EOS", 20);
    appendU16s (phdr, { 0, 0, 2 });           // presetBagNdx do terminador
    append32 (phdr, 0); append32 (phdr, 0); append32 (phdr, 0);

    // pbag: 2 zonas de preset + terminador implicito (genNdx; modNdx sempre 0)
    juce::MemoryBlock pbag;
    appendU16s (pbag, { 0, 0, 1, 0, 2, 0 });

    // pgen: cada zona aponta o instrumento 0 (41) com wordAmount
    juce::MemoryBlock pgen;
    appendU16s (pgen, { 41, 0, 41, 0 });

    // pmod/imod: so os terminadores (10 bytes)
    juce::MemoryBlock modTerm;
    appendU16s (modTerm, { 0, 0, 0, 0, 0 });

    // inst: 1 instrumento + terminador (22 bytes)
    juce::MemoryBlock inst;
    appendText (inst, "Test Zone", 20);
    append16 (inst, 0);                        // instBagNdx
    appendText (inst, "EOI", 20);
    append16 (inst, 1);                        // instBagNdx do terminador

    // ibag: zona do sample + terminador
    juce::MemoryBlock ibag;
    appendU16s (ibag, { 0, 0, 2, 0 });

    // igen: SampleModes (54) = 0 sem loop, SampleID (53) = shdr 0, terminador
    juce::MemoryBlock igen;
    appendU16s (igen, { 54, 0, 53, 0 });
    appendU16s (igen, { 0, 0xFFFF });

    // shdr: sample real + terminador (46 bytes). start/end em indices do smpl;
    // sampleRate aqui e obrigatorio (o tsf nao tem fallback para zero).
    constexpr int sampleCount = 22050;   // meio ciclo de 441 Hz @ 44100

    juce::MemoryBlock shdr;
    appendText (shdr, "sine441", 20);
    append32 (shdr, 1);                      // start
    append32 (shdr, sampleCount + 1);        // end
    append32 (shdr, 0);                      // startLoop
    append32 (shdr, 0);                      // endLoop (0 = sem loop)
    append32 (shdr, 44100);                  // sampleRate
    const char pitchBytes[2] = { 60, 0 };     // originalPitch, pitchCorrection
    shdr.append (pitchBytes, 2);
    append16 (shdr, 0);                      // sampleLink
    append16 (shdr, 1);                      // sampleType (mono)
    appendText (shdr, "EOS", 20);
    append32 (shdr, 0); append32 (shdr, 0); append32 (shdr, 0); append32 (shdr, 0);
    append32 (shdr, 0);
    const char tailBytes[2] = { 0, 0 };
    shdr.append (tailBytes, 2);
    append16 (shdr, 0); append16 (shdr, 0);

    juce::MemoryBlock pdta;
    appendChunk (pdta, "phdr", phdr);
    appendChunk (pdta, "pbag", pbag);
    appendChunk (pdta, "pmod", modTerm);
    appendChunk (pdta, "pgen", pgen);
    appendChunk (pdta, "inst", inst);
    appendChunk (pdta, "ibag", ibag);
    appendChunk (pdta, "imod", modTerm);
    appendChunk (pdta, "igen", igen);
    appendChunk (pdta, "shdr", shdr);

    // ---- sdta: smpl PCM (16 bits) com pad ao final ----
    juce::MemoryBlock smpl;
    std::vector<short> pcm (static_cast<size_t> (sampleCount + 4), 0);

    for (int i = 0; i < sampleCount; ++i)
        pcm[static_cast<size_t> (i) + 1] = static_cast<short> (
            32767.0 * 0.5 * std::sin (2.0 * 3.14159265358979 * 441.0 * static_cast<double> (i) / 44100.0));

    smpl.append (pcm.data(), static_cast<int> (pcm.size()) * static_cast<int> (sizeof (short)));

    juce::MemoryBlock sdta;
    appendChunk (sdta, "smpl", smpl);

    // ---- info minimo (ifil isft ieng) para nao enganar leitores curiosos ----
    juce::MemoryBlock ifilChunk, isftChunk, iengChunk, info;
    append16 (ifilChunk, 2);
    append16 (ifilChunk, 1);
    appendChunk (info, "ifil", ifilChunk);
    appendText (isftChunk, "forja-test", 16);
    appendChunk (info, "isft", isftChunk);
    appendText (iengChunk, "forja-canivete-test", 24);
    appendChunk (info, "ieng", iengChunk);

    juce::MemoryBlock body;
    appendListChunk (body, "LIST", "info", info);
    appendChunk (body, "sdta", sdta);
    appendListChunk (body, "LIST", "pdta", pdta);

    juce::MemoryBlock fileData;
    fileData.append ("RIFF", 4);
    append32 (fileData, riffSizePlaceholder());
    fileData.append ("sfbk", 4);
    appendChunk (fileData, "hdts", hdts);
    fileData.append (body.getData(), static_cast<int> (body.getSize()));

    auto* sizeSlot = reinterpret_cast<juce::uint32*> (
        static_cast<char*> (fileData.getData()) + 4);
    *sizeSlot = static_cast<juce::uint32> (fileData.getSize() - 8);

    const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                          .getChildFile ("forja-canivete").getChildFile (fileName + ".sf2");

    // NAO apaga a pasta inteira: os testes compartilham o bank principal
    // (forja-canivete-full) e ele precisa sobreviver entre as cargas. Apaga
    // apenas o arquivo deste nome, se existir de uma rodada anterior.
    file.getParentDirectory().createDirectory();
    file.deleteFile();

    if (auto stream = file.createOutputStream())
    {
        stream->write (fileData.getData(), fileData.getSize());
    }

    return file;
}

// ---------------------------------------------------------------------------
// utilidades de carga e render
// ---------------------------------------------------------------------------

void pumpMessages (int ms)
{
    if (auto* messages = juce::MessageManager::getInstanceWithoutCreating())
        messages->runDispatchLoopUntil (ms);
}

// Colhe o resultado da thread de carga direto no poll (sem timer aqui; o
// pump mantém o contratante do MessageManager vivo em debug builds).
bool loadAndWait (SoundFontEngine& engine, const juce::File& file, juce::String& errorOut)
{
    errorOut = "timeout";
    std::atomic<bool> done { false };

    engine.loadBankFile (file, [&] (juce::String result)
    {
        errorOut = result;
        done.store (true, std::memory_order_release);
    });

    const auto deadline = juce::Time::getMillisecondCounter() + 8000;

    while (! done.load (std::memory_order_acquire)
             && juce::Time::getMillisecondCounter() < deadline)
    {
        engine.pollLoadResult();
        pumpMessages (10);
    }

    return done.load (std::memory_order_acquire);
}

struct RenderResult
{
    float peak = 0.0f;
    int firstNonZero = -1;
};

// Adota o bank pendente num bloco mudo ANTES de contar latencia: a troca e
// colhida no primeiro process(), entao o note-on so entra no bloco seguinte.
void forceAdoption (SoundFontEngine& engine)
{
    juce::AudioBuffer<float> out (2, testBlockSize);
    juce::MidiBuffer midi;
    engine.process (midi, out, testBlockSize, false);
}

RenderResult triggerRender (SoundFontEngine& engine, int blocks,
                            const std::vector<juce::MidiMessage>& onset,
                            int atFrame = 0, bool bypassed = false)
{
    juce::AudioBuffer<float> out (2, testBlockSize);
    juce::MidiBuffer midi;
    midi.ensureSize (16 * static_cast<int> (sizeof (juce::int32)));

    for (const auto& message : onset)
        midi.addEvent (message, atFrame);

    RenderResult result;

    for (int block = 0; block < blocks; ++block)
    {
        out.clear();
        engine.process (midi, out, testBlockSize, bypassed);
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

RenderResult silentRender (SoundFontEngine& engine, int blocks, bool bypassed = false)
{
    return triggerRender (engine, blocks, {}, 0, bypassed);
}

// Nota sustentada (so note-on): o sample senoidal sem loop toca sua cauda e se
// auto-encerra; note-off no MESMO frame mataria a voz (release 0), entao as
// medicoes de presenca usam apenas o note-on.
std::vector<juce::MidiMessage> oneNote (int channel, int note, float velocity)
{
    return { juce::MidiMessage::noteOn (channel, note, velocity) };
}

// Toca uma nota sustentada, mede o pico e depois a libera: cada medicao de
// razao comeca com o engine limpo, sem sobrepor a cauda da nota anterior.
float peakOneNote (SoundFontEngine& engine, int channel, int note, float velocity)
{
    const auto peak = triggerRender (engine, 8, oneNote (channel, note, velocity)).peak;
    triggerRender (engine, 4, { juce::MidiMessage::noteOff (channel, note) });
    return peak;
}

float peakWithCc7 (SoundFontEngine& engine, int ccValue)
{
    const auto peak = triggerRender (engine, 6,
        { juce::MidiMessage::controllerEvent (1, 7, ccValue),
          juce::MidiMessage::noteOn (1, 60, 0.8f) }).peak;
    triggerRender (engine, 4, { juce::MidiMessage::noteOff (1, 60) });
    return peak;
}

juce::String loadedBankPath;   // para os testes compartilharem o arquivo vivo

// ---------------------------------------------------------------------------
// testes
// ---------------------------------------------------------------------------

void testSilenceWithoutBank()
{
    std::printf ("\n[sem bank]\n");

    SoundFontEngine engine;
    engine.prepare (testSampleRate, 512);

    const auto silent = silentRender (engine, 4);
    check (silent.peak == 0.0f, "sem bank o par estereo fica mudo");
    check (! engine.hasBank(), "nada adotado sem carga");
    check (engine.getStatus().contains ("Nenhum"), "status informa ausencia de bank");
}

void testBankLoadAndAdoption()
{
    std::printf ("\n[carga e adocao]\n");

    const auto file = writeTestSf2 ("forja-canivete-full", "Test Piano");
    loadedBankPath = file.getFullPathName();

    SoundFontEngine engine;
    engine.prepare (testSampleRate, 512);

    juce::String error;
    check (loadAndWait (engine, file, error), "carga concluiu", error);
    check (error.isEmpty(), "carga sem erro", error);

    check (engine.hasBank(), "bank ficou ativo");
    check (engine.getNumPresets() == 2, "os dois presets do SF2 foram lidos",
           juce::String (engine.getNumPresets()));
    check (engine.getPresetName (0).startsWith ("Test Piano"), "nome do preset 0",
           engine.getPresetName (0));
    check (engine.getBankName() == "forja-canivete-full", "nome do bank vem do arquivo",
           engine.getBankName());
    check (engine.getStatus().contains ("pronto"), "status publica bank pronto", engine.getStatus());

    forceAdoption (engine);
    const auto note = triggerRender (engine, 8, oneNote (1, 60, 0.8f));
    check (note.peak > 0.05f, "nota toca apos a adocao", juce::String (note.peak));
}

void testVolumeKnob()
{
    std::printf ("\n[volume no trim]\n");

    SoundFontEngine engine;
    engine.prepare (testSampleRate, 512);

    juce::String error;
    loadAndWait (engine, juce::File (loadedBankPath), error);
    forceAdoption (engine);

    engine.setVolume (0.5f);
    const auto unity = peakOneNote (engine, 1, 60, 0.8f);

    // 0,75 = +12 dB no taper trimGain(): o pico tem que quadruplicar.
    engine.setVolume (0.75f);
    const auto boosted = peakOneNote (engine, 1, 60, 0.8f);

    const auto ratio = unity > 0.0f ? boosted / unity : 0.0f;
    check (ratio > 3.4f && ratio < 4.6f, "volume 0,75 = +12 dB (x4)", juce::String (ratio, 2));

    engine.setVolume (0.25f);
    const auto soft = peakOneNote (engine, 1, 60, 0.8f);
    const auto softRatio = unity > 0.0f ? soft / unity : 0.0f;
    check (softRatio > 0.2f && softRatio < 0.31f, "volume 0,25 = -12 dB (div por 4)",
           juce::String (softRatio, 3));
}

void testZeroLatency()
{
    std::printf ("\n[latencia]\n");

    SoundFontEngine engine;
    engine.prepare (testSampleRate, 512);

    juce::String error;
    loadAndWait (engine, juce::File (loadedBankPath), error);
    forceAdoption (engine);

    // O wrapper colhe o MIDI no comeco do bloco e renderiza o bloco inteiro:
    // o disparo e por bloco, nao por amostra. Isso significa PDC 0 / latencia
    // zero: a voz comeca no MESMO bloco do note-on, sem nenhum look-ahead.
    // Colocar o note-on no frame 0 e exigir audio ainda no bloco 0 prova isso.
    const auto hit = triggerRender (engine, 2, oneNote (1, 60, 0.8f), 0);
    check (hit.firstNonZero >= 0 && hit.firstNonZero < testBlockSize,
           "audio comeca no mesmo bloco do note-on (sem latencia adicionada)",
           juce::String (hit.firstNonZero));
}

void testVelocityAndRelease()
{
    std::printf ("\n[velocidade e release]\n");

    SoundFontEngine engine;
    engine.prepare (testSampleRate, 512);

    juce::String error;
    loadAndWait (engine, juce::File (loadedBankPath), error);
    forceAdoption (engine);
    engine.setVolume (0.5f);

    const auto forte = peakOneNote (engine, 1, 60, 1.0f);
    const auto piano = peakOneNote (engine, 1, 60, 0.5f);

    const auto ratio = forte > 0.0f ? piano / forte : 0.0f;
    check (ratio > 0.4f && ratio < 0.6f, "velocidade 0,5 cai a metade do pico (ganho linear em vel)",
           juce::String (ratio, 3));

    // Nota sustentada enche o medidor; note-off com release 0 mata a voz e o
    // ultimo bloco processado republisha silencio (medidor por bloco).
    triggerRender (engine, 4, oneNote (1, 60, 0.8f));
    const auto held = engine.readMeter();
    triggerRender (engine, 6, { juce::MidiMessage::noteOff (1, 60) });
    const auto after = engine.readMeter();

    check (held.activeVoices > 0, "voz ativa com a nota sustentada",
           juce::String (held.activeVoices));
    check (after.activeVoices == 0 && after.peakLeft == 0.0f,
           "note-off libera a voz (release 0 corta a cauda)",
           juce::String (after.activeVoices));
}

void testBypass()
{
    std::printf ("\n[bypass]\n");

    SoundFontEngine engine;
    engine.prepare (testSampleRate, 512);

    juce::String error;
    loadAndWait (engine, juce::File (loadedBankPath), error);
    forceAdoption (engine);

    silentRender (engine, 30, true);    // a rampa de 10 ms precisa fechar antes da nota

    const auto muted = triggerRender (engine, 40, oneNote (1, 60, 0.8f), 0, true);
    check (muted.peak == 0.0f, "bypass continuo entrega silencio", juce::String (muted.peak));

    const auto back = triggerRender (engine, 8, oneNote (1, 60, 0.8f));
    check (back.peak > 0.0f, "sair do bypass volta a tocar");
}

void testPerBlockMeter()
{
    std::printf ("\n[medidor por bloco]\n");

    SoundFontEngine engine;
    engine.prepare (testSampleRate, 512);

    juce::String error;
    loadAndWait (engine, juce::File (loadedBankPath), error);
    forceAdoption (engine);
    engine.setVolume (0.5f);

    triggerRender (engine, 1, oneNote (1, 60, 0.8f));

    const auto meter = engine.readMeter();
    check (meter.peakLeft > 0.0f && meter.peakRight > 0.0f, "pico estereo publicado",
           juce::String (meter.peakLeft));
    check (meter.activeVoices > 0, "contagem de vozes ativas", juce::String (meter.activeVoices));

    // Diferente do MiniDrum: o medidor do canivete NAO e self-clearing; ele
    // republisha o estado do bloco. Num bloco mudo (nota solta), o pico deve
    // zerar por recomputacao, nao por leitura.
    silentRender (engine, 1);
    const auto still = engine.readMeter();
    check (still.peakLeft > 0.0f, "sem nota nova o ultimo bloco continua publicado",
           juce::String (still.peakLeft));

    triggerRender (engine, 4, { juce::MidiMessage::noteOff (1, 60) });
    const auto cleared = engine.readMeter();
    check (cleared.peakLeft == 0.0f && cleared.activeVoices == 0,
           "bloco mudo apos release republisha zero", juce::String (cleared.peakLeft));
}

void testMultiTimbral()
{
    std::printf ("\n[multi-timbral]\n");

    SoundFontEngine engine;
    engine.prepare (testSampleRate, 512);

    juce::String error;
    loadAndWait (engine, juce::File (loadedBankPath), error);
    forceAdoption (engine);
    engine.setVolume (0.5f);

    // Canal 1 (melodic 0): programa 1 -> preset "Test Strings" espelhado na UI.
    // Canais 3 e 10 tocam junto (multi-timbral). Sem note-off no mesmo frame: a
    // nota sustentada precisa soar durante a janela de medicao.
    const auto three = triggerRender (engine, 6,
        { juce::MidiMessage::programChange (1, 1),
          juce::MidiMessage::noteOn (1, 60, 0.8f),
          juce::MidiMessage::noteOn (3, 60, 0.8f),
          juce::MidiMessage::noteOn (10, 60, 0.8f) });

    check (three.peak > 0.05f, "canais multiples tocam juntos", juce::String (three.peak));
    check (engine.getSelectedPresetIndex() == 1, "program change no canal 1 espelha preset 1",
           juce::String (engine.getSelectedPresetIndex()));
    check (engine.getSelectedPresetName().contains ("Strings"), "nome espelhado",
           engine.getSelectedPresetName());

    triggerRender (engine, 4, { juce::MidiMessage::noteOff (1, 60),
                                juce::MidiMessage::noteOff (3, 60),
                                juce::MidiMessage::noteOff (10, 60) });

    // CC7 (volume do canal) atenua a nota seguinte: 64 ~ metade de 127.
    const auto loud = peakWithCc7 (engine, 127);
    const auto quiet = peakWithCc7 (engine, 64);
    const auto ccRatio = loud > 0.0f ? quiet / loud : 1.0f;
    check (ccRatio < 0.75f, "CC7 reduz o ganho do canal", juce::String (ccRatio, 3));

    // Pitch bend e CC genericos tem que passar sem crash nem travar o estado.
    const auto bent = triggerRender (engine, 4,
        { juce::MidiMessage::pitchWheel (1, 0x2000 + 2000),
          juce::MidiMessage::controllerEvent (1, 7, 127),
          juce::MidiMessage::noteOn (1, 62, 0.6f) });
    triggerRender (engine, 4, { juce::MidiMessage::noteOff (1, 62) });
    check (bent.peak > 0.0f, "pitch bend e CC atravessam o caminho e tocam",
           juce::String (bent.peak));
}

void testBankSwapAndReap()
{
    std::printf ("\n[troca de bank e reap]\n");

    const auto first = juce::File (loadedBankPath);
    const auto second = writeTestSf2 ("swap-b", "Segundo Piano");

    SoundFontEngine engine;
    engine.prepare (testSampleRate, 512);

    juce::String error;
    loadAndWait (engine, first, error);
    forceAdoption (engine);

    loadAndWait (engine, second, error);
    check (engine.getBankName() == "swap-b", "bank novo assume o nome", engine.getBankName());
    check (engine.retiredBankCountForTest() == 1, "bank antigo aguarda reap");

    forceAdoption (engine);   // audio colhe o token 2
    engine.reapRetiredBanks();
    check (engine.retiredBankCountForTest() == 0, "reap liberou o bank substituido");
    check (engine.activeBankUseCountForTest() == 1, "so o bank ativo permanece vivo");

    const auto note = triggerRender (engine, 8, oneNote (1, 60, 0.8f));
    check (note.peak > 0.05f, "o bank novo toca", juce::String (note.peak));

    second.deleteFile();
}

void testInvalidBankKeepsPlaying()
{
    std::printf ("\n[bank invalido]\n");

    const auto good = juce::File (loadedBankPath);
    const auto bad = good.getParentDirectory().getChildFile ("lixo.sf2");
    bad.replaceWithText ("isto nao e um RIFF sfbk de jeito nenhum");

    SoundFontEngine engine;
    engine.prepare (testSampleRate, 512);

    juce::String error;
    loadAndWait (engine, good, error);
    forceAdoption (engine);

    juce::String secondError;
    const auto completed = loadAndWait (engine, bad, secondError);

    check (completed, "bank invalido conclui com erro (nao trava)");
    check (secondError.isNotEmpty(), "erro de leitura e descritivo", secondError);
    check (engine.getBankName() == good.getFileNameWithoutExtension(),
           "bank invalido nao substitui o tocador atual", engine.getBankName());

    const auto note = triggerRender (engine, 8, oneNote (1, 60, 0.8f));
    check (note.peak > 0.0f, "o bank anterior continua tocando");

    bad.deleteFile();
}

void testNoAllocations()
{
    std::printf ("\n[sem alocacao no audio]\n");

    SoundFontEngine engine;
    engine.prepare (testSampleRate, 512);

    juce::String error;
    loadAndWait (engine, juce::File (loadedBankPath), error);

    juce::AudioBuffer<float> out (2, testBlockSize);
    juce::MidiBuffer midi;
    midi.ensureSize (32 * static_cast<int> (sizeof (juce::int32)));

    // Eventos prontos fora da janela monitorada; dentro dela so clear/addEvent
    // com capacidade garantida e o process() do wrapper.
    const auto program = juce::MidiMessage::programChange (1, 1);
    const auto noteOn = juce::MidiMessage::noteOn (1, 60, 0.8f);
    const auto noteOff = juce::MidiMessage::noteOff (1, 60);
    const auto cc = juce::MidiMessage::controllerEvent (1, 7, 100);
    const auto bend = juce::MidiMessage::pitchWheel (1, 8192);

    // Adocao + vozes + canais fora da janela trackada.
    for (int block = 0; block < 12; ++block)
    {
        midi.clear();

        if (block == 0)
        {
            midi.addEvent (program, 0);
            midi.addEvent (noteOn, 8);
        }

        if (block == 6)
            midi.addEvent (noteOff, 0);

        out.clear();
        engine.process (midi, out, testBlockSize, block % 7 == 0);
    }

    trackAllocations.store (true, std::memory_order_relaxed);

    for (int block = 0; block < 64; ++block)
    {
        midi.clear();

        if (block % 8 == 0)
        {
            midi.addEvent (cc, 0);
            midi.addEvent (bend, 4);
            midi.addEvent (noteOn, 8);
        }

        if (block % 8 == 4)
            midi.addEvent (noteOff, 0);

        out.clear();
        engine.process (midi, out, testBlockSize, block % 9 == 0);
    }

    trackAllocations.store (false, std::memory_order_relaxed);

    check (! allocationTripped.load (std::memory_order_relaxed),
           "nenhuma alocacao em 64 blocos de audio");
}

void testRackModule()
{
    std::printf ("\n[gaveta do ForjaRack]\n");

    CaniveteModule module;

    // prepare varre a biblioteca compartilhada (Documents/Forja/Canivete); na CI
    // ela esta vazia e nada deve acontecer alem do contrato basico.
    module.prepare (testSampleRate);

    std::array<forjarack::MidiEvent, 2> events {};
    events[0] = { 10, 1, 60, 100, true };
    events[1] = { 120, 1, 60, 0, false };
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

    // idle() com a biblioteca compartilhada real: se houver bank, ele deve ser
    // adotado sem tocar no caminho de audio; se nao houver, nada acontece.
    const bool hasLibrary = ! findBanks (SoundFontEngine::sf3Available()).isEmpty();

    for (int pass = 0; pass < 50 && hasLibrary; ++pass)
    {
        module.idle();
        pumpMessages (20);
    }

    if (hasLibrary)
        check (true, "idle adota bank da biblioteca sem crash (biblioteca presente)");
    else
        std::printf ("  SKIP  biblioteca compartilhada vazia nesta maquina\n");
}

void testDrawerFiles()
{
    std::printf ("\n[gaveta de arquivos do rack]\n");

    // SF2 minimo valido fora da biblioteca (Documents/Forja/Canivete).
    const auto file = writeTestSf2 ("forja-canivete-drawer", "Gaveta Piano");

    CaniveteModule module;
    module.prepare (testSampleRate);

    check (module.supportsDrawerFiles(), "gaveta expoe a banda de arquivos");

    // Nome fora da biblioteca nao e escolha de lista.
    check (! module.drawerSelectFile ("C:/nao/existe/bank.sf2"),
           "nome fora da lista nao e aceito como escolha");

    // Externo de verdade: o idle colhe a carga e passa a exibir o nome; o knob
    // de indice fica mudo ate a proxima escolha pela lista.
    module.drawerLoadExternalFile (file.getFullPathName().toStdString());

    for (int pass = 0; pass < 5; ++pass)
    {
        module.idle();
        pumpMessages (20);
    }

    const auto shown = juce::String (module.drawerCurrentName());
    check (shown.containsIgnoreCase ("forja-canivete-drawer"), "externo colhido pelo idle",
           shown);

    module.setParam (1, 0.5f);   // o knob nao pode recolher o externo

    for (int pass = 0; pass < 5; ++pass)
    {
        module.idle();
        pumpMessages (20);
    }

    check (juce::String (module.drawerCurrentName()).containsIgnoreCase ("forja-canivete-drawer"),
           "knob fica mudo enquanto o externo manda");

    // Caminho inexistente: a gaveta tenta, falha e devolve o comando ao knob.
    module.drawerLoadExternalFile ("C:/forja-que-nao-existe/bank.sf2");
    module.idle();
    pumpMessages (20);
    module.idle();

    for (int pass = 0; pass < 50; ++pass)
    {
        module.idle();
        pumpMessages (10);
    }

    check (true, "externo inexistente nao derruba a gaveta");

    file.deleteFile();
}

void testDrawerInstruments()
{
    std::printf ("\n[gaveta de instrumento do rack]\n");

    // O SF2 de teste tem dois presets: "Gaveta Instrumento" (prog 0) e
    // "Test Strings" (prog 1). A banda so pode ser exercida pelo contrato
    // publico da gaveta; o engine e privado do wrapper de proposito.
    const auto file = writeTestSf2 ("forja-canivete-instr", "Gaveta Instrumento");
    const auto other = writeTestSf2 ("forja-canivete-instr-b", "Segundo Gaveta");

    CaniveteModule module;
    module.prepare (testSampleRate);

    check (module.supportsDrawerInstruments(), "gaveta CANIVETE expoe a banda de instrumento");

    // Sem bank assentado: lista vazia (a UI esconde a fileira nessa fase).
    check (module.drawerInstrumentNames().empty(), "sem bank nao ha lista de instrumento");

    module.drawerLoadExternalFile (file.getFullPathName().toStdString());

    std::vector<float> left (static_cast<size_t> (testBlockSize), 0.0f);
    std::vector<float> right (static_cast<size_t> (testBlockSize), 0.0f);

    // Deixa a carga assentar: o idle colhe, o process adota o bank. Pedir um
    // preset ANTES de assentar e legal mas significa outra coisa (o modulo
    // limpa o pedido na colheita: bank novo volta ao primeiro preset).
    for (int pass = 0; pass < 120 && module.drawerInstrumentNames().empty(); ++pass)
    {
        module.idle();
        module.process (left.data(), right.data(), testBlockSize);
        pumpMessages (20);
    }

    const auto names = module.drawerInstrumentNames();
    check (names.size() == 2, "bank assentado expoe os dois presets", juce::String (names.size()));

    // Pedido com o bank ainda carregando de verdade (segunda carga): o nome
    // fica pendente no modulo e o applyPendingInstrument do idle resolve.
    module.drawerSelectInstrument ("Test Strings");

    for (int pass = 0; pass < 120; ++pass)
    {
        module.idle();
        module.process (left.data(), right.data(), testBlockSize);
        pumpMessages (20);

        if (juce::String (module.drawerCurrentInstrument()) == "Test Strings")
            break;
    }

    check (juce::String (module.drawerCurrentInstrument()) == "Test Strings",
           "pedido por nome resolve via idle", juce::String (module.drawerCurrentInstrument()));

    // Troca direta com bank pronto: aplica no mesmo requestPreset que o
    // process seguinte espelha (leitura da UI sempre um bloco atras do audio).
    module.drawerSelectInstrument ("Gaveta Instrumento");
    module.process (left.data(), right.data(), testBlockSize);
    check (juce::String (module.drawerCurrentInstrument()) == "Gaveta Instrumento",
           "escolha com bank pronto aplica no proximo bloco");

    // Nome que nao existe no bank: ignora, mantem o preset em uso.
    module.drawerSelectInstrument ("Piano Que Nao Existe");
    module.idle();
    module.process (left.data(), right.data(), testBlockSize);
    check (juce::String (module.drawerCurrentInstrument()) == "Gaveta Instrumento",
           "nome inexistente mantem o preset atual");

    // Bank novo limpa o pedido: o preset do bank velho nao e re-aplicado por
    // acaso no bank novo (indice nao e estavel entre banks); a colheita volta
    // ao primeiro preset. "Gaveta Instrumento" nem existe no bank novo.
    module.drawerSelectInstrument ("Gaveta Instrumento");
    module.drawerLoadExternalFile (other.getFullPathName().toStdString());

    for (int pass = 0; pass < 120; ++pass)
    {
        module.idle();
        module.process (left.data(), right.data(), testBlockSize);
        pumpMessages (20);

        if (! module.drawerInstrumentNames().empty()
            && juce::String (module.drawerCurrentInstrument()) == "Segundo Gaveta")
            break;
    }

    check (juce::String (module.drawerCurrentInstrument()) == "Segundo Gaveta",
           "bank novo limpa o pedido e volta ao primeiro preset",
           juce::String (module.drawerCurrentInstrument()));

    file.deleteFile();
    other.deleteFile();
}

void testDrawerAudio()
{
    std::printf ("\n[audio pela gaveta do rack]\n");

    // Prova o caminho real do rack com um bank de verdade: nota entra pelo span
    // POD (processMidi), o wrapper a materializa no MidiBuffer e o process (com
    // o bank ja adotado) precisa gerar audio. testDrawerFiles e
    // testDrawerInstruments mexem no contrato e nos nomes, mas nunca exigiram
    // pico audivel pela trilha do span: e aqui que o silencio de host apareceria.
    const auto file = writeTestSf2 ("forja-canivete-audio", "Audio Piano");

    CaniveteModule module;
    module.prepare (testSampleRate);
    module.setParam (0, 0.5f);   // volume nominal (o rack reenvia todo bloco)

    module.drawerLoadExternalFile (file.getFullPathName().toStdString());

    std::vector<float> left (static_cast<size_t> (testBlockSize), 0.0f);
    std::vector<float> right (static_cast<size_t> (testBlockSize), 0.0f);

    // Deixa a carga assentar: o idle colhe e o process adota o bank.
    for (int pass = 0; pass < 120 && module.drawerInstrumentNames().empty(); ++pass)
    {
        module.idle();
        module.process (left.data(), right.data(), testBlockSize);
        pumpMessages (20);
    }

    check (! module.drawerInstrumentNames().empty(), "bank carregado pela gaveta");

    std::array<forjarack::MidiEvent, 1> events {
        forjarack::MidiEvent { 0, 1, 60, 100, true } };
    const forjarack::MidiEventSpan span { events.data(), 1 };

    module.processMidi (span);   // nota sustentada: so note-on

    float peak = 0.0f;
    for (int block = 0; block < 8; ++block)
    {
        std::fill (left.begin(), left.end(), 0.0f);
        std::fill (right.begin(), right.end(), 0.0f);
        module.process (left.data(), right.data(), testBlockSize);
        peak = std::max (peak, module.readOutputPeak());
    }

    check (peak > 0.05f, "nota pelo span gera audio audivel na gaveta", juce::String (peak));

    file.deleteFile();
}

void testRegistration()
{
    std::printf ("\n[registro no catalogo]\n");

    const int before = forjarack::ModuleCatalog::count();

    registerCaniveteModule();
    registerCaniveteModule();

    const int index = forjarack::ModuleCatalog::indexOf ("canivete");
    check (index >= 0, "canivete esta registrado");
    check (forjarack::ModuleCatalog::count() == before + (index >= before ? 1 : 0),
           "registro duplo e ignorado (append-only)",
           juce::String (forjarack::ModuleCatalog::count()) + " vs " + juce::String (before));

    if (index >= 0)
    {
        const auto& spec = forjarack::ModuleCatalog::specByIndex (index);
        check (juce::String (spec.name) == "CANIVETE", "rotulo da gaveta");
        check (spec.role == forjarack::ModuleRole::instrument, "papel de instrumento");
        check (spec.face == forjarack::ModuleFace::drum, "fachada drum");
        check (spec.numParams == 2, "dois knobs (VOLUME e BANK)");
        check (juce::String (spec.knobs[1].label) == "BANK", "rotulo do segundo knob");

        auto created = forjarack::ModuleCatalog::create (index);
        check (created != nullptr && created->isInstrument(), "fabrica entrega um instrument");
        check (created != nullptr && created->latencySamples() == 0, "instrumento com latencia zero");
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

    std::printf ("Canivete Suico tests (SF3 %s)\n",
                 SoundFontEngine::sf3Available() ? "habilitado" : "ausente neste build");

    testSilenceWithoutBank();
    testBankLoadAndAdoption();
    testVolumeKnob();
    testZeroLatency();
    testVelocityAndRelease();
    testBypass();
    testPerBlockMeter();
    testMultiTimbral();
    testBankSwapAndReap();
    testInvalidBankKeepsPlaying();
    testNoAllocations();
    testRackModule();
    testDrawerFiles();
    testDrawerInstruments();
    testDrawerAudio();
    testRegistration();

    if (failures == 0)
    {
        std::printf ("\nPASS: wrapper Canivete\n");
        return 0;
    }

    std::printf ("\nFAIL: %d verificacao(oes)\n", failures);
    return 1;
}
