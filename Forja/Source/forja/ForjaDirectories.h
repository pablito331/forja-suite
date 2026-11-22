#pragma once

#include <juce_core/juce_core.h>

namespace forja
{

/**
 * Garante que a estrutura de pastas do ecossistema Forja no diretório de
 * documentos do usuário existe em qualquer sistema operacional (Windows, Linux, macOS).
 *
 * Estrutura criada:
 *   Documents/Forja/
 *     ├── Canivete/           (Soundfonts .sf2 / .sf3)
 *     ├── Drumforge/
 *     │    ├── Kits/          (Kits de bateria .dfkit)
 *     │    └── Loops/         (Arquivos MIDI de grooves/loops)
 *     ├── Pianoforge/         (Soundfonts .sf2 / .sf3 e bundles .pforge)
 *     └── Rack/               (Plugins/presets hospedados no ForjaRack)
 */
inline void ensureForjaDirectories()
{
    const auto docs = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
    if (! docs.isDirectory())
        return;

    const auto forjaRoot = docs.getChildFile ("Forja");

    const juce::StringArray subdirs = {
        "Canivete",
        "Drumforge/Kits",
        "Drumforge/Loops",
        "Pianoforge",
        "Rack"
    };

    for (const auto& sub : subdirs)
    {
        auto dir = forjaRoot.getChildFile (sub);
        if (! dir.isDirectory())
            dir.createDirectory();
    }
}

} // namespace forja
