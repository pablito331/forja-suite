#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace forja::drumforge::loops
{

// Um golpe do loop em beats (nao em ticks nem em segundos): o preview toca em
// qualquer BPM sem reescrever o arquivo.
struct Hit
{
    double beat = 0.0;
    int note = 36;
    float velocity = 0.8f;
};

struct Sequence
{
    juce::String name;
    juce::String file;
    double bpm = 120.0;
    double beats = 4.0;
    std::vector<Hit> hits;
};

struct Entry
{
    juce::File file;
    juce::String name;
    juce::String folder;
    double bpm = 120.0;
    double beats = 4.0;
    int hits = 0;

    juce::String bpmText() const { return juce::String (juce::roundToInt (bpm)); }
    int bars() const { return juce::jmax (1, juce::roundToInt (beats / 4.0)); }
};

inline juce::File libraryDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
        .getChildFile ("Forja")
        .getChildFile ("Drumforge")
        .getChildFile ("Loops");
}

namespace detail
{

inline double findTempoBpm (const juce::MidiFile& midi)
{
    for (int track = 0; track < midi.getNumTracks(); ++track)
    {
        const auto* sequence = midi.getTrack (track);

        if (sequence == nullptr)
            continue;

        for (int event = 0; event < sequence->getNumEvents(); ++event)
        {
            const auto& message = sequence->getEventPointer (event)->message;

            if (message.isTempoMetaEvent())
            {
                const double secondsPerQuarter = message.getTempoSecondsPerQuarterNote();

                if (secondsPerQuarter > 0.0)
                    return juce::jlimit (20.0, 400.0, 60.0 / secondsPerQuarter);
            }
        }
    }

    return 120.0;
}

} // namespace detail

inline bool readSequence (const juce::File& file, Sequence& out, juce::String& error)
{
    juce::FileInputStream stream (file);

    if (! stream.openedOk())
    {
        error = "Nao foi possivel abrir " + file.getFileName();
        return false;
    }

    juce::MidiFile midi;

    if (! midi.readFrom (stream))
    {
        error = file.getFileName() + " nao e um arquivo MIDI valido.";
        return false;
    }

    out = {};
    out.name = file.getFileNameWithoutExtension();
    out.file = file.getFullPathName();
    out.bpm = detail::findTempoBpm (midi);

    const double ticksPerQuarter = midi.getTimeFormat() > 0 ? (double) midi.getTimeFormat() : 0.0;
    const double beatsPerSecond = out.bpm / 60.0;
    double lastBeat = 0.0;

    for (int track = 0; track < midi.getNumTracks(); ++track)
    {
        const auto* events = midi.getTrack (track);

        if (events == nullptr)
            continue;

        for (int index = 0; index < events->getNumEvents(); ++index)
        {
            const auto& timed = *events->getEventPointer (index);
            const auto& message = timed.message;

            if (! message.isNoteOn() || message.getVelocity() <= 0)
                continue;

            // Sem resolucao de ticks (SMPTE), o tempo vem do proprio arquivo.
            const double beat = ticksPerQuarter > 0.0
                                    ? timed.message.getTimeStamp() / ticksPerQuarter
                                    : timed.message.getTimeStamp() * beatsPerSecond;

            Hit hit;
            hit.beat = juce::jmax (0.0, beat);
            hit.note = message.getNoteNumber();
            hit.velocity = juce::jlimit (0.01f, 1.0f, (float) message.getVelocity() / 127.0f);

            out.hits.push_back (hit);
            lastBeat = juce::jmax (lastBeat, hit.beat);
        }
    }

    if (out.hits.empty())
    {
        error = file.getFileName() + " nao tem notas.";
        return false;
    }

    std::stable_sort (out.hits.begin(), out.hits.end(),
                      [] (const Hit& a, const Hit& b) { return a.beat < b.beat; });

    // O loop fecha em compassos inteiros para o preview nao cortar a virada.
    const int bars = juce::jmax (1, (int) std::ceil ((lastBeat + 1.0e-6) / 4.0));
    out.beats = juce::jmax (4.0, bars * 4.0);

    return true;
}

// Caminho da pasta do loop relativo à raiz da biblioteca, sempre com "/"
// (ex.: "Ritmos/Fills"). Vazio para arquivos na raiz. Sem raiz conhecida,
// cai no nome da pasta mae como antes.
inline juce::String folderPathIn (const juce::File& root, const juce::File& parent)
{
    if (root.getFullPathName().isEmpty())
        return parent.getFileName();

    if (parent == root)
        return {};

    auto relative = parent.getRelativePathFrom (root).replaceCharacter ('\\', '/');

    if (relative.startsWith ("..") || relative.isEmpty())
        return parent.getFileName();

    return relative;
}

inline Entry describe (const juce::File& file, const juce::File& root = {})
{
    Entry entry;
    entry.file = file;
    entry.name = file.getFileNameWithoutExtension();
    entry.folder = folderPathIn (root, file.getParentDirectory());

    Sequence sequence;
    juce::String error;

    if (readSequence (file, sequence, error))
    {
        entry.bpm = sequence.bpm;
        entry.beats = sequence.beats;
        entry.hits = (int) sequence.hits.size();
    }

    return entry;
}

// Varre a pasta de loops (e as subpastas de pacotes e categorias) por .mid/.midi.
inline std::vector<Entry> scan (const juce::File& root)
{
    std::vector<Entry> entries;

    auto files = root.findChildFiles (juce::File::findFiles, true, "*.mid;*.midi");
    files.sort();

    entries.reserve ((size_t) files.size());

    for (const auto& file : files)
        entries.push_back (describe (file, root));

    return entries;
}

// Copia um .mid avulso para a biblioteca, mantendo o nome original.
inline juce::File importFile (const juce::File& file, const juce::File& root)
{
    if (! root.isDirectory() && root.createDirectory().failed())
        return {};

    auto target = root.getChildFile (juce::File::createLegalFileName (file.getFileName()));

    if (file.getFullPathName() == target.getFullPathName())
        return target;

    if (! file.copyFileTo (target))
        return {};

    return target;
}

// True se a pasta (ou qualquer subpasta dela) tem .mid/.midi.
inline bool hasMidiFiles (const juce::File& directory)
{
    return directory.findChildFiles (juce::File::findFiles, true, "*.mid;*.midi").size() > 0;
}

namespace detail
{

// Copia a arvore de .mid preservando as subpastas (categorias: Fills, Chorus...).
// Pastas sem MIDI sao puladas; nada alem de .mid/.midi e copiado.
inline int copyMidiTree (const juce::File& source, const juce::File& target)
{
    if (! target.exists() && target.createDirectory().failed())
        return 0;

    int copied = 0;

    for (const auto& file : source.findChildFiles (juce::File::findFiles, false, "*.mid;*.midi"))
    {
        const auto destination = target.getChildFile (juce::File::createLegalFileName (file.getFileName()));

        if (file.getFullPathName() != destination.getFullPathName() && file.copyFileTo (destination))
            ++copied;
    }

    for (const auto& sub : source.findChildFiles (juce::File::findDirectories, false))
    {
        if (! hasMidiFiles (sub))
            continue;

        copied += copyMidiTree (sub, target.getChildFile (juce::File::createLegalFileName (sub.getFileName())));
    }

    return copied;
}

} // namespace detail

// Copia uma pasta inteira para a biblioteca preservando a estrutura de
// categorias. So .mid/.midi vao junto; devolve quantos loops entraram.
inline int importFolder (const juce::File& folder, const juce::File& root)
{
    if (! folder.isDirectory() || ! hasMidiFiles (folder))
        return 0;

    if (! root.isDirectory() && root.createDirectory().failed())
        return 0;

    const auto target = root.getChildFile (juce::File::createLegalFileName (folder.getFileName()));

    return detail::copyMidiTree (folder, target);
}

} // namespace forja::drumforge::loops
