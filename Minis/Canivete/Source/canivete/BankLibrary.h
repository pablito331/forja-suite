#pragma once

#include <juce_core/juce_core.h>

namespace forja::canivete
{

// Biblioteca do CANIVETE: Documents/Forja/Canivete. O usuario deposita ai os
// banks .sf2 (e .sf3 quando o build traz stb_vorbis); o knob BANK da UI e a
// gaveta listam esta pasta na ordem alfabetica. Nada e copiado nem cacheado:
// o arquivo aberto e lido direto do lugar, em thread de background.

inline juce::File libraryDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
        .getChildFile ("Forja")
        .getChildFile ("Canivete");
}

// Thread de mensagens. Lista os banks disponiveis; includeSf3 sai de
// SoundFontEngine::sf3Available() para a ordem dos indices bater com o que o
// player realmente consegue abrir.
inline juce::Array<juce::File> findBanks (bool includeSf3)
{
    const auto directory = libraryDirectory();

    if (! directory.isDirectory())
        return {};

    auto banks = directory.findChildFiles (juce::File::findFiles, false, "*.sf2");

    if (includeSf3)
        banks.addArray (directory.findChildFiles (juce::File::findFiles, false, "*.sf3"));

    banks.sort();
    return banks;
}

// Garante que a pasta existe (a UI explica este caminho ao usuario).
inline void ensureLibraryDirectory()
{
    auto directory = libraryDirectory();

    if (! directory.isDirectory())
        directory.createDirectory();
}

} // namespace forja::canivete
