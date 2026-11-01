#pragma once

#include <juce_core/juce_core.h>

#include <algorithm>
#include <array>
#include <utility>
#include <vector>

#include "Mapping.h"

namespace forja::drumforge
{

// Cada pad guarda camadas de velocity e, dentro de cada camada, round-robins.
// O sampler escolhe a camada pela velocity e gira os round-robins por disparo.
struct Pad
{
    juce::String name;
    juce::String type { "perc" };
    int note = -1;
    int chokeGroup = 0;
    std::vector<std::vector<juce::String>> layers;

    bool hasSamples() const noexcept
    {
        for (const auto& layer : layers)
            if (! layer.empty())
                return true;

        return false;
    }

    int layerCount() const noexcept
    {
        return (int) layers.size();
    }

    int sampleCount() const noexcept
    {
        int total = 0;
        for (const auto& layer : layers)
            total += (int) layer.size();

        return total;
    }
};

// Mix de um pad, guardada no proprio kit para ele chegar premixado. Os valores
// sao os normalizados 0-1 (passo 0,01) iguais aos do APVTS; o escalonamento em
// dB/Hz/ratio continua no engine. Nao entram mute/solo/fase nem a escolha de
// saida: sao decisao de sessao, nao de som do kit.
struct PadMixer
{
    float trim = 0.50f;
    float fader = 0.91f;
    float pan = 0.50f;
    float hpf = 0.00f;
    float eqLow = 0.50f;
    float eqMid = 0.50f;
    float eqHigh = 0.50f;
    float compThr = 0.70f;
    float compRatio = 0.10f;
    float send1 = 0.00f;
    float send2 = 0.00f;
};

struct MasterMixer
{
    float trim = 0.50f;
    float comp = 0.00f;
    float fx1Regen = 0.50f;
    float fx1Position = 1.00f;
    float fx2Drive = 0.20f;
};

struct Kit
{
    juce::String name { "Kit vazio" };
    juce::String description;
    juce::String sourceFile;

    // Imagem de preview do kit. No drum_sklad/drumlabooh e convencao de nome
    // fixo (image.png/jpg na pasta do kit); o .dfkit carrega em linha #image.
    juce::String imageFile;

    std::array<Pad, kNumPads> pads {};

    // Mix embutida no kit (formato nativo .dfkit). hasMix so e true quando o
    // arquivo traz as linhas #mix; um kit de texto do drumlabooh ou uma pasta
    // crua chega sem mix e o host mantem o que estiver nos knobs.
    bool hasMix = false;
    std::array<PadMixer, kNumPads> mixers {};
    MasterMixer master;

    int padsWithAudio() const noexcept
    {
        int total = 0;
        for (const auto& pad : pads)
            if (pad.hasSamples())
                ++total;

        return total;
    }

    int sampleCount() const noexcept
    {
        int total = 0;
        for (const auto& pad : pads)
            total += pad.sampleCount();

        return total;
    }

    int layerCount() const noexcept
    {
        int total = 0;
        for (const auto& pad : pads)
            total = juce::jmax (total, pad.layerCount());

        return total;
    }

    int roundRobinCount() const noexcept
    {
        int total = 0;
        for (const auto& pad : pads)
            for (const auto& layer : pad.layers)
                total = juce::jmax (total, (int) layer.size());

        return total;
    }

    juce::File baseDirectory() const noexcept
    {
        return sourceFile.isNotEmpty() ? juce::File (sourceFile).getParentDirectory()
                                       : juce::File();
    }

    static Kit createDefault();
    static Kit createFromSampleFolder (const juce::File& folder, juce::String& error);

    // Kits de texto do Drumlabooh (drum_sklad): aceitam a pasta do kit ou o
    // proprio drumkit.txt. createFromSampleFolder delega para aqui quando a
    // pasta tem o arquivo, entao o botao IMPORTAR PASTA e o arrastar e soltar
    // herdam o mapa MIDI e as camadas declaradas pelo autor do kit.
    static bool folderHasTxtKit (const juce::File& folder);
    static bool isTxtKitFile (const juce::File& file);
    static Kit createFromDrumlaboohTxt (const juce::File& fileOrFolder, juce::String& error);

    static Kit createFromXml (const juce::XmlElement& xml, const juce::File& baseDirectory);
    std::unique_ptr<juce::XmlElement> createXml (const juce::File& baseDirectory) const;

    // Formato nativo .dfkit em texto, na mesma linguagem do drumlabooh mais uns
    // marcadores Forja (@pad, ~choque) para o round-trip ser sem perdas. Com
    // withMix, grava tambem a mix de cada pad e do master como linhas de
    // comentario (#mix@N / #master), que outro leitor do drumlabooh so ignora.
    juce::String createText (const juce::File& baseDirectory, bool withMix = false) const;
    static Kit createFromText (const juce::String& text, const juce::File& baseDirectory,
                               const juce::String& fallbackName);

    static bool loadFromFile (const juce::File& file, Kit& kit, juce::String& error);
    // Grava .dfkit em texto; com hasMix, inclui a mix como linhas de comentario.
    bool saveToFile (const juce::File& file, juce::String& error) const;

    // Pacote portavel: cria |bundleDirectory| (pasta com o nome do kit), copia
    // para dentro os samples (em samples/) e a imagem, e grava o .dfkit la com
    // caminhos relativos a propria pasta. O kit passa a nao depender dos
    // arquivos originais nem de onde eles estavam.
    bool saveAsBundle (const juce::File& bundleDirectory, juce::String& error) const;
};

namespace kitdetail
{

inline juce::String relativize (const juce::File& base, const juce::File& file)
{
    if (! base.isDirectory())
        return file.getFullPathName();

    const auto relative = file.getRelativePathFrom (base);
    return relative.startsWith ("..") ? file.getFullPathName() : relative;
}

inline juce::File resolve (const juce::File& base, const juce::String& path)
{
    const juce::File file (path);
    if (juce::File::isAbsolutePath (path))
        return file;

    return base.getChildFile (path);
}

// Convencao do drum_sklad/drumlabooh: image.png/jpg/jpeg na pasta do kit e a
// foto de preview. Nome fixo, png tem prioridade; devolve vazio se nao ha.
inline juce::String previewImageIn (const juce::File& folder)
{
    if (! folder.isDirectory())
        return {};

    for (const auto& name : juce::StringArray { "image.png", "image.jpg", "image.jpeg" })
    {
        const auto candidate = folder.getChildFile (name);

        if (candidate.existsAsFile())
            return candidate.getFullPathName();
    }

    return {};
}

inline juce::String classify (const juce::String& fileName)
{
    const auto lower = fileName.toLowerCase();

    if (lower.contains ("kick") || lower.contains ("bumbo") || lower.contains ("bd"))
        return "kick";
    if (lower.contains ("snare") || lower.contains ("caixa") || lower.contains ("sn"))
        return "snare";
    if (lower.contains ("open") && lower.contains ("hat"))
        return "openhat";
    if (lower.contains ("hat") || lower.contains ("hh") || lower.contains ("chimbal"))
        return "hat";
    if (lower.contains ("floor"))
        return "floor";
    if (lower.contains ("tom") || lower.contains ("tm"))
        return "tom";
    if (lower.contains ("crash"))
        return "crash";
    if (lower.contains ("ride") || lower.contains ("condu"))
        return "ride";
    if (lower.contains ("rim") || lower.contains ("aro"))
        return "rim";
    if (lower.contains ("clap") || lower.contains ("palma"))
        return "clap";

    return "perc";
}

inline int padIndexFor (const juce::String& type) noexcept
{
    if (type == "kick")    return 0;
    if (type == "snare")   return 1;
    if (type == "tom")     return 2;
    if (type == "floor")   return 4;
    if (type == "hat")     return 5;
    if (type == "openhat") return 6;
    if (type == "crash")   return 7;
    if (type == "ride")    return 8;
    if (type == "rim")     return 9;
    if (type == "clap")    return 10;

    return 11;
}

struct SampleTags
{
    int velocityLayer = 0;    // 1..kMaxLayers; 0 = nao informado no nome
    int roundRobin = 0;       // 1..kMaxRoundRobins; 0 = nao informado no nome
};

inline int numberAfter (const juce::String& token, const juce::String& prefix) noexcept
{
    if (! token.startsWith (prefix))
        return 0;

    const auto digits = token.substring (prefix.length());

    if (digits.isEmpty() || digits.length() > 2 || ! digits.containsOnly ("0123456789"))
        return 0;

    return digits.getIntValue();
}

// Bibliotecas de bateria marcam camada de velocidade e round-robin no proprio
// nome do arquivo: Kick_vl2_rr1.wav, Snare_rr3.wav, Tom1_v3.wav. O sufixo mais
// especifico vence, para "vel" nao ser lido como "v".
inline SampleTags parseTags (const juce::String& fileName)
{
    SampleTags tags;

    auto tokens = juce::StringArray::fromTokens (fileName.toLowerCase(), " _-.#()[]", "\"");
    tokens.removeEmptyStrings();

    for (const auto& token : tokens)
    {
        if (tags.velocityLayer == 0)
        {
            int layer = numberAfter (token, "vl");

            if (layer == 0) layer = numberAfter (token, "vel");
            if (layer == 0) layer = numberAfter (token, "layer");
            if (layer == 0) layer = numberAfter (token, "v");

            if (layer > 0)
                tags.velocityLayer = juce::jlimit (1, kMaxLayers, layer);
        }

        if (tags.roundRobin == 0)
        {
            int roundRobin = numberAfter (token, "rr");

            if (roundRobin == 0) roundRobin = numberAfter (token, "rt");
            if (roundRobin == 0) roundRobin = numberAfter (token, "take");

            if (roundRobin > 0)
                tags.roundRobin = juce::jlimit (1, kMaxRoundRobins, roundRobin);
        }
    }

    return tags;
}

} // namespace kitdetail

// ===========================================================================
// drumkit.txt: formato de texto nativo do Drumlabooh e dos kits do drum_sklad
// (especificacao 1.1). Uma linha por instrumento:
//
//     [36]>Bass Drum=bass/b01.wav,bass/b02.wav
//
//   - o numero entre colchetes e a nota MIDI do mapa embutido no kit;
//   - o primeiro caractere do nome define o modo: '>' round-robin, '*' ordem
//     aleatoria, '^' multicamada em que a velocity so escolhe a camada;
//   - a lista separada por virgula vai da camada mais suave para a mais forte;
//   - um item pode ser um arquivo (em subpasta), um .txt com um nome por linha
//     ou uma pasta com os samples do instrumento.
//
// O formato nao traz mute group, entao o unico choke herdado e o par chimbal
// fechado/aberto, como no kit padrao. O '*' e tratado como round-robin porque
// o motor gira as tomadas em ordem sequencial.
// ===========================================================================
namespace txtkit
{

inline constexpr const char* kFileName = "drumkit.txt";
inline constexpr const char* kLaboohFileName = "drumkit.labooh";
inline constexpr int kFirstPercNote = 35;
inline constexpr int kLastPercNote = 81;

inline bool isAudioFile (const juce::String& path)
{
    const auto extension = juce::File (path).getFileExtension().toLowerCase();

    return extension == ".wav" || extension == ".aif" || extension == ".aiff"
        || extension == ".flac" || extension == ".ogg" || extension == ".mp3";
}

inline juce::String cleanName (const juce::String& label)
{
    // Os kits escrevem o tamanho do prato entre aspas (Tom "12), que o parser do
    // drumlabooh engole junto com o nome. Aqui as aspas simplesmente saem.
    return label.trim().removeCharacters ("\"").trim();
}

inline std::vector<juce::String> filesFromList (const juce::File& listFile)
{
    std::vector<juce::String> files;

    juce::StringArray lines;
    lines.addLines (listFile.loadFileAsString());

    for (const auto& line : lines)
    {
        const auto name = line.trim().removeCharacters ("\"");

        if (name.isNotEmpty() && isAudioFile (name))
            files.push_back (kitdetail::resolve (listFile.getParentDirectory(), name)
                                 .getFullPathName());
    }

    return files;
}

inline std::vector<juce::String> filesFromFolder (const juce::File& folder)
{
    auto children = folder.findChildFiles (juce::File::findFiles, false,
                                           "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
    children.sort();

    std::vector<juce::String> files;
    for (const auto& child : children)
        files.push_back (child.getFullPathName());

    return files;
}

struct Instrument
{
    int note = -1;
    juce::String name;
    bool roundRobin = false;
    std::vector<juce::String> files;
};

// Expande a lista de arquivos de uma peca: itens separados por virgula, cada um
// podendo ser um WAV, um subfolder ou uma lista .txt (um nome por linha). Item
// repetido nao vira tomada duplicada; a ordem da virgula e preservada porque e
// ela que diz qual sample e o mais suave. Compartilhada entre o drumkit.txt e o
// drumkit.labooh, que declaram a peca do mesmo jeito.
inline std::vector<juce::String> expandItems (const juce::String& value, const juce::File& base)
{
    std::vector<juce::String> files;

    juce::StringArray items;
    items.addTokens (value, ",", "\"");

    for (const auto& item : items)
    {
        const auto token = item.trim().removeCharacters ("\"");

        if (token.isEmpty())
            continue;

        const auto file = kitdetail::resolve (base, token.replaceCharacter ('\\', '/'));

        if (file.hasFileExtension ("txt"))
        {
            for (const auto& listed : filesFromList (file))
                files.push_back (listed);
        }
        else if (file.isDirectory())
        {
            for (const auto& inside : filesFromFolder (file))
                files.push_back (inside);
        }
        else if (isAudioFile (file.getFileName()))
        {
            files.push_back (file.getFullPathName());
        }
    }

    std::vector<juce::String> unique;

    for (const auto& file : files)
        if (std::find (unique.begin(), unique.end(), file) == unique.end())
            unique.push_back (file);

    return unique;
}

inline bool parseLine (const juce::String& rawLine, const juce::File& base, Instrument& out)
{
    const auto equals = rawLine.indexOf ("=");

    if (equals <= 0)
        return false;

    out = {};

    auto label = rawLine.substring (0, equals).trim();
    const auto value = rawLine.substring (equals + 1);

    if (label.startsWith ("["))
    {
        const auto close = label.indexOf ("]");
        const auto digits = close > 1 ? label.substring (1, close) : juce::String();

        if (digits.isNotEmpty() && digits.containsOnly ("0123456789"))
        {
            out.note = digits.getIntValue();
            label = label.substring (close + 1).trim();
        }
    }

    if (out.note < 0 || out.note > 127)
        out.note = -1;

    while (label.startsWith (">") || label.startsWith ("*") || label.startsWith ("^"))
    {
        out.roundRobin = label[0] == '>' || label[0] == '*';
        label = label.substring (1).trim();
    }

    out.name = cleanName (label);

    if (out.name.isEmpty())
        out.name = "Instrumento";

    out.files = expandItems (value, base);

    return ! out.files.empty();
}

// Nome do instrumento decide o pad quando a linha nao traz nota. A tabela cobre
// as variantes em ingles e portugues que aparecem nos kits do drum_sklad e cai
// no classificador de nomes da varredura de pasta no que sobrar.
inline juce::String classify (const juce::String& instrumentName)
{
    const auto lower = instrumentName.toLowerCase().removeCharacters ("-_. ");

    if (lower.contains ("kick") || lower.contains ("bassdrum") || lower.contains ("bumbo")
        || lower == "bd")
        return "kick";
    if (lower.contains ("rim") || lower.contains ("crossstick")
        || (lower.contains ("side") && lower.contains ("stick")))
        return "rim";
    if (lower.contains ("snare") || lower.contains ("caixa"))
        return "snare";
    if (lower.contains ("open") && lower.contains ("hat"))
        return "openhat";
    if (lower.contains ("hat") || lower.contains ("chimbal"))
        return "hat";
    if (lower.contains ("floor") || lower.contains ("surdo"))
        return "floor";
    if (lower.contains ("crash") || lower.contains ("cymbal") || lower.contains ("prato"))
        return "crash";
    if (lower.contains ("ride") || lower.contains ("condu"))
        return "ride";
    if (lower.contains ("tom") || lower.contains ("drum"))
        return "tom";
    if (lower.contains ("clap") || lower.contains ("palma"))
        return "clap";

    return kitdetail::classify (instrumentName);
}

inline int defaultNote (const juce::String& type) noexcept
{
    if (type == "kick")    return 36;
    if (type == "snare")   return 38;
    if (type == "tom")     return 50;
    if (type == "floor")   return 43;
    if (type == "hat")     return 42;
    if (type == "openhat") return 46;
    if (type == "crash")   return 49;
    if (type == "ride")    return 51;
    if (type == "rim")     return 37;
    if (type == "clap")    return 39;

    return 56;
}

// Pad reservado sem nota precisa de uma nota propria para ficar tocavel: varre
// a faixa de percussao do GM comecando pelo padrao do tipo.
inline int pickNote (const Kit& kit, const juce::String& type)
{
    const auto range = kLastPercNote - kFirstPercNote + 1;
    const auto offset = juce::jmax (0, defaultNote (type) - kFirstPercNote);

    for (int step = 0; step < range; ++step)
    {
        const int note = kFirstPercNote + (offset + step) % range;

        bool used = false;
        for (const auto& pad : kit.pads)
            used = used || pad.note == note;

        if (! used)
            return note;
    }

    return -1;
}

inline bool hasPath (const Pad& pad, const juce::String& path)
{
    for (const auto& layer : pad.layers)
        for (const auto& existing : layer)
            if (existing == path)
                return true;

    return false;
}

inline std::vector<std::vector<juce::String>> layersFor (const Instrument& ins)
{
    std::vector<std::vector<juce::String>> layers;

    if (ins.files.empty())
        return layers;

    if (ins.roundRobin)
    {
        std::vector<juce::String> takes;

        for (const auto& file : ins.files)
        {
            if ((int) takes.size() >= kMaxRoundRobins)
                break;

            takes.push_back (file);
        }

        layers.push_back (std::move (takes));
        return layers;
    }

    const int total = (int) ins.files.size();
    const int groups = juce::jmin (kMaxLayers, total);

    // Mais camadas do que o motor suporta: agrupa em blocos contiguos do mais
    // suave ao mais forte; as sobras do bloco viram round-robin daquela camada.
    for (int group = 0; group < groups; ++group)
    {
        const int from = (group * total) / groups;
        const int to = ((group + 1) * total) / groups;

        std::vector<juce::String> layer;

        for (int i = from; i < to && i < from + kMaxRoundRobins; ++i)
            layer.push_back (ins.files[(size_t) i]);

        layers.push_back (std::move (layer));
    }

    return layers;
}

inline void mergeLayers (Pad& pad, const std::vector<std::vector<juce::String>>& layers)
{
    if (pad.layers.empty())
    {
        pad.layers = layers;
        return;
    }

    for (const auto& layer : layers)
    {
        std::vector<juce::String> fresh;

        for (const auto& path : layer)
            if (! hasPath (pad, path))
                fresh.push_back (path);

        if (fresh.empty())
            continue;

        if ((int) pad.layers.size() < kMaxLayers)
        {
            pad.layers.push_back (std::move (fresh));
            continue;
        }

        auto& last = pad.layers.back();

        for (const auto& path : fresh)
            if ((int) last.size() < kMaxRoundRobins)
                last.push_back (path);
    }
}

// Nota ja usada por outro pad. Os pads livres do mapa GM entram na conta para
// o kit de texto nao criar duas pecas tocando pela mesma nota.
inline bool noteTaken (const Kit& kit, int note, int exceptIndex = -1) noexcept
{
    if (note < 0)
        return false;

    for (int index = 0; index < kNumPads; ++index)
        if (index != exceptIndex && kit.pads[(size_t) index].note == note)
            return true;

    return false;
}

// Escolhe o pad de um instrumento em estagios, do mais fiel ao mais generico.
// Rodar por estagio (e nao por linha) faz o mapa do kit vencer a ordem do
// arquivo: 1 = a mesma peca em outra linha, 2 = a nota declarada pelo kit,
// 3 = o pad padrao do tipo classificado, 4 = qualquer pad livre (claimFreePad).
inline int choosePad (Kit& kit, const Instrument& ins, const juce::String& type, int stage)
{
    auto claim = [&ins, &type, &kit] (Pad& pad, int index)
    {
        pad.name = ins.name;
        pad.type = type;
        pad.chokeGroup = (type == "hat" || type == "openhat") ? 1 : 0;

        // O mapa do kit manda, mas nunca empata duas pecas na mesma nota.
        if (ins.note >= 0 && ! noteTaken (kit, ins.note, index))
            pad.note = ins.note;
    };

    for (int index = 0; index < kNumPads; ++index)
    {
        const auto& pad = kit.pads[(size_t) index];

        if (pad.name.equalsIgnoreCase (ins.name)
            || (pad.hasSamples() && ins.note >= 0 && pad.note == ins.note))
            return index;
    }

    if (stage == 2 && ins.note >= 0)
    {
        for (int index = 0; index < kNumPads; ++index)
        {
            auto& pad = kit.pads[(size_t) index];

            if (! pad.hasSamples() && pad.note == ins.note)
            {
                claim (pad, index);
                return index;
            }
        }
    }

    if (stage == 3)
    {
        for (int index = 0; index < kNumPads; ++index)
        {
            auto& pad = kit.pads[(size_t) index];

            if (! pad.hasSamples() && pad.name.isNotEmpty() && pad.type == type)
            {
                claim (pad, index);
                return index;
            }
        }
    }

    return -1;
}

// O que sobrou entra em qualquer pad livre, comecando pelos do mapa GM ainda
// sem samples e terminando nos quatro reservados. A nota do pad so muda quando
// o instrumento pede uma nota que ainda esteja livre. -1 = kit sem espaco.
inline int claimFreePad (Kit& kit, const Instrument& ins, const juce::String& type)
{
    for (int index = 0; index < kNumPads; ++index)
    {
        auto& pad = kit.pads[(size_t) index];

        if (pad.hasSamples() || pad.name.isEmpty())
            continue;

        pad.name = ins.name;
        pad.type = type;
        pad.chokeGroup = (type == "hat" || type == "openhat") ? 1 : 0;

        if (ins.note >= 0 && ! noteTaken (kit, ins.note, index))
            pad.note = ins.note;

        return index;
    }

    for (int index = 0; index < kNumPads; ++index)
    {
        auto& pad = kit.pads[(size_t) index];

        if (pad.hasSamples())
            continue;

        pad.name = ins.name;
        pad.type = type;
        pad.chokeGroup = (type == "hat" || type == "openhat") ? 1 : 0;
        pad.note = ins.note >= 0 && ! noteTaken (kit, ins.note, index) ? ins.note
                                                                      : pickNote (kit, type);
        return index;
    }

    return -1;
}

inline juce::String readInfo (const juce::File& folder)
{
    const auto info = folder.getChildFile ("info.txt");

    if (! info.existsAsFile())
        return {};

    juce::StringArray lines;
    lines.addLines (info.loadFileAsString());

    juce::StringArray kept;
    for (const auto& line : lines)
    {
        const auto text = line.trim();
        if (text.isNotEmpty())
            kept.add (text);
    }

    auto description = kept.joinIntoString (" ");

    if (description.length() > 220)
        description = description.substring (0, 220).trim();

    return description;
}

// Aloca as pecas nos pads em estagios e devolve quantas nao coubaram. Rodar por
// estagio (e nao por linha) faz o mapa do kit vencer a ordem do arquivo.
inline int assignInstruments (Kit& kit, std::vector<Instrument> waiting)
{
    for (int stage = 1; stage <= 3 && ! waiting.empty(); ++stage)
    {
        std::vector<Instrument> next;

        for (const auto& ins : waiting)
        {
            const auto type = classify (ins.name);
            const auto index = choosePad (kit, ins, type, stage);

            if (index >= 0)
                mergeLayers (kit.pads[(size_t) index], layersFor (ins));
            else
                next.push_back (ins);
        }

        waiting = std::move (next);
    }

    int dropped = 0;

    for (const auto& ins : waiting)
    {
        const auto type = classify (ins.name);
        const auto index = claimFreePad (kit, ins, type);

        if (index < 0)
            ++dropped;
        else
            mergeLayers (kit.pads[(size_t) index], layersFor (ins));
    }

    return dropped;
}

inline int collect (const juce::File& kitFile, Kit& kit)
{
    juce::StringArray lines;
    lines.addLines (kitFile.loadFileAsString());

    const auto folder = kitFile.getParentDirectory();
    std::vector<Instrument> waiting;

    for (const auto& rawLine : lines)
    {
        const auto line = rawLine.trim();

        if (line.isEmpty() || line.startsWith ("#") || line.startsWith ("//"))
            continue;

        Instrument ins;

        if (parseLine (line, folder, ins))
            waiting.push_back (std::move (ins));
    }

    return assignInstruments (kit, std::move (waiting));
}

// Um <sample name note layer_index_mode> do drumkit.labooh vira o mesmo
// Instrument que a linha do drumkit.txt: nome, nota e lista de arquivos. Sem
// mute group lido (o choke fica so no par chimbal fechado/aberto).
inline bool parseLaboohSample (const juce::XmlElement& element, const juce::File& base,
                               Instrument& out, bool kitAlternates)
{
    if (! element.hasTagName ("sample"))
        return false;

    out = {};

    out.name = cleanName (element.getStringAttribute ("name"));

    if (out.name.isEmpty())
        out.name = "Instrumento";

    if (element.hasAttribute ("note"))
    {
        out.note = element.getIntAttribute ("note", -1);

        if (out.note < 0 || out.note > 127)
            out.note = -1;
    }

    const auto mode = element.getStringAttribute ("layer_index_mode").toLowerCase();
    out.roundRobin = (mode == "alt" || mode == "random")
                     || (mode.isEmpty() && kitAlternates);

    out.files = expandItems (element.getAllSubText(), base);

    return ! out.files.empty();
}

inline int collectLabooh (const juce::File& kitFile, Kit& kit)
{
    const auto folder = kitFile.getParentDirectory();

    const auto xml = juce::XmlDocument (kitFile.loadFileAsString()).getDocumentElement();

    if (xml == nullptr)
        return 0;

    // <root type="alt"> faz o kit inteiro alternar as camadas (round-robin), a
    // nao ser que o sample declare o proprio layer_index_mode.
    const bool kitAlternates = xml->getStringAttribute ("type").equalsIgnoreCase ("alt");

    std::vector<Instrument> waiting;

    for (auto* child : xml->getChildWithTagNameIterator ("sample"))
    {
        Instrument ins;

        if (parseLaboohSample (*child, folder, ins, kitAlternates))
            waiting.push_back (std::move (ins));
    }

    return assignInstruments (kit, std::move (waiting));
}

// O arquivo que define o kit numa pasta: o drumkit.txt tem prioridade; sem ele
// entra o drumkit.labooh. Retorna drumkit.txt mesmo quando nao existe, para o
// chamador reportar o ausencia com o nome esperado.
inline juce::File definitionIn (const juce::File& folder)
{
    const auto txt = folder.getChildFile (kFileName);
    if (txt.existsAsFile())
        return txt;

    const auto labooh = folder.getChildFile (kLaboohFileName);
    if (labooh.existsAsFile())
        return labooh;

    return txt;
}

inline bool isLaboohFile (const juce::File& file)
{
    return file.hasFileExtension ("labooh");
}

} // namespace txtkit

// Formato nativo .dfkit em texto. A linha de cada pad e a mesma linguagem do
// drumkit.txt do drumlabooh - "[nota] nome=arquivos" - com virgula separando
// camadas de velocity, so que as tomadas de round-robin de uma camada usam '+'
// (o drumlabooh nao tem como dizer "camadas E tomadas na mesma linha"). Os
// marcadores Forja vao na frente do nome: @<pad> trava a posicao do pad e
// ~<grupo> guarda o choke. As linhas de cima, comentadas com '#', sao lidas so
// pelo Drumforge e ignoradas por qualquer outro leitor. Entre esses comentarios,
// #mix@N e #master guardam a mix premixada do kit (valores normalizados 0-1),
// uma linha por pad mais o master; um kit sem essas linhas chega sem mix.
namespace dfkit
{

inline constexpr const char* kMagic = "forja-dfkit";
inline constexpr int kFormatVersion = 1;

inline juce::String sanitizeName (const juce::String& raw)
{
    // Nem '=', virgula, '+' nem os marcadores podem sobreviver dentro do nome:
    // eles quebrariam a linha. Nome vazio nunca acontece (pad sem sample nao sai).
    auto cleaned = raw.trim().replaceCharacter ('=', ' ').replaceCharacter (',', ' ')
                       .replaceCharacter ('+', ' ').replaceCharacter ('[', ' ')
                       .replaceCharacter (']', ' ').replaceCharacter ('@', ' ')
                       .replaceCharacter ('~', ' ').replaceCharacter ('#', ' ')
                       .replaceCharacter ('"', ' ').replaceCharacter ('\n', ' ')
                       .replaceCharacter ('\r', ' ').trim();

    if (cleaned.isEmpty())
        cleaned = "Pad";

    return cleaned;
}

inline juce::String oneLine (const juce::String& raw)
{
    return raw.replace ("\r\n", " ").replace ("\n", " ").replace ("\r", " ").trim();
}

inline juce::String itemToken (const juce::File& base, const juce::String& path)
{
    return kitdetail::relativize (base, juce::File (path))
        .replaceCharacter ('\\', '/');
}

// Le os digitos que comecam em |start|; devolve o numero e avanca |cursor|.
inline int readDigits (const juce::String& text, int& cursor)
{
    int value = 0;
    int seen = 0;

    while (cursor < text.length() && text[cursor] >= '0' && text[cursor] <= '9')
    {
        value = value * 10 + (text[cursor] - '0');
        ++cursor;
        ++seen;
    }

    return seen > 0 ? value : -1;
}

inline juce::String mixValueToken (float value)
{
    return juce::String (juce::jlimit (0.0, 1.0, (double) value), 2);
}

// Uma linha "#mix@N chave=valor ..." guarda a mix do pad. Os nomes sao fixos e
// batem com as chaves do #master; o que faltar na linha fica no neutro do
// PadMixer. Como e comentario '#', um leitor do drumlabooh so descarta a linha.
inline juce::String mixToken (const char* key, float value)
{
    return juce::String (key) + "=" + mixValueToken (value);
}

inline juce::String serialize (const Kit& kit, const juce::File& base, bool withMix)
{
    juce::StringArray lines;
    lines.add (juce::String ("#") + kMagic + " " + juce::String (kFormatVersion));
    lines.add ("#name " + oneLine (kit.name));
    lines.add ("#desc " + oneLine (kit.description));

    if (kit.imageFile.isNotEmpty())
        lines.add ("#image " + itemToken (base, kit.imageFile));

    lines.add ("");

    for (int index = 0; index < kNumPads; ++index)
    {
        const auto& pad = kit.pads[(size_t) index];

        if (! pad.hasSamples())
            continue;

        juce::String label;

        if (pad.note >= 0)
            label += "[" + juce::String (pad.note) + "]";

        label += "@" + juce::String (index);

        if (pad.chokeGroup > 0)
            label += "~" + juce::String (pad.chokeGroup);

        label += " " + sanitizeName (pad.name) + "=";

        juce::StringArray layers;

        for (const auto& layer : pad.layers)
        {
            if (layer.empty())
                continue;

            juce::StringArray takes;

            for (const auto& path : layer)
                takes.add (itemToken (base, path));

            layers.add (takes.joinIntoString ("+"));
        }

        lines.add (label + layers.joinIntoString (","));

        if (withMix)
        {
            const auto& mix = kit.mixers[(size_t) index];

            lines.add ("#mix@" + juce::String (index)
                       + " " + mixToken ("trim", mix.trim)
                       + " " + mixToken ("fader", mix.fader)
                       + " " + mixToken ("pan", mix.pan)
                       + " " + mixToken ("hpf", mix.hpf)
                       + " " + mixToken ("eqLow", mix.eqLow)
                       + " " + mixToken ("eqMid", mix.eqMid)
                       + " " + mixToken ("eqHigh", mix.eqHigh)
                       + " " + mixToken ("compThr", mix.compThr)
                       + " " + mixToken ("compRatio", mix.compRatio)
                       + " " + mixToken ("send1", mix.send1)
                       + " " + mixToken ("send2", mix.send2));
        }
    }

    if (withMix)
    {
        const auto& master = kit.master;

        lines.add ("#master " + mixToken ("trim", master.trim)
                   + " " + mixToken ("comp", master.comp)
                   + " " + mixToken ("fx1Regen", master.fx1Regen)
                   + " " + mixToken ("fx1Position", master.fx1Position)
                   + " " + mixToken ("fx2Drive", master.fx2Drive));
    }

    return lines.joinIntoString ("\n") + "\n";
}

inline void parsePadLine (const juce::String& line, const juce::File& base, Kit& kit)
{
    const auto equals = line.indexOf ("=");

    if (equals <= 0)
        return;

    auto label = line.substring (0, equals).trim();
    const auto items = line.substring (equals + 1);

    int note = -1;
    int pad = -1;
    int choke = 0;

    int cursor = 0;

    while (cursor < label.length())
    {
        while (cursor < label.length() && label[cursor] == ' ')
            ++cursor;

        const auto ch = label[cursor];

        if (ch == '[')
        {
            ++cursor;
            note = readDigits (label, cursor);

            while (cursor < label.length() && label[cursor] != ']')
                ++cursor;

            if (cursor < label.length())
                ++cursor;
        }
        else if (ch == '@')
        {
            ++cursor;
            pad = readDigits (label, cursor);
        }
        else if (ch == '~')
        {
            ++cursor;
            choke = readDigits (label, cursor);
        }
        else if (ch == '>' || ch == '*' || ch == '^')
        {
            ++cursor;
        }
        else
        {
            break;
        }
    }

    if (note < 0 || note > 127)
        note = -1;

    if (choke < 0)
        choke = 0;

    const auto name = txtkit::cleanName (label.substring (cursor));

    std::vector<std::vector<juce::String>> layers;

    juce::StringArray layerTokens;
    layerTokens.addTokens (items, ",", "");

    for (const auto& layerToken : layerTokens)
    {
        std::vector<juce::String> layer;

        juce::StringArray takeTokens;
        takeTokens.addTokens (layerToken, "+", "");

        for (const auto& take : takeTokens)
            for (const auto& file : txtkit::expandItems (take.trim(), base))
                layer.push_back (file);

        if (! layer.empty())
            layers.push_back (std::move (layer));
    }

    if (layers.empty())
        return;

    int target = -1;

    if (pad >= 0 && pad < kNumPads)
        target = pad;
    else if (note >= 0)
        for (int i = 0; i < kNumPads; ++i)
            if (kit.pads[(size_t) i].note == note)
                target = i;

    if (target < 0)
        for (int i = 0; i < kNumPads; ++i)
            if (! kit.pads[(size_t) i].hasSamples())
                target = i;

    if (target < 0)
        return;

    auto& padRef = kit.pads[(size_t) target];
    padRef.name = name.isEmpty() ? juce::String ("Pad") : name;
    padRef.type = txtkit::classify (padRef.name);
    padRef.note = note;
    padRef.chokeGroup = choke;
    padRef.layers = std::move (layers);
}

// Valor 0-1 lido do texto. getDoubleValue usa '.' como decimal (independente do
// locale), que e exatamente o que mixValueToken escreve.
inline float readMixLevel (const juce::String& text)
{
    return (float) juce::jlimit (0.0, 1.0, text.trim().getDoubleValue());
}

// Le uma linha de mix já sem o '#': "mix@N k=v k=v ..." ou "master k=v ...".
// O que nao for chave conhecida e ignorado, para o formato crescer sem quebrar
// kit velho. A linha só marca hasMix quando ha ao menos um k=v.
inline void parseMixLine (const juce::String& body, Kit& kit)
{
    const auto space = body.indexOfChar (' ');

    if (space < 0)
        return;

    const auto head = body.substring (0, space).trim();
    const bool isMaster = head.equalsIgnoreCase ("master");

    int index = -1;

    if (! isMaster)
    {
        if (! head.startsWithIgnoreCase ("mix@"))
            return;

        int cursor = 4;
        index = readDigits (head, cursor);

        if (index < 0 || index >= kNumPads)
            return;
    }

    juce::StringArray tokens;
    tokens.addTokens (body.substring (space + 1), " ", "\"");
    tokens.removeEmptyStrings();

    bool any = false;

    for (const auto& token : tokens)
    {
        const auto eq = token.indexOfChar ('=');

        if (eq <= 0)
            continue;

        const auto key = token.substring (0, eq).trim();
        const float value = readMixLevel (token.substring (eq + 1).trim());
        any = true;

        if (isMaster)
        {
            auto& master = kit.master;

            if (key == "trim") master.trim = value;
            else if (key == "comp") master.comp = value;
            else if (key == "fx1Regen") master.fx1Regen = value;
            else if (key == "fx1Position") master.fx1Position = value;
            else if (key == "fx2Drive") master.fx2Drive = value;
        }
        else
        {
            auto& mix = kit.mixers[(size_t) index];

            if (key == "trim") mix.trim = value;
            else if (key == "fader") mix.fader = value;
            else if (key == "pan") mix.pan = value;
            else if (key == "hpf") mix.hpf = value;
            else if (key == "eqLow") mix.eqLow = value;
            else if (key == "eqMid") mix.eqMid = value;
            else if (key == "eqHigh") mix.eqHigh = value;
            else if (key == "compThr") mix.compThr = value;
            else if (key == "compRatio") mix.compRatio = value;
            else if (key == "send1") mix.send1 = value;
            else if (key == "send2") mix.send2 = value;
        }
    }

    if (any)
        kit.hasMix = true;
}

inline Kit parse (const juce::String& text, const juce::File& base,
                  const juce::String& fallbackName)
{
    Kit kit;
    kit.name = fallbackName.isNotEmpty() ? fallbackName : "Kit";
    kit.sourceFile.clear();
    kit.mixers = {};

    juce::StringArray lines;
    lines.addLines (text);

    for (const auto& rawLine : lines)
    {
        const auto line = rawLine.trim();

        if (line.isEmpty())
            continue;

        if (line.startsWith ("#"))
        {
            const auto body = line.substring (1).trim();

            if (body.startsWithIgnoreCase ("name "))
                kit.name = body.substring (5).trim();
            else if (body.startsWithIgnoreCase ("desc "))
                kit.description = body.substring (5).trim();
            else if (body.startsWithIgnoreCase ("image "))
                kit.imageFile = kitdetail::resolve (base, body.substring (5).trim()).getFullPathName();
            else if (body.startsWithIgnoreCase ("mix@") || body.startsWithIgnoreCase ("master"))
                parseMixLine (body, kit);

            continue;
        }

        if (line.startsWith ("//"))
            continue;

        parsePadLine (line, base, kit);
    }

    return kit;
}

} // namespace dfkit

inline Kit Kit::createDefault()
{
    Kit kit;
    kit.name = "Kit vazio";

    struct Entry { const char* name; const char* type; int note; int choke; };

    // Mapa GM: o kit já nasce tocável quando o usuário importa samples.
    const Entry entries[] = {
        { "KICK",     "kick",    36, 0 },
        { "SNARE",    "snare",   38, 0 },
        { "TOM 1",    "tom",     50, 0 },
        { "TOM 2",    "tom",     47, 0 },
        { "FLOOR",    "floor",   43, 0 },
        { "HI-HAT",   "hat",     42, 1 },
        { "OPEN HAT", "openhat", 46, 1 },
        { "CRASH",    "crash",   49, 0 },
        { "RIDE",     "ride",    51, 0 },
        { "RIM",      "rim",     37, 0 },
        { "CLAP",     "clap",    39, 0 },
        { "PERC",     "perc",    56, 0 }
    };

    int index = 0;
    for (const auto& entry : entries)
    {
        auto& pad = kit.pads[(size_t) index++];
        pad.name = entry.name;
        pad.type = entry.type;
        pad.note = entry.note;
        pad.chokeGroup = entry.choke;
    }

    return kit;
}

inline bool Kit::folderHasTxtKit (const juce::File& folder)
{
    if (! folder.isDirectory())
        return false;

    return folder.getChildFile (txtkit::kFileName).existsAsFile()
        || folder.getChildFile (txtkit::kLaboohFileName).existsAsFile();
}

inline bool Kit::isTxtKitFile (const juce::File& file)
{
    if (file.isDirectory())
        return folderHasTxtKit (file);

    if (! file.existsAsFile())
        return false;

    // drumkit.labooh (XML) e sempre um kit; uma lista .txt avulsa nao e.
    if (txtkit::isLaboohFile (file))
        return file.getFileNameWithoutExtension().equalsIgnoreCase ("drumkit");

    if (! file.hasFileExtension ("txt"))
        return false;

    if (file.getFileNameWithoutExtension().equalsIgnoreCase ("drumkit"))
        return true;

    // Lista de arquivos do kit (snares.txt) nao e um kit: nenhuma linha tem '='.
    juce::StringArray lines;
    lines.addLines (file.loadFileAsString());

    txtkit::Instrument instrument;

    for (const auto& line : lines)
        if (txtkit::parseLine (line.trim(), file.getParentDirectory(), instrument))
            return true;

    return false;
}

inline Kit Kit::createFromDrumlaboohTxt (const juce::File& fileOrFolder, juce::String& error)
{
    error.clear();

    const auto kitFile = fileOrFolder.isDirectory()
                             ? txtkit::definitionIn (fileOrFolder)
                             : fileOrFolder;

    const auto folder = kitFile.getParentDirectory();

    Kit kit = createDefault();
    kit.name = folder.getFileName();
    kit.sourceFile = kitFile.getFullPathName();
    kit.description = txtkit::readInfo (folder);
    kit.imageFile = kitdetail::previewImageIn (folder);

    const bool labooh = txtkit::isLaboohFile (kitFile);

    if (kit.description.isEmpty())
        kit.description = "Kit de texto do Drumlabooh";

    if (! kitFile.existsAsFile())
    {
        error = "Nao encontrei drumkit.txt nem drumkit.labooh em " + folder.getFullPathName();
        return kit;
    }

    const auto dropped = labooh ? txtkit::collectLabooh (kitFile, kit)
                                : txtkit::collect (kitFile, kit);

    if (kit.padsWithAudio() == 0)
    {
        error = "O kit do Drumlabooh nao apontou nenhum sample utilizavel.";
        return kit;
    }

    if (dropped > 0)
        kit.description += " - " + juce::String (dropped)
                         + " instrumento(s) sem espaco nos 16 pads";

    return kit;
}

inline Kit Kit::createFromSampleFolder (const juce::File& folder, juce::String& error)
{
    error.clear();

    if (folderHasTxtKit (folder))
    {
        juce::String txtError;
        auto txtKit = createFromDrumlaboohTxt (folder, txtError);

        // Um drumkit.txt quebrado nao pode esconder os samples da pasta: cai na
        // varredura pelo nome dos arquivos, que sempre monta alguma coisa.
        if (txtKit.sampleCount() > 0)
            return txtKit;
    }

    Kit kit = createDefault();
    kit.sourceFile.clear();
    kit.name = folder.getFileName();
    kit.description = "Kit importado da pasta " + folder.getFileName();
    kit.imageFile = kitdetail::previewImageIn (folder);

    if (! folder.isDirectory())
    {
        error = "Pasta de kit invalida.";
        return kit;
    }

    // Subpastas entram: bibliotecas de bateria (drum_sklad/drumlabooh, Addictive,
    // BFD) guardam uma pasta por peca. O nome do arquivo continua definindo o pad.
    const auto files = folder.findChildFiles (juce::File::findFiles, true,
                                              "*.wav;*.aif;*.aiff;*.flac");
    if (files.isEmpty())
    {
        error = "Nenhum sample (.wav/.aif/.flac) encontrado na pasta ou nas subpastas.";
        return kit;
    }

    // Ordena por nome para que a numeração `vl`/`rr` do usuário continue valendo.
    auto sorted = files;
    sorted.sort();

    std::array<std::vector<juce::File>, kNumPads> perPad;
    int tomCount = 0;

    for (const auto& file : sorted)
    {
        const auto type = kitdetail::classify (file.getFileNameWithoutExtension());

        int index = kitdetail::padIndexFor (type);
        if (type == "tom")
            index = tomCount++ == 0 ? 2 : 3;

        index = juce::jlimit (0, kNumPads - 1, index);
        perPad[(size_t) index].push_back (file);
    }

    for (int padIndex = 0; padIndex < kNumPads; ++padIndex)
    {
        const auto& filesForPad = perPad[(size_t) padIndex];
        if (filesForPad.empty())
            continue;

        auto& pad = kit.pads[(size_t) padIndex];
        pad.layers.clear();

        std::vector<std::pair<const juce::File*, kitdetail::SampleTags>> tagged;
        tagged.reserve (filesForPad.size());
        int layerCount = 0;

        for (const auto& file : filesForPad)
        {
            const auto tags = kitdetail::parseTags (file.getFileNameWithoutExtension());
            layerCount = juce::jmax (layerCount, tags.velocityLayer);
            tagged.push_back ({ &file, tags });
        }

        // Pasta sem marcacao de camada vira uma unica camada com varias tomadas
        // em round-robin, que e o esperado quando o usuario grava alternates.
        pad.layers.resize ((size_t) juce::jlimit (1, kMaxLayers, layerCount));

        for (const auto& entry : tagged)
        {
            const int layerIndex = juce::jlimit (0, (int) pad.layers.size() - 1,
                                                 entry.second.velocityLayer > 0
                                                     ? entry.second.velocityLayer - 1
                                                     : 0);
            auto& layer = pad.layers[(size_t) layerIndex];

            int slot = entry.second.roundRobin > 0 ? entry.second.roundRobin - 1 : 0;

            if (entry.second.roundRobin <= 0)
                while (slot < (int) layer.size() && layer[(size_t) slot].isNotEmpty())
                    ++slot;

            if (slot >= kMaxRoundRobins)
                continue;

            if ((int) layer.size() <= slot)
                layer.resize ((size_t) slot + 1);

            if (layer[(size_t) slot].isEmpty())
                layer[(size_t) slot] = entry.first->getFullPathName();
        }

        while (! pad.layers.empty() && pad.layers.back().empty())
            pad.layers.pop_back();

        // Buracos na numeracao (rr1 e rr3, sem rr2) sao compactados para o
        // kit salvo em XML nao perder a posicao dos round-robins.
        for (auto& layer : pad.layers)
            layer.erase (std::remove_if (layer.begin(), layer.end(),
                                         [] (const juce::String& path) { return path.isEmpty(); }),
                         layer.end());
    }

    if (kit.padsWithAudio() == 0)
        error = "Nao foi possivel classificar nenhum sample pelo nome do arquivo.";

    return kit;
}

inline Kit Kit::createFromXml (const juce::XmlElement& xml, const juce::File& baseDirectory)
{
    Kit kit = createDefault();
    kit.name = xml.getStringAttribute ("name", kit.name);
    kit.description = xml.getStringAttribute ("description", kit.description);
    kit.sourceFile = xml.getStringAttribute ("source");

    for (auto* padXml : xml.getChildWithTagNameIterator ("Pad"))
    {
        const int index = padXml->getIntAttribute ("index", -1);
        if (! juce::isPositiveAndBelow (index, kNumPads))
            continue;

        auto& pad = kit.pads[(size_t) index];
        pad.name = padXml->getStringAttribute ("name", pad.name);
        pad.type = padXml->getStringAttribute ("type", pad.type);
        pad.note = padXml->getIntAttribute ("note", pad.note);
        pad.chokeGroup = padXml->getIntAttribute ("choke", pad.chokeGroup);
        pad.layers.clear();

        for (auto* layerXml : padXml->getChildWithTagNameIterator ("Layer"))
        {
            std::vector<juce::String> layer;

            for (auto* sampleXml : layerXml->getChildWithTagNameIterator ("Sample"))
            {
                const auto path = sampleXml->getStringAttribute ("path");
                if (path.isNotEmpty())
                    layer.push_back (kitdetail::resolve (baseDirectory, path).getFullPathName());
            }

            if (! layer.empty())
                pad.layers.push_back (std::move (layer));
        }
    }

    return kit;
}

inline std::unique_ptr<juce::XmlElement> Kit::createXml (const juce::File& baseDirectory) const
{
    auto xml = std::make_unique<juce::XmlElement> ("DrumforgeKit");
    xml->setAttribute ("version", 1);
    xml->setAttribute ("name", name);
    xml->setAttribute ("description", description);
    xml->setAttribute ("source", sourceFile);

    for (int index = 0; index < kNumPads; ++index)
    {
        const auto& pad = pads[(size_t) index];
        if (! pad.hasSamples())
            continue;

        auto* padXml = xml->createNewChildElement ("Pad");
        padXml->setAttribute ("index", index);
        padXml->setAttribute ("name", pad.name);
        padXml->setAttribute ("type", pad.type);
        padXml->setAttribute ("note", pad.note);
        padXml->setAttribute ("choke", pad.chokeGroup);

        for (const auto& layer : pad.layers)
        {
            if (layer.empty())
                continue;

            auto* layerXml = padXml->createNewChildElement ("Layer");

            for (const auto& path : layer)
            {
                const auto file = juce::File (path);
                auto* sampleXml = layerXml->createNewChildElement ("Sample");
                sampleXml->setAttribute ("path", kitdetail::relativize (baseDirectory, file));
            }
        }
    }

    return xml;
}

inline juce::String Kit::createText (const juce::File& baseDirectory, bool withMix) const
{
    return dfkit::serialize (*this, baseDirectory, withMix);
}

inline Kit Kit::createFromText (const juce::String& text, const juce::File& baseDirectory,
                                const juce::String& fallbackName)
{
    return dfkit::parse (text, baseDirectory, fallbackName);
}

inline bool Kit::loadFromFile (const juce::File& file, Kit& kit, juce::String& error)
{
    error.clear();

    // drumkit.txt do Drumlabooh e a pasta que o contem entram pelo leitor de texto.
    if (isTxtKitFile (file))
    {
        kit = createFromDrumlaboohTxt (file, error);
        return error.isEmpty();
    }

    if (file.isDirectory())
    {
        kit = createFromSampleFolder (file, error);

        if (kit.sampleCount() == 0)
        {
            if (error.isEmpty())
                error = "A pasta nao tem samples utilizaveis.";

            return false;
        }

        return true;
    }

    if (! file.existsAsFile())
    {
        error = "Kit nao encontrado: " + file.getFullPathName();
        return false;
    }

    const auto content = file.loadFileAsString();
    const auto head = content.trimStart();

    // .dfkit e texto no estilo drumlabooh. Um .dfkit antigo em XML (<DrumforgeKit)
    // ainda abre, para nao quebrar os kits ja gravados na biblioteca.
    if (head.startsWith ("<"))
    {
        auto xml = juce::XmlDocument::parse (content);

        if (xml == nullptr || ! xml->hasTagName ("DrumforgeKit"))
        {
            error = "Arquivo de kit invalido.";
            return false;
        }

        kit = createFromXml (*xml, file.getParentDirectory());
    }
    else
    {
        kit = createFromText (content, file.getParentDirectory(),
                              file.getFileNameWithoutExtension());
    }

    kit.sourceFile = file.getFullPathName();

    if (kit.name.isEmpty())
        kit.name = file.getFileNameWithoutExtension();

    if (kit.padsWithAudio() == 0)
    {
        error = "O kit nao tem samples associados.";
        return false;
    }

    return true;
}

inline bool Kit::saveToFile (const juce::File& file, juce::String& error) const
{
    if (! file.hasFileExtension ("dfkit"))
    {
        // Pediram XML explicitamente (compat): grava o formato antigo.
        auto xml = createXml (file.getParentDirectory());

        if (xml == nullptr || ! xml->writeTo (file))
        {
            error = "Nao foi possivel gravar " + file.getFullPathName();
            return false;
        }

        return true;
    }

    // O kit carrega a propria bandeira: captura de mix (getKitForSaving) liga
    // hasMix e o arquivo salvo sai com as linhas #mix@N / #master.
    const auto text = createText (file.getParentDirectory(), hasMix);

    if (! file.getParentDirectory().exists())
        file.getParentDirectory().createDirectory();

    if (! file.replaceWithText (text))
    {
        error = "Nao foi possivel gravar " + file.getFullPathName();
        return false;
    }

    return true;
}

inline bool Kit::saveAsBundle (const juce::File& bundleDirectory, juce::String& error) const
{
    error.clear();

    if (padsWithAudio() == 0)
    {
        error = "O kit nao tem samples para levar no pacote.";
        return false;
    }

    if (bundleDirectory.existsAsFile())
    {
        error = "Ja existe um arquivo com o nome da pasta do kit.";
        return false;
    }

    if (! bundleDirectory.isDirectory() && bundleDirectory.createDirectory().failed())
    {
        error = "Nao foi possivel criar " + bundleDirectory.getFullPathName();
        return false;
    }

    const auto samplesDir = bundleDirectory.getChildFile ("samples");

    Kit packed = *this;   // reescreve os caminhos para dentro do pacote

    // Origem -> destino, para um WAV usado em varios pads nao ser copiado duas
    // vezes. O prefixo com indice guarda nomes iguais de pastas diferentes.
    juce::StringPairArray copied;
    int index = 0;

    auto place = [&] (const juce::File& source, const juce::File& directory,
                      const juce::String& forcedName, juce::String& outError)
    {
        const auto key = source.getFullPathName();

        if (copied.containsKey (key))
            return juce::File (copied[key]);

        if (! source.existsAsFile())
        {
            outError = "Arquivo nao encontrado: " + key;
            return juce::File();
        }

        if (! directory.isDirectory() && directory.createDirectory().failed())
        {
            outError = "Nao foi possivel criar " + directory.getFullPathName();
            return juce::File();
        }

        const auto name = forcedName.isNotEmpty()
                              ? forcedName
                              : juce::String (index).paddedLeft ('_', 4) + "_"
                                + juce::File::createLegalFileName (source.getFileName());

        const auto target = directory.getChildFile (name);

        // Re-salvar o proprio pacote: a origem ja e o destino, nada a copiar.
        if (target.getFullPathName() != key && ! source.copyFileTo (target))
        {
            outError = "Nao foi possivel copiar " + source.getFileName();
            return juce::File();
        }

        copied.set (key, target.getFullPathName());
        ++index;
        return target;
    };

    for (auto& pad : packed.pads)
    {
        for (auto& layer : pad.layers)
        {
            for (auto& path : layer)
            {
                const auto target = place (juce::File (path), samplesDir, {}, error);

                if (target == juce::File())
                {
                    samplesDir.deleteRecursively();
                    bundleDirectory.deleteFile();
                    return false;
                }

                path = target.getFullPathName();
            }
        }
    }

    if (packed.imageFile.isNotEmpty())
    {
        const juce::File image (packed.imageFile);

        if (image.existsAsFile())
        {
            // Normaliza para image.<ext>, a convencao que o drum_sklad le.
            const auto forced = juce::String ("image") + image.getFileExtension();
            const auto target = place (image, bundleDirectory, forced, error);
            packed.imageFile = target != juce::File() ? target.getFullPathName() : juce::String();
        }
        else
        {
            packed.imageFile.clear();
        }
    }

    // A base passa a ser a propria pasta: o texto sai com caminhos relativos.
    packed.sourceFile.clear();

    const auto kitFile = bundleDirectory.getChildFile (bundleDirectory.getFileName() + ".dfkit");

    if (! packed.saveToFile (kitFile, error))
    {
        samplesDir.deleteRecursively();
        bundleDirectory.deleteFile();
        return false;
    }

    return true;
}

} // namespace forja::drumforge
