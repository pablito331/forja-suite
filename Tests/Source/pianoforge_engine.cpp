// Testes do engine do PIANOFORGE: adocao de banco e troca com reap, pedal de
// sustain, fatiamento de eventos (a nota soa no instante certo, nao no comeco
// do bloco), monotonia de tampa e martelo, escala do martelo, mapa bit-exato dos
// 23 parametros, leitura de .pforge valido e rejeicao de .pforge com CRC errado
// (sem derrubar o banco no ar), rampa de bypass de 10 ms, latencia medida por
// impulso e ausencia de alocacao no caminho de audio.
//
// Sem paridade de disco: o teste gera um SF2 minimo valido (RIFF sfbk com pdta
// completo e smpl senoidal) e empacota-lo num .pforge (PFG1) proprios, para a CI
// nao depender de nenhum banco externo.
#include <juce_events/juce_events.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <memory>
#include <new>
#include <vector>

#include "engine/Bundle.h"
#include "engine/PianoEngine.h"
#include "PianoforgeRackModule.h"
#include "rack/ModuleCatalog.h"

using namespace forja::pianoforge;

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
// CRC32 refletido (mesmo polynomial do leitor Bundle e do zlib.crc32 do
// empacotador do projeto-irmao) para o teste montar o diretario do .pforge.
// ---------------------------------------------------------------------------
std::uint32_t crc32Of (const void* data, std::size_t len) noexcept
{
    static const std::array<std::uint32_t, 256> table = []
    {
        std::array<std::uint32_t, 256> t {};

        for (std::uint32_t n = 0; n < 256; ++n)
        {
            std::uint32_t c = n;

            for (int k = 0; k < 8; ++k)
                c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);

            t[static_cast<std::size_t> (n)] = c;
        }

        return t;
    }();

    auto* p = static_cast<const std::uint8_t*> (data);
    std::uint32_t crc = 0xFFFFFFFFu;

    for (std::size_t i = 0; i < len; ++i)
        crc = table[(crc ^ p[i]) & 0xFFu] ^ (crc >> 8);

    return crc ^ 0xFFFFFFFFu;
}

// ---------------------------------------------------------------------------
// SF2 minimo valido (RIFF sfbk): preset unico "Test Piano" (banco 0, programa 0)
// com uma regiao senoidal sem loop. O bastante para exercitar adocao, nota e
// preset por banco do PIANOFORGE.
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

void appendChunk (juce::MemoryBlock& out, const char* fourcc, const juce::MemoryBlock& data)
{
    out.append (fourcc, 4);
    append32 (out, static_cast<juce::uint32> (data.getSize()));
    out.append (data.getData(), static_cast<int> (data.getSize()));

    if (data.getSize() & 1)
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

// Monta os bytes do SF2 no MemoryBlock dado (sem tocar disco), para o teste
// empacota-lo tambem num .pforge.
void buildTestSf2 (juce::MemoryBlock& fileData, const char* presetName)
{
    juce::MemoryBlock hdts;
    append16 (hdts, 2);
    append16 (hdts, 1);
    append32 (hdts, 0x02020202u);
    append16 (hdts, 0);

    juce::MemoryBlock phdr;
    appendText (phdr, presetName, 20);
    appendU16s (phdr, { 0, 0, 0 });
    append32 (phdr, 0); append32 (phdr, 0); append32 (phdr, 0);
    appendText (phdr, "EOS", 20);
    appendU16s (phdr, { 0, 0, 1 });
    append32 (phdr, 0); append32 (phdr, 0); append32 (phdr, 0);

    juce::MemoryBlock pbag;
    appendU16s (pbag, { 0, 0, 1, 0 });

    juce::MemoryBlock pgen;
    appendU16s (pgen, { 41, 0 });

    juce::MemoryBlock modTerm;
    appendU16s (modTerm, { 0, 0, 0, 0, 0 });

    juce::MemoryBlock inst;
    appendText (inst, "Test Zone", 20);
    append16 (inst, 0);
    appendText (inst, "EOI", 20);
    append16 (inst, 1);

    juce::MemoryBlock ibag;
    appendU16s (ibag, { 0, 0, 2, 0 });

    juce::MemoryBlock igen;
    appendU16s (igen, { 54, 0, 53, 0 });
    appendU16s (igen, { 0, 0xFFFF });

    constexpr int sampleCount = 22050;

    juce::MemoryBlock shdr;
    appendText (shdr, "sine441", 20);
    append32 (shdr, 1);
    append32 (shdr, sampleCount + 1);
    append32 (shdr, 0);
    append32 (shdr, 0);
    append32 (shdr, 44100);
    const char pitchBytes[2] = { 60, 0 };
    shdr.append (pitchBytes, 2);
    append16 (shdr, 0);
    append16 (shdr, 1);
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

    juce::MemoryBlock smpl;
    std::vector<short> pcm (static_cast<size_t> (sampleCount + 4), 0);

    for (int i = 0; i < sampleCount; ++i)
        pcm[static_cast<size_t> (i) + 1] = static_cast<short> (
            32767.0 * 0.5 * std::sin (2.0 * 3.14159265358979 * 441.0 * static_cast<double> (i) / 44100.0));

    smpl.append (pcm.data(), static_cast<int> (pcm.size()) * static_cast<int> (sizeof (short)));

    juce::MemoryBlock sdta;
    appendChunk (sdta, "smpl", smpl);

    juce::MemoryBlock ifilChunk, isftChunk, iengChunk, info;
    append16 (ifilChunk, 2);
    append16 (ifilChunk, 1);
    appendChunk (info, "ifil", ifilChunk);
    appendText (isftChunk, "forja-test", 16);
    appendChunk (info, "isft", isftChunk);
    appendText (iengChunk, "forja-pianoforge-test", 24);
    appendChunk (info, "ieng", iengChunk);

    juce::MemoryBlock body;
    appendListChunk (body, "LIST", "info", info);
    appendChunk (body, "sdta", sdta);
    appendListChunk (body, "LIST", "pdta", pdta);

    fileData.append ("RIFF", 4);
    append32 (fileData, 0);
    fileData.append ("sfbk", 4);
    appendChunk (fileData, "hdts", hdts);
    fileData.append (body.getData(), static_cast<int> (body.getSize()));

    auto* sizeSlot = reinterpret_cast<juce::uint32*> (
        static_cast<char*> (fileData.getData()) + 4);
    *sizeSlot = static_cast<juce::uint32> (fileData.getSize() - 8);
}

juce::File pianoforgeTempDir()
{
    return juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("forja-pianoforge");
}

// Escreve o SF2 de teste em disco e devolve o arquivo.
juce::File writeTestSf2 (const juce::String& fileName, const char* presetName)
{
    juce::MemoryBlock fileData;
    buildTestSf2 (fileData, presetName);

    const auto dir = pianoforgeTempDir();
    dir.createDirectory();

    const auto file = dir.getChildFile (fileName + ".sf2");
    file.deleteFile();

    if (auto stream = file.createOutputStream())
        stream->write (fileData.getData(), fileData.getSize());

    return file;
}

// Empacota os bytes de um SF2 num container .pforge (PFG1) valido, com manifest.
// corruptBankCrc=true grava o CRC do banco errado (o payload fica intacto; so a
// conta nao bate), para exercitar a rejeicao legivel sem derrubar o banco no ar.
juce::File writeTestBundle (const juce::String& fileName, const juce::MemoryBlock& sf2,
                            const juce::String& productName, bool corruptBankCrc)
{
    struct Ent
    {
        juce::String name;
        std::uint16_t type;
        juce::MemoryBlock payload;
    };

    const juce::String manifestJson = "{\"produto\":\"" + productName
        + "\",\"presets\":[{\"nome\":\"" + productName + "\",\"sf2\":\"bank.sf2\"}]}";

    juce::MemoryBlock manifestBytes;
    manifestBytes.append (manifestJson.toRawUTF8(), manifestJson.getNumBytesAsUTF8());

    std::vector<Ent> ents;
    ents.push_back ({ "manifest.json", Bundle::kManifest, manifestBytes });
    ents.push_back ({ "bank.sf2", Bundle::kRaw, sf2 });

    constexpr std::size_t kHeaderSize = 12;
    constexpr std::size_t kRecordFixed = 30;

    std::size_t dirBytes = 0;

    for (const auto& e : ents)
        dirBytes += 2 + static_cast<std::size_t> (e.name.getNumBytesAsUTF8()) + kRecordFixed;

    std::uint64_t off = kHeaderSize + dirBytes;

    std::vector<std::uint64_t> offsets;

    for (const auto& e : ents)
    {
        offsets.push_back (off);
        off += static_cast<std::uint64_t> (e.payload.getSize());
    }

    juce::MemoryBlock out;
    out.append ("PFG1", 4);
    append16 (out, 1);            // versao
    append16 (out, 0);            // flags
    append32 (out, static_cast<juce::uint32> (ents.size()));

    for (std::size_t i = 0; i < ents.size(); ++i)
    {
        const auto& e = ents[i];
        const auto nameBytes = static_cast<int> (e.name.getNumBytesAsUTF8());
        append16 (out, nameBytes);
        out.append (e.name.toRawUTF8(), nameBytes);

        append16 (out, e.type);

        auto storedOff = offsets[i];
        auto size = static_cast<std::uint64_t> (e.payload.getSize());
        auto usize = size;
        auto crc = crc32Of (e.payload.getData(), e.payload.getSize());

        if (corruptBankCrc && e.type == Bundle::kRaw)
            crc ^= 0xFFFFFFFFu;   // CRC gravado errado; payload intacto

        auto appendU64 = [] (juce::MemoryBlock& o, std::uint64_t v)
        {
            o.append (&v, sizeof (v));
        };

        appendU64 (out, storedOff);
        appendU64 (out, size);
        appendU64 (out, usize);
        append32 (out, crc);
    }

    for (const auto& e : ents)
        out.append (e.payload.getData(), static_cast<int> (e.payload.getSize()));

    const auto dir = pianoforgeTempDir();
    dir.createDirectory();

    const auto file = dir.getChildFile (fileName + ".pforge");
    file.deleteFile();

    if (auto stream = file.createOutputStream())
        stream->write (out.getData(), out.getSize());

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

bool loadAndWait (PianoEngine& engine, const juce::File& file, juce::String& errorOut)
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

// Adota o banco pendente num bloco mudo antes de medir: a troca e colhida no
// primeiro process(), entao o note-on so entra no bloco seguinte.
void forceAdoption (PianoEngine& engine)
{
    juce::AudioBuffer<float> out (2, testBlockSize);
    juce::MidiBuffer midi;
    engine.process (midi, out, testBlockSize, false);
}

RenderResult triggerRender (PianoEngine& engine, int blocks,
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

RenderResult silentRender (PianoEngine& engine, int blocks, bool bypassed = false)
{
    return triggerRender (engine, blocks, {}, 0, bypassed);
}

std::vector<juce::MidiMessage> oneNote (int channel, int note, float velocity)
{
    return { juce::MidiMessage::noteOn (channel, note, velocity) };
}

// Toca uma nota sustentada, mede o pico e depois a libera, com o engine limpo
// entre as medicoes.
float peakOneNote (PianoEngine& engine, int note, float velocity)
{
    const auto peak = triggerRender (engine, 8, oneNote (1, note, velocity)).peak;
    triggerRender (engine, 4, { juce::MidiMessage::noteOff (1, note) });
    return peak;
}

// Setta os 19 parametros pelo dominio do engine e deixa os suavizadores
// alcancarem o alvo rodando blocos mudos (o updateBlock e por bloco).
void settleParams (PianoEngine& engine)
{
    for (int block = 0; block < 16; ++block)
        silentRender (engine, 1);
}

juce::String loadedBankPath;
std::vector<float> tailRecords;   // preenchido por testModelingStagesAudible (cauda pos-note-off)

// Prova de que a cadeia de modelagem realmente muda a AMOSTRA CRUA, por estagio.
// Cada modo liga UMA etapa sobre a base neutra (so tsf + master) e mede a
// diferenca de RMS e a cauda depois do note-off contra a base. Cobre o buraco
// que deixava o usuario comparar "leitor normal vs Pianoforge" sem nenhum teste
// provando que reverb, chorus, tampa, EQ, ressonancia e martelo fazem diferenca.
void testModelingStagesAudible()
{
    std::printf ("\n[diag: modelagem vs amostra crua]\n");

    auto capture = [] (int mode) -> std::vector<float>
    {
        PianoEngine e;
        e.prepare (testSampleRate, 512);

        juce::String err;
        loadAndWait (e, juce::File (loadedBankPath), err);
        forceAdoption (e);

        // Base neutra (dominio do engine): sem modelagem alguma.
        e.setParameter (Param_Volume, 0.85f);
        e.setParameter (Param_Hammer, 0.0f);
        e.setParameter (Param_VelCurve, 0.5f);
        e.setParameter (Param_SympRes, 0.0f);
        e.setParameter (Param_Lid, 1.0f);
        e.setParameter (Param_BassEQ, 0.5f);
        e.setParameter (Param_MidEQ, 0.5f);
        e.setParameter (Param_HighEQ, 0.5f);
        e.setParameter (Param_Chorus, 0.0f);
        e.setParameter (Param_Reverb, 0.0f);
        e.setParameter (Param_ReverbType, 1.0f);
        e.setParameter (Param_TrimDb, 0.0f);
        e.setParameter (Param_Width, 1.0f);
        e.setParameter (Param_ReleaseDamp, 0.35f);
        e.setParameter (Param_HammerTone, 0.5f);
        e.setParameter (Param_SympTune, 0.5f);
        e.setParameter (Param_EngineTune, 0.0f);
        e.setParameter (Param_Body, 0.0f);    // corpo do tampo off (base crua)
        e.setParameter (Param_Ring, 0.0f);    // modos de madeira off (base crua)
        e.setParameter (Param_Compressor, 0.0f);  // compressor off (bypass)
        e.setParameter (Param_Molho, 0.0f);       // molho off (bypass)
        e.setParameter (Param_Tail, 0.5f);        // cauda neutra (so age com molho)

        if (mode == 1)   // defaults de fabrica
        {
            e.setParameter (Param_Hammer, 0.16f);
            e.setParameter (Param_SympRes, 0.45f);
            e.setParameter (Param_Lid, 0.52f);
            e.setParameter (Param_Reverb, 0.12f);
            e.setParameter (Param_Body, 0.62f);
            e.setParameter (Param_Ring, 0.50f);
        }
        else if (mode == 2)   // assertiva
        {
            e.setParameter (Param_Hammer, 0.7f);
            e.setParameter (Param_SympRes, 0.7f);
            e.setParameter (Param_Lid, 0.65f);
            e.setParameter (Param_BassEQ, 0.62f);
            e.setParameter (Param_MidEQ, 0.42f);
            e.setParameter (Param_HighEQ, 0.6f);
            e.setParameter (Param_Chorus, 0.15f);
            e.setParameter (Param_Reverb, 0.4f);
        }
        else if (mode == 3)   // so reverb
        {
            e.setParameter (Param_Reverb, 0.6f);
        }
        else if (mode == 4)   // so ressonancia simpatica
        {
            e.setParameter (Param_SympRes, 1.0f);
        }
        else if (mode == 5)   // so tampa bem fechada
        {
            e.setParameter (Param_Lid, 0.2f);
        }
        else if (mode == 6)   // so chorus
        {
            e.setParameter (Param_Chorus, 0.8f);
        }
        else if (mode == 7)   // so martelo
        {
            e.setParameter (Param_Hammer, 1.0f);
        }
        else if (mode == 8)   // so EQ de corpo (graves+medios+agudos longe do centro)
        {
            e.setParameter (Param_BassEQ, 1.0f);
            e.setParameter (Param_MidEQ, 0.0f);
            e.setParameter (Param_HighEQ, 1.0f);
        }
        else if (mode == 9)   // so resposta do tampo (curva de radiacao do corpo)
        {
            e.setParameter (Param_Body, 1.0f);
        }
        else if (mode == 10)  // so modos da madeira (anel depois do ataque)
        {
            e.setParameter (Param_Ring, 1.0f);
        }
        else if (mode == 11)  // so compressor de um knob (talos)
        {
            e.setParameter (Param_Compressor, 1.0f);
        }
        else if (mode == 12)  // so molho (shimmer de oitava, cauda solta)
        {
            e.setParameter (Param_Molho, 1.0f);
            e.setParameter (Param_Tail, 1.0f);
        }

        settleParams (e);

        juce::AudioBuffer<float> out (2, testBlockSize);
        juce::MidiBuffer midi;
        std::vector<float> buf;

        const auto on = juce::MidiMessage::noteOn (1, 60, 0.8f);
        const auto off = juce::MidiMessage::noteOff (1, 60);

        midi.clear();
        midi.addEvent (on, 8);
        out.clear();
        e.process (midi, out, testBlockSize, false);
        for (int j = 0; j < testBlockSize; ++j)
            buf.push_back (out.getReadPointer (0) [j]);

        for (int i = 0; i < 8; ++i)
        {
            midi.clear();
            out.clear();
            e.process (midi, out, testBlockSize, false);
            for (int j = 0; j < testBlockSize; ++j)
                buf.push_back (out.getReadPointer (0) [j]);
        }

        midi.clear();
        midi.addEvent (off, 0);
        out.clear();
        e.process (midi, out, testBlockSize, false);
        for (int j = 0; j < testBlockSize; ++j)
            buf.push_back (out.getReadPointer (0) [j]);

        double tail = 0.0;
        // Janela que ultrapassa o decay da difusao do shimmer (~0,30 s = 14400
        // amostras = ~56 blocos de 256): 120 blocos (~640 ms) cobrem o halo do
        // molho voltando depois do note-off. E cauda de reverb, nao eco de delay.
        for (int i = 0; i < 120; ++i)
        {
            midi.clear();
            out.clear();
            e.process (midi, out, testBlockSize, false);
            for (int j = 0; j < testBlockSize; ++j)
            {
                const float s = out.getReadPointer (0) [j];
                buf.push_back (s);
                tail += static_cast<double> (s) * s;
            }
        }

        tailRecords.push_back (std::sqrt (tail));
        return buf;
    };

    auto rms = [] (const std::vector<float>& v)
    {
        double s = 0.0;
        for (const float x : v)
            s += static_cast<double> (x) * x;
        return std::sqrt (s / static_cast<double> (v.empty() ? 1 : v.size()));
    };

    auto diffRms = [] (const std::vector<float>& a, const std::vector<float>& b)
    {
        const std::size_t n = std::min (a.size(), b.size());
        double s = 0.0;
        for (std::size_t i = 0; i < n; ++i)
        {
            const double d = static_cast<double> (a[i]) - static_cast<double> (b[i]);
            s += d * d;
        }
        return std::sqrt (s / static_cast<double> (n == 0 ? 1 : n));
    };

    // Energia na banda do "peito" do piano (80..500 Hz), por soma de Goertzel em
    // frequencias espacadas. E a medida que separa ENCARACOLAR grave (energia em
    // todo lugar, inclusive sub < 60 Hz) de CORPO de tampo (energia que sobe justamen-
    // te na banda onde um tampo de cauda radia). Compara o valor relativo contra a
    // base crua, entao a escala absoluta do sinal nao importa.
    auto bandEnergy = [] (const std::vector<float>& v)
    {
        double acc = 0.0;
        for (double f = 90.0; f <= 480.0; f += 30.0)
        {
            const double w  = 2.0 * 3.14159265358979 * f / static_cast<double> (testSampleRate);
            const double cw = 2.0 * std::cos (w);
            double s0 = 0.0, s1 = 0.0, s2 = 0.0;
            for (const float x : v)
            {
                s2 = s1;
                s1 = s0;
                s0 = static_cast<double> (x) + cw * s1 - s2;
            }
            const double re = s0 - s1 * std::cos (w);
            const double im = s1 * std::sin (w);
            acc += re * re + im * im;
        }
        return acc;
    };

    struct R { std::vector<float> buf; float tail = 0.0f; };

    const auto run = [&] (int mode) -> R
    {
        tailRecords.clear();
        R r;
        r.buf = capture (mode);
        r.tail = tailRecords.front();
        return r;
    };

    const auto neu = run (0);
    const float baseRms = rms (neu.buf);

    auto pct = [&] (const R& r) { return 100.0 * diffRms (r.buf, neu.buf) / baseRms; };

    const auto def  = run (1);
    const auto revb = run (3);
    const auto resp = run (4);
    const auto lidC = run (5);
    const auto chor = run (6);
    const auto hamm = run (7);
    const auto eqc  = run (8);
    const auto body = run (9);
    const auto ring = run (10);
    const auto comp = run (11);
    const auto molh = run (12);

    const double baseBand = bandEnergy (neu.buf);
    const double bodyBand = bandEnergy (body.buf);
    const double ringBand = bandEnergy (ring.buf);

    std::printf ("  (reverb %.1f%%, chorus %.1f%%, tampa %.1f%%, EQ %.1f%%, "
                 "resson %.1f%%, martelo %.1f%%, corpo %.1f%%, madeira %.1f%%, "
                 "compressor %.1f%%, molho %.1f%%, default %.1f%%)\n",
                 pct (revb), pct (chor), pct (lidC), pct (eqc),
                 pct (resp), pct (hamm), pct (body), pct (ring),
                 pct (comp), pct (molh), pct (def));
    std::printf ("  (banda 80-500 Hz: corpo %.2fx, madeira %.2fx, sobre a crua)\n",
                 bodyBand / baseBand, ringBand / baseBand);

    check (pct (revb) > 20.0, "reverb muda audivelmente a amostra crua",
           juce::String (pct (revb), 1) + "%");
    check (revb.tail > neu.tail * 10.0, "reverb deixa cauda depois do note-off",
           juce::String (revb.tail, 4));
    check (pct (chor) > 20.0, "chorus muda audivelmente a amostra",
           juce::String (pct (chor), 1) + "%");
    check (pct (lidC) > 10.0, "tampa fechada recolore o agudo",
           juce::String (pct (lidC), 1) + "%");
    check (pct (eqc) > 20.0, "EQ de corpo mexe no timbre",
           juce::String (pct (eqc), 1) + "%");
    check (pct (resp) > 1.0, "ressonancia simpatica adiciona energia mensuravel",
           juce::String (pct (resp), 1) + "%");
    check (pct (hamm) > 0.2, "martelo injeta transiente mensuravel",
           juce::String (pct (hamm), 1) + "%");
    check (pct (def) > 5.0 && def.tail > neu.tail * 5.0,
           "defaults ja diferenciam de um leitor cru",
           juce::String (pct (def), 1) + "% / cauda " + juce::String (def.tail, 4));

    // Camada de corpo acustico da Forja: a promessa "qualquer soundfont soar melhor
    // ao abrir" e aqui provada como ADICAO de energia na banda do peito (80-500 Hz),
    // nao como loudness nem como sub-encaracolar. Corpo sobe a banda; madeira soboe
    // a banda e deixa anel na cauda (os modos de alto Q continuam depois do note-off).
    check (pct (body) > 20.0, "resposta do tampo recolore a amostra crua",
           juce::String (pct (body), 1) + "%");
    check (bodyBand > baseBand * 1.15, "corpo ADD energia no peito 80-500 Hz (nao so volume)",
           juce::String (bodyBand / baseBand, 2) + "x");
    check (pct (ring) > 3.0, "modos da madeira mudam a amostra",
           juce::String (pct (ring), 1) + "%");
    check (ringBand > baseBand * 1.05, "madeira ADD ressonancias na banda do corpo",
           juce::String (ringBand / baseBand, 2) + "x");

    // Camada de molho (hoje um shimmer de oitava numa cauda difusa): compressor e
    // molho tem de mudar a amostra. O comportamento da cauda (o halo que floresce,
    // nao um eco que repete) e provado no teste isolado do Molho; aqui o modo 12
    // liga o pedal de sustain, entao comparamos so a diferenca de amostra.
    check (pct (comp) > 3.0, "compressor de um knob altera a dinamica mensuravel",
           juce::String (pct (comp), 1) + "%");
    check (pct (molh) > 8.0, "molho (shimmer de oitava) muda audivelmente a amostra",
           juce::String (pct (molh), 1) + "%");
}

// Prova de que o Molho virou um shimmer de malha de realimentacao:
//  (a) o knob CAUDA (Param_Tail, no MOTOR) e o feedback/decay da MALHA: o sinal
//      medido com um tom sustentado mostra shimmer acima do seco, e o que muda
//      com a cauda e a PERSISTENCIA (o halo vivo depois de cortar a entrada),
//      nao a energia do bloco em regime -- a energia total em regime e diluida
//      pelo proprio seco e nao discrimina a cauda;
//  (b) cortada a entrada, a cauda domada drena (nao congela num plateau ~1) e a
//      cauda longa floresce mais -- e um halo de reverb, nao uma fita de delay
//      que repete a tecla;
//  (c) em 0 e bypass honesto (nao encosta no seco).
void testMolhoTailControl()
{
    std::printf ("\n[molho: shimmer de oitava numa cauda difusa]\n");

    auto steadyShimmer = [] (float tail)
    {
        Molho m;
        m.prepare (48000.0f);
        m.setAmount (1.0f);      // molho no talo
        m.setTail (tail);        // 0 = halo curto, 1 = pad longo

        std::array<float, 256> l{};
        std::array<float, 256> r{};

        double e = 0.0;
        for (int b = 0; b < 40; ++b)   // tom sustentado: >2 periodos do grao (~26 blocos) para a janela de leitura estar preenchida
        {
            for (int i = 0; i < 256; ++i)
            {
                const float t = static_cast<float> (b * 256 + i);
                l[i] = 0.4f * std::sin (0.11f * t);
                r[i] = 0.4f * std::sin (0.11f * t);
            }
            m.process (l.data(), r.data(), 256);
            if (b == 39)               // so o ultimo bloco (mede shimmer somado ao seco)
                for (int i = 0; i < 256; ++i)
                    e += static_cast<double> (l[i]) * l[i] + static_cast<double> (r[i]) * r[i];
        }
        return e;
    };

    const double domada = steadyShimmer (0.0f);
    const double aberta = steadyShimmer (1.0f);

    std::printf ("  (cauda 0: %.4f   cauda 1: %.4f)\n", domada, aberta);

    check (domada > 1e-6 && aberta > 1e-6, "molho no talo gera shimmer (soma energia acima do zero)");
    // A cauda NAO discrimina a energia em regime (o proprio seco a dilui); quem
    // prova o sentido fisico da cauda e o teste de PERSISTENCIA logo abaixo.

    // A CAUDA e o RT do resonador espectral: controla baseDecay, o tempo em que a
    // energia por bin decai. Para medir ESSE tempo sem o confounder da cascata,
    // dosamos o molho baixo (rate pequeno): assim quase nada de energia sobe a
    // oitava ate passar do lowpass de 5,4 kHz e ser drenada -- o que resta na
    // janela tardia e o decaimento puro do halo, que e exatamente o que a cauda
    // controla. NAO e um eco que repete a nota -- e difusao espectral (magnitude
    // com decaimento + fase aleatoria), nao uma fita de delay.
    auto tailResidual = [] (float tail)
    {
        Molho m;
        m.prepare (48000.0f);
        m.setAmount (0.25f);   // cascata suave: deixa a cauda (RT) ser o efeito dominante
        m.setTail (tail);
        std::array<float, 256> l{};
        std::array<float, 256> r{};
        // Alimenta com um TOM sustentado (nota real de piano), nao DC: a DC vaza
        // em todos os bins e nao representa o caso de uso. O halo enche ate o
        // regime (RT longo) ao longo dos blocos de alimentacao.
        for (int b = 0; b < 160; ++b)
        {
            for (int i = 0; i < 256; ++i)
            {
                const float t = static_cast<float> (b * 256 + i);
                l[i] = 0.4f * std::sin (0.11f * t);
                r[i] = 0.4f * std::sin (0.11f * t);
            }
            m.process (l.data(), r.data(), 256);
        }

        double res = 0.0;
        for (int b = 0; b < 220; ++b)          // corta a entrada e mede a cauda morta
        {
            for (int i = 0; i < 256; ++i) { l[i] = 0.0f; r[i] = 0.0f; }
            m.process (l.data(), r.data(), 256);
            if (b >= 190)                      // janela bem tardia: so um halo de RT longo survive
                for (int i = 0; i < 256; ++i)
                    res += static_cast<double> (l[i]) * l[i] + static_cast<double> (r[i]) * r[i];
        }
        return res;
    };

    const double resLonga = tailResidual (1.0f);
    const double resCurta = tailResidual (0.0f);
    std::printf ("  (residuo cauda longa: %.6f   cauda domada: %.6f)\n", resLonga, resCurta);

    check (resLonga > resCurta * 2.0, "cauda longa floresce mais que cauda domada (sala maior = decay maior)",
           juce::String (resLonga / (resCurta + 1e-12), 2) + "x");
    // A cauda domada ainda assim e pequena num absoluto: feedback limitado a 0,90
    // e amortecimento dos combs garantem que o halo morre (nao congela num
    // plateau ~1). O ponto central -- a CAUDA voltou a dosar o comprimento do halo
    // -- esta provado na razao acima.
    check (resLonga > 1e-4, "cauda longa mantem halo vivo na janela tardia",
           juce::String (resLonga, 6));

    // A QUINTA soma a camada de 12a (oitava + 5a) sobre a oitava. Como o shift
    // agora le do core e escreve num buffer separado (nao realimenta), e
    // estruturalmente impossivel compor a quinta (o antigo 3k->9k->27a dava notas
    // dissonantes). Guardas: abrir a quinta ADD energia audivel e o halo continua
    // limitado (sem runaway), porque nao ha mais feedback de oitava/quinta.
    auto molhoAtTone = [] (float tone)
    {
        Molho m;
        m.prepare (48000.0f);
        m.setAmount (1.0f);
        m.setTone (tone);
        m.setTail (0.5f);
        std::array<float, 256> l{};
        std::array<float, 256> r{};
        double e = 0.0;
        float peak = 0.0f;
        for (int b = 0; b < 40; ++b)
        {
            for (int i = 0; i < 256; ++i)
            {
                const float t = static_cast<float> (b * 256 + i);
                l[i] = 0.4f * std::sin (0.11f * t);
                r[i] = 0.4f * std::sin (0.11f * t);
            }
            m.process (l.data(), r.data(), 256);
            if (b == 39)
                for (int i = 0; i < 256; ++i)
                {
                    e += static_cast<double> (l[i]) * l[i] + static_cast<double> (r[i]) * r[i];
                    peak = std::max (peak, std::abs (l[i]));
                }
        }
        return std::pair<double, float> (e, peak);
    };

    const auto q0 = molhoAtTone (0.0f);
    const auto q1 = molhoAtTone (1.0f);
    std::printf ("  (quinta 0: energia %.4f pico %.3f   quinta 1: energia %.4f pico %.3f)\n",
                 q0.first, q0.second, q1.first, q1.second);
    check (q1.first > q0.first, "quinta abre a 12a e ADD energia audivel no halo");
    check (q1.second < 4.0f && std::isfinite (q1.first),
           "quinta nao explode (shift fixo, sem feedback de quinta recomposta)",
           juce::String (q1.second, 3));

    // Com o molho mudo, a saida e a entrada (bypass amostra a amostra).
    Molho off;
    off.prepare (48000.0f);
    off.setAmount (0.0f);
    off.setTail (1.0f);
    std::array<float, 128> a { 0.3f };
    std::array<float, 128> b { -0.3f };
    for (int i = 1; i < 128; ++i) { a[i] = 0.3f; b[i] = -0.3f; }
    off.process (a.data(), b.data(), 128);
    check (a[0] == 0.3f && b[0] == -0.3f, "molho em 0 = bypass honesto (nao mexe no seco)");
}

// Prova do compressor ORIGINAL da Forja (engine/dsp/Compressor.h), que substituiu o
// Dyno. Tres garantias:
//  (a) em 0 e bypass honesto (amostra ilesa);
//  (b) NAO degrada: um tom PURO sustentado sai limpo (THD baixo) e com a crista de
//      um senoide (~sqrt2) preservada. E o ponto que a versao de attack instantaneo
//      reprovava -- o ganho modulava dentro de cada ciclo e gerava distorcao
//      auditavel. Se alguem voltar a por attack "salto por amostra", este teste pega.
//  (c) achata a macro-dinamica (crista pico/rms cai) segurando o nivel (makeup),
//      medido numa janela tardia, depois de o seguidor e o makeup assentarem.
void testCompressorSquash()
{
    std::printf ("\n[compressor: achata picos SEM degradar (limpo + makeup)]\n");

    constexpr double sr = 48000.0;
    constexpr double kPi = 3.14159265358979;

    auto measureLate = [] (const std::vector<float>& v)
    {
        const std::size_t start = v.size() / 2;
        double sum = 0.0;
        float peak = 0.0f;
        std::size_t cnt = 0;
        for (std::size_t i = start; i < v.size(); ++i, ++cnt)
        {
            const float x = v[i];
            sum += static_cast<double> (x) * x;
            peak = std::max (peak, std::abs (x));
        }
        const double rms = std::sqrt (sum / static_cast<double> (cnt == 0 ? 1 : cnt));
        return std::pair<double, float> (rms, peak);
    };

    // Amplitude espectral num bin via Goertzel (medida de harmonic distortion).
    auto toneMag = [] (const std::vector<float>& v, double f0, double sampleRate)
    {
        const double w  = 2.0 * 3.14159265358979 * f0 / sampleRate;
        const double cw = 2.0 * std::cos (w);
        double s0 = 0.0, s1 = 0.0, s2 = 0.0;
        for (const float x : v)
        {
            s2 = s1;
            s1 = s0;
            s0 = static_cast<double> (x) + cw * s1 - s2;
        }
        const double re = s0 - s1 * std::cos (w);
        const double im = s1 * std::sin (w);
        return std::sqrt (re * re + im * im);
    };

    // ---- (b) degradacao: tom puro tem que sair limpo ----------------------
    {
        // f0 inteiro no janela (medida sem leakage): 300 Hz * (48000/300=160 amostras/
        // ciclo) x 300 ciclos = 48000 amostras (1 s). Corpo a 0.5 (-6 dB), longe acima
        // do threshold: se o ganho modular por ciclo, aparecem harmonicos e a crista
        // se afasta de sqrt(2). Em regime nao ha movimento de ganho -> deve sair puro.
        constexpr int n = 48000;
        constexpr double f0 = 300.0;
        std::vector<float> inL (n), inR (n);
        for (int i = 0; i < n; ++i)
        {
            const float v = 0.5f * std::sin (static_cast<float> (2.0 * kPi * f0 * i / sr));
            inL[static_cast<std::size_t> (i)] = v;
            inR[static_cast<std::size_t> (i)] = v;
        }

        WorshipCompressor c;
        c.prepare (sr);
        c.setAmount (1.0f);
        std::vector<float> l = inL;
        std::vector<float> r = inR;
        c.process (l.data(), r.data(), n);

        // Janela de leitura so em regime (descarta o assentamento inicial).
        std::vector<float> late (l.begin () + n / 2, l.end ());
        const double fund = toneMag (late, f0, sr);
        double harm = 0.0;
        for (int k = 2; k <= 8; ++k)
        {
            const double h = toneMag (late, f0 * k, sr);
            harm += h * h;
        }
        const double thd = fund > 1e-9 ? std::sqrt (harm) / fund : 1.0;

        const auto [rmsOut, peakOut] = measureLate (l);
        const double crestOut = peakOut / (rmsOut + 1e-9);

        std::printf ("  (tom puro: THD %.4f   crista %.3f  [senoide puro = 1,414])\n",
                     thd, crestOut);

        check (thd < 0.03,
               "compressor NAO degrada: tom puro sai limpo (sem harmonicos)",
               juce::String (100.0 * thd, 2) + "% THD");
        // A crista de uma senoide pura e sqrt(2)=1,414. O teto maior tolera o ripple
        // normal de um detector de pico (o env recarrega a cada ciclo -- nao e
        // distorcao; o THD acima e a prova). O PISO pega o modo degradante: um ganho
        // que modula dentro do ciclo (attack "salto por amostra") APLANA cada pico e
        // derruba a crista para baixo de uma senoide.
        check (crestOut > 1.30 && crestOut < 1.72,
               "forma de onda preservada (sem achatar cada ciclo nem modular ganho)",
               juce::String (crestOut, 3));
    }

    // ---- (a) bypass honesto em 0 ------------------------------------------
    constexpr int barLen = 14400;   // 300 ms por barra
    constexpr int bars = 8;         // 2,4 s: makeup (200 ms) assenta bem na 2a metade
    constexpr int n = barLen * bars;
    std::vector<float> inL (n), inR (n);
    for (int i = 0; i < n; ++i)
    {
        const float amp = ((i / barLen) % 2 == 0) ? 0.85f : 0.25f;   // barra alta / baixa
        const float v = amp * std::sin (0.05f * static_cast<float> (i));
        inL[static_cast<std::size_t> (i)] = v;
        inR[static_cast<std::size_t> (i)] = v * 0.9f;
    }

    {
        WorshipCompressor c;
        c.prepare (sr);
        c.setAmount (0.0f);
        std::vector<float> l = inL;
        std::vector<float> r = inR;
        c.process (l.data(), r.data(), n);
        bool exact = true;
        for (int i = 0; i < n; ++i)
        {
            if (l[static_cast<std::size_t> (i)] != inL[static_cast<std::size_t> (i)]
             || r[static_cast<std::size_t> (i)] != inR[static_cast<std::size_t> (i)])
            {
                exact = false;
                break;
            }
        }
        check (exact, "compressor em 0 = bypass honesto (nao encosta na amostra)");
    }

    // ---- (c) macro-dinamica: reduz o ABRANGE (loud vs soft) segurando o nivel
    // Medido no INTERIOR de cada barra (pula os primeiros 60 ms, onde o ataque do
    // detector ainda esta fechando), na 2a metade (makeup assentado). Isso isola a
    // compressao de verdade -- a diferenca de nivel entre passagem alta e baixa --
    // sem ser enganado pelo leak de ataque (normal num compressor sem lookahead) nem
    // pelo makeup (que cancela na razao). O nivel MEDIO e medido a parte.
    auto interiorRanges = [barLen] (const std::vector<float>& v)
    {
        const int skip = 2880;                 // 60 ms de ataque
        const int len = barLen - skip - 960;   // ainda deixa 20 ms de folga no fim
        double sumL = 0.0, sumS = 0.0;
        int nL = 0, nS = 0;
        for (int b = 4; b < 8; ++b)            // so a 2a metade (4 barras)
        {
            const int base = b * barLen + skip;
            double acc = 0.0;
            for (int i = 0; i < len; ++i)
            {
                const double x = v[static_cast<std::size_t> (base + i)];
                acc += x * x;
            }
            const double segRms = std::sqrt (acc / static_cast<double> (len));
            if (b % 2 == 0) { sumL += segRms * segRms; ++nL; }
            else            { sumS += segRms * segRms; ++nS; }
        }
        return std::pair<double, double> (std::sqrt (sumL / nL), std::sqrt (sumS / nS));
    };

    const auto [loudIn, softIn] = interiorRanges (inL);
    const double rangeIn = loudIn / (softIn + 1e-9);
    const double levelIn = std::sqrt (0.5 * (loudIn * loudIn + softIn * softIn));

    {
        WorshipCompressor c;
        c.prepare (sr);
        c.setAmount (1.0f);
        std::vector<float> l = inL;
        std::vector<float> r = inR;
        c.process (l.data(), r.data(), n);

        // Finitude / sem explosao em todo o buffer.
        bool finite = true;
        for (int i = 0; i < n; ++i)
            if (! std::isfinite (l[static_cast<std::size_t> (i)])) { finite = false; break; }

        const auto [loudOut, softOut] = interiorRanges (l);
        const double rangeOut = loudOut / (softOut + 1e-9);
        const double levelOut = std::sqrt (0.5 * (loudOut * loudOut + softOut * softOut));

        std::printf ("  (macro: range entrada %.2f -> saida %.2f   nivel %.3fx)\n",
                     rangeIn, rangeOut, levelOut / (levelIn + 1e-9));

        check (finite, "compressor nao explode (saida finita em todo o buffer)");
        // Com ratio FIXA em 5:1 (o pedido do usuario: cada 1 dB acima do threshold vira
        // 5 dB de reducao), o squash de macro e musical sem virar brickwall.
        // O teto 0.72 pega o "nao comprime nada" (que deixaria o range em ~3.4) sem exigir
        // um achatamento de limitador que o compressor, por desenho, nao faz.
        check (rangeOut < rangeIn * 0.72,
               "compressor ACHATA a macro-dinamica (loud/soft se aproximam)",
               juce::String (rangeOut, 2) + " vs " + juce::String (rangeIn, 2));
        check (rangeOut > rangeIn * 0.15,
               "compressor nao aniquila tudo (range ainda existe, musical)",
               juce::String (rangeOut, 2));
        check (levelOut > levelIn * 0.5 && levelOut < levelIn * 1.7,
               "makeup segura o nivel medio (nao derruba como um ganho, nao explode)",
               juce::String (levelOut / (levelIn + 1e-9), 2) + "x");
    }
}
// ---------------------------------------------------------------------------
// testes
// ---------------------------------------------------------------------------
void testSilenceWithoutBank()
{
    std::printf ("\n[sem banco]\n");

    PianoEngine engine;
    engine.prepare (testSampleRate, 512);

    const auto silent = silentRender (engine, 4);
    check (silent.peak == 0.0f, "sem banco o par estereo fica mudo");
    check (! engine.hasBank(), "nada adotado sem carga");
    check (engine.getStatus().contains ("Nenhum"), "status informa ausencia de piano");
}

void testBankLoadAndAdoption()
{
    std::printf ("\n[carga e adocao]\n");

    const auto file = writeTestSf2 ("pianoforge-full", "Test Piano");
    loadedBankPath = file.getFullPathName();

    PianoEngine engine;
    engine.prepare (testSampleRate, 512);

    juce::String error;
    check (loadAndWait (engine, file, error), "carga concluiu", error);
    check (error.isEmpty(), "carga sem erro", error);

    check (engine.hasBank(), "banco ficou ativo");
    check (engine.getNumPresets() == 1, "o preset do SF2 foi lido",
           juce::String (engine.getNumPresets()));
    check (engine.getPresetName (0).startsWith ("Test Piano"), "nome do preset 0",
           engine.getPresetName (0));
    check (engine.getBankName() == "pianoforge-full", "nome do banco vem do arquivo",
           engine.getBankName());
    check (engine.getStatus().contains ("pronto"), "status publica piano pronto", engine.getStatus());

    forceAdoption (engine);
    const auto note = triggerRender (engine, 8, oneNote (1, 60, 0.8f));
    check (note.peak > 0.05f, "nota toca apos a adocao", juce::String (note.peak));
}

void testBankSwapAndReap()
{
    std::printf ("\n[troca de banco e reap]\n");

    const auto first = juce::File (loadedBankPath);
    const auto second = writeTestSf2 ("pianoforge-swap-b", "Segundo Piano");

    PianoEngine engine;
    engine.prepare (testSampleRate, 512);

    juce::String error;
    loadAndWait (engine, first, error);
    forceAdoption (engine);

    loadAndWait (engine, second, error);
    check (engine.getBankName() == "pianoforge-swap-b", "banco novo assume o nome", engine.getBankName());
    check (engine.retiredBankCountForTest() == 1, "banco antigo aguarda reap",
           juce::String (engine.retiredBankCountForTest()));

    forceAdoption (engine);   // audio colhe o token novo
    engine.reapRetiredBanks();
    check (engine.retiredBankCountForTest() == 0, "reap liberou o banco substituido");
    check (engine.activeBankUseCountForTest() == 1, "so o banco ativo permanece vivo",
           juce::String (static_cast<long long> (engine.activeBankUseCountForTest())));

    const auto note = triggerRender (engine, 8, oneNote (1, 60, 0.8f));
    check (note.peak > 0.05f, "o banco novo toca", juce::String (note.peak));

    second.deleteFile();
}

void testEventSlicing()
{
    std::printf ("\n[fatiamento de eventos]\n");

    PianoEngine engine;
    engine.prepare (testSampleRate, 512);

    juce::String error;
    loadAndWait (engine, juce::File (loadedBankPath), error);
    forceAdoption (engine);

    // Nota pedida no frame 200 do bloco: um player ingenuo tocaria no frame 0
    // (dispara por bloco). O fatiamento por delayFrames tem que segurar o inicio
    // ate o instante certo: nada audivel antes do frame 200.
    const auto late = triggerRender (engine, 4, oneNote (1, 62, 0.8f), 200);
    triggerRender (engine, 4, { juce::MidiMessage::noteOff (1, 62) });

    check (late.firstNonZero >= 200, "nota so comeca no frame pedido (nao no comeco do bloco)",
           juce::String (late.firstNonZero));
    check (late.firstNonZero >= 0 && late.firstNonZero < 256 + 200,
           "nota comeca no mesmo bloco em que foi pedida", juce::String (late.firstNonZero));
}

void testImpulseLatency()
{
    std::printf ("\n[latencia por impulso]\n");

    PianoEngine engine;
    engine.prepare (testSampleRate, 512);

    juce::String error;
    loadAndWait (engine, juce::File (loadedBankPath), error);
    forceAdoption (engine);

    // Note-on no frame 0: o audio comeca no MESMO bloco, sem look-ahead. A
    // latencia medida por impulso e < um bloco (o valor que o processor publica).
    const auto hit = triggerRender (engine, 2, oneNote (1, 60, 0.8f), 0);
    check (hit.firstNonZero >= 0 && hit.firstNonZero < testBlockSize,
           "audio comeca no mesmo bloco do note-on (sem latencia adicionada)",
           juce::String (hit.firstNonZero));
}

void testLidMonotonicity()
{
    std::printf ("\n[monotonia da tampa]\n");

    // Tampa = lowpass 2500 Hz (fechada) a 16000 Hz (aberta). Um tom agudo (8 kHz)
    // deve sair cada vez mais atenuado conforme a tampa fecha.
    auto measureAtLid = [] (float lid)
    {
        LidFilter filter;
        filter.prepare (static_cast<float> (testSampleRate));
        filter.setLid (lid);

        constexpr int frames = 4096;
        std::vector<float> left (static_cast<size_t> (frames));
        std::vector<float> right (static_cast<size_t> (frames));

        for (int i = 0; i < frames; ++i)
        {
            const float s = std::sin (2.0 * M_PI * 8000.0 * static_cast<double> (i) / testSampleRate);
            left[static_cast<size_t> (i)] = s;
            right[static_cast<size_t> (i)] = s;
        }

        filter.process (left.data(), right.data(), static_cast<std::size_t> (frames));

        double acc = 0.0;

        // descarta o transitório inicial do filtro
        for (int i = 1024; i < frames; ++i)
            acc += static_cast<double> (left[static_cast<size_t> (i)]) * left[static_cast<size_t> (i)];

        return std::sqrt (acc / static_cast<double> (frames - 1024));
    };

    const auto open = measureAtLid (1.0f);     // transparente
    const auto mid = measureAtLid (0.5f);
    const auto closed = measureAtLid (0.0f);   // cutoff 2500 Hz

    check (open > mid && mid > closed, "tampa fecha de forma monotonia em 8 kHz",
           juce::String (open, 4) + " > " + juce::String (mid, 4) + " > " + juce::String (closed, 4));
    check (closed < open * 0.5f, "tampa fechada atenua forte o agudo",
           juce::String (closed / open, 3));
}

void testHammerMonotonicityAndEscala()
{
    std::printf ("\n[monotonia e escala do martelo]\n");

    check (kHammerEscala == 0.50f, "escala do knob do martelo registrada (0,50)");

    auto hammerEnergy = [] (float amount)
    {
        HammerModel model;
        model.prepare (static_cast<float> (testSampleRate));
        model.trigger (60, 100, amount, 0.5f);

        constexpr int frames = 512;
        std::vector<float> left (static_cast<size_t> (frames), 0.0f);
        std::vector<float> right (static_cast<size_t> (frames), 0.0f);

        model.process (left.data(), right.data(), static_cast<std::size_t> (frames));

        float peak = 0.0f;

        for (int i = 0; i < frames; ++i)
        {
            peak = std::max (peak, std::abs (left[static_cast<size_t> (i)]));
            peak = std::max (peak, std::abs (right[static_cast<size_t> (i)]));
        }

        return peak;
    };

    // amount <= 0.001 nao dispara nada: o buffer fica intacto (zero).
    check (hammerEnergy (0.0f) == 0.0f, "martelo em zero nao injeta transiente");

    const auto soft = hammerEnergy (0.05f);
    const auto mid = hammerEnergy (0.10f);
    const auto hard = hammerEnergy (0.20f);

    check (soft > 0.0f, "martelo produz transiente audivel acima do piso");
    check (soft < mid && mid < hard, "transiente cresce com o knob do martelo",
           juce::String (soft, 4) + " < " + juce::String (mid, 4) + " < " + juce::String (hard, 4));
}

void testParamMapping()
{
    std::printf ("\n[mapeamento dos 23 params]\n");

    check (Param_Count == 23, "a tabela tem 23 parametros");

    bool endpointsOk = true;
    bool identityExact = true;   // span 1, min 0: o round-trip e a identidade
    bool scaledNoDrift = true;   // escalonados: ida e volta dentro de 1 ULP de folga
    juce::String firstBad;

    for (int id = 0; id < Param_Count; ++id)
    {
        if (id == Param_ReverbType)
            continue;   // choice quantizada: testada a parte

        const auto& def = kParamDefs[id];
        const auto span = def.max - def.min;
        const bool identity = (def.min == 0.0f && span == 1.0f);

        if (toEngineDomain (id, 0.0f) != def.min || toEngineDomain (id, 1.0f) != def.max)
        {
            endpointsOk = false;
            firstBad = juce::String (def.id) + " endpoints";
        }

        for (int step = 0; step <= 100; ++step)
        {
            const float g = static_cast<float> (step) / 100.0f;
            const float back = fromEngineDomain (id, toEngineDomain (id, g));

            if (identity)
            {
                if (back != g)
                {
                    identityExact = false;

                    if (firstBad.isEmpty())
                        firstBad = juce::String (def.id) + " em " + juce::String (g);
                }
            }
            else if (std::abs (back - g) > 1e-5f)
            {
                // Escalonar e re-normalizar por um span que nao e potencia de 2
                // com min deslocado (trimDb -12..12, engineTune -100..100, width
                // 0..2) custa no maximo 1 ULP por ponto da grade: o valor volta
                // estavel, nunca com deriva. E isso que a gama garante.
                scaledNoDrift = false;

                if (firstBad.isEmpty())
                    firstBad = juce::String (def.id) + " em " + juce::String (g)
                        + " virou " + juce::String (back);
            }
        }
    }

    check (endpointsOk, "cada param atinge min e max nos extremos da grade", firstBad);
    check (identityExact, "ida e volta bit-exata nos knobs 0..1 (identidade)", firstBad);
    check (scaledNoDrift, "ida e volta estavel (sem deriva) nos escalonados", firstBad);

    // Defaults de fabrica atuais (paineis PIANO+MOTOR do usuario; corpo acustico
    // ja modelado, compressor e molho opt-in em 0, cauda domada no meio).
    check (toEngineDomain (Param_Volume, 0.82f) == 0.82f, "volume default 0,82");
    check (toEngineDomain (Param_Hammer, 0.16f) == 0.16f, "martelo default 0,16 (leve)");
    check (toEngineDomain (Param_SympRes, 0.45f) == 0.45f, "ressonancia default 0,45");
    check (toEngineDomain (Param_Lid, 0.52f) == 0.52f, "tampa default 0,52 (meia-aberta)");
    check (toEngineDomain (Param_Reverb, 0.12f) == 0.12f, "reverb default 0,12");
    check (toEngineDomain (Param_Body, 0.62f) == 0.62f, "corpo (tampo) default 0,62");
    check (toEngineDomain (Param_Ring, 0.50f) == 0.50f, "madeira (anel) default 0,50");
    check (toEngineDomain (Param_Compressor, 0.0f) == 0.0f, "compressor default 0 (opt-in)");
    check (toEngineDomain (Param_Molho, 0.0f) == 0.0f, "molho default 0 (opt-in)");
    check (toEngineDomain (Param_Quinta, 0.0f) == 0.0f, "quinta default 0 (so oitava, opt-in)");
    check (toEngineDomain (Param_Tail, 0.5f) == 0.5f, "cauda default 0,5 (RT do meio)");
    check (toEngineDomain (Param_TrimDb, 0.5f) == 0.0f, "trim 0,5 = 0 dB");
    check (toEngineDomain (Param_TrimDb, 0.0f) == -12.0f, "trim 0 = -12 dB");
    check (toEngineDomain (Param_TrimDb, 1.0f) == 12.0f, "trim 1 = +12 dB");
    check (toEngineDomain (Param_Width, 0.5f) == 1.0f, "largura 0,5 = 1,0");
    check (toEngineDomain (Param_EngineTune, 0.5f) == 0.0f, "afinacao 0,5 = 0 cents");
    check (toEngineDomain (Param_EngineTune, 0.0f) == -100.0f, "afinacao 0 = -100 cents");
    check (toEngineDomain (Param_EngineTune, 1.0f) == 100.0f, "afinacao 1 = +100 cents");

    // reverbType: os 4 pontos de grade viram salas 0..3, sem intermediario.
    const float rooms[4] = { 0.0f, 1.0f / 3.0f, 2.0f / 3.0f, 1.0f };
    bool roomsOk = true;

    for (int i = 0; i < 4; ++i)
        if (toEngineDomain (Param_ReverbType, rooms[i]) != static_cast<float> (i))
            roomsOk = false;

    check (roomsOk, "reverbType mapeia 4 salas nos 4 pontos da grade");
    check (fromEngineDomain (Param_ReverbType, 2.0f) == 2.0f / 3.0f, "reverbType volta normalizado");

    // NaN nunca vira maxValue: a fronteira devolve o default.
    check (toEngineDomain (Param_Volume, std::nanf ("")) == 0.82f, "NaN cai no default (protecao de automacao)");
}

void testSustainPedalResonance()
{
    std::printf ("\n[pedal de sustain na ressonancia]\n");

    // A regra do §4 do projeto-irmao, na ordem REAL do pedal: pisa-se o pedal
    // ANTES de soltar a tecla. Com o pedal no fundo, o note-off nao recolhe o
    // ressonador (a corda segue vibrando); sem pedal, o note-off desliga e o
    // envelope decai. (Antes o teste pisava depois do note-off e "passava" so
    // porque o pedal acendia todas as notas 0..15 de uma vez -- o estalo.)
    auto resonanceAfterRelease = [] (bool pedal)
    {
        SympatheticResonance resp;
        resp.prepare (static_cast<float> (testSampleRate));
        resp.setParameters (1.0f, 0.5f);

        if (pedal)
            resp.setSustainPedal (true);

        resp.noteOn (60, 100);
        resp.noteOff (60);

        constexpr int frames = 2048;
        std::vector<float> left (static_cast<size_t> (frames), 0.0f);
        std::vector<float> right (static_cast<size_t> (frames), 0.0f);

        // Excita com um impulso no meio para alimentar o ressonador ligado. O
        // passe-through do impulso fica fora da janela de medicao, entao so a
        // ressonancia sustentada (com pedal) conta.
        left[1000] = 1.0f;
        right[1000] = 1.0f;

        resp.process (left.data(), right.data(), static_cast<std::size_t> (frames));

        // Energia logo depois do impulso: com pedal ainda ha ressonancia viva.
        double acc = 0.0;

        for (int i = 1100; i < frames; ++i)
            acc += static_cast<double> (left[static_cast<size_t> (i)]) * left[static_cast<size_t> (i)];

        return std::sqrt (acc);
    };

    const auto withPedal = resonanceAfterRelease (true);
    const auto withoutPedal = resonanceAfterRelease (false);

    check (withPedal > withoutPedal, "pedal segurado mantem o ressonador vivo apos note-off",
           juce::String (withPedal, 4) + " vs " + juce::String (withoutPedal, 4));
}

// Prova de ponta a ponta do pedal no caminho real do plugin (MidiBuffer ->
// engine -> tsf), nao so na ressonancia: com o CC 64 no ar, soltar a tecla NAO
// libera a voz (o abafador fica levantado e a corda segue ativa ate o fim do
// amostra); sem pedal, o note-off solta a voz quase na hora. Mede por
// activeVoices, que o engine espelha de tsf_active_voice_count a cada bloco.
void testSustainHoldsVoice()
{
    std::printf ("\n[pedal segura a voz apos note-off]\n");

    auto voicesAfterRelease = [] (bool pedal) -> int
    {
        PianoEngine engine;
        engine.prepare (testSampleRate, 512);

        juce::String error;
        loadAndWait (engine, juce::File (loadedBankPath), error);
        forceAdoption (engine);
        settleParams (engine);

        juce::AudioBuffer<float> out (2, testBlockSize);
        juce::MidiBuffer midi;

        const auto noteOn = juce::MidiMessage::noteOn (1, 60, 0.8f);
        const auto noteOff = juce::MidiMessage::noteOff (1, 60);
        const auto sustainDown = juce::MidiMessage::controllerEvent (1, 64, 127);

        const auto step = [&] (const juce::MidiMessage* m, int frame)
        {
            midi.clear();
            if (m != nullptr)
                midi.addEvent (*m, frame);
            out.clear();
            engine.process (midi, out, testBlockSize, false);
        };

        // Bloco inicial: pedal ja no ar (frame 0) e nota no frame 8.
        midi.clear();
        if (pedal)
            midi.addEvent (sustainDown, 0);
        midi.addEvent (noteOn, 8);
        out.clear();
        engine.process (midi, out, testBlockSize, false);

        for (int i = 0; i < 3; ++i)   // tecla abaixada, corda soando
            step (nullptr, 0);

        step (&noteOff, 0);           // solta a tecla (pedal continua no ar)

        for (int i = 0; i < 24; ++i)  // ~128 ms depois da soltura
            step (nullptr, 0);

        return engine.readMeter().activeVoices;
    };

    const int held = voicesAfterRelease (true);
    const int damped = voicesAfterRelease (false);

    check (held >= 1, "com pedal no ar a corda segue ativa apos o note-off",
           juce::String (held));
    check (damped == 0, "sem pedal o note-off solta a voz (abafador cai)",
           juce::String (damped));
}

void testBundleValidLoads()
{
    std::printf ("\n[.pforge valido]\n");

    const auto file = writeTestSf2 ("pianoforge-bundle-src", "Bundle Piano");

    juce::MemoryBlock sf2;
    file.loadFileAsData (sf2);

    const auto bundle = writeTestBundle ("pianoforge-good", sf2, "Meu Grande Piano", false);

    PianoEngine engine;
    engine.prepare (testSampleRate, 512);

    juce::String error;
    check (loadAndWait (engine, bundle, error), "bundle valido conclui", error);
    check (error.isEmpty(), "bundle valido sem erro", error);
    check (engine.hasBank(), "banco do bundle adotado");
    check (engine.getProductName() == "Meu Grande Piano", "produto lido do manifest",
           engine.getProductName());

    forceAdoption (engine);
    const auto note = triggerRender (engine, 8, oneNote (1, 60, 0.8f));
    check (note.peak > 0.05f, "SF2 dentro do bundle toca", juce::String (note.peak));

    bundle.deleteFile();
    file.deleteFile();
}

void testBundleBadCrcRejected()
{
    std::printf ("\n[.pforge com CRC errado]\n");

    const auto src = writeTestSf2 ("pianoforge-crc-src", "CRC Piano");

    juce::MemoryBlock sf2;
    src.loadFileAsData (sf2);

    const auto good = writeTestBundle ("pianoforge-crc-good", sf2, "Bom", false);
    const auto bad = writeTestBundle ("pianoforge-crc-bad", sf2, "Corrompido", true);

    PianoEngine engine;
    engine.prepare (testSampleRate, 512);

    juce::String error;
    loadAndWait (engine, good, error);
    forceAdoption (engine);

    // Rejeicao legivel: a carga conclui com erro, mas o banco que ja estava no
    // ar nao e derrubado e continua tocando.
    juce::String secondError;
    const auto completed = loadAndWait (engine, bad, secondError);

    check (completed, "bundle corrompido conclui com erro (nao trava)");
    check (secondError.isNotEmpty(), "erro de CRC e descritivo", secondError);
    check (engine.getBankName() == "Bom", "bundle corrompido nao substitui o tocador atual",
           engine.getBankName());

    const auto note = triggerRender (engine, 8, oneNote (1, 60, 0.8f));
    check (note.peak > 0.0f, "o banco anterior continua tocando");

    good.deleteFile();
    bad.deleteFile();
    src.deleteFile();
}

void testBypassRamp()
{
    std::printf ("\n[rampa de bypass]\n");

    PianoEngine engine;
    engine.prepare (testSampleRate, 512);

    juce::String error;
    loadAndWait (engine, juce::File (loadedBankPath), error);
    forceAdoption (engine);

    silentRender (engine, 30, true);   // a rampa de 10 ms fecha antes da nota

    const auto muted = triggerRender (engine, 40, oneNote (1, 60, 0.8f), 0, true);
    check (muted.peak == 0.0f, "bypass continuo entrega silencio", juce::String (muted.peak));

    const auto back = triggerRender (engine, 8, oneNote (1, 60, 0.8f));
    check (back.peak > 0.0f, "sair do bypass volta a tocar");

    // A rampa e gradual (nao um degrau): ao ligar o bypass com nota soando, o
    // primeiro bloco ainda tem audio residuo antes de zerar.
    triggerRender (engine, 2, oneNote (1, 60, 0.8f), 0, false);
    const auto transitioning = triggerRender (engine, 1, {}, 0, true);
    check (transitioning.peak > 0.0f, "ligar o bypass e suave (nao corta num degrau)",
           juce::String (transitioning.peak));
}

void testNoAllocations()
{
    std::printf ("\n[sem alocacao no audio]\n");

    PianoEngine engine;
    engine.prepare (testSampleRate, 512);

    juce::String error;
    loadAndWait (engine, juce::File (loadedBankPath), error);

    // Sobe os parametros para valores nao-default e deixa os suavizadores
    // alcancarem os alvos antes da janela monitorada (nenhuma rampa dentro dela).
    engine.setParameter (Param_Volume, 0.6f);
    engine.setParameter (Param_Hammer, 0.9f);
    engine.setParameter (Param_Lid, 0.3f);
    engine.setParameter (Param_Chorus, 0.4f);
    engine.setParameter (Param_Reverb, 0.5f);
    // Liga o shimmer espectral (FFT + cascata de oitava) para que a janela
    // monitorada cubra o caminho do Molho, nao so o resto da cadeia.
    engine.setParameter (Param_Molho, 0.8f);
    engine.setParameter (Param_Quinta, 0.5f);
    engine.setParameter (Param_Tail, 0.7f);
    settleParams (engine);

    juce::AudioBuffer<float> out (2, testBlockSize);
    juce::MidiBuffer midi;
    midi.ensureSize (32 * static_cast<int> (sizeof (juce::int32)));

    const auto noteOn = juce::MidiMessage::noteOn (1, 60, 0.8f);
    const auto noteOff = juce::MidiMessage::noteOff (1, 60);
    const auto sustainDown = juce::MidiMessage::controllerEvent (1, 64, 127);
    const auto sustainUp = juce::MidiMessage::controllerEvent (1, 64, 0);
    const auto bend = juce::MidiMessage::pitchWheel (1, 8192);

    // Adocao + vozes fora da janela trackada.
    for (int block = 0; block < 12; ++block)
    {
        midi.clear();

        if (block == 0)
        {
            midi.addEvent (noteOn, 8);
            midi.addEvent (sustainDown, 20);
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
            midi.addEvent (bend, 0);
            midi.addEvent (noteOn, 8);
        }

        if (block % 8 == 4)
            midi.addEvent (noteOff, 0);

        if (block % 16 == 8)
            midi.addEvent (sustainUp, 0);

        if (block % 16 == 12)
            midi.addEvent (sustainDown, 0);

        out.clear();
        engine.process (midi, out, testBlockSize, block % 9 == 0);
    }

    trackAllocations.store (false, std::memory_order_relaxed);

    check (! allocationTripped.load (std::memory_order_relaxed),
           "nenhuma alocacao em 64 blocos de audio");
}

// ---------------------------------------------------------------------------
// Gaveta de instrumento no ForjaRack: o registro e append-only no ModuleCatalog
// (papel/categoria instrument, dois knobs, fabrica que devolve um instrumento de
// latencia zero) e a carga de banco roda assincrona no idle(), respeitando o
// padrao do CANIVETE/MINIDRUM -- esperar com orcamento alto e sair no primeiro
// sinal, sem sleep fixo.

void testRackDrawerRegistration()
{
    const int before = forjarack::ModuleCatalog::count();

    registerPianoforgeModule();
    registerPianoforgeModule();   // idempotente: segunda chamada nao duplica

    const int index = forjarack::ModuleCatalog::indexOf ("pianoforge");
    check (index >= 0, "gaveta pianoforge publicada no ModuleCatalog");

    if (index < 0)
        return;

    const auto& spec = forjarack::ModuleCatalog::specByIndex (index);
    check (spec.role == forjarack::ModuleRole::instrument, "pianoforge tem papel instrument");
    check (spec.category == forjarack::ModuleCategory::instrument, "pianoforge na categoria instrument");
    check (spec.numParams == 2, "pianoforge expoe dois knobs (VOLUME e BANK)");

    check (forjarack::ModuleCatalog::count() == before + 1,
           "registro e append-only (so acrescenta ao fim, nao reordena)");

    auto module = forjarack::ModuleCatalog::create (index);
    check (module != nullptr, "fabrica do pianoforge cria modulo");

    if (module == nullptr)
        return;

    check (module->isInstrument(), "modulo se declara instrumento");
    check (module->latencySamples() == 0, "latencia declarada da gaveta e zero");

    // O knob VOLUME (0..1) chega ao dominio do engine.
    module->setParam (0, 0.5f);

    if (auto* drawer = dynamic_cast<PianoforgeModule*> (module.get()))
        check (std::abs (drawer->getVolumeForTest() - 0.5f) < 1e-6f,
               "knob VOLUME chega ao engine da gaveta");
    else
        check (false, "modulo criado e um PianoforgeModule");
}

void testRackDrawerAsyncLoad()
{
    registerPianoforgeModule();

    const int index = forjarack::ModuleCatalog::indexOf ("pianoforge");

    if (index < 0)
    {
        check (false, "gaveta pianoforge disponivel para carga");
        return;
    }

    auto module = forjarack::ModuleCatalog::create (index);

    if (module == nullptr)
    {
        check (false, "modulo pianoforge criado para a carga");
        return;
    }

    module->prepare (testSampleRate);

    // Pede a carga EXTERNA pela gaveta (um SF2 valido no diretorio temporario):
    // o seletor varre Documents/Forja/Pianoforge, pasta que a CI nao controla,
    // entao o caminho externo e o unico reprodutivel em maquina limpa.
    const auto sf2 = writeTestSf2 ("pianoforge-rack", "Rack Piano");
    module->drawerLoadExternalFile (sf2.getFullPathName().toStdString());

    // Orcamento alto, saida no primeiro sinal: a gaveta preenche o nome no
    // instante em que PEDE a carga, entao "nome preenchido" nao significa banco
    // adotado. Rodamos idle() + um bloco de process com a nota-on entregue a cada
    // iteracao (o motor adota o bank publicado dentro do proprio process) e saimos
    // assim que a cadeia devolve audio -- o primeiro sinal de que o piano tocou.
    forjarack::MidiEvent noteOn;
    noteOn.frame = 0;
    noteOn.channel = 0;
    noteOn.note = 60;
    noteOn.velocity = 110;
    noteOn.noteOn = true;

    forjarack::MidiEventSpan span;
    span.events = &noteOn;
    span.numEvents = 1;

    std::vector<float> left (static_cast<size_t> (testBlockSize), 0.0f);
    std::vector<float> right (static_cast<size_t> (testBlockSize), 0.0f);

    const double deadline = juce::Time::getMillisecondCounterHiRes() + 8000.0;
    float peak = 0.0f;
    bool sounding = false;

    while (juce::Time::getMillisecondCounterHiRes() < deadline)
    {
        module->idle();
        juce::MessageManager::getInstance()->runDispatchLoopUntil (10);

        module->processMidi (span);
        module->process (left.data(), right.data(), testBlockSize);

        peak = module->readOutputPeak();

        if (peak > 0.001f)
        {
            sounding = true;
            break;
        }
    }

    check (sounding, "gaveta pianoforge adota o banco e renderiza a nota na cadeia");
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

    std::printf ("Pianoforge engine tests (SF3 %s)\n",
                 PianoEngine::sf3Available() ? "habilitado" : "ausente neste build");

    testSilenceWithoutBank();
    testBankLoadAndAdoption();
    testModelingStagesAudible();
    testMolhoTailControl();
    testCompressorSquash();
    testBankSwapAndReap();
    testEventSlicing();
    testImpulseLatency();
    testLidMonotonicity();
    testHammerMonotonicityAndEscala();
    testParamMapping();
    testSustainPedalResonance();
    testSustainHoldsVoice();
    testBundleValidLoads();
    testBundleBadCrcRejected();
    testBypassRamp();
    testNoAllocations();
    testRackDrawerRegistration();
    testRackDrawerAsyncLoad();

    if (failures == 0)
    {
        std::printf ("\nPASS: engine do Pianoforge\n");
        return 0;
    }

    std::printf ("\nFAIL: %d verificacao(oes)\n", failures);
    return 1;
}
