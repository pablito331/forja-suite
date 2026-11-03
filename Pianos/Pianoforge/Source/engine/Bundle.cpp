#include "Bundle.h"

#include <juce_core/juce_core.h>

#include <array>
#include <cstdint>
#include <cstring>

namespace forja::pianoforge
{

namespace
{
constexpr std::size_t kHeaderSize = 12;
constexpr std::size_t kRecordFixed = 30;   // type + off + size + usize + crc
constexpr std::uint16_t kVersion = 1;
constexpr std::size_t kMaxName = 512;
constexpr std::size_t kMaxEntries = 4096;

// CRC32 refletido (polinomio 0xEDB88320, init/final 0xFFFFFFFF) -- exatamente o
// que o zlib.crc32 do empacotador do projeto-irmao gravou. A suíte nao expoe um
// CRC32 publico (juce nao tem juce::CRC32), entao o laço mora aqui, com a tabela
// de 256 entradas montada uma vez (std::array por valor, sem pendurar ponteiro de
// pilha), fora do tempo real.
inline const std::array<std::uint32_t, 256>& crc32Table()
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

    return table;
}

std::uint32_t crc32Update (std::uint32_t crc, const std::uint8_t* data, std::size_t len) noexcept
{
    const auto& table = crc32Table();

    for (std::size_t i = 0; i < len; ++i)
        crc = table[(crc ^ data[i]) & 0xFFu] ^ (crc >> 8);

    return crc;
}

std::uint32_t crc32Of (const void* data, std::size_t len) noexcept
{
    return crc32Update (0xFFFFFFFFu, static_cast<const std::uint8_t*> (data), len) ^ 0xFFFFFFFFu;
}

bool readU16 (const std::uint8_t* p, std::size_t n, std::size_t& i, std::uint16_t& out)
{
    if (i + 2 > n) return false;
    out = static_cast<std::uint16_t> (p[i]) | static_cast<std::uint16_t> (p[i + 1]) << 8;
    i += 2;
    return true;
}
bool readU32le (const std::uint8_t* p, std::size_t n, std::size_t& i, std::uint32_t& out)
{
    if (i + 4 > n) return false;
    out = static_cast<std::uint32_t> (p[i])
        | static_cast<std::uint32_t> (p[i + 1]) << 8
        | static_cast<std::uint32_t> (p[i + 2]) << 16
        | static_cast<std::uint32_t> (p[i + 3]) << 24;
    i += 4;
    return true;
}
bool readU64le (const std::uint8_t* p, std::size_t n, std::size_t& i, std::uint64_t& out)
{
    if (i + 8 > n) return false;
    out = 0;
    for (int b = 0; b < 8; ++b)
        out |= static_cast<std::uint64_t> (p[i + b]) << (8 * b);
    i += 8;
    return true;
}

bool looksLikeBank (const juce::String& name)
{
    const auto ext = name.substring (name.lastIndexOfChar ('.')).toLowerCase();
    return ext == ".sf2" || ext == ".sf3" || ext == ".sfz";
}

// Recusa caminho absoluto ou com ".." (o mesmo contrato do leitor antigo): um
// entry malicioso nao pode escrever fora do diretorio do bundle.
bool safeRelativeName (const juce::String& name, juce::String& err)
{
    if (name.isEmpty() || name.containsChar('/') || name.startsWithChar ('\\'))
    {
        err = "entry com caminho absoluto ou vazio: '" + name + "'";
        return false;
    }

    if (name.contains (".."))
    {
        err = "entry com \"..\" no caminho: " + name;
        return false;
    }

    return true;
}

// Le uma string de um objeto JSON (via var/NamedValueSet do JUCE 9). Fallback
// vazio quando a chave nao existe ou o objeto e nulo.
juce::String jsonString (const juce::NamedValueSet& props, const char* key)
{
    return props[juce::Identifier (key)].toString();
}

juce::Array<Bundle::Preset> parsePresets (const juce::String& manifest)
{
    juce::Array<Bundle::Preset> out;

    if (manifest.isEmpty())
        return out;

    const auto parsed = juce::JSON::parse (manifest);

    if (! parsed.isObject())
        return out;

    const auto& props = parsed.getDynamicObject()->getProperties();
    const auto& presetsVar = props[juce::Identifier ("presets")];

    if (! presetsVar.isArray())
        return out;

    auto* array = presetsVar.getArray();

    for (const auto& item : *array)
    {
        if (! item.isObject())
            continue;

        const auto& p = item.getDynamicObject()->getProperties();
        Bundle::Preset preset;

        preset.name = jsonString (p, "nome");
        if (preset.name.isEmpty())
            preset.name = jsonString (p, "name");

        preset.image = jsonString (p, "imagem");
        if (preset.image.isEmpty())
            preset.image = jsonString (p, "image");

        preset.sf2 = jsonString (p, "sf2");

        const auto& paramsVar = p[juce::Identifier ("params")];

        if (paramsVar.isObject())
        {
            const auto& pp = paramsVar.getDynamicObject()->getProperties();

            for (int i = 0; i < pp.size(); ++i)
            {
                const auto key = pp.getName (i);
                preset.params.set (key, pp[key]);
            }
        }

        out.add (std::move (preset));
    }

    return out;
}
} // namespace

bool Bundle::open (const juce::File& file, juce::String& err)
{
    entries_.clear();
    manifest_.clear();
    presets_.clear();
    path_ = file;

    if (! file.existsAsFile())
    {
        err = "nao e um arquivo: " + file.getFullPathName();
        return false;
    }

    juce::FileInputStream in (file);

    if (! in.openedOk())
    {
        err = "nao abriu: " + file.getFullPathName();
        return false;
    }

    const std::int64_t fileLen = file.getSize();

    std::uint8_t head[kHeaderSize];

    if (in.read (head, static_cast<int> (kHeaderSize)) != static_cast<std::int64_t> (kHeaderSize))
    {
        err = "arquivo curto demais para ser um .pforge";
        return false;
    }

    if (std::memcmp (head, "PFG1", 4) != 0)
    {
        err = "magic invalido (nao e .pforge)";
        return false;
    }

    std::size_t hi = 4;
    std::uint16_t version = 0, flags = 0;
    std::uint32_t ndir = 0;

    if (! readU16 (head, kHeaderSize, hi, version)
        || ! readU16 (head, kHeaderSize, hi, flags)
        || ! readU32le (head, kHeaderSize, hi, ndir))
    {
        err = "cabecalho truncado";
        return false;
    }

    juce::ignoreUnused (flags);

    if (version != kVersion)
    {
        err = "versao de bundle nao suportada: " + juce::String (version);
        return false;
    }

    if (ndir == 0 || ndir > kMaxEntries)
    {
        err = "diretorio com " + juce::String (ndir) + " entradas";
        return false;
    }

    for (std::uint32_t e = 0; e < ndir; ++e)
    {
        std::uint8_t two[2];

        if (in.read (two, 2) != 2)
        {
            err = "diretorio truncado";
            return false;
        }

        const std::uint16_t nameLen = static_cast<std::uint16_t> (two[0])
                                    | static_cast<std::uint16_t> (two[1]) << 8;

        if (nameLen == 0 || nameLen > kMaxName)
        {
            err = "nome de entry ilegio na entrada " + juce::String (e);
            return false;
        }

        juce::HeapBlock<char> nm (nameLen + 1, false);

        if (in.read (nm.getData(), nameLen) != nameLen)
        {
            err = "nome truncado na entrada " + juce::String (e);
            return false;
        }

        nm[nameLen] = 0;

        std::uint8_t rec[kRecordFixed];

        if (in.read (rec, static_cast<int> (kRecordFixed)) != static_cast<std::int64_t> (kRecordFixed))
        {
            err = "diretorio truncado na entrada " + juce::String (e);
            return false;
        }

        Entry ent;
        ent.name = juce::String::fromUTF8 (nm.getData(), nameLen);

        std::size_t i = 0;
        std::uint16_t type = 0;
        std::uint64_t off = 0, size = 0, usize = 0;
        std::uint32_t crc = 0;

        if (! readU16 (rec, kRecordFixed, i, type)
            || ! readU64le (rec, kRecordFixed, i, off)
            || ! readU64le (rec, kRecordFixed, i, size)
            || ! readU64le (rec, kRecordFixed, i, usize)
            || ! readU32le (rec, kRecordFixed, i, crc))
        {
            err = "registro ilegivel em " + ent.name;
            return false;
        }

        ent.type = type;
        ent.off = off;
        ent.size = size;
        ent.usize = usize;
        ent.crc = crc;

        if (type > kManifest)
        {
            err = "tipo desconhecido (" + juce::String (type) + ") em " + ent.name;
            return false;
        }

        if (size == 0 || off < kHeaderSize || size > static_cast<std::uint64_t> (fileLen)
            || off + size > static_cast<std::uint64_t> (fileLen))
        {
            err = "payload fora do arquivo em " + ent.name;
            return false;
        }

        if (type == kZlib && usize < size)
        {
            err = "tamanho descompactado menor que o gravado em " + ent.name;
            return false;
        }

        if (! safeRelativeName (ent.name, err))
            return false;

        entries_.add (ent);
    }

    // O manifest entra inteiro (texto pequeno); sem ele o bundle ainda funciona,
    // so fica sem nome de produto nem lista de presets.
    const Entry* manifestEntry = nullptr;

    for (const Entry& entry : entries_)
        if (entry.type == kManifest) { manifestEntry = &entry; break; }

    if (manifestEntry != nullptr)
    {
        juce::MemoryBlock bytes;
        juce::String merr;

        if (readEntry (*manifestEntry, bytes, merr))
            manifest_ = juce::String::fromUTF8 (static_cast<const char*> (bytes.getData()),
                                                static_cast<int> (bytes.getSize()));
    }

    presets_ = parsePresets (manifest_);
    return true;
}

const Bundle::Entry* Bundle::bankEntry() const noexcept
{
    if (entries_.isEmpty())
        return nullptr;

    // presets[0].sf2 manda, se houver.
    if (! presets_.isEmpty() && presets_[0].sf2.isNotEmpty())
    {
        const auto& want = presets_[0].sf2;

        for (const Entry& e : entries_)
            if (e.name == want || e.name.endsWithIgnoreCase (want))
                return &e;
    }

    for (const Entry& e : entries_)
        if (looksLikeBank (e.name))
            return &e;

    return nullptr;
}

juce::String Bundle::productName() const
{
    const auto parsed = juce::JSON::parse (manifest_);

    if (parsed.isObject())
    {
        const auto name = jsonString (parsed.getDynamicObject()->getProperties(), "produto");

        if ((name.isEmpty() || name == "PianoForge") && ! presets_.isEmpty())
            return presets_[0].name;

        return name;
    }

    return {};
}

bool Bundle::verifyEntry (const Entry& e, juce::String& err) const
{
    if (e.type != kRaw)
    {
        err = e.name + " nao esta gravado cru";
        return false;
    }

    juce::FileInputStream in (path_);

    if (! in.openedOk() || ! in.setPosition (static_cast<std::int64_t> (e.off)))
    {
        err = "nao abriu " + path_.getFullPathName();
        return false;
    }

    std::uint32_t crc = 0xFFFFFFFFu;
    std::uint64_t left = e.size;
    std::uint8_t window[64 * 1024];

    while (left > 0)
    {
        const std::size_t want = static_cast<std::size_t> (juce::jmin<std::uint64_t> (left, sizeof (window)));
        const std::int64_t got = in.read (window, static_cast<int> (want));

        if (got != static_cast<std::int64_t> (want))
        {
            err = "leitura de " + e.name + " parou no meio";
            return false;
        }

        crc = crc32Update (crc, window, want);
        left -= want;
    }

    crc ^= 0xFFFFFFFFu;

    if (crc != e.crc)
    {
        err = "CRC32 nao bate em " + e.name + ": esperado "
            + juce::String::toHexString (e.crc).paddedLeft ('0', 8)
            + ", veio " + juce::String::toHexString (crc).paddedLeft ('0', 8);
        return false;
    }

    return true;
}

bool Bundle::readEntry (const Entry& e, juce::MemoryBlock& out, juce::String& err) const
{
    juce::FileInputStream in (path_);

    if (! in.openedOk() || ! in.setPosition (static_cast<std::int64_t> (e.off)))
    {
        err = "nao abriu " + path_.getFullPathName();
        return false;
    }

    juce::MemoryBlock stored;
    stored.setSize (static_cast<std::size_t> (e.size));

    if (in.read (stored.getData(), static_cast<int> (e.size)) != static_cast<std::int64_t> (e.size))
    {
        err = "leitura do payload de " + e.name + " falhou";
        return false;
    }

    // O CRC gravado e do payload tal como esta no disco (cru ou compactado); a
    // conta e feita sobre os bytes lidos antes de qualquer descompactacao.
    if (crc32Of (stored.getData(), stored.getSize()) != e.crc)
    {
        err = "CRC32 nao bate em " + e.name + ": esperado "
            + juce::String::toHexString (e.crc).paddedLeft ('0', 8)
            + ", veio " + juce::String::toHexString (crc32Of (stored.getData(), stored.getSize())).paddedLeft ('0', 8);
        return false;
    }

    if (e.type != kZlib)
    {
        out = stored;
        return true;
    }

    // zlib cru (sem cabecalho gzip): o GZIPDecompressorInputStream no modo zlib
    // le exatamente o formato que o bundler do projeto-irmao grava.
    juce::MemoryInputStream source (stored, false);
    juce::GZIPDecompressorInputStream unzip (&source, false,
                                             juce::GZIPDecompressorInputStream::zlibFormat,
                                             static_cast<std::int64_t> (e.usize));

    out.setSize (static_cast<std::size_t> (e.usize));
    const std::int64_t did = unzip.read (out.getData(), static_cast<int> (e.usize));

    if (did != static_cast<std::int64_t> (e.usize))
    {
        err = "tamanho descompactado nao bate em " + e.name;
        out.setSize (0);
        return false;
    }

    return true;
}

bool Bundle::extractBankBytes (juce::MemoryBlock& out, juce::String& err) const
{
    const Entry* e = bankEntry();

    if (e == nullptr)
    {
        err = "bundle sem banco (nem manifest.sf2, nem .sf2/.sf3 dentro)";
        return false;
    }

    if (! looksLikeBank (e->name))
    {
        err = "entry de banco com formato estranho: " + e->name;
        return false;
    }

    // SF3/SFZ dentro do bundle ficam para uma expansao: o caminho cru e o SF2.
    if (e->name.endsWithIgnoreCase (".sfz") || e->name.endsWithIgnoreCase (".sf3"))
    {
        err = "apenas SF2 dentro do bundle e suportado no v1 (" + e->name + ")";
        return false;
    }

    if (e->type == kRaw)
    {
        // O SF2 cru: confere o CRC em janelas e so entao sobe os bytes, sem
        // duplicar o banco duas vezes sem necessidade.
        if (! verifyEntry (*e, err))
            return false;
    }

    return readEntry (*e, out, err);
}

} // namespace forja::pianoforge
