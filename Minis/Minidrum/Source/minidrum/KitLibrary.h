#pragma once

#include <juce_core/juce_core.h>

#include "engine/Kit.h"

namespace forja::minidrum
{

// Biblioteca de kits compartilhada com o Drumforge: os MESMOS lugares de onde
// ele le. O .dfkit vive em Documents/Forja/Drumforge/Kits; as pastes de kit do
// drumlabooh ficam na raiz da conta. O que o Drumforge deixou premixado na
// biblioteca, o MiniDrum toca sem copiar seja o que for.

inline juce::File libraryDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
        .getChildFile ("Forja")
        .getChildFile ("Drumforge")
        .getChildFile ("Kits");
}

// O drumlabooh instala os kits do drum_sklad em pastas proprias da conta.
inline juce::Array<juce::File> txtKitRoots()
{
    const auto home = juce::File::getSpecialLocation (juce::File::userHomeDirectory);

    juce::Array<juce::File> roots = {
        home.getChildFile ("drum_sklad"),
        home.getChildFile ("drumlabooh-kits"),
        home.getChildFile ("drumrox-kits")
    };

    roots.removeIf ([] (const juce::File& root) { return ! root.isDirectory(); });
    return roots;
}

inline void addTxtKitsBelow (const juce::File& root, juce::Array<juce::File>& kits)
{
    for (const auto& child : root.findChildFiles (juce::File::findDirectories, false))
    {
        if (forja::drumforge::Kit::folderHasTxtKit (child))
        {
            kits.addIfNotAlreadyThere (child);
            continue;
        }

        // Arquivo baixado do GitHub costuma trazer um nivel a mais (drum_sklad-main).
        for (const auto& inside : child.findChildFiles (juce::File::findDirectories, false))
            if (forja::drumforge::Kit::folderHasTxtKit (inside))
                kits.addIfNotAlreadyThere (inside);
    }
}

// Thread de mensagens. Lista dos kits que o MiniDrum pode puxar da biblioteca.
inline juce::Array<juce::File> findKits()
{
    auto kits = libraryDirectory().findChildFiles (juce::File::findFiles, true, "*.dfkit");

    for (const auto& root : txtKitRoots())
        addTxtKitsBelow (root, kits);

    kits.sort();
    return kits;
}

} // namespace forja::minidrum
